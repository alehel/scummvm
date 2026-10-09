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

#ifndef CASTLE_DATABASE_H
#define CASTLE_DATABASE_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "common/str.h"
#include "common/stream.h"

namespace Castle {

/*
 * CASTLE.PNG is the page database of the DK Multimedia engine ("DKC").
 * Despite its extension it is not an image. All integers are big-endian.
 *
 * Layout:
 *   header   : 7 length-prefixed strings ("C000", "10", "Windows", "PP", "US",
 *              title, version), then u32 pageDataStart, u32 stringTableSize,
 *              u32 pageTableStart
 *   strings  : u16 count, then (u16 len, bytes) * count
 *   document : document header, the global extension (variables), the
 *              template pages (layouts) and document events
 *   pagetable: u32 count, then u32 offsets relative to pageDataStart
 *   records  : page records (first word = page type) or bare panel records
 *              (first word = 0), referenced by index
 */

enum PageType {
	kPageNull = 0,
	kPageBase = 1,
	kPageChild = 2,
	kPagePopup = 3,
	kPageDragPopup = 4,
	kPageRolloffClose = 5
};

enum PanelType {
	kPanelNull = 0,
	kPanelDisplay = 1,
	kPanelScroll = 2,
	kPanelSprite = 3,
	kPanelTransparent = 4,
	kPanelTransparentSprite = 5,
	kPanelTransparentText = 6,
	kPanelOptions = 7,
	kPanelSpyChest = 8,
	kPanelZoomSprite = 9,
	kPanelPopupClose = 10,
	kPanelLibrary = 11
};

enum EventType {
	kEventNull = 0,
	kEventOpen,
	kEventPreOpen,
	kEventClose,
	kEventFinish,
	kEventClick,
	kEventRollOn,
	kEventRollOff,
	kEventEnableRequest,
	kEventDisableRequest,
	kEventActivateApp,
	kEventDeactivateApp,
	kEventLoad,
	kEventPicsRewind,
	kEventTimerEnd,
	kEventQuestionsAnswered,
	kEventToggleScroll,
	kEventSpyChanged,
	kEventLast
};

enum ActionType {
	kActNull = 0,
	kActChangePage,
	kActOpenPage,
	kActClosePage,
	kActAppendPage,
	kActPlayPics,
	kActStopPics,
	kActCommand,
	kActPlayWave,
	kActNewPlayWave,
	kActStopWave,
	kActScrollPanel,
	kActSendMessage,
	kActZoom,
	kActPaintBitmap,
	kActClearBitmap,
	kActResetResources,
	kActQuit,
	kActLink,
	kActBack,
	kActPaintZoomObject,
	kActClearZoomObject,
	kActPaintZoomArea,
	kActPlayResponse,
	kActUpdateNodeHtsp,
	kActChangeSpyType,
	kActPlayVideo,
	kActStopVideo,
	kActPlayVideoInd,
	kActDoTransition,
	kActChangeColourRefBitmap,
	kActHighlightCastleSection,
	kActBribeGuard,
	kActStopDungeonTimer,
	kActStartDungeonTimer,
	kActScrollListBox,
	kActActivityCompleted,
	kActOptions,
	kActPlayPicsEx,
	kActPaintHelpText,
	kActClearHelpText,
	kActPlayWaveChannel,
	kActStopWaveChannel,
	kActGeneralPurpose,
	kActTransitionVideo,
	kActSetSpriteFrame,
	kActFileSaveAs,
	kActOpenHelp,
	kActLast
};

// Object classes, numbered by their position in the alphabetically
// (case-insensitively) sorted list of registered class names.
enum ObjectClass {
	kObjAmbientAnimation = 0,
	kObjBitmap,
	kObjBoxIconBitmap,
	kObjButton,
	kObjClipRect,
	kObjCoinBitmap,
	kObjCollage,
	kObjCollageButton,
	kObjCollectBitmap,
	kObjColourHotspot,
	kObjDitherBitmap,
	kObjDungeonDisableHotspot,
	kObjDungeonTimer,
	kObjEditBox,
	kObjExploreHotspot,
	kObjGroupCollage,
	kObjHatchBitmap,
	kObjHighlightingCastle,
	kObjHotspot,
	kObjInclude,
	kObjInclude3D,
	kObjLocator,
	kObjMaskBitmap,
	kObjNavRollOverButton,
	kObjNoBitmap,
	kObjNodalHotspot,
	kObjOptionsButton,
	kObjPageTurn,
	kObjPaletteBitmap,
	kObjParaText,
	kObjPlaceHolder,
	kObjPlayResponseHotspot,
	kObjQuestionOKButton,
	kObjRandomMapBitmap,
	kObjRandomScenarioHotspot,
	kObjRepeatingHotspot,
	kObjRollRepeatHotspot,
	kObjRoomEditBox,
	kObjSaveAsHotspot,
	kObjScrollBar,
	kObjScrollBitmap,
	kObjScrollEditBox,
	kObjScrollObject,
	kObjScrollQuestAnswerHtsp,
	kObjScrollTickBitmap,
	kObjSegmentedBitmap,
	kObjSoundHotspot,
	kObjSprite,
	kObjSpyDitherBitmap,
	kObjToggleButton,
	kObjVanillaHotspot,
	kObjVideoStartBitmap,
	kObjWordBox,
	kObjWrapScrollObject,
	kObjZoomAmbientSoundObj,
	kObjZoomAreaHotspot,
	kObjZoomBorderHotspot,
	kObjZoomCaption,
	kObjZoomHotspot,
	kObjClassCount
};

// Object flags (+0x44 in the original object)
enum ObjectFlags {
	kObjFlagNoRect = 0x100,     // no rectangle stored in the record
	kObjFlagCopyRect = 0x200,   // keeps a copy of its rect: fixed on screen, not scrolled with the panel
	kObjFlagHasMask = 0x800     // a scanline mask follows the base fields
};

struct Action;
struct ScriptObject;

struct Expression;

// One operand of an expression (a constant, a variable reference, ...)
struct Operand {
	uint32 u;
	int16 a, b, type, c, e, f;
	uint32 u2;
	Common::Point pt;
	Common::Rect rect;
	Common::String str;
	double real;
};

// Stack-based expression program
struct Expression {
	Common::Array<int16> ops;
	Common::Array<Operand> operands;
	Common::Array<int16> ints;
};

struct Event {
	int16 type;
	Common::Array<Action *> actions;
	~Event();
};

struct VariableTable {
	int16 name;
	int16 type;
	Common::Array<int16> dims;
	Common::Array<int16> ints;
	Common::Array<uint32> u32s;
	Common::Array<Common::Point> pts;
	Common::Array<Common::Rect> rects;
	Common::Array<Common::String> strs;
	Common::Array<double> reals;
	Common::Array<int16> l1, l2;
	Common::Array<uint32> l3;
};

struct Extension;

// Object reference stored in an extension (named script holder)
struct ObjectRef {
	int16 a, b, c;
	Extension *ext;
	ScriptObject *script;
	ObjectRef() : a(0), b(0), c(0), ext(nullptr), script(nullptr) {}
	~ObjectRef();
};

// "Extension" record: variable tables and object references
struct Extension {
	int16 tag;
	Common::Array<VariableTable> vars;
	Common::Array<ObjectRef *> objs;
	~Extension();
};

// Script object: an opcode program with sub-expressions and an action list
struct ScriptObject {
	int16 a;
	Common::String name;
	Common::Rect rect;
	int16 b, c;
	Common::Array<int16> ops;
	Common::Array<Expression *> opA;
	Common::Array<Expression *> opB1, opB2;
	Common::Array<Expression *> opC;
	Extension *ext;
	Common::Array<Action *> actions;
	ScriptObject() : a(0), b(0), c(0), ext(nullptr) {}
	~ScriptObject();
};

struct LinkEntry {
	uint32 page;
	int16 x;
};

struct Action {
	int16 type;
	uint32 page;        // page index for page actions, generic u32 otherwise
	int16 x;            // generic short (transition index, etc.)
	Common::String name;
	int16 p[8];
	Common::Point pt;
	Common::Rect rect;
	Common::Array<Common::String> strs;
	Common::Array<LinkEntry> links;
	ScriptObject *script;
	Action() : type(0), page(0), x(0), script(nullptr) { memset(p, 0, sizeof(p)); }
	~Action();
};

// Scanline mask of a hotspot (stored when flag 0x800 is set)
struct HotspotMask {
	Common::Point pos;
	Common::Point size;
	int16 a, b, c;
	Common::Array<uint32> data;
};

struct GameObject {
	int16 cls;
	int16 id;
	int16 b;
	int16 flags;
	Common::Rect rect;
	Common::String cursor;
	int16 c, d, e;
	HotspotMask *mask;
	Common::Array<Event *> events;
	Extension *ext;

