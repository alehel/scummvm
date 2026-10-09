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
#include "common/file.h"
#include "common/memstream.h"
#include "common/textconsole.h"

#include "castle/database.h"
#include "castle/detection.h"

namespace Castle {

static const char *const kObjectClassNames[kObjClassCount] = {
	"AmbientAnimation", "Bitmap", "BoxIconBitmap", "BUTTON", "ClipRect", "CoinBitmap",
	"Collage", "CollageButton", "CollectBitmap", "ColourHotspot", "DitherBitmap",
	"DungeonDisableHotspot", "DungeonTimer", "EditBox", "ExploreHotspot", "GroupCollage",
	"HatchBitmap", "HighlightingCastle", "Hotspot", "Include", "Include3D", "Locator",
	"MaskBitmap", "NavRollOverButton", "noBitmap", "NodalHotspot", "OptionsButton",
	"PageTurn", "PaletteBitmap", "ParaText", "PlaceHolder", "PlayResponseHotspot",
	"QuestionOKButton", "RandomMapBitmap", "RandomScenarioHotspot", "RepeatingHotspot",
	"RollRepeatHotspot", "RoomEditBox", "SaveAsHotspot", "ScrollBar", "ScrollBitmap",
	"ScrollEditBox", "ScrollObject", "ScrollQuestAnswerHtsp", "ScrollTickBitmap",
	"SegmentedBitmap", "SoundHotspot", "Sprite", "SpyDitherBitmap", "togglebutton",
	"VanillaHotspot", "VideoStartBitmap", "WordBox", "WrapScrollObject",
	"ZoomAmbientSoundObj", "ZoomAreaHotspot", "ZoomBorderHotspot", "ZoomCaption", "ZoomHotspot"
};

static const char *const kActionNames[kActLast] = {
	"NULL_ACT", "CHANGEPAGE", "OPENPAGE", "CLOSEPAGE", "AppendPage", "PLAYPICS", "STOPPICS",
	"Command", "PLAYWAVE", "NEWPLAYWAVE", "STOPWAVE", "SCROLLPANEL", "SENDMESSAGE", "ZOOM",
	"PaintBitmap", "ClearBitmap", "ResetResources", "Quit", "Link", "Back", "PaintZoomObject",
	"ClearZoomObject", "PaintZoomArea", "PlayResponse", "UpdateNodeHtsp", "ChangeSpyType",
	"playvideo", "stopvideo", "playvideoInd", "DoTransition", "ChangeColourRefBitmapAction",
	"HighlightCastleSection", "BribeGuardAction", "StopDungeonTimer", "StartDungeonTimer",
	"ScrollListBox", "ActivityCompleted", "OptionsAction", "PLAYPICSEX", "PaintHelpText",
	"ClearHelpText", "PlayWaveChannel", "StopWaveChannel", "GeneralPurposeAction",
	"TransitionVideo", "SetSpriteFrame", "FileSaveAs", "OpenHelp"
};

static const char *const kEventNames[kEventLast] = {
	"NULL_EVT", "on_open", "on_preopen", "on_close", "on_finish", "on_click", "on_rollon",
	"on_rolloff", "on_enable_request", "on_disable_request", "on_activateapp",
	"on_deactivateappEvt", "on_load", "on_picsrewind", "on_timerEnd", "on_questionsAnswered",
	"on_toggleScroll", "on_SpyChanged"
};

const char *objectClassName(int cls) {
	return (cls >= 0 && cls < kObjClassCount) ? kObjectClassNames[cls] : "?";
}

const char *actionName(int type) {
	return (type >= 0 && type < kActLast) ? kActionNames[type] : "?";
}

const char *eventName(int type) {
	return (type >= 0 && type < kEventLast) ? kEventNames[type] : "?";
}

// ---------------------------------------------------------------------------

Event::~Event() {
	for (uint i = 0; i < actions.size(); i++)
		delete actions[i];
}

ObjectRef::~ObjectRef() {
	delete ext;
	delete script;
}

Extension::~Extension() {
	for (uint i = 0; i < objs.size(); i++)
		delete objs[i];
}

ScriptObject::~ScriptObject() {
	for (uint i = 0; i < opA.size(); i++)
		delete opA[i];
	for (uint i = 0; i < opB1.size(); i++)
		delete opB1[i];
	for (uint i = 0; i < opB2.size(); i++)
		delete opB2[i];
	for (uint i = 0; i < opC.size(); i++)
		delete opC[i];
	delete ext;
	for (uint i = 0; i < actions.size(); i++)
		delete actions[i];
}

Action::~Action() {
	delete script;
}

GameObject::~GameObject() {
	delete mask;
	for (uint i = 0; i < events.size(); i++)
		delete events[i];
	delete ext;
	for (uint i = 0; i < scripts.size(); i++)
		delete scripts[i];
}

bool GameObject::isHotspot() const {
	switch (cls) {
	case kObjVanillaHotspot:
	case kObjHotspot:
	case kObjZoomAreaHotspot:
	case kObjScrollQuestAnswerHtsp:
	case kObjZoomBorderHotspot:
	case kObjExploreHotspot:
	case kObjDungeonDisableHotspot:
	case kObjZoomHotspot:
	case kObjNodalHotspot:
	case kObjPlayResponseHotspot:
	case kObjRepeatingHotspot:
	case kObjRollRepeatHotspot:
	case kObjRandomScenarioHotspot:
	case kObjPageTurn:
	case kObjSaveAsHotspot:
	case kObjOptionsButton:
	case kObjSoundHotspot:
	case kObjColourHotspot:
		return true;
	default:
		return false;
	}
}

bool GameObject::isBitmap() const {
	switch (cls) {
	case kObjBitmap:
	case kObjMaskBitmap:
	case kObjPaletteBitmap:
	case kObjSegmentedBitmap:
	case kObjRandomMapBitmap:
	case kObjDitherBitmap:
	case kObjSpyDitherBitmap:
	case kObjCoinBitmap:
	case kObjBoxIconBitmap:
	case kObjScrollTickBitmap:
		return true;
	default:
		return false;
	}
}

const Event *GameObject::findEvent(int type) const {
	for (uint i = 0; i < events.size(); i++)
		if (events[i]->type == type)
			return events[i];
	return nullptr;
}

Panel::~Panel() {
	for (uint i = 0; i < objects.size(); i++)
		delete objects[i];
	for (uint i = 0; i < events.size(); i++)
		delete events[i];
	delete ext;
}

PageRecord::~PageRecord() {
	delete ext;
	for (uint i = 0; i < panels.size(); i++)
		delete panels[i];
	for (uint i = 0; i < events.size(); i++)
		delete events[i];
}

PageTemplate::~PageTemplate() {
	for (uint i = 0; i < events.size(); i++)
		delete events[i];
}

// ---------------------------------------------------------------------------

Database::Database() : _data(nullptr), _size(0), _pageDataStart(0), _stringTableSize(0), _pageTableStart(0), _docExt(nullptr) {
}

Database::~Database() {
	for (Common::HashMap<uint, PageRecord *>::iterator it = _records.begin(); it != _records.end(); ++it)
		delete it->_value;
	for (uint i = 0; i < _templates.size(); i++)
		delete _templates[i];
	for (uint i = 0; i < _docEvents.size(); i++)
		delete _docEvents[i];
	delete _docExt;
	free(_data);
}

int Database::readCount(Common::SeekableReadStream &s) const {
	int16 n = s.readSint16BE();
	if (n < 0)
		error("Castle: negative count %d at %d", n, (int)s.pos());
	return n;
}

Common::Point Database::readPoint(Common::SeekableReadStream &s) const {
	int16 x = s.readSint16BE();
	int16 y = s.readSint16BE();
	return Common::Point(x, y);
}

Common::Rect Database::readRect(Common::SeekableReadStream &s) const {
	int16 l = s.readSint16BE();
	int16 t = s.readSint16BE();
	int16 r = s.readSint16BE();
	int16 b = s.readSint16BE();
	Common::Rect rect;
	rect.left = l;
	rect.top = t;
	rect.right = r;
	rect.bottom = b;
	return rect;
}

Common::String Database::readInlineString(Common::SeekableReadStream &s) const {
	int16 len = s.readSint16BE();
	if (len <= 0)
		return Common::String();
	Common::String str;
	for (int i = 0; i < len; i++)
		str += (char)s.readByte();
	return str;
}

Common::String Database::readStringRef(Common::SeekableReadStream &s) const {
	int16 idx = s.readSint16BE();
	return getString(idx);
}

void Database::readRGB(Common::SeekableReadStream &s, byte *rgb) const {
	rgb[0] = s.readByte();
	rgb[1] = s.readByte();
	rgb[2] = s.readByte();
}

const Common::String &Database::getString(int index1) const {
	if (index1 <= 0 || index1 > (int)_strings.size())
		return _empty;
	return _strings[index1 - 1];
}

// ---------------------------------------------------------------------------

void Database::readEvents(Common::SeekableReadStream &s, Common::Array<Event *> &events) {
	int n = readCount(s);
	for (int i = 0; i < n; i++) {
		Event *ev = new Event();
		ev->type = s.readSint16BE();
		readActions(s, ev->actions);
		events.push_back(ev);
	}
}

void Database::readActions(Common::SeekableReadStream &s, Common::Array<Action *> &actions) {
	int n = readCount(s);
	for (int i = 0; i < n; i++) {
		int16 type = s.readSint16BE();
		actions.push_back(readAction(s, type));
	}
}

Action *Database::readAction(Common::SeekableReadStream &s, int16 type) {
	Action *a = new Action();
	a->type = type;
	switch (type) {
	case kActChangePage:
	case kActOpenPage:
	case kActAppendPage:
	case kActOpenHelp:
		a->page = s.readUint32BE();
		a->x = s.readSint16BE();
		break;
	case kActClosePage:
		a->page = s.readUint32BE();
		break;
	case kActPlayPics:
	case kActPlayPicsEx:
		a->name = readStringRef(s);
		for (int i = 0; i < 7; i++)
			a->p[i] = s.readSint16BE();
		a->pt = readPoint(s);
		break;
	case kActStopPics:
	case kActStopWave:
	case kActStopVideo:
		a->name = readStringRef(s);
		break;
	case kActCommand:
		a->script = readScriptObject(s);
		break;
	case kActPlayWave:
		a->name = readInlineString(s);
		for (int i = 0; i < 6; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActNewPlayWave:
	case kActPlayWaveChannel:
		a->name = readStringRef(s);
		for (int i = 0; i < 4; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActScrollPanel:
		for (int i = 0; i < 6; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActSendMessage:
	case kActScrollListBox:
	case kActSetSpriteFrame:
		a->p[0] = s.readSint16BE();
		a->p[1] = s.readSint16BE();
		break;
	case kActZoom:
		a->page = s.readUint32BE();
		a->pt = readPoint(s);
		for (int i = 0; i < 3; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActPaintBitmap:
		a->name = readInlineString(s);
		a->pt = readPoint(s);
		break;
	case kActClearBitmap:
	case kActResetResources:
	case kActQuit:
	case kActBack:
	case kActBribeGuard:
	case kActStopDungeonTimer:
	case kActActivityCompleted:
	case kActClearHelpText:
	case kActFileSaveAs:
		break;
	case kActLink: {
		a->page = s.readUint32BE();
		int n = readCount(s);
		for (int i = 0; i < n; i++) {
			LinkEntry e;
			e.page = s.readUint32BE();
			e.x = s.readSint16BE();
			a->links.push_back(e);
		}
		break;
	}
	case kActPaintZoomObject:
	case kActPaintZoomArea:
		a->name = readInlineString(s);
		a->x = s.readSint16BE();
		break;
	case kActClearZoomObject:
	case kActUpdateNodeHtsp:
	case kActChangeSpyType:
	case kActHighlightCastleSection:
	case kActOptions:
	case kActGeneralPurpose:
		a->x = s.readSint16BE();
		break;
	case kActPlayResponse:
		for (int i = 0; i < 6; i++)
			a->strs.push_back(readStringRef(s));
		break;
	case kActPlayVideo:
		a->name = readInlineString(s);
		for (int i = 0; i < 7; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActPlayVideoInd:
	case kActTransitionVideo:
		a->name = readInlineString(s);
		for (int i = 0; i < 5; i++)
			a->p[i] = s.readSint16BE();
		a->rect = readRect(s);
		a->p[5] = s.readSint16BE();
		a->p[6] = s.readSint16BE();
		break;
	case kActDoTransition:
		for (int i = 0; i < 3; i++)
			a->p[i] = s.readSint16BE();
		break;
	case kActChangeColourRefBitmap:
	case kActPaintHelpText:
		a->name = readInlineString(s);
		break;
	case kActStartDungeonTimer:
		a->page = s.readUint32BE();
		break;
	case kActStopWaveChannel:
		a->x = s.readSint16BE();
		a->name = readInlineString(s);
		break;
	default:
		error("Castle: unknown action type %d at %d", type, (int)s.pos());
	}
	return a;
}

void Database::readVariableTable(Common::SeekableReadStream &s, VariableTable &v) {
	v.name = s.readSint16BE();
	v.type = s.readSint16BE();
	int n = readCount(s);
	for (int i = 0; i < n; i++)
		v.dims.push_back(s.readSint16BE());
	n = readCount(s);
	for (int i = 0; i < n; i++)
		v.ints.push_back(s.readSint16BE());
	n = readCount(s);
	for (int i = 0; i < n; i++)
		v.u32s.push_back(s.readUint32BE());
	n = readCount(s);
	for (int i = 0; i < n; i++)
		v.pts.push_back(readPoint(s));
	n = readCount(s);
	for (int i = 0; i < n; i++)
		v.rects.push_back(readRect(s));
	uint32 n32 = s.readUint32BE();
	for (uint32 i = 0; i < n32; i++)
		v.strs.push_back(readInlineString(s));
	n = readCount(s);
	for (int i = 0; i < n; i++)
		v.reals.push_back(s.readDoubleBE());
	n32 = s.readUint32BE();
	for (uint32 i = 0; i < n32; i++)
		v.l1.push_back(s.readSint16BE());
	n32 = s.readUint32BE();
	for (uint32 i = 0; i < n32; i++)
		v.l2.push_back(s.readSint16BE());
	n32 = s.readUint32BE();
	for (uint32 i = 0; i < n32; i++)
		v.l3.push_back(s.readUint32BE());
}

ObjectRef *Database::readObjectRef(Common::SeekableReadStream &s) {
	ObjectRef *r = new ObjectRef();
	r->a = s.readSint16BE();
	r->b = s.readSint16BE();
	r->c = s.readSint16BE();
	r->ext = readExtension(s);
	r->script = readScriptObject(s);
	return r;
}

Extension *Database::readExtension(Common::SeekableReadStream &s) {
	int16 tag = s.readSint16BE();
	if (tag == 0)
		return nullptr;
	Extension *e = new Extension();
	e->tag = tag;
	int n = readCount(s);
	e->vars.resize(n);
	for (int i = 0; i < n; i++)
		readVariableTable(s, e->vars[i]);
	n = readCount(s);
	for (int i = 0; i < n; i++)
		e->objs.push_back(readObjectRef(s));
	return e;
}

void Database::readOperand(Common::SeekableReadStream &s, Operand &o) {
	o.u = s.readUint32BE();
	o.a = s.readSint16BE();
	o.b = s.readSint16BE();
	o.type = s.readSint16BE();
	o.c = s.readSint16BE();
	o.e = s.readSint16BE();
	o.f = s.readSint16BE();
	o.u2 = s.readUint32BE();
	o.pt = readPoint(s);
	o.rect = readRect(s);
	o.str = readInlineString(s);
	o.real = s.readDoubleBE();
}

Expression *Database::readExpression(Common::SeekableReadStream &s) {
	Expression *e = new Expression();
	int n = readCount(s);
	for (int i = 0; i < n; i++)
		e->ops.push_back(s.readSint16BE());
	n = readCount(s);
	e->operands.resize(n);
	for (int i = 0; i < n; i++)
		readOperand(s, e->operands[i]);
	n = readCount(s);
	for (int i = 0; i < n; i++)
		e->ints.push_back(s.readSint16BE());
	return e;
}

ScriptObject *Database::readScriptObject(Common::SeekableReadStream &s) {
	ScriptObject *so = new ScriptObject();
	so->a = s.readSint16BE();
	so->name = readInlineString(s);
	so->rect = readRect(s);
	so->b = s.readSint16BE();
	so->c = s.readSint16BE();
	int n = readCount(s);
	for (int i = 0; i < n; i++)
		so->ops.push_back(s.readSint16BE());
	n = readCount(s);
	for (int i = 0; i < n; i++) {
		int16 flag = s.readSint16BE();
		so->opA.push_back(flag ? readExpression(s) : nullptr);
	}
	n = readCount(s);
	for (int i = 0; i < n; i++) {
		so->opB1.push_back(readExpression(s));
		so->opB2.push_back(readExpression(s));
	}
	n = readCount(s);
	for (int i = 0; i < n; i++)
		so->opC.push_back(readExpression(s));
	so->ext = readExtension(s);
	readActions(s, so->actions);
	return so;
}

HotspotMask *Database::readMask(Common::SeekableReadStream &s) {
	HotspotMask *m = new HotspotMask();
	m->pos = readPoint(s);
	m->size = readPoint(s);
	m->a = s.readSint16BE();
	m->b = s.readSint16BE();
	int n = readCount(s);
	m->c = s.readSint16BE();
	for (int i = 0; i < n; i++)
		m->data.push_back(s.readUint32BE());
	return m;
}

void Database::readObjectBase(Common::SeekableReadStream &s, GameObject *o) {
	o->id = s.readSint16BE();
	o->b = s.readSint16BE();
	o->flags = s.readSint16BE();
	if (!(o->flags & kObjFlagNoRect))
		o->rect = readRect(s);
	o->cursor = readStringRef(s);
	o->c = s.readSint16BE();
	o->d = s.readSint16BE();
	if (o->flags & kObjFlagHasMask)
		o->mask = readMask(s);
	o->e = s.readSint16BE();
	readEvents(s, o->events);
	o->ext = readExtension(s);
}

void Database::readCollageSub(Common::SeekableReadStream &s, const Common::String &cls, GameObject *o) {
	Common::String c = cls;
	c.toLowercase();
	if (c == "linear")
		return;
	if (c == "tracker") {
		o->ints.push_back(s.readSint16BE());
		for (int i = 0; i < 1 + 34 + 34; i++)
			o->strs.push_back(readStringRef(s));
		return;
	}
	if (c == "castleindex") {
		o->ints.push_back(s.readSint16BE());
		o->u32s.push_back(s.readUint32BE());
		o->u32s.push_back(s.readUint32BE());
		int n = readCount(s);
		for (int i = 0; i < n; i++) {
			o->strs.push_back(readInlineString(s));
			o->strs.push_back(readInlineString(s));
			o->u32s.push_back(s.readUint32BE());
			o->u32s.push_back(s.readUint32BE());
			o->points.push_back(readPoint(s));
		}
		return;
	}
	error("Castle: unknown collage class '%s'", cls.c_str());
}

GameObject *Database::readObject(Common::SeekableReadStream &s, int16 cls) {
	GameObject *o = new GameObject();
	o->cls = cls;
	readObjectBase(s, o);

	switch (cls) {
	case kObjBitmap:
	case kObjMaskBitmap:
	case kObjPaletteBitmap:
	case kObjSegmentedBitmap:
		o->file = readStringRef(s);
		break;
	case kObjRandomMapBitmap:
		o->file = readStringRef(s);
		for (int i = 0; i < 3; i++)
			o->ints.push_back(s.readSint16BE());
		break;
	case kObjButton:
	case kObjToggleButton:
	case kObjQuestionOKButton:
	case kObjCollageButton:
		for (int i = 0; i < 3; i++)
			o->strs.push_back(readStringRef(s));
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		if (cls == kObjQuestionOKButton)
			o->ints.push_back(s.readSint16BE());
		if (cls == kObjCollageButton) {
			o->ints.push_back(s.readSint16BE());
			o->u32s.push_back(s.readUint32BE());
		}
		o->file = o->strs[0];
		break;
	case kObjVanillaHotspot:
	case kObjHotspot:
	case kObjZoomAreaHotspot:
	case kObjScrollQuestAnswerHtsp:
	case kObjZoomBorderHotspot:
	case kObjExploreHotspot:
	case kObjDungeonDisableHotspot:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjZoomHotspot:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->u32s.push_back(s.readUint32BE());
		break;
	case kObjNodalHotspot:
	case kObjPlayResponseHotspot:
	case kObjRepeatingHotspot:
	case kObjRollRepeatHotspot:
		for (int i = 0; i < 3; i++)
			o->ints.push_back(s.readSint16BE());
		break;
	case kObjRandomScenarioHotspot:
		for (int i = 0; i < 5; i++)
			o->ints.push_back(s.readSint16BE());
		o->strs.push_back(readStringRef(s));
		break;
	case kObjPageTurn:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->u32s.push_back(s.readUint32BE());
		for (int i = 0; i < 3; i++)
			o->strs.push_back(readStringRef(s));
		for (int i = 0; i < 3; i++)
			o->points.push_back(readPoint(s));
		o->strs.push_back(readInlineString(s));
		o->strs.push_back(readInlineString(s));
		break;
	case kObjSaveAsHotspot:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		for (int i = 0; i < 7; i++)
			o->strs.push_back(readInlineString(s));
		break;
	case kObjOptionsButton:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->strs.push_back(readInlineString(s));
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjSoundHotspot:
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->strs.push_back(readInlineString(s));
		break;
	case kObjInclude:
	case kObjInclude3D:
	case kObjNoBitmap:
	case kObjWordBox:
	case kObjZoomCaption:
	case kObjDungeonTimer:
	case kObjVideoStartBitmap:
	case kObjPlaceHolder:
	case kObjZoomAmbientSoundObj:
		break;
	case kObjLocator:
		o->strs.push_back(readStringRef(s));
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjColourHotspot:
	case kObjScrollObject:
	case kObjWrapScrollObject:
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjParaText: {
		for (int i = 0; i < 3; i++)
			o->ints.push_back(s.readSint16BE());
		int n = readCount(s);
		for (int i = 0; i < n; i++)
			o->text += (char)s.readByte();
		o->ints.push_back(s.readSint16BE());
		break;
	}
	case kObjEditBox:
	case kObjRoomEditBox:
	case kObjScrollEditBox:
		for (int i = 0; i < 3; i++)
			o->strs.push_back(readStringRef(s));
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		if (cls == kObjScrollEditBox)
			for (int i = 0; i < 3; i++)
				o->ints.push_back(s.readSint16BE());
		break;
	case kObjDitherBitmap:
		o->file = readStringRef(s);
		o->strs.push_back(readInlineString(s));
		break;
	case kObjSpyDitherBitmap:
		o->file = readStringRef(s);
		o->strs.push_back(readInlineString(s));
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjCollectBitmap:
		o->ints.push_back(s.readSint16BE());
		o->strs.push_back(readInlineString(s));
		o->strs.push_back(readInlineString(s));
		o->file = o->strs[0];
		break;
	case kObjCoinBitmap:
		o->file = readStringRef(s);
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjBoxIconBitmap:
		o->file = readStringRef(s);
		o->ints.push_back(s.readSint16BE());
		o->strs.push_back(readInlineString(s));
		break;
	case kObjScrollTickBitmap:
		o->file = readStringRef(s);
		o->ints.push_back(s.readSint16BE());
		break;
	case kObjHighlightingCastle:
	case kObjNavRollOverButton:
		o->strs.push_back(readInlineString(s));
		o->file = o->strs[0];
		break;
	case kObjHatchBitmap:
		o->strs.push_back(readInlineString(s));
		o->strs.push_back(readInlineString(s));
		o->file = o->strs[0];
		break;
	case kObjAmbientAnimation: {
		o->ints.push_back(s.readSint16BE());
		int n = s.readSint16BE();
		o->spriteFrames = n;
		int files = MAX(1, MIN(n, 5));
		for (int i = 0; i < files; i++)
			o->strs.push_back(readInlineString(s));
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		o->file = o->strs[0];
		break;
	}
	case kObjGroupCollage: {
		for (int i = 0; i < 3; i++) {
			int16 f = s.readSint16BE();
			o->ints.push_back(f ? s.readSint16BE() : -1);
		}
		int n = readCount(s);
		for (int i = 0; i < n; i++)
			o->ints.push_back(s.readSint16BE());
		int16 f = s.readSint16BE();
		o->ints.push_back(f ? s.readSint16BE() : -1);
		break;
	}
	case kObjClipRect: {
		o->ints.push_back(s.readSint16BE());
		o->ints.push_back(s.readSint16BE());
		int n = readCount(s);
		for (int i = 0; i < n; i++)
			o->ints.push_back(s.readSint16BE());
		n = readCount(s);
		for (int i = 0; i < n; i++)
			o->points.push_back(readPoint(s));
		break;
	}
	case kObjSprite: {
		o->spriteFrames = s.readSint16BE();
		o->file = readInlineString(s);
		o->points.push_back(readPoint(s));
		o->ints.push_back(s.readSint16BE());
		o->points.push_back(readPoint(s));
		for (int i = 0; i < 3; i++)
			o->rects.push_back(readRect(s));
		for (int i = 0; i < 15; i++)
			o->ints.push_back(s.readSint16BE());
		o->points.push_back(readPoint(s));
		int16 flag = s.readSint16BE();
		if (flag) {
			HotspotMask *m = readMask(s);
			delete m;
			int n = readCount(s);
			for (int i = 0; i < n; i++)
				readPoint(s);
		}
		int n = readCount(s);
		for (int i = 0; i < n; i++)
			o->points.push_back(readPoint(s));
		n = readCount(s);
		for (int i = 0; i < n; i++)
			o->scripts.push_back(readScriptObject(s));
		break;
	}
	case kObjScrollBar:
	case kObjScrollBitmap:
		// These derive from a different base class; the base fields were
		// already consumed above in a compatible way only partially, so
		// re-read the record with the scroll layout.
		error("Castle: scroll object class %s not supported in this layout", objectClassName(cls));
	case kObjCollage:
		o->ints.push_back(s.readByte());
		o->ints.push_back(s.readByte());
		o->ints.push_back(s.readByte());
		o->u32s.push_back(s.readUint32BE());
		o->collageA = readInlineString(s);
		o->collageB = readInlineString(s);
		readCollageSub(s, o->collageA, o);
		readCollageSub(s, o->collageB, o);
		break;
	default:
		error("Castle: unknown object class %d at %d", cls, (int)s.pos());
	}
	return o;
}

Panel *Database::readPanelBody(Common::SeekableReadStream &s, int16 type) {
	Panel *p = new Panel();
	p->type = type;
	p->id = s.readSint16BE();
	int n = readCount(s);
	for (int i = 0; i < n; i++) {
		int16 cls = s.readSint16BE();
		if (cls == kObjScrollBar || cls == kObjScrollBitmap) {
			// Scroll objects use a different base layout:
			// s16, rect, strref, s16, s16, u32, u32, u32 then class fields.
			GameObject *o = new GameObject();
			o->cls = cls;
			o->id = s.readSint16BE();
			o->rect = readRect(s);
			o->cursor = readStringRef(s);
			o->ints.push_back(s.readSint16BE());
			o->ints.push_back(s.readSint16BE());
			for (int k = 0; k < 3; k++)
				o->u32s.push_back(s.readUint32BE());
			if (cls == kObjScrollBar) {
				for (int k = 0; k < 8; k++)
					o->strs.push_back(readStringRef(s));
				for (int k = 0; k < 5; k++)
					o->ints.push_back(s.readSint16BE());
				o->u32s.push_back(s.readUint32BE());
			} else {
				o->file = readStringRef(s);
				o->ints.push_back(s.readSint16BE());
				o->rects.push_back(readRect(s));
				o->ints.push_back(s.readSint16BE());
				int m = readCount(s);
				for (int k = 0; k < m; k++)
					o->u32s.push_back(s.readUint32BE());
			}
			p->objects.push_back(o);
		} else {
			p->objects.push_back(readObject(s, cls));
		}
	}
	readEvents(s, p->events);
	p->ext = readExtension(s);
	if (type == kPanelTransparentText) {
		int len = readCount(s);
		for (int i = 0; i < len; i++)
			p->text += (char)s.readByte();
		p->textFlags = s.readUint32BE();
	} else if (type == kPanelSpyChest) {
		for (int i = 0; i < 31; i++)
			p->spy.push_back(s.readSint16BE());
	}
	return p;
}

PageRecord *Database::readRecord(uint index) {
	uint32 start = _pageDataStart + _offsets[index];
	if (start >= _size)
		error("Castle: record %u out of range", index);
	Common::MemoryReadStream s(_data + start, _size - start);
	PageRecord *rec = new PageRecord();
	int16 t = s.readSint16BE();
	if (t == 0) {
		rec->isPage = false;
		rec->panels.push_back(readPanelBody(s, kPanelNull));
		return rec;
	}
	rec->isPage = true;
	rec->type = t;
	rec->id = s.readSint16BE();
	rec->pos = readPoint(s);
	rec->dir = readStringRef(s);
	rec->ext = readExtension(s);
	int n = readCount(s);
	for (int i = 0; i < n; i++) {
		int16 pt = s.readSint16BE();
		rec->panels.push_back(readPanelBody(s, pt));
	}
	readEvents(s, rec->events);
	return rec;
}

PageRecord *Database::getRecord(uint index) {
	if (index >= _offsets.size())
		return nullptr;
	if (_records.contains(index))
		return _records[index];
	PageRecord *rec = readRecord(index);
	_records[index] = rec;
	return rec;
}

int Database::getBuiltinNumber(int id) const {
	for (uint i = 0; i < _builtinIds.size(); i++)
		if (_builtinIds[i] == id)
			return _builtinNums[i];
	return 0;
}

const PageTemplate *Database::findTemplate(int16 id) const {
	for (uint i = 0; i < _templates.size(); i++)
		if (_templates[i]->id == id)
			return _templates[i];
	return nullptr;
}

bool Database::readDocument(Common::SeekableReadStream &s, uint32 end) {
	_title = readStringRef(s);
	readPoint(s); // document size (640x480)
	_version = readStringRef(s);
	_defaultCursor = readStringRef(s);
	byte rgb[3];
	readRGB(s, rgb);
	s.readSint16BE();

	// Document extension: two variable lists, object references, pairs
	_docExt = new Extension();
	int n = readCount(s);
	_docExt->vars.resize(n);
	for (int i = 0; i < n; i++)
		readVariableTable(s, _docExt->vars[i]);
	n = readCount(s);
	uint base = _docExt->vars.size();
	_docExt->vars.resize(base + n);
	for (int i = 0; i < n; i++)
		readVariableTable(s, _docExt->vars[base + i]);
	n = readCount(s);
	for (int i = 0; i < n; i++)
		_docExt->objs.push_back(readObjectRef(s));
	n = readCount(s);
	for (int i = 0; i < n; i++) {
		_builtinIds.push_back(s.readSint16BE());
		_builtinNums.push_back(s.readSint16BE());
	}

	s.readSint16BE();
	uint32 n32 = s.readUint32BE();
	for (uint32 i = 0; i < n32; i++)
		_startPages.push_back(s.readUint32BE());

	n = readCount(s);
	for (int i = 0; i < n; i++) {
		PageTemplate *t = new PageTemplate();
		t->type = s.readSint16BE();
		t->id = s.readSint16BE();
		int16 f = s.readSint16BE();
		if (f) {
			s.readSint16BE();
			s.readSint16BE();
		}
		t->flags = s.readSint16BE();
		t->size = readPoint(s);
		f = s.readSint16BE();
		t->hasPos4c = f != 0;
		if (f)
			t->pos4c = readPoint(s);
		int16 extra[12];
		for (int k = 0; k < 2 + 6 + 4; k++)
			extra[k] = s.readSint16BE();
		t->centred = extra[6] != 0;
		debugC(3, kDebugDatabase, "Castle: template %d type %d flags %d size %d,%d pos4c %d,%d (%d) extra %d %d %d %d %d %d %d %d %d %d %d %d", t->id, t->type, t->flags, t->size.x, t->size.y, t->pos4c.x, t->pos4c.y, t->hasPos4c ? 1 : 0, extra[0], extra[1], extra[2], extra[3], extra[4], extra[5], extra[6], extra[7], extra[8], extra[9], extra[10], extra[11]);
		f = s.readSint16BE();
		if (f)
			readRect(s);
		int k = readCount(s);
		for (int j = 0; j < k; j++)
			s.readSint16BE();
		k = readCount(s);
		for (int j = 0; j < k; j++)
			s.readSint16BE();
		k = readCount(s);
		for (int j = 0; j < k; j++) {
			PanelDesc d;
			d.type = s.readSint16BE();
			d.id = s.readSint16BE();
			int16 fl = s.readSint16BE();
			d.inStream = fl != 0;
			d.index = fl ? s.readUint32BE() : 0;
			readRGB(s, d.rgb);
			d.size = readPoint(s);
			d.pos = readPoint(s);
			t->panels.push_back(d);
		}
		readEvents(s, t->events);
		_templates.push_back(t);
	}
	readEvents(s, _docEvents);
	return readDocumentTail(s, end);
}

void Database::readAnswerList(Common::SeekableReadStream &s, AnswerList &a, bool withInts) {
	if (withInts)
		for (int i = 0; i < 3; i++)
			a.ints[i] = s.readSint16BE();
	int n = readCount(s);
	if (withInts)
		a.ints[3] = n;
	for (int i = 0; i < n; i++)
		a.accepted.push_back(readInlineString(s));
	n = readCount(s);
	for (int i = 0; i < n; i++)
		a.misspelled.push_back(readInlineString(s));
}

// The data after the document events: two text style tables, the chest
// quiz, the answer lists, and the ids of the variables, pages and objects
// the built-in game logic works with.
bool Database::readDocumentTail(Common::SeekableReadStream &s, uint32 end) {
	// Text styles of the edit boxes: object id, font id, 4 colours, 5 shorts, font name, short
	int n = readCount(s);
	for (int i = 0; i < n; i++) {
		s.skip(4 + 12 + 2 + 8);
		readInlineString(s);
		s.skip(2);
	}
	n = readCount(s);
	s.skip(n * 15);

	for (int spy = 0; spy < 3; spy++) {
		for (int q = 0; q < 4; q++) {
			Question &qu = _tail.questions[spy][q];
			int m = readCount(s);
			for (int i = 0; i < m; i++) {
				QuestionStep st;
				for (int k = 0; k < 15; k++)
					st.v[k] = s.readSint16BE();
				qu.steps.push_back(st);
			}
			m = readCount(s);
			for (int i = 0; i < m; i++) {
				QuestionObject o;
				o.type = s.readSint16BE();
				o.value = s.readSint16BE();
				switch (o.type) {
				case 0:
				case 2:
					o.str = readInlineString(s);
					break;
				case 1:
					o.str = readInlineString(s);
					o.a = s.readSint16BE();
					break;
				case 3:
				case 8:
					o.u = s.readUint32BE();
					o.a = s.readSint16BE();
					break;
				case 4:
					o.a = s.readSint16BE();
					o.b = s.readSint16BE();
					break;
				case 5:
				case 6:
				case 7:
					o.a = s.readSint16BE();
					break;
				case 9:
					o.str = readInlineString(s);
					o.a = s.readSint16BE();
					o.b = s.readSint16BE();
					break;
				case 10:
				case 11:
					break;
				default:
					warning("Castle: unknown question object type %d at %d", o.type, (int)s.pos());
					return false;
				}
				qu.objects.push_back(o);
			}
		}
	}

	s.readUint32BE();
	n = readCount(s);
	for (int i = 0; i < n; i++) {
		AnswerList a;
		readAnswerList(s, a, true);
		_tail.scenarios.push_back(a);
	}
	n = readCount(s);
	for (int i = 0; i < n; i++)
		_tail.commonWords.push_back(readInlineString(s));
	for (int i = 0; i < 12; i++)
		readAnswerList(s, _tail.answers[i], false);

	for (int i = 0; i < 11; i++)
		_tail.vars[i] = s.readSint16BE();
	for (int i = 0; i < 10; i++)
		_tail.pages[i] = s.readUint32BE();
	for (int i = 0; i < 16; i++)
		_tail.ints[i] = s.readSint16BE();
	for (int i = 0; i < 4; i++)
		_tail.pages2[i] = s.readUint32BE();
	_tail.page144 = s.readUint32BE();
	for (int i = 0; i < 3; i++)
		_tail.ints3[i] = s.readSint16BE();
	for (int i = 0; i < 11; i++)
		s.readSint16BE(); // font ids
	n = readCount(s);
	for (int i = 0; i < n; i++) {
		ToggleDesc t;
		t.objectId = s.readSint16BE();
		t.state = s.readSint16BE();
		t.code = s.readSint16BE();
		_tail.toggles.push_back(t);
	}
	for (int i = 0; i < 4; i++)
		_tail.ints4[i] = s.readSint16BE();
	_tail.questId = s.readSint16BE();
	for (int i = 0; i < 10; i++)
		_tail.questMasks[i] = s.readSint16BE();
	_tail.questId2 = s.readSint16BE();
	for (int i = 0; i < 3; i++)
		_tail.questPages[i] = s.readUint32BE();
	for (int i = 0; i < 3; i++)
		_tail.quitPages[i] = s.readUint32BE();

	debugC(1, kDebugDatabase, "Castle: document parse ended at %d, page table at %u; spy var %d, scenario var %d, new game var %d",
	       (int)s.pos(), end, _tail.getSpyVar(), _tail.getScenarioVar(), _tail.getNewGameVar());
	if ((uint32)s.pos() != end)
		warning("Castle: document tail ended at %d, expected %u", (int)s.pos(), end);
	return true;
}

bool Database::load(const Common::Path &filename) {
	Common::File f;
	if (!f.open(filename)) {
		warning("Castle: cannot open %s", filename.toString().c_str());
		return false;
	}
	_size = f.size();
	_data = (byte *)malloc(_size);
	f.read(_data, _size);
	f.close();

	Common::MemoryReadStream s(_data, _size);
	// Header strings
	for (int i = 0; i < 7; i++)
		readInlineString(s);
	_pageDataStart = s.readUint32BE();
	_stringTableSize = s.readUint32BE();
	_pageTableStart = s.readUint32BE();
	uint32 strStart = s.pos();
	int n = s.readUint16BE();
	for (int i = 0; i < n; i++)
		_strings.push_back(readInlineString(s));
	if ((uint32)s.pos() != strStart + _stringTableSize)
		warning("Castle: string table size mismatch");

	readDocument(s, _pageTableStart);

	s.seek(_pageTableStart);
	uint32 count = s.readUint32BE();
	for (uint32 i = 0; i < count; i++)
		_offsets.push_back(s.readUint32BE());

	debugC(1, kDebugDatabase, "Castle: loaded database '%s' (%s): %u strings, %u templates, %u records, start page %u",
	       _title.c_str(), _version.c_str(), _strings.size(), _templates.size(), _offsets.size(), getStartPage());
	return true;
}

} // End of namespace Castle
