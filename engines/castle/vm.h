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

#ifndef CASTLE_VM_H
#define CASTLE_VM_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "castle/database.h"

namespace Castle {

class CastleEngine;
class LivePage;
struct LiveObject;
struct LivePanel;

// Value types of the script language
enum ValueType {
	kTypeNone = 0,
	kTypeNumber = 1,
	kTypeLogical = 2,
	kTypeString = 3,
	kTypePoint = 4,
	kTypeRect = 5,
	kTypeObject = 6,
	kTypeReal = 7
};

// Object property ids (names from the EXE string table)
enum PropertyId {
	kPropSelf = 0,
	kPropLeft = 1,
	kPropTop = 2,
	kPropWidth = 3,
	kPropHeight = 4,
	kPropHighlight = 5,
	kPropDisabled = 8,
	kPropOverButton = 9,
	kPropSelected = 10,
	kPropValue = 0x1c,
	kPropZOrder = 0x1d,
	kPropVisible = 0x1e,
	kPropText = 0x20,
	kPropCursor = 0x21,
	kPropSpriteFrame = 0x23,
	kPropSpriteFrameCount = 0x24,
	kPropSpriteFps = 0x25,
	kPropSpriteCounter1 = 0x26,
	kPropSpriteCounter2 = 0x27,
	kPropSpriteCounter3 = 0x28,
	kPropSpriteCounter4 = 0x29,
	kPropSpriteCounter5 = 0x2a,
	kPropSpriteRight = 0x2b,
	kPropSpriteSetPos = 0x39,
	kPropSpriteFrameB = 0x3d,
	kPropSpriteRectA = 0x3e,
	kPropSpriteRectB = 0x3f,
	kPropSpriteMoveTo = 0x61,
	kPropSpriteStart = 0x62,
	kPropSpriteBottom = 0x65,
	kPropSpriteDelay = 0x6c
};

struct Value {
	ValueType type;
	int32 i;
	bool b;
	Common::String s;
	Common::Point pt;
	Common::Rect rect;
	double r;
	int32 objId;

	Value() : type(kTypeNone), i(0), b(false), r(0.0), objId(0) {}
	static Value number(int32 v) { Value x; x.type = kTypeNumber; x.i = v; return x; }
	static Value logical(bool v) { Value x; x.type = kTypeLogical; x.b = v; return x; }
	static Value string(const Common::String &v) { Value x; x.type = kTypeString; x.s = v; return x; }
	static Value point(const Common::Point &v) { Value x; x.type = kTypePoint; x.pt = v; return x; }
	static Value rectangle(const Common::Rect &v) { Value x; x.type = kTypeRect; x.rect = v; return x; }
	static Value object(int32 id) { Value x; x.type = kTypeObject; x.objId = id; return x; }
	static Value real(double v) { Value x; x.type = kTypeReal; x.r = v; return x; }

	int32 toInt() const;
	bool toBool() const;
	double toReal() const;
	Common::String toString() const;
};

// Runtime storage for one variable
struct Variable {
	int16 id;
	int16 type;
	Common::Array<Value> values; // one per element (dims product)
};

// A variable scope (document, page, panel or script)
struct Scope {
	Common::Array<Variable> vars;
	Common::Array<ObjectRef *> subs;   // callable named scripts
	void init(const Extension *ext);
};

// Execution context of a script
struct Context {
	LivePage *page;
	LivePanel *panel;
	LiveObject *object;
	Common::Array<Scope *> scopes;    // innermost first
	Value retVal;                     // value of the script's return opcode
	bool hasRet;
	Context() : page(nullptr), panel(nullptr), object(nullptr), hasRet(false) {}
};

class ScriptVM {
public:
	ScriptVM(CastleEngine *vm);
	~ScriptVM();

	// Runs a script object. Returns the script's result (true unless an
	// explicit false return happened).
	bool runScript(const ScriptObject *script, Context &ctx);
	// Evaluates an expression and returns its value (none if the stack is empty)
	Value evaluate(const Expression *expr, Context &ctx);

	Scope &docScope() { return _docScope; }
	void initDocScope(const Extension *ext) { _docScope.init(ext); }
	// Document variables by id (the quest module keeps the spy type and
	// scenario number in script variables)
	bool setDocVariable(int id, const Value &v);
	Value getDocVariable(int id);

private:
	Value readOperand(const Operand &op, Context &ctx);
	int resolveKind(const Operand &op, Context &ctx);
	int resolveObject(const Operand &op, Context &ctx);
	Variable *findVariable(int id, Context &ctx);
	ObjectRef *findSub(int id, Context &ctx);
	Value getVariable(const Operand &op, Context &ctx);
	// index1: 1 based element index, 0 = the operand's own index
	Value getVariableElement(const Operand &op, int index1, Context &ctx);
	void setVariableElement(const Operand &op, int index1, const Value &val, Context &ctx);
	void setVariable(const Operand &op, const Value &v, Context &ctx);
	bool assign(const Expression *lhs, const Expression *rhs, Context &ctx);
	Value binaryOp(int op, const Value &a, const Value &b);
	Value unaryOp(int op, const Value &a);
	Value callFunction(int id, Common::Array<Value> &stack, Context &ctx);
	Value getProperty(int objId, int prop, Context &ctx);
	void setProperty(int objId, int prop, const Value &v, Context &ctx);
	Value getPropertyOf(LiveObject *lo, int prop);
	void setPropertyOf(LiveObject *lo, int prop, const Value &v);

	CastleEngine *_vm;
	Scope _docScope;
	int _depth;
};

} // End of namespace Castle

#endif