	// class specific data
	Common::String file;                   // bitmap file name
	Common::Array<Common::String> strs;    // string refs / inline strings
	Common::Array<int16> ints;             // shorts in declaration order
	Common::Array<uint32> u32s;
	Common::Array<Common::Point> points;
	Common::Array<Common::Rect> rects;
	Common::String text;                   // ParaText contents

	// Sprite
	int16 spriteFrames;
	Common::Array<ScriptObject *> scripts;

	// Collage
	Common::String collageA, collageB;

	GameObject() : cls(0), id(0), b(0), flags(0), c(0), d(0), e(0), mask(nullptr), ext(nullptr), spriteFrames(0) {}
	~GameObject();

	bool isHotspot() const;
	bool isBitmap() const;
	const Event *findEvent(int type) const;
};

struct Panel {
	int16 type;
	int16 id;
	Common::Array<GameObject *> objects;
	Common::Array<Event *> events;
	Extension *ext;
	Common::String text;        // TransparentTextPanel
	uint32 textFlags;
	Common::Array<int16> spy;   // SpyChestPanel
	Panel() : type(0), id(0), ext(nullptr), textFlags(0) {}
	~Panel();
};

// A record from the page stream: either a whole page or a bare panel
struct PageRecord {
	bool isPage;
	int16 type;         // page type
	int16 id;           // template id
	Common::Point pos;
	Common::String dir; // resource directory
	Extension *ext;
	Common::Array<Panel *> panels;
	Common::Array<Event *> events;
	Common::String title;   // name shown on the Trail page
	int16 icon;             // shield icon of the Trail page (1..34, 0 = none)
	PageRecord() : isPage(false), type(0), id(0), ext(nullptr), icon(0) {}
	~PageRecord();
};

// Panel slot of a page template
struct PanelDesc {
	int16 type;
	int16 id;
	bool inStream;
	uint32 index;       // stream record of the panel when inStream
	byte rgb[3];
	Common::Point pos;
	Common::Point size;
};

// Page template (layout) from the document
struct PageTemplate {
	int16 type;
	int16 id;
	Common::Point size;
	int16 flags;
	Common::Point pos4c;
	bool hasPos4c;
	bool centred;       // popups without a position open centred on the screen
	Common::Array<PanelDesc> panels;
	Common::Array<Event *> events;
	~PageTemplate();
};

// A quiz question of the spy's chest: the records describe the steps of the
// conversation, the objects the answers (strings, videos, pages...)
struct QuestionStep {
	int16 v[15];
};
struct QuestionObject {
	int16 type;
	int16 value;
	Common::String str;
	uint32 u;
	int16 a, b;
	QuestionObject() : type(0), value(0), u(0), a(0), b(0) {}
};
struct Question {
	Common::Array<QuestionStep> steps;
	Common::Array<QuestionObject> objects;
};

// An answer list: accepted spellings and recognised misspellings
struct AnswerList {
	int16 ints[4];
	Common::Array<Common::String> accepted;
	Common::Array<Common::String> misspelled;
	AnswerList() { memset(ints, 0, sizeof(ints)); }
};

struct ToggleDesc {
	int16 objectId;
	int16 state;
	int16 code;       // OptionsAction code the toggle belongs to
};

// Trailing document data: quest tables, variable and page references
// A text style of the document: colours and font of the edit boxes and lists
struct TextStyle {
	int16 fontId;
	int16 id;                           // style id referenced by the objects
	byte rgb[4][3];                     // background, text, highlight bar, highlighted text
	int16 size;                         // tenths of a point
	int16 flags[4];
	Common::String fontName;
	TextStyle() : fontId(0), id(0), size(0) {
		memset(rgb, 0, sizeof(rgb));
		memset(flags, 0, sizeof(flags));
	}
};

struct DocumentTail {
	Common::Array<TextStyle> styles;
	Question questions[3][4];           // [spy][question]
	Common::Array<AnswerList> scenarios;
	Common::Array<Common::String> commonWords;
	AnswerList answers[12];
	int16 vars[11];                     // document variable ids (+0x910..)
	uint32 pages[10];                   // zoom pages (+0xc0..)
	int16 ints[16];                     // object ids (+0xe8..)
	uint32 pages2[4];                   // +0x128..
	uint32 page144;
	int16 ints3[3];
	int16 titleStrs[11];                // trail titles: 6 library books, 4 rooms, castle guide
	Common::Array<ToggleDesc> toggles;
	int16 ints4[4];
	int16 questId;
	int16 questMasks[10];
	int16 questId2;
	uint32 spellPage;                   // "check your spelling" popup of the chest scrolls
	uint32 questPages[3];               // save-before-load, save-before-new-game, ending
	uint32 quitPages[3];                // save-before-quit, main, quit confirmation
	DocumentTail() : page144(0), spellPage(0), questId(0), questId2(0) {
		memset(vars, 0, sizeof(vars));
		memset(pages, 0, sizeof(pages));
		memset(ints, 0, sizeof(ints));
		memset(pages2, 0, sizeof(pages2));
		memset(ints3, 0, sizeof(ints3));
		memset(titleStrs, 0, sizeof(titleStrs));
		memset(ints4, 0, sizeof(ints4));
		memset(questMasks, 0, sizeof(questMasks));
		memset(questPages, 0, sizeof(questPages));
		memset(quitPages, 0, sizeof(quitPages));
	}
	int16 getScenarioVar() const { return vars[8]; }
	int16 getSpyVar() const { return vars[9]; }
	int16 getNewGameVar() const { return vars[10]; }
};

class Database {
public:
	Database();
	~Database();

