/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/random.h"
#include "common/textconsole.h"

#include "castle/castle.h"
#include "castle/detection.h"
#include "castle/page.h"
#include "castle/vm.h"

namespace Castle {

/*
 * Script language summary (reverse engineered from CASTLE.EXE):
 *
 * A ScriptObject holds an opcode list interpreted by runScript():
 *   2 cond, target / 9 cond, target : if expression A[cond] is false jump to target
 *   3 target                         : jump
 *   4 n                              : perform assignment pair B[n]
 *   6 n                              : run action n of the action list
 *   7 n                              : evaluate expression C[n] (function call)
 *   8 cond                           : return A[cond]
 *   10 n                             : jump to ops[2 + n]
 *   11 target                        : jump
 *   12 cond                          : evaluate A[cond], ignore result
 *
 * Expressions are stack programs:
 *   0x12 push next operand
 *   1..12 binary operators: == < > != <= >= + - ? * / %
 *   0x0e logical not, 0x0f negate
 *   0x17 call function (next entry of the ints list) with the stacked args
 *   0x19 assignment helper
 */

int32 Value::toInt() const {
	switch (type) {
	case kTypeNumber: return i;
	case kTypeLogical: return b ? 1 : 0;
	case kTypeReal: return (int32)r;
	case kTypeObject: return objId;
	case kTypeString: return atoi(s.c_str());
	default: return 0;
	}
}

bool Value::toBool() const {
	switch (type) {
	case kTypeNumber: return i != 0;
	case kTypeLogical: return b;
	case kTypeReal: return r != 0.0;
	case kTypeString: return !s.empty();
	case kTypePoint: return pt.x != 0 && pt.y != 0;
	case kTypeRect: return rect.left < rect.right && rect.top < rect.bottom;
	case kTypeObject: return objId != 0;
	default: return false;
	}
}

double Value::toReal() const {
	switch (type) {
	case kTypeNumber: return i;
	case kTypeLogical: return b ? 1 : 0;
	case kTypeReal: return r;
	default: return 0.0;
	}
}

Common::String Value::toString() const {
	switch (type) {
	case kTypeNumber: return Common::String::format("%d", i);
	case kTypeLogical: return b ? "TRUE" : "FALSE";
	case kTypeReal: return Common::String::format("%g", r);
	case kTypeString: return s;
	case kTypePoint: return Common::String::format("%d,%d", pt.x, pt.y);
	case kTypeRect: return Common::String::format("%d,%d,%d,%d", rect.left, rect.top, rect.right, rect.bottom);
	case kTypeObject: return Common::String::format("obj%d", objId);
	default: return "";
	}
}

void Scope::init(const Extension *ext) {
	vars.clear();
	subs.clear();
	if (!ext)
		return;
	for (uint i = 0; i < ext->vars.size(); i++) {
		const VariableTable &t = ext->vars[i];
		Variable v;
		v.id = t.name;
		v.type = t.type;
		uint count = 1;
		for (uint d = 0; d < t.dims.size(); d++)
			count *= MAX<int>(1, t.dims[d]);
		for (uint k = 0; k < count; k++) {
			Value val;
			switch (t.type) {
			case kTypeNumber: val = Value::number(k < t.u32s.size() ? (int32)t.u32s[k] : 0); break;
			case kTypeLogical: val = Value::logical(k < t.ints.size() ? t.ints[k] != 0 : false); break;
			case kTypeString: val = Value::string(k < t.strs.size() ? t.strs[k] : Common::String()); break;
			case kTypePoint: val = Value::point(k < t.pts.size() ? t.pts[k] : Common::Point()); break;
			case kTypeRect: val = Value::rectangle(k < t.rects.size() ? t.rects[k] : Common::Rect()); break;
			case kTypeObject: val = Value::object(k < t.u32s.size() ? (int32)t.u32s[k] : 0); break;
			case kTypeReal: val = Value::real(k < t.reals.size() ? t.reals[k] : 0.0); break;
			default: break;
			}
			v.values.push_back(val);
		}
		vars.push_back(v);
	}
	for (uint i = 0; i < ext->objs.size(); i++)
		subs.push_back(ext->objs[i]);
}

ScriptVM::ScriptVM(CastleEngine *vm) : _vm(vm), _depth(0) {
}

ScriptVM::~ScriptVM() {
}

Variable *ScriptVM::findVariable(int id, Context &ctx) {
	for (uint s = 0; s < ctx.scopes.size(); s++) {
		Scope *sc = ctx.scopes[s];
		for (uint i = 0; i < sc->vars.size(); i++)
			if (sc->vars[i].id == id)
				return &sc->vars[i];
	}
	for (uint i = 0; i < _docScope.vars.size(); i++)
		if (_docScope.vars[i].id == id)
			return &_docScope.vars[i];
	return nullptr;
}

ObjectRef *ScriptVM::findSub(int id, Context &ctx) {
	for (uint s = 0; s < ctx.scopes.size(); s++) {
		Scope *sc = ctx.scopes[s];
		for (uint i = 0; i < sc->subs.size(); i++)
			if (sc->subs[i]->a == id)
				return sc->subs[i];
	}
	for (uint i = 0; i < _docScope.subs.size(); i++)
		if (_docScope.subs[i]->a == id)
			return _docScope.subs[i];
	return nullptr;
}

Value ScriptVM::getVariable(const Operand &op, Context &ctx) {
	Variable *var = findVariable(op.b, ctx);
	if (!var) {
		debugC(1, kDebugScript, "Castle: variable %d not found", op.b);
		return Value();
	}
	uint index = op.u2 > 0 ? op.u2 - 1 : 0;
	if (index >= var->values.size())
		index = 0;
	Value v = var->values.size() ? var->values[index] : Value();
	if (op.e != 0 && v.type == kTypeObject)
		return getProperty(v.objId, op.e, ctx);
	if (op.e != 0 && (v.type == kTypePoint || v.type == kTypeRect)) {
		switch (op.e) {
		case kPropLeft: return Value::number(v.type == kTypePoint ? v.pt.x : v.rect.left);
		case kPropTop: return Value::number(v.type == kTypePoint ? v.pt.y : v.rect.top);
		case kPropWidth: return Value::number(v.type == kTypePoint ? v.pt.y - v.pt.x : v.rect.width());
		case kPropHeight: return Value::number(v.rect.height());
		default: break;
		}
	}
	return v;
}

void ScriptVM::setVariable(const Operand &op, const Value &val, Context &ctx) {
	Variable *var = findVariable(op.b, ctx);
	if (!var) {
		debugC(1, kDebugScript, "Castle: variable %d not found for assignment", op.b);
		return;
	}
	uint index = op.u2 > 0 ? op.u2 - 1 : 0;
	if (var->values.empty())
		var->values.push_back(Value());
	if (index >= var->values.size())
		index = 0;
	Value &slot = var->values[index];
	if (op.e != 0 && slot.type == kTypeObject) {
		setProperty(slot.objId, op.e, val, ctx);
		return;
	}
	if (op.e != 0 && (slot.type == kTypePoint || slot.type == kTypeRect)) {
		int n = val.toInt();
		switch (op.e) {
		case kPropLeft: if (slot.type == kTypePoint) slot.pt.x = n; else slot.rect.left = n; break;
		case kPropTop: if (slot.type == kTypePoint) slot.pt.y = n; else slot.rect.top = n; break;
		case kPropWidth: slot.rect.right = slot.rect.left + n; break;
		case kPropHeight: slot.rect.bottom = slot.rect.top + n; break;
		default: break;
		}
		return;
	}
	// Convert to the variable's declared type
	switch (var->type) {
	case kTypeNumber: slot = Value::number(val.toInt()); break;
	case kTypeLogical: slot = Value::logical(val.toBool()); break;
	case kTypeString: slot = Value::string(val.toString()); break;
	case kTypeReal: slot = Value::real(val.toReal()); break;
	case kTypeObject: slot = Value::object(val.type == kTypeObject ? val.objId : val.toInt()); break;
	default: slot = val; break;
	}
}

int ScriptVM::resolveKind(const Operand &op, Context &ctx) {
	if (op.type != 0)
		return op.type;
	// Unresolved names are variables if a variable with that id exists,
	// otherwise object references.
	return findVariable(op.b, ctx) ? 1 : 3;
}

int ScriptVM::resolveObject(const Operand &op, Context &ctx) {
	if (op.b == -1)
		return ctx.object ? ctx.object->obj->id : 0;
	return op.b;
}

Value ScriptVM::readOperand(const Operand &op, Context &ctx) {
	switch (resolveKind(op, ctx)) {
	case 1: // variable
		return getVariable(op, ctx);
	case 2: // constant
		switch (op.c) {
		case kTypeNumber: return Value::number((int32)op.u2);
		case kTypeLogical: return Value::logical(op.f != 0);
		case kTypeString: return Value::string(op.str);
		case kTypePoint: return Value::point(op.pt);
		case kTypeRect: return Value::rectangle(op.rect);
		case kTypeReal: return Value::real(op.real);
		default: return Value::number((int32)op.u2);
		}
	case 3: { // object property
		if (op.b == -1 && ctx.object) {
			if (op.e == kPropSelf)
				return Value::object(ctx.object->obj->id);
			return getPropertyOf(ctx.object, op.e);
		}
		if (op.e == kPropSelf)
			return Value::object(op.b);
		return getProperty(op.b, op.e, ctx);
	}
	default:
		return Value();
	}
}

Value ScriptVM::binaryOp(int op, const Value &a, const Value &b) {
	bool numeric = (a.type == kTypeNumber || a.type == kTypeLogical || a.type == kTypeReal || a.type == kTypeNone) &&
	               (b.type == kTypeNumber || b.type == kTypeLogical || b.type == kTypeReal || b.type == kTypeNone);
	if (a.type == kTypeString || b.type == kTypeString) {
		Common::String x = a.toString(), y = b.toString();
		switch (op) {
		case 1: return Value::logical(x == y);
		case 4: return Value::logical(x != y);
		case 7: return Value::string(x + y);
		case 2: return Value::logical(x < y);
		case 3: return Value::logical(x > y);
		default: return Value::logical(false);
		}
	}
	if (a.type == kTypeObject || b.type == kTypeObject) {
		switch (op) {
		case 1: return Value::logical(a.toInt() == b.toInt());
		case 4: return Value::logical(a.toInt() != b.toInt());
		default: return Value::logical(false);
		}
	}
	if (a.type == kTypePoint && b.type == kTypePoint) {
		switch (op) {
		case 1: return Value::logical(a.pt == b.pt);
		case 4: return Value::logical(a.pt != b.pt);
		case 7: return Value::point(Common::Point(a.pt.x + b.pt.x, a.pt.y + b.pt.y));
		case 8: return Value::point(Common::Point(a.pt.x - b.pt.x, a.pt.y - b.pt.y));
		default: return Value::logical(false);
		}
	}
	if (a.type == kTypeRect && b.type == kTypeRect) {
		switch (op) {
		case 1: return Value::logical(a.rect == b.rect);
		case 4: return Value::logical(a.rect != b.rect);
		default: return Value::logical(false);
		}
	}
	if (!numeric)
		return Value::logical(false);
	if (a.type == kTypeReal || b.type == kTypeReal) {
		double x = a.toReal(), y = b.toReal();
		switch (op) {
		case 1: return Value::logical(x == y);
		case 2: return Value::logical(x < y);
		case 3: return Value::logical(x > y);
		case 4: return Value::logical(x != y);
		case 5: return Value::logical(x <= y);
		case 6: return Value::logical(x >= y);
		case 7: return Value::real(x + y);
		case 8: return Value::real(x - y);
		case 10: return Value::real(x * y);
		case 11: return Value::real(y == 0.0 ? x : x / y);
		default: return Value::logical(false);
		}
	}
	if (a.type == kTypeLogical && b.type == kTypeLogical) {
		switch (op) {
		case 1: return Value::logical(a.b == b.b);
		case 4: return Value::logical(a.b != b.b);
		case 7: case 10: return Value::logical(a.b && b.b);   // and
		case 9: return Value::logical(a.b || b.b);            // or
		default: break;
		}
	}
	int32 x = a.toInt(), y = b.toInt();
	switch (op) {
	case 1: return Value::logical(x == y);
	case 2: return Value::logical(x < y);
	case 3: return Value::logical(x > y);
	case 4: return Value::logical(x != y);
	case 5: return Value::logical(x <= y);
	case 6: return Value::logical(x >= y);
	case 7: return Value::number(x + y);
	case 8: return Value::number(x - y);
	case 9: return Value::logical(a.toBool() || b.toBool());
	case 10: return Value::number(x * y);
	case 11: return Value::number(y == 0 ? x : x / y);
	case 12: return Value::number(y == 0 ? 0 : x % y);
	default:
		debugC(1, kDebugScript, "Castle: unknown binary op %d", op);
		return Value::logical(false);
	}
}

Value ScriptVM::unaryOp(int op, const Value &a) {
	if (op == 0x0e)
		return Value::logical(!a.toBool());
	switch (a.type) {
	case kTypeNumber: return Value::number(-a.i);
	case kTypeLogical: return Value::logical(!a.b);
	case kTypeReal: return Value::real(-a.r);
	case kTypePoint: return Value::point(Common::Point(-a.pt.x, -a.pt.y));
	default: return a;
	}
}

Value ScriptVM::callFunction(int id, Common::Array<Value> &stack, Context &ctx) {
	ObjectRef *sub = findSub(id, ctx);
	if (!sub) {
		debugC(1, kDebugScript, "Castle: function %d not found", id);
		return Value();
	}
	// Built-in functions are document level objects without a script body
	// and are numbered through the document's pair list.
	int builtin = _vm->getBuiltinNumber(id);
	if (builtin) {
		Value args[4];
		int argc = 0;
		switch (builtin) {
		case 1: argc = 1; break;   // Random(n)
		case 2: argc = 2; break;   // RandomList(table, n)
		case 3: argc = 2; break;   // Point(x, y)
		case 4: argc = 4; break;   // Rect(l, t, r, b)
		case 5: argc = 2; break;   // HitTest(object, point)
		case 6: argc = 4; break;   // IniGetInt
		case 7: argc = 4; break;   // IniGetString
		case 8: argc = 4; break;   // IniWriteInt
		case 9: argc = 4; break;   // IniWriteString
		case 10: argc = 1; break;  // IntToString(n)
		default: argc = 0; break;
		}
		for (int i = argc - 1; i >= 0; i--) {
			if (!stack.empty()) {
				args[i] = stack.back();
				stack.pop_back();
			}
		}
		switch (builtin) {
		case 1: {
			int n = args[0].toInt();
			return Value::number(n > 0 ? _vm->getRandom().getRandomNumber(n - 1) + 1 : 1);
		}
		case 3: return Value::point(Common::Point(args[0].toInt(), args[1].toInt()));
		case 4: return Value::rectangle(Common::Rect(args[0].toInt(), args[1].toInt(), args[2].toInt(), args[3].toInt()));
		case 5: {
			LiveObject *lo = _vm->findLiveObject(args[0].toInt(), ctx.page);
			return Value::logical(lo && args[1].type == kTypePoint && lo->rect.contains(args[1].pt));
		}
		case 6: return args[2];
		case 7: return args[2];
		case 10: return Value::string(args[0].toString());
		default:
			debugC(1, kDebugScript, "Castle: built-in function %d not implemented", builtin);
			return Value::logical(true);
		}
	}
	if (!sub->script) {
		return Value();
	}
	Context sub_ctx = ctx;
	Scope local;
	local.init(sub->ext);
	sub_ctx.scopes.insert_at(0, &local);
	Scope scriptScope;
	scriptScope.init(sub->script->ext);
	sub_ctx.scopes.insert_at(0, &scriptScope);
	bool r = runScript(sub->script, sub_ctx);
	return Value::logical(r);
}

Value ScriptVM::evaluate(const Expression *expr, Context &ctx) {
	Common::Array<Value> stack;
	uint operandIdx = 0;
	uint callIdx = 0;
	for (uint pc = 0; pc < expr->ops.size(); pc++) {
		int op = expr->ops[pc];
		if (op == 0x12) {
			if (operandIdx < expr->operands.size())
				stack.push_back(readOperand(expr->operands[operandIdx++], ctx));
			else
				stack.push_back(Value());
		} else if (op >= 1 && op <= 12) {
			if (stack.size() < 2) {
				debugC(1, kDebugScript, "Castle: expression stack underflow");
				return Value();
			}
			Value b = stack.back(); stack.pop_back();
			Value a = stack.back(); stack.pop_back();
			stack.push_back(binaryOp(op, a, b));
		} else if (op == 0x0e || op == 0x0f) {
			if (stack.empty())
				return Value();
			Value a = stack.back(); stack.pop_back();
			stack.push_back(unaryOp(op, a));
		} else if (op == 0x17) {
			int id = callIdx < expr->ints.size() ? expr->ints[callIdx++] : 0;
			Value r = callFunction(id, stack, ctx);
			stack.push_back(r);
		} else if (op == 0x19) {
			// assignment marker; handled by assign()
		} else {
			debugC(1, kDebugScript, "Castle: unknown expression op %d", op);
		}
	}
	return stack.empty() ? Value() : stack.back();
}

bool ScriptVM::assign(const Expression *lhs, const Expression *rhs, Context &ctx) {
	if (!lhs || !rhs || lhs->operands.empty())
		return false;
	Value v = evaluate(rhs, ctx);
	const Operand &target = lhs->operands[0];
	int kind = resolveKind(target, ctx);
	if (kind == 1) {
		setVariable(target, v, ctx);
	} else if (kind == 3) {
		if (target.b == -1 && ctx.object)
			setPropertyOf(ctx.object, target.e, v);
		else
			setProperty(target.b, target.e, v, ctx);
	} else {
		debugC(1, kDebugScript, "Castle: cannot assign to operand type %d", target.type);
		return false;
	}
	return true;
}

bool ScriptVM::runScript(const ScriptObject *script, Context &ctx) {
	if (_depth > 32) {
		warning("Castle: script recursion too deep");
		return false;
	}
	_depth++;
	bool result = true;
	const Common::Array<int16> &ops = script->ops;
	int count = ops.size();
	int pc = 0;
	int steps = 0;
	while (pc >= 0 && pc < count && result) {
		if (++steps > 100000) {
			warning("Castle: script runaway");
			break;
		}
		int op = ops[pc];
		int arg = pc + 1 < count ? ops[pc + 1] : 0;
		switch (op) {
		case 2:
		case 9: {
			bool cond = false;
			if (arg >= 0 && arg < (int)script->opA.size() && script->opA[arg])
				cond = evaluate(script->opA[arg], ctx).toBool();
			if (cond)
				pc += 3;
			else
				pc = pc + 2 < count ? ops[pc + 2] : count;
			break;
		}
		case 3:
		case 11:
			pc = arg;
			break;
		case 4:
			if (arg >= 0 && arg < (int)script->opB1.size())
				assign(script->opB1[arg], script->opB2[arg], ctx);
			pc += 2;
			break;
		case 6:
			if (arg >= 0 && arg < (int)script->actions.size())
				_vm->runScriptAction(script->actions[arg], ctx);
			pc += 2;
			break;
		case 7:
			if (arg >= 0 && arg < (int)script->opC.size())
				evaluate(script->opC[arg], ctx);
			pc += 2;
			break;
		case 8:
			if (arg >= 0 && arg < (int)script->opA.size() && script->opA[arg])
				result = evaluate(script->opA[arg], ctx).toBool();
			pc = count;
			break;
		case 10:
			pc = (2 + arg < count) ? ops[2 + arg] : count;
			break;
		case 12:
			if (arg >= 0 && arg < (int)script->opA.size() && script->opA[arg])
				evaluate(script->opA[arg], ctx);
			pc += 2;
			break;
		default:
			pc = count;
			break;
		}
		if (_vm->scriptShouldStop())
			break;
	}
	_depth--;
	return result;
}

Value ScriptVM::getProperty(int objId, int prop, Context &ctx) {
	LiveObject *lo = _vm->findLiveObject(objId, ctx.page);
	if (!lo) {
		debugC(1, kDebugScript, "Castle: object %d not found (get prop %d)", objId, prop);
		return Value();
	}
	return getPropertyOf(lo, prop);
}

void ScriptVM::setProperty(int objId, int prop, const Value &v, Context &ctx) {
	LiveObject *lo = _vm->findLiveObject(objId, ctx.page);
	if (!lo) {
		debugC(1, kDebugScript, "Castle: object %d not found (set prop %d)", objId, prop);
		return;
	}
	setPropertyOf(lo, prop, v);
}

// Sprite boolean properties stored in two bit fields of the original object
static int spriteFlagBit(int prop) {
	switch (prop) {
	case 0x1a: return 1;
	case 0x41: return 2;
	case 0x43: return 4;
	case 0x44: return 8;
	case 0x42: return 0x10;
	case 0x45: return 0x20;
	case 0x46: return 0x40;
	case 0x3c: return 0x80;
	case 0x6d: return 0x100;
	default: return 0;
	}
}

static int spriteStateBit(int prop) {
	switch (prop) {
	case 0x4b: return 1;
	case 0x4a: return 2;
	case 0x3b: return 4;
	case 0x1e: return 0x10;
	case 0x49: return 0x20;
	case 0x48: return 0x40;
	default: return 0;
	}
}

Value ScriptVM::getPropertyOf(LiveObject *lo, int prop) {
	if (lo->obj->cls == kObjSprite) {
		if (prop == kPropVisible)
			return Value::logical(lo->visible);
		if (spriteFlagBit(prop))
			return Value::logical((lo->spriteFlags & spriteFlagBit(prop)) != 0);
		if (spriteStateBit(prop))
			return Value::logical((lo->spriteState & spriteStateBit(prop)) != 0);
	}
	switch (prop) {
	case kPropSelf: return Value::object(lo->obj->id);
	case kPropLeft: return Value::number(lo->rect.left - lo->panel->rect.left);
	case kPropTop: return Value::number(lo->rect.top - lo->panel->rect.top);
	case kPropWidth: return Value::number(lo->rect.width());
	case kPropHeight: return Value::number(lo->rect.height());
	case kPropDisabled: return Value::logical(lo->disabled);
	case kPropZOrder: return Value::number(lo->zOrder);
	case kPropVisible: return Value::logical(lo->visible);
	case kPropValue: return Value::number(lo->value);
	case kPropSpriteFrame: return Value::number(lo->frame);
	case kPropSpriteFrameB: return Value::number(lo->frame);
	case kPropSpriteFrameCount: return Value::number(lo->frameCount);
	case kPropSpriteFps: return Value::number(lo->frameDelay ? 1000 / lo->frameDelay : 0);
	case kPropSpriteDelay: return Value::number(lo->frameDelay);
	case kPropSpriteCounter1: case kPropSpriteCounter2: case kPropSpriteCounter3:
	case kPropSpriteCounter4: case kPropSpriteCounter5:
		return Value::number(lo->counters[prop - kPropSpriteCounter1]);
	case kPropSpriteRight: return Value::number(lo->rect.right - lo->panel->rect.left);
	case kPropSpriteBottom: return Value::number(lo->rect.bottom - lo->panel->rect.top);
	default:
		if (prop >= 0x30 && prop <= 0x38)
			return Value::number(lo->extra[prop - 0x30]);
		debugC(1, kDebugScript, "Castle: get property %d of %s not implemented", prop, objectClassName(lo->obj->cls));
		return Value();
	}
}

void ScriptVM::setPropertyOf(LiveObject *lo, int prop, const Value &v) {
	int n = v.toInt();
	if (lo->obj->cls == kObjSprite && prop != kPropVisible) {
		if (spriteFlagBit(prop)) {
			if (v.toBool())
				lo->spriteFlags |= spriteFlagBit(prop);
			else
				lo->spriteFlags &= ~spriteFlagBit(prop);
			if (prop == 0x44) {
				lo->playing = v.toBool() && lo->frameDelay != 0;
				lo->nextFrameTime = 0;
			}
			_vm->markDirty();
			return;
		}
		if (spriteStateBit(prop)) {
			if (v.toBool())
				lo->spriteState |= spriteStateBit(prop);
			else
				lo->spriteState &= ~spriteStateBit(prop);
			_vm->markDirty();
			return;
		}
	}
	switch (prop) {
	case kPropLeft: {
		int w = lo->rect.width();
		lo->rect.left = lo->panel->rect.left + n;
		lo->rect.right = lo->rect.left + w;
		break;
	}
	case kPropTop: {
		int h = lo->rect.height();
		lo->rect.top = lo->panel->rect.top + n;
		lo->rect.bottom = lo->rect.top + h;
		break;
	}
	case kPropWidth: lo->rect.right = lo->rect.left + n; break;
	case kPropHeight: lo->rect.bottom = lo->rect.top + n; break;
	case kPropDisabled: lo->disabled = v.toBool(); break;
	case kPropZOrder:
		lo->zOrder = n;
		lo->visible = n > -30000;
		break;
	case kPropVisible: lo->visible = v.toBool(); break;
	case kPropValue: lo->value = n; break;
	case kPropSpriteFrame:
	case kPropSpriteFrameB:
		_vm->setSpriteFrame(lo, n);
		break;
	case kPropSpriteFrameCount: lo->frameCount = n; break;
	case kPropSpriteDelay: lo->frameDelay = MAX(25, n); break;
	case kPropSpriteCounter1: case kPropSpriteCounter2: case kPropSpriteCounter3:
	case kPropSpriteCounter4: case kPropSpriteCounter5:
		lo->counters[prop - kPropSpriteCounter1] = n;
		break;
	case kPropSpriteSetPos:
	case kPropSpriteMoveTo:
		if (v.type == kTypePoint) {
			int w = lo->rect.width(), h = lo->rect.height();
			lo->rect.left = lo->panel->rect.left + v.pt.x;
			lo->rect.top = lo->panel->rect.top + v.pt.y;
			lo->rect.right = lo->rect.left + w;
			lo->rect.bottom = lo->rect.top + h;
		}
		break;
	default:
		if (prop >= 0x30 && prop <= 0x38) {
			lo->extra[prop - 0x30] = n;
			break;
		}
		debugC(1, kDebugScript, "Castle: set property %d of %s = %s not implemented", prop, objectClassName(lo->obj->cls), v.toString().c_str());
		break;
	}
	_vm->markDirty();
}

} // End of namespace Castle