	bool load(const Common::Path &filename);

	const Common::String &getString(int index1) const; // 1-based, 0 = empty
	uint getRecordCount() const { return _offsets.size(); }
	PageRecord *getRecord(uint index);
	const PageTemplate *findTemplate(int16 id) const;

	const Common::String &getTitle() const { return _title; }
	uint32 getStartPage() const { return _startPages.size() > 0 ? _startPages[0] : 0; }
	uint32 getMainPage() const { return _startPages.size() > 1 ? _startPages[1] : 0; }
	const Common::String &getDefaultCursor() const { return _defaultCursor; }
	const Extension *getDocExtension() const { return _docExt; }
	int getBuiltinNumber(int id) const;
	const DocumentTail &getTail() const { return _tail; }
	const TextStyle *findStyle(int16 id) const;

private:
	// low level readers (big endian)
	int16 readS16(Common::SeekableReadStream &s) const { return s.readSint16BE(); }
	uint32 readU32(Common::SeekableReadStream &s) const { return s.readUint32BE(); }
	int readCount(Common::SeekableReadStream &s) const;
	Common::Point readPoint(Common::SeekableReadStream &s) const;
	Common::Rect readRect(Common::SeekableReadStream &s) const;
	Common::String readInlineString(Common::SeekableReadStream &s) const;
	Common::String readStringRef(Common::SeekableReadStream &s) const;
	void readRGB(Common::SeekableReadStream &s, byte *rgb) const;

	void readEvents(Common::SeekableReadStream &s, Common::Array<Event *> &events);
	void readActions(Common::SeekableReadStream &s, Common::Array<Action *> &actions);
	Action *readAction(Common::SeekableReadStream &s, int16 type);
	Extension *readExtension(Common::SeekableReadStream &s);
	void readVariableTable(Common::SeekableReadStream &s, VariableTable &v);
	ObjectRef *readObjectRef(Common::SeekableReadStream &s);
	ScriptObject *readScriptObject(Common::SeekableReadStream &s);
	Expression *readExpression(Common::SeekableReadStream &s);
	void readOperand(Common::SeekableReadStream &s, Operand &o);
	HotspotMask *readMask(Common::SeekableReadStream &s);
	void readObjectBase(Common::SeekableReadStream &s, GameObject *o);
	GameObject *readObject(Common::SeekableReadStream &s, int16 cls);
	void readCollageSub(Common::SeekableReadStream &s, const Common::String &cls, GameObject *o);
	Panel *readPanelBody(Common::SeekableReadStream &s, int16 type);
	PageRecord *readRecord(uint index);
	bool readDocument(Common::SeekableReadStream &s, uint32 end);
	bool readDocumentTail(Common::SeekableReadStream &s, uint32 end);
	void readAnswerList(Common::SeekableReadStream &s, AnswerList &a, bool withInts);

	byte *_data;
	uint32 _size;
	uint32 _pageDataStart;
	uint32 _stringTableSize;
	uint32 _pageTableStart;
	Common::Array<Common::String> _strings;
	Common::Array<uint32> _offsets;
	Common::Array<PageTemplate *> _templates;
	Common::Array<uint32> _startPages;
	Common::String _title, _version, _defaultCursor;
	Extension *_docExt;
	Common::Array<int16> _builtinIds, _builtinNums;
	Common::Array<Event *> _docEvents;
	DocumentTail _tail;
	Common::HashMap<uint, PageRecord *> _records;
	Common::String _empty;
};

const char *objectClassName(int cls);
const char *actionName(int type);
const char *eventName(int type);

} // End of namespace Castle

#endif
