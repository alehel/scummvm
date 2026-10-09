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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "engines/util.h"
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "graphics/cursor.h"
#include "graphics/cursorman.h"
#include "graphics/palette.h"
#include "graphics/paletteman.h"
#include "video/qt_decoder.h"

#include "common/file.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/tokenizer.h"
#include "engines/metaengine.h"
#include "image/png.h"

#include "castle/ani.h"
#include "castle/castle.h"
#include "castle/collage.h"
#include "castle/database.h"
#include "castle/detection.h"
#include "castle/page.h"
#include "castle/quest.h"
#include "castle/quiz.h"
#include "castle/resources.h"
#include "castle/vm.h"

namespace Castle {

CastleEngine::CastleEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc),
		_rnd("castle"), _script(nullptr), _hoverObject(nullptr), _hoverPage(nullptr), _db(nullptr), _res(nullptr), _basePage(nullptr), _dirty(true), _paletteDirty(true), _pendingBasePage(0),
		_pendingBase(false), _dumpCount(0), _ani(nullptr), _aniNextFrame(0), _quest(nullptr), _quiz(nullptr), _activityCompleted(false), _spyChangedFlag(false), _saveSlot(-1), _savedPage(0), _pressedObject(nullptr), _pressedPage(nullptr), _dragging(false), _dragPage(nullptr), _dungeonTimerEnd(0), _scrollObject(nullptr), _scrollPage(nullptr), _scrollNext(0), _scrollStep(1), _ambientNext(0),
		_trailNavigating(false), _pendingPopup(0), _scrollBarDrag(false), _scrollBarGrab(0), _editFocus(nullptr), _editFocusPage(nullptr), _pendingTransition(0), _noScreenUpdate(false), _repeatNext(0) {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "dkcode");
	SearchMan.addSubDirectoryMatching(gameDataDir, "3drooms", 0, 3);
	SearchMan.addSubDirectoryMatching(gameDataDir, "chest", 0, 3);
	SearchMan.addSubDirectoryMatching(gameDataDir, "library", 0, 3);
	SearchMan.addSubDirectoryMatching(gameDataDir, "slices", 0, 3);
	SearchMan.addSubDirectoryMatching(gameDataDir, "rotcast", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "intro", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "topl", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "trail", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "gloss", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "help", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "hut", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "dungeon", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "ending", 0, 2);
	SearchMan.addSubDirectoryMatching(gameDataDir, "options", 0, 2);
}

CastleEngine::~CastleEngine() {
	closeAllPopups();
	delete _basePage;
	delete _ani;
	_screen.free();
	_aniBackground.free();
	delete _script;
	delete _quiz;
	delete _quest;
	delete _res;
	delete _db;
}

bool CastleEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
}

Common::Error CastleEngine::run() {
	initGraphics(640, 480);
	_screen.create(640, 480, Graphics::PixelFormat::createFormatCLUT8());

	_db = new Database();
	if (!_db->load(Common::Path("CASTLE.PNG")))
		return Common::kNoGameDataFoundError;
	_res = new Resources();
	_res->init();
	_script = new ScriptVM(this);
	_script->initDocScope(_db->getDocExtension());
	// Debug harness: castle_seed makes the random choices reproducible
	if (ConfMan.hasKey("castle_seed"))
		_rnd.setSeed(ConfMan.getInt("castle_seed"));
	_quest = new Quest(_rnd);
	_quiz = new Quiz(this, _db, _quest, _rnd);
	for (uint i = 0; i < ARRAYSIZE(_toggles); i++)
		_toggles[i] = true;
	const Common::Array<ToggleDesc> &toggles = _db->getTail().toggles;
	for (uint i = 0; i < toggles.size(); i++)
		setToggleState(toggles[i].code, toggles[i].state != 0);
	afterQuestLoad(true, false);

	uint start = _db->getStartPage();
	if (ConfMan.hasKey("boot_param"))
		start = ConfMan.getInt("boot_param");
	if (ConfMan.hasKey("castle_dump"))
		_dumpDir = ConfMan.get("castle_dump");
	// A game chosen in the launcher resumes on the page it was saved from
	if (ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot") >= 0) {
		if (loadGameState(ConfMan.getInt("save_slot")).getCode() == Common::kNoError && _savedPage)
			start = _savedPage;
	}
	// Debug harness: castle_spy presets the chosen spy
	if (ConfMan.hasKey("castle_spy"))
		setSpy(ConfMan.getInt("castle_spy"), false);
	openBasePage(start);

	setCursor(_db->getDefaultCursor());
	CursorMan.showMouse(true);

	// Debug harness: CASTLE_DUMP=<dir> writes a PNG of the screen after each
	// render, CASTLE_CLICKS="x,y;x,y;..." performs scripted clicks and quits.
	Common::String dumpDirStr = ConfMan.hasKey("castle_dump") ? ConfMan.get("castle_dump") : Common::String();
	const char *dumpDir = dumpDirStr.empty() ? nullptr : dumpDirStr.c_str();
	// Each entry is "x,y" for a click, "m:x,y" for a mouse move, "p:x,y"
	// for a button press, "r:x,y" for a release, "s:slot,0" saves and
	// "l:slot,0" loads a game, "t:code,0" types the character code
	// (13 = Enter, 8 = Backspace, 9 = Tab), "o:page,0" opens a popup page.
	Common::Array<Common::Point> clicks;
	Common::Array<char> clickKind;
	if (ConfMan.hasKey("castle_clicks")) {
		Common::StringTokenizer tok(ConfMan.get("castle_clicks"), ";");
		while (!tok.empty()) {
			Common::String t = tok.nextToken();
			char kind = 'c';
			if (t.size() > 2 && t[1] == ':') {
				kind = t[0];
				t = t.substr(2);
			}
			int x = 0, y = 0;
			sscanf(t.c_str(), "%d,%d", &x, &y);
			clicks.push_back(Common::Point(x, y));
			clickKind.push_back(kind);
		}
	}
	uint clickIdx = 0;
	// castle_clickdelay: milliseconds between scripted clicks (default 1500)
	uint32 clickDelay = ConfMan.hasKey("castle_clickdelay") ? ConfMan.getInt("castle_clickdelay") : 1500;
	uint32 nextClick = _system->getMillis() + clickDelay;
	_dumpDir = dumpDirStr;

	while (!shouldQuit()) {
		handleEvents();
		if (dumpDir && !clicks.empty() && _system->getMillis() >= nextClick) {
			if (clickIdx >= clicks.size()) {
				quitGame();
			} else {
				Common::Point pt = clicks[clickIdx];
				char kind = clickKind[clickIdx++];
				LivePage *page = nullptr;
				LiveObject *lo = (kind == 'c' || kind == 'p') ? hitTest(pt, &page) : nullptr;
				debugC(1, kDebugScript, "Castle: scripted %c %d,%d -> %s", kind, pt.x, pt.y, lo ? objectClassName(lo->obj->cls) : "nothing");
				if (kind == 'm') {
					if (_dragging || _dragPage || _scrollBarDrag || (_pressedObject && _pressedObject->obj->cls == kObjCollage))
						dragTo(pt);
					else
						handleMouseMove(pt);
				} else if (kind == 's') {
					saveGameState(pt.x, "harness", false);
				} else if (kind == 'l') {
					if (loadGameState(pt.x).getCode() == Common::kNoError)
						afterQuestLoad(true, true);
				} else if (kind == 'r') {
					releaseMouse(pt);
				} else if (kind == 'o') {
					openPopup(pt.x);
				} else if (kind == 't') {
					// Control codes map to their keys (13 Enter, 8 Backspace, 9 Tab);
					// letters carry their own code
					// codes from 256 up are key codes (273 Up, 274 Down, 280/281 Page Up/Down, 278 Home, 279 End)
					typeKey(pt.x < 256 ? pt.x : 0, pt.x == 13 ? Common::KEYCODE_RETURN : pt.x == 8 ? Common::KEYCODE_BACKSPACE : pt.x == 9 ? Common::KEYCODE_TAB : ((pt.x >= 'a' && pt.x <= 'z') || pt.x >= 256) ? pt.x : 0);
				} else if (lo) {
					pressObject(lo, page, pt);
					if (kind == 'c')
						releaseMouse(pt);
				}
				nextClick = _system->getMillis() + clickDelay;
			}
		}
		flushPendingPage();
		uint32 now = _system->getMillis();
		if (_basePage) {
			_basePage->update(now, *_res);
			if (_basePage->takeChanged())
				_dirty = true;
		}
		for (uint i = 0; i < _popups.size(); i++) {
			_popups[i]->update(now, *_res);
			if (_popups[i]->takeChanged())
				_dirty = true;
		}
		updateSprites(now);
		updateScrolling(now);
		updateAmbientSound(now, false);
		updateDungeonTimer(now);
		if (_quiz->idleExpired(now) && !_ani) {
			_quiz->clearIdle();
			_quiz->fireEvent(Quiz::kEvtTimeout, 0);
		}
		updateAnimation();
		updateWaveQueue();
		updateRepeat(now);
		render();
		_system->delayMillis(10);
	}
	return Common::kNoError;
}

void CastleEngine::handleEvents() {
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_LBUTTONDOWN: {
			LivePage *page = nullptr;
			LiveObject *lo = hitTest(event.mouse, &page);
			if (lo) {
				debugC(1, kDebugScript, "Castle: click on %s '%s' (id %d) at %d,%d", objectClassName(lo->obj->cls),
				       lo->obj->file.c_str(), lo->obj->id, event.mouse.x, event.mouse.y);
				pressObject(lo, page, event.mouse);
			} else if ((page = popupAt(event.mouse)) && page->getType() == kPageDragPopup) {
				// Drag popups are moved by their background
				_dragPage = page;
				_dragOffset = Common::Point(event.mouse.x - page->getBounds().left, event.mouse.y - page->getBounds().top);
			} else if (_ani) {
				// Clicking skips a running animation
				delete _ani;
				_ani = nullptr;
				_dirty = true;
			}
			break;
		}
		case Common::EVENT_LBUTTONUP:
			releaseMouse(event.mouse);
			break;
		case Common::EVENT_WHEELUP:
		case Common::EVENT_WHEELDOWN: {
			// The wheel scrolls the list or help text of the topmost page
			LivePage *top = _popups.empty() ? _basePage : _popups.back();
			int pos, maxPos, pageSize;
			if (top && top->getScrollState(pos, maxPos, pageSize)) {
				top->setScrollPos(pos + (event.type == Common::EVENT_WHEELUP ? -1 : 1));
				_dirty = true;
			}
			break;
		}
		case Common::EVENT_MOUSEMOVE:
			if (_dragging || _dragPage || _scrollBarDrag || (_pressedObject && _pressedObject->obj->cls == kObjCollage))
				dragTo(event.mouse);
			else
				handleMouseMove(event.mouse);
			break;
		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE && _ani) {
				delete _ani;
				_ani = nullptr;
				_dirty = true;
			} else {
				typeKey(event.kbd.ascii, event.kbd.keycode);
			}
			break;
		default:
			break;
		}
	}
}

LiveObject *CastleEngine::hitTest(const Common::Point &p, LivePage **pageOut) {
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		LiveObject *lo = _popups[i]->hitTest(p);
		if (lo) {
			*pageOut = _popups[i];
			return lo;
		}
		// Popups are modal over their bounds
		if (_popups[i]->getBounds().contains(p))
			return nullptr;
	}
	if (_basePage) {
		LiveObject *lo = _basePage->hitTest(p);
		if (lo) {
			*pageOut = _basePage;
			return lo;
		}
	}
	return nullptr;
}

void CastleEngine::applyPalette() {
	const Image *img = _basePage ? _basePage->getPaletteImage() : nullptr;
	for (uint i = 0; i < _popups.size() && !img; i++)
		img = _popups[i]->getPaletteImage();
	debugC(2, kDebugGraphics, "Castle: palette from image %dx%d with %u entries", img ? img->surface.w : 0, img ? img->surface.h : 0, img ? img->palette.size() : 0);
	if (img && img->palette.size() > 0) {
		_system->getPaletteManager()->setPalette(img->palette.data(), 0, MIN<uint>(256, img->palette.size()));
		byte pal[768];
		_system->getPaletteManager()->grabPalette(pal, 0, 256);
		_res->buildHighlightTable(pal);
	}
	_paletteDirty = false;
}

void CastleEngine::render() {
	if (_paletteDirty)
		applyPalette();
	if (!_dirty) {
		// The backend only repaints the mouse cursor from updateScreen(), so
		// it must be called every frame even when the page itself is unchanged.
		_system->updateScreen();
		return;
	}
	_screen.fillRect(Common::Rect(0, 0, _screen.w, _screen.h), 0);
	if (_basePage)
		_basePage->draw(_screen, *_res);
	for (uint i = 0; i < _popups.size(); i++)
		_popups[i]->draw(_screen, *_res);
	if (_ani && _aniBackground.getPixels()) {
		// Composite the animation's current state on top
		_screen.copyRectToSurface(_aniBackground, _aniPos.x, _aniPos.y, Common::Rect(0, 0, _aniBackground.w, _aniBackground.h));
	}
	_dirty = false;
	if (_noScreenUpdate)
		return;
	_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	_system->updateScreen();
	if (!_dumpDir.empty() && !_ani)
		dumpSurface(_screen);
}

void CastleEngine::dumpSurface(const Graphics::Surface &surf) {
	if (_dumpDir.empty())
		return;
	{
		Common::DumpFile f;
		Common::Path path(Common::String::format("%s/castle%03d.png", _dumpDir.c_str(), _dumpCount++), '/');
		debugC(1, kDebugGraphics, "Castle: dump %s", path.toString().c_str());
		if (f.open(path)) {
			byte pal[768];
			_system->getPaletteManager()->grabPalette(pal, 0, 256);
			::Image::writePNG(f, surf, pal, 256);
			f.close();
		}
	}
}

void CastleEngine::openBasePage(uint index) {
	updateTrailScroll();
	_hoverObject = nullptr;
	_scrollObject = nullptr;
	_pressedObject = nullptr;
	_dragging = false;
	_dragPage = nullptr;
	_hoverPage = nullptr;
	closeAllPopups();
	if (_basePage)
		pageClosing(_basePage);
	delete _basePage;
	_basePage = new LivePage();
	if (!_basePage->open(*_db, *_res, index, Common::Point(0, 0))) {
		delete _basePage;
		_basePage = nullptr;
		return;
	}
	applyQuestObjects(_basePage, true);
	setupCollages(_basePage);
	recordTrail(_basePage, false);
	_dirty = true;
	_paletteDirty = true;
	render();
	runPageEvents(_basePage, kEventOpen);
	if (!_pendingBase && _basePage)
		startRoomQuestions(_basePage);
}

void CastleEngine::runPageEvents(LivePage *page, int eventType) {
	PageRecord *rec = page->getRecord();
	if (!rec)
		return;
	for (uint i = 0; i < rec->events.size(); i++)
		if (rec->events[i]->type == eventType)
			runEvent(rec->events[i], page, nullptr);
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint p = 0; p < panels.size(); p++) {
		Panel *panel = panels[p]->panel;
		for (uint i = 0; i < panel->events.size(); i++)
			if (panel->events[i]->type == eventType)
				runEvent(panel->events[i], page, nullptr);
		for (uint k = 0; k < panels[p]->objects.size(); k++) {
			LiveObject &lo = panels[p]->objects[k];
			const Event *ev = lo.obj->findEvent(eventType);
			if (ev)
				runEvent(ev, page, &lo);
		}
		if (shouldQuit() || _pendingBase)
			return;
	}
}

void CastleEngine::openPopup(uint index) {
	for (uint i = 0; i < _popups.size(); i++)
		if (_popups[i]->getIndex() == index)
			return;
	LivePage *page = new LivePage();
	if (!page->open(*_db, *_res, index, Common::Point(0, 0))) {
		delete page;
		return;
	}
	_popups.push_back(page);
	debugC(2, kDebugGraphics, "Castle: popup %u at %d,%d %dx%d", index, page->getBounds().left, page->getBounds().top, page->getBounds().width(), page->getBounds().height());
	applyQuestObjects(page, true);
	setupCollages(page);
	recordTrail(page, true);
	_dirty = true;
	render();
	runPageEvents(page, kEventOpen);
}

void CastleEngine::closePopup(uint index) {
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		if (_popups[i]->getIndex() == index || index == 0xffffffff) {
			if (_hoverPage == _popups[i]) {
				_hoverObject = nullptr;
				_scrollObject = nullptr;
	_pressedObject = nullptr;
	_dragging = false;
	_dragPage = nullptr;
				_hoverPage = nullptr;
			}
			if (_pressedPage == _popups[i]) {
				_pressedObject = nullptr;
				_pressedPage = nullptr;
				_scrollBarDrag = false;
			}
			debugC(2, kDebugGraphics, "Castle: closing popup %u", _popups[i]->getIndex());
			pageClosing(_popups[i]);
			delete _popups[i];
			_popups.remove_at(i);
			_dirty = true;
			return;
		}
	}
}

void CastleEngine::closeAllPopups() {
	if (_hoverPage && _hoverPage != _basePage) {
		_hoverObject = nullptr;
		_scrollObject = nullptr;
	_pressedObject = nullptr;
	_dragging = false;
	_dragPage = nullptr;
		_hoverPage = nullptr;
	}
	_pressedObject = nullptr;
	_pressedPage = nullptr;
	_scrollBarDrag = false;
	if (!_popups.empty())
		debugC(2, kDebugGraphics, "Castle: closing all %u popups", _popups.size());
	for (uint i = 0; i < _popups.size(); i++) {
		pageClosing(_popups[i]);
		delete _popups[i];
	}
	_popups.clear();
	_dirty = true;
}

void CastleEngine::runEvent(const Event *ev, LivePage *page, LiveObject *obj) {
	runActions(ev->actions, page, obj);
}

void CastleEngine::runActions(const Common::Array<Action *> &actions, LivePage *page, LiveObject *obj) {
	for (uint i = 0; i < actions.size(); i++) {
		runAction(actions[i], page, obj);
		if (shouldQuit() || _pendingBase)
			return;
		// An action may have closed the page (CLOSEPAGE followed by Quit...)
		if (page) {
			bool alive = page == _basePage;
			for (uint k = 0; k < _popups.size() && !alive; k++)
				if (_popups[k] == page)
					alive = true;
			if (!alive) {
				page = nullptr;
				obj = nullptr;
			}
		}
	}
}

void CastleEngine::runAction(const Action *a, LivePage *page, LiveObject *obj) {
	debugC(2, kDebugScript, "Castle: action %s page=%u x=%d name='%s'", actionName(a->type), a->page, a->x, a->name.c_str());
	switch (a->type) {
	case kActChangePage: {
		PageRecord *target = _db->getRecord(a->page);
		if (!target)
			return;
		if (target->isPage && (target->type == kPagePopup || target->type == kPageDragPopup || target->type == kPageRolloffClose)) {
			// Turning the page of a book wipes the new page over the old one
			bool wipe = (a->x == 4 || a->x == 5) && toggleState(18) && !_noScreenUpdate;
			Graphics::Surface old;
			if (wipe) {
				old.copyFrom(_screen);
				_noScreenUpdate = true;
			}
			if (page && page->isPopup())
				closePopup(page->getIndex());
			openPopup(a->page);
			if (wipe) {
				render();
				_noScreenUpdate = false;
				wipeTransition(old, a->x);
				old.free();
				_dirty = true;
			}
		} else {
			_pendingBasePage = a->page;
			_pendingBase = true;
			_pendingTransition = a->x;
		}
		break;
	}
	case kActOpenPage:
	case kActAppendPage:
	case kActOpenHelp:
		openPopup(a->page);
		break;
	case kActZoom:
		_pendingBasePage = a->page;
		_pendingBase = true;
		_pendingScroll = a->pt;
		_pendingTransition = 0;
		break;
	case kActScrollListBox:
		// p[0]: 0 scrolls the page's list down a line, otherwise up
		scrollCollage(page, a->p[0] ? -1 : 1);
		break;
	case kActClosePage:
		if (page && page->isPopup())
			closePopup(page->getIndex());
		else
			closePopup(a->page);
		break;
	case kActBack:
		closePopup(0xffffffff);
		break;
	case kActQuit: {
		// A game in progress gets a save prompt first (the "save before
		// quitting" page, or the quit confirmation while a quest is run)
		const DocumentTail &t = _db->getTail();
		bool prompt = page && (page->getIndex() == t.quitPages[0] || page->getIndex() == t.quitPages[2]);
		if (!prompt && _quest->getMode() != 0 && _quest->isDirty()) {
			openPopup(_quest->getMode() != 2 ? t.quitPages[0] : t.quitPages[2]);
			break;
		}
		quitGame();
		break;
	}
	case kActPlayWave:
		playWave(page ? page->getDir() : Common::String(), a->name, false);
		break;
	case kActNewPlayWave:
	case kActPlayWaveChannel:
		playWaveChannel(page ? page->getDir() : Common::String(), a->name, a->p[1]);
		break;
	case kActStopWave:
		stopWave();
		break;
	case kActStopWaveChannel:
		stopWaveChannel(a->x, a->name);
		break;
	case kActChangeSpyType:
		setSpy(a->x, true);
		break;
	case kActOptions:
		runOptionsAction(a->x, page, obj);
		break;
	case kActActivityCompleted:
		_activityCompleted = true;
		break;
	case kActBribeGuard:
		_quest->bribe(3);
		break;
	case kActPlayResponse: {
		// Three wave/animation pairs, one per spy character
		int spy = _quest->getSpy();
		int idx = spy == 1 ? 0 : spy == 2 ? 1 : 2;
		if (a->strs.size() < 6)
			break;
		Common::String dir = page ? page->getDir() : Common::String();
		if (!a->strs[idx].empty())
			playWaveChannel(dir, a->strs[idx], -1);
		if (!a->strs[3 + idx].empty()) {
			Common::Point origin(0, 0);
			if (obj)
				origin = Common::Point(obj->panel->rect.left, obj->panel->rect.top);
			else if (page && !page->getPanels().empty())
				origin = Common::Point(page->getPanels()[0]->rect.left, page->getPanels()[0]->rect.top);
			playAnimation(dir, a->strs[3 + idx], origin);
		}
		break;
	}
	case kActPlayPics:
	case kActPlayPicsEx:
		playAnimation(page ? page->getDir() : Common::String(), a->name, a->pt + (obj ? Common::Point(obj->panel->rect.left, obj->panel->rect.top) : Common::Point(0, 0)));
		break;
	case kActPlayVideoInd:
	case kActTransitionVideo:
	case kActPlayVideo: {
		Common::Rect dest = a->rect;
		Common::Point origin(0, 0);
		if (obj)
			origin = Common::Point(obj->panel->rect.left, obj->panel->rect.top);
		else if (page && !page->getPanels().empty())
			origin = Common::Point(page->getPanels()[0]->rect.left, page->getPanels()[0]->rect.top);
		dest.translate(origin.x, origin.y);
		playVideo(page ? page->getDir() : Common::String(), a->name, dest);
		break;
	}
	case kActStopPics:
		delete _ani;
		_ani = nullptr;
		_dirty = true;
		break;
	case kActCommand:
		runCommand(a, page, obj);
		break;
	case kActPaintBitmap: {
		LivePanel *lp = obj ? obj->panel : (page && !page->getPanels().empty() ? page->getPanels()[0] : nullptr);
		if (lp) {
			Overlay ov;
			ov.image = _res->loadImage(page ? page->getDir() : Common::String(), a->name);
			ov.pos = Common::Point(lp->rect.left + a->pt.x, lp->rect.top + a->pt.y);
			if (ov.image)
				lp->overlays.push_back(ov);
			_dirty = true;
		}
		break;
	}
	case kActClearBitmap: {
		LivePanel *lp = obj ? obj->panel : (page && !page->getPanels().empty() ? page->getPanels()[0] : nullptr);
		if (lp) {
			lp->overlays.clear();
			_dirty = true;
		}
		break;
	}
	case kActGeneralPurpose:
		switch (a->x) {
		case 0:
		case 6:
			chestFlash();
			break;
		case 1:
			if (toggleState(0x12))
				playWave(Common::String(), "@rot1s", true);
			break;
		case 2:
		case 0x10:
			stopWave();
			break;
		case 3: {
			// Toggles the chest's lid artwork (objects 0 and 1 of the chest panel)
			LivePage *cp = nullptr;
			LivePanel *chest = findSpyChest(&cp);
			if (!chest)
				break;
			LiveObject *a0 = chest->panel->spy.size() > 1 ? cp->findObject(chest->panel->spy[0]) : nullptr;
			LiveObject *a1 = chest->panel->spy.size() > 1 ? cp->findObject(chest->panel->spy[1]) : nullptr;
			if (a0 && a1) {
				if (a0->visible) {
					a0->visible = false;
					a1->zOrder = -30000;
				} else if (_quest->getStage() != 2) {
					a0->visible = true;
					a1->zOrder = 5;
				}
				_dirty = true;
			}
			break;
		}
		case 4: {
			// Reveals the chest's result artwork once every task is done
			LivePage *cp = nullptr;
			LivePanel *chest = findSpyChest(&cp);
			if (!chest || chest->panel->spy.size() < 9)
				break;
			if (_quest->countTasksDone() == 4) {
				LiveObject *a7 = cp->findObject(chest->panel->spy[7]);
				LiveObject *a8 = cp->findObject(chest->panel->spy[8]);
				if (a7 && a8) {
					a7->visible = true;
					a8->zOrder = 5;
				}
			} else {
				LiveObject *a6 = cp->findObject(chest->panel->spy[6]);
				if (a6)
					a6->visible = true;
			}
			_dirty = true;
			break;
		}
		case 7:
			_spyChangedFlag = true;
			break;
		case 0xc:
			playWave(Common::String(), "@con01", false);
			break;
		case 5: {
			// Hide the dungeon's disabling hotspots
			Common::Array<LivePanel *> panels = page ? page->getPanels() : Common::Array<LivePanel *>();
			for (uint i = 0; i < panels.size(); i++)
				for (uint k = 0; k < panels[i]->objects.size(); k++)
					if (panels[i]->objects[k].obj->cls == kObjDungeonDisableHotspot) {
						panels[i]->objects[k].zOrder = -30000;
						panels[i]->objects[k].visible = false;
					}
			break;
		}
		case 8: _quest->setMode(0); break;
		case 9: _quest->setMode(1); break;
		case 10: _quest->setMode(3); break;
		case 0xb: _quest->setMode(2); break;
		default:
			debugC(1, kDebugScript, "Castle: GeneralPurposeAction %d not implemented", a->x);
			break;
		}
		break;
	case kActChangeColourRefBitmap: {
		LiveObject *hl = findHighlightObject(page);
		if (hl) {
			hl->image = _res->loadImage(page ? page->getDir() : Common::String(), a->name);
			_dirty = true;
		}
		break;
	}
	case kActHighlightCastleSection: {
		LiveObject *hl = findHighlightObject(page);
		if (hl) {
			hl->value = a->x;
			hl->visible = a->x > 0;
			_dirty = true;
		}
		break;
	}
	case kActSetSpriteFrame: {
		LiveObject *lo = findLiveObject(a->p[0], page);
		if (lo)
			setSpriteFrame(lo, a->p[1]);
		break;
	}
	case kActUpdateNodeHtsp:
		updateNodeHotspots(page, a->x);
		break;
	case kActStartDungeonTimer:
		_dungeonTimerEnd = _system->getMillis() + MAX<uint32>(1, a->page);
		break;
	case kActStopDungeonTimer:
		_dungeonTimerEnd = 0;
		break;
	case kActPaintHelpText:
	case kActClearHelpText: {
		// Help pages paint the explanation bitmap into a fixed box of the panel
		LivePanel *lp = obj ? obj->panel : (page && !page->getPanels().empty() ? page->getPanels()[0] : nullptr);
		if (!lp)
			break;
		lp->overlays.clear();
		if (a->type == kActPaintHelpText) {
			Overlay ov;
			ov.image = _res->loadImage(page ? page->getDir() : Common::String(), a->name);
			ov.pos = Common::Point(lp->rect.left + 61, lp->rect.top + 281);
			if (ov.image)
				lp->overlays.push_back(ov);
		}
		_dirty = true;
		break;
	}
	case kActSendMessage: {
		// Ambient animations answer their enable and disable message numbers
		LiveObject *lo = findLiveObject(a->p[0], page);
		if (lo && lo->obj->cls == kObjAmbientAnimation && lo->obj->ints.size() > 2) {
			if (a->p[1] == lo->obj->ints[1])
				lo->disabled = false;
			else if (a->p[1] == lo->obj->ints[2])
				lo->disabled = true;
			_dirty = true;
		}
		break;
	}
	case kActPaintZoomArea:
	case kActPaintZoomObject:
	case kActClearZoomObject: {
		LiveObject *cap = findZoomCaption(a->x, page);
		if (!cap)
			break;
		Image *img = a->type == kActClearZoomObject || a->name.empty() ? nullptr : _res->loadImage(page ? page->getDir() : Common::String(), a->name);
		if (a->type == kActPaintZoomArea) {
			cap->overlayA = img;
			if (!img)
				cap->overlayB = nullptr;
		} else {
			cap->overlayB = img;
		}
		_dirty = true;
		break;
	}
	case kActDoTransition:
		doTransition(page, a->p[0], a->p[1], a->p[2]);
		break;
	default:
		debugC(1, kDebugScript, "Castle: unimplemented action %s", actionName(a->type));
		break;
	}
}

// UpdateNodeHtsp: a 3D room page keeps one panoramic sprite whose frames are
// the views from the room's nodes. Every node-bound object carries the node
// it belongs to; this enables the ones for the given node and disables the
// rest (-1 while turning disables everything).
void CastleEngine::updateNodeHotspots(LivePage *page, int node) {
	if (!page)
		page = _basePage;
	if (!page)
		return;
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++) {
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			LiveObject &lo = panels[i]->objects[k];
			const GameObject *obj = lo.obj;
			switch (obj->cls) {
			case kObjNodalHotspot:
			case kObjPlayResponseHotspot:
				// ints: highlight, h2, node
				lo.disabled = obj->ints.size() < 3 || obj->ints[2] != node;
				break;
			case kObjAmbientAnimation:
				lo.disabled = obj->ints.empty() || obj->ints[0] != node;
				break;
			case kObjRandomScenarioHotspot:
				lo.value = node;
				break;
			default:
				break;
			}
		}
	}
	_dirty = true;
}

// DoTransition: plays the walk animation sprite that leads to the next node;
// its last frame script then changes to the destination page.
void CastleEngine::doTransition(LivePage *page, int mode, int spriteId, int async) {
	LiveObject *lo = findLiveObject(spriteId, page);
	if (!lo || lo->obj->cls != kObjSprite)
		return;
	lo->zOrder = 10;
	lo->visible = true;
	if (mode)
		lo->spriteState |= 0x40;
	else
		lo->spriteState &= ~0x40;
	if (async) {
		lo->spriteFlags |= 8;
		lo->playing = lo->frameDelay > 0;
		lo->nextFrameTime = 0;
		_dirty = true;
		return;
	}
	// Synchronous variant: the original steps the frames itself, 200 ms apart
	setSpriteFrame(lo, 1);
	for (int f = 2; f <= lo->frameCount && !shouldQuit(); f++) {
		uint32 until = _system->getMillis() + 200;
		while (_system->getMillis() < until && !shouldQuit()) {
			handleEvents();
			render();
			_system->delayMillis(10);
		}
		setSpriteFrame(lo, f);
		runSpriteFrameScripts(page, lo, 9, f);
		if (scriptShouldStop())
			return;
	}
}

// The zoom caption object (by id, else the first one on the base page)
LiveObject *CastleEngine::findZoomCaption(int id, LivePage *page) {
	LiveObject *lo = findLiveObject(id, page);
	if (lo && lo->obj->cls == kObjZoomCaption)
		return lo;
	Common::Array<LivePage *> pages;
	if (page)
		pages.push_back(page);
	if (_basePage && _basePage != page)
		pages.push_back(_basePage);
	for (uint p = 0; p < pages.size(); p++) {
		const Common::Array<LivePanel *> &panels = pages[p]->getPanels();
		for (uint i = 0; i < panels.size(); i++)
			for (uint k = 0; k < panels[i]->objects.size(); k++)
				if (panels[i]->objects[k].obj->cls == kObjZoomCaption)
					return &panels[i]->objects[k];
	}
	return nullptr;
}

// Mouse button down on an object. Hotspots run their on_click event right
// away (as the original does); sprites run their press scripts (event 4)
// and, when draggable (flag 0x10), start following the mouse.
void CastleEngine::pressObject(LiveObject *lo, LivePage *page, const Common::Point &p) {
	_pressedObject = lo;
	_pressedPage = page;
	_dragging = false;
	if (lo->obj->cls == kObjSprite) {
		if (lo->spriteFlags & 0x10) {
			_dragging = true;
			_dragOffset = Common::Point(p.x - lo->rect.left, p.y - lo->rect.top);
			lo->spriteState |= 0x20;
		}
		runSpriteFrameScripts(page, lo, 4, lo->frame);
		return;
	}
	if (lo->obj->cls == kObjScrollObject || lo->obj->cls == kObjWrapScrollObject) {
		_scrollStep = 8;
		return;
	}
	if (lo->obj->cls == kObjCollage) {
		collagePress(lo, page, p);
		return;
	}
	if (lo->obj->cls == kObjEditBox || lo->obj->cls == kObjRoomEditBox || lo->obj->cls == kObjScrollEditBox) {
		focusEditBox(lo, page);
		return;
	}
	if (lo->obj->cls == kObjScrollBar) {
		scrollBarPress(lo, page, p);
		return;
	}
	if (lo->obj->cls == kObjRepeatingHotspot) {
		// Clicks again on a timer while the button is held (ints[2] ms)
		int interval = lo->obj->ints.size() > 2 ? lo->obj->ints[2] : 0;
		_repeatNext = _system->getMillis() + MAX(interval, 100);
	}
	clickObject(lo, page);
}

void CastleEngine::updateRepeat(uint32 now) {
	if (!_pressedObject || _pressedObject->obj->cls != kObjRepeatingHotspot || now < _repeatNext)
		return;
	int interval = _pressedObject->obj->ints.size() > 2 ? _pressedObject->obj->ints[2] : 0;
	_repeatNext = now + MAX(interval, 100);
	clickObject(_pressedObject, _pressedPage);
}

// Mouse button up: ends a drag and runs the sprite's release scripts (5)
void CastleEngine::releaseMouse(const Common::Point &p) {
	LiveObject *lo = _pressedObject;
	LivePage *page = _pressedPage;
	_pressedObject = nullptr;
	_pressedPage = nullptr;
	_dragPage = nullptr;
	if (!lo)
		return;
	if (_dragging) {
		_dragging = false;
		lo->spriteState &= ~0x20;
	}
	if (lo->obj->cls == kObjSprite)
		runSpriteFrameScripts(page, lo, 5, lo->frame);
	else if (lo->obj->cls == kObjCollage)
		collageRelease(lo, page, p);
	else if (lo->obj->cls == kObjScrollBar) {
		lo->value = 0;
		_scrollBarDrag = false;
		_dirty = true;
	}
}

// Moves a dragged sprite with the mouse, kept inside its limit rectangle
// when one is stored, then runs its drag scripts (6)
void CastleEngine::dragTo(const Common::Point &p) {
	if (_dragPage) {
		int dx = p.x - _dragOffset.x - _dragPage->getBounds().left;
		int dy = p.y - _dragOffset.y - _dragPage->getBounds().top;
		if (dx || dy) {
			_dragPage->moveBy(dx, dy);
			_dirty = true;
		}
		return;
	}
	LiveObject *lo = _pressedObject;
	if (lo && lo->obj->cls == kObjScrollBar) {
		if (_scrollBarDrag)
			scrollBarDrag(lo, _pressedPage, p);
		return;
	}
	if (lo && lo->obj->cls == kObjCollage) {
		// Dragging over the list moves the selection with the mouse
		Collage *c = lo->collage;
		int item = c ? c->itemAt(lo->rect, p) : -1;
		if (item >= 0 && c->select(item, 1))
			collageSelected(_pressedPage, lo);
		return;
	}
	if (!lo || !_dragging)
		return;
	int w = lo->rect.width(), h = lo->rect.height();
	int nx = p.x - _dragOffset.x, ny = p.y - _dragOffset.y;
	if (!lo->obj->rects.empty() && !lo->obj->rects[0].isEmpty()) {
		Common::Rect lim = lo->obj->rects[0];
		lim.translate(lo->panel->rect.left, lo->panel->rect.top);
		nx = CLIP<int>(nx, lim.left, MAX<int>(lim.left, lim.right - w));
		ny = CLIP<int>(ny, lim.top, MAX<int>(lim.top, lim.bottom - h));
	}
	if (nx == lo->rect.left && ny == lo->rect.top)
		return;
	lo->rect.moveTo(nx, ny);
	debugC(3, kDebugScript, "Castle: drag sprite %d to %d,%d", lo->obj->id, nx, ny);
	_dirty = true;
	runSpriteFrameScripts(_pressedPage, lo, 6, lo->frame);
}

// The topmost popup under a point
LivePage *CastleEngine::popupAt(const Common::Point &p) {
	for (int i = (int)_popups.size() - 1; i >= 0; i--)
		if (_popups[i]->getBounds().contains(p))
			return _popups[i];
	return nullptr;
}

// The dungeon's countdown: when it runs out the page's DungeonTimer object
// gets its timer-end event
void CastleEngine::updateDungeonTimer(uint32 now) {
	if (!_dungeonTimerEnd || now < _dungeonTimerEnd)
		return;
	_dungeonTimerEnd = 0;
	Common::Array<LivePage *> pages;
	for (int i = (int)_popups.size() - 1; i >= 0; i--)
		pages.push_back(_popups[i]);
	if (_basePage)
		pages.push_back(_basePage);
	for (uint p = 0; p < pages.size(); p++) {
		const Common::Array<LivePanel *> &panels = pages[p]->getPanels();
		for (uint i = 0; i < panels.size(); i++)
			for (uint k = 0; k < panels[i]->objects.size(); k++) {
				LiveObject &lo = panels[i]->objects[k];
				if (lo.obj->cls != kObjDungeonTimer)
					continue;
				const Event *ev = lo.obj->findEvent(kEventTimerEnd);
				if (ev)
					runEvent(ev, pages[p], &lo);
				return;
			}
	}
}

// A click on an object: its on_click event or the built-in behaviour of a
// page-turn corner (sound and a page change
// with the curl transition towards the stored page).
void CastleEngine::clickObject(LiveObject *lo, LivePage *page) {
	const GameObject *o = lo->obj;
	switch (o->cls) {
	case kObjCoinBitmap:
		// Picking up a coin, as long as the purse is not full
		if (lo->visible && _quest->countCoins() < Quest::kMaxCoins && o->ints.size() > 1) {
			playWaveChannel(Common::String(), "@coin1s", -1);
			lo->visible = false;
			lo->zOrder = -30000;
			_quest->collectCoin(o->ints[1], o->ints[0]);
			chestFlash();
			_dirty = true;
		}
		return;
	case kObjCollectBitmap: {
		static const char *const sounds[] = { "@fish1s", "@herb1s", "@hamm1s", "@cand1s" };
		int item = o->ints.empty() ? -1 : o->ints[0];
		lo->visible = false;
		lo->zOrder = -30000;
		_quest->collectItem(item);
		if (item >= 0 && item < 4)
			playWaveChannel(Common::String(), sounds[item], -1);
		chestFlash();
		_dirty = true;
		return;
	}
	case kObjRandomScenarioHotspot: {
		// Clicking the spy makes it ask again, up to three times
		const Event *ev = o->findEvent(kEventClick);
		if (ev)
			runEvent(ev, page, lo);
		if (o->ints.size() > 3 && o->ints[2] == 0) {
			int count = ++lo->value;
			if (count < 4) {
				_quiz->setIdle(_system->getMillis());
				_quiz->fireEvent(Quiz::kEvtAsk, count);
			}
		}
		return;
	}
	case kObjRandomMapBitmap: {
		// The map item of the current task: the first clicks play the spy's
		// hints, then (or at once after the zoom page was seen) the task is done
		int count = ++lo->value;
		int task = o->ints.size() > 1 ? o->ints[0] : -1;
		int choice = o->ints.size() > 1 ? o->ints[1] : -1;
		bool done = _quiz->getFlag();
		if (!done) {
			LivePage *before = _basePage;
			bool fired = _quiz->fireEvent(Quiz::kEvtHint, count);
			if (_basePage != before)
				return;
			if (!fired && _quiz->getQuestion() == 1 && count == 1)
				done = true;
		}
		if (done) {
			lo->visible = false;
			_quest->completeTask(task, choice);
			playWaveChannel(Common::String(), "@chest1s", -1);
			playWaveChannel(Common::String(), "@pape1s", -2);
			chestFlash();
			_dirty = true;
		}
		return;
	}
	case kObjQuestionOKButton:
		answerClicked(lo, page);
		return;
	case kObjScrollQuestAnswerHtsp:
		// The OK of a chest scroll: misspelt answers keep the scroll open
		checkScrollAnswers(lo, page);
		if (lo->value != 1)
			return;
		break;
	case kObjCollageButton: {
		// The OK button of the Index: goes to the selected entry
		LiveObject *list = page->findCollage();
		if (list)
			activateCollageItem(page, list);
		return;
	}
	case kObjPageTurn:
		// Seeing the zoom page lets the map item complete its task directly
		_quiz->setFlag(true);
		break;
	case kObjToggleButton: {
		int code = toggleCodeOf(o->id);
		lo->value = lo->value ? 0 : 1;
		if (code >= 0)
			setToggleState(code, lo->value != 0);
		_dirty = true;
		break;
	}
	default:
		break;
	}
	const Event *ev = lo->obj->findEvent(kEventClick);
	if (ev) {
		runEvent(ev, page, lo);
		return;
	}
	if (lo->obj->cls == kObjPageTurn && !lo->obj->u32s.empty()) {
		playWave(Common::String(), "@page1s", false);
		Action a;
		a.type = kActChangePage;
		a.page = lo->obj->u32s[0];
		a.x = (lo->obj->ints.size() > 2 && lo->obj->ints[2] == 1) ? 5 : 4;
		runAction(&a, page, lo);
	}
}

// Scroll-edge objects of the zoom pages scroll their panel while the mouse
// rests on them, speeding up from 1 to 8 pixels per tick.
void CastleEngine::updateScrolling(uint32 now) {
	if (!_scrollObject || !_scrollPage || now < _scrollNext)
		return;
	_scrollNext = now + 40;
	int dir = _scrollObject->obj->ints.empty() ? -1 : _scrollObject->obj->ints[0];
	int dx = 0, dy = 0, st = _scrollStep;
	switch (dir) {
	case 0: dy = -st; break;
	case 1: dy = st; break;
	case 2: dx = -st; break;
	case 3: dx = st; break;
	case 4: dx = -st; dy = -st; break;
	case 5: dx = st; dy = -st; break;
	case 6: dx = -st; dy = st; break;
	case 7: dx = st; dy = st; break;
	default: return;
	}
	if (_scrollPage->scrollBy(dx, dy)) {
		_dirty = true;
		updateAmbientSound(now, true);
	}
	if (_scrollStep < 8)
		_scrollStep *= 2;
}

// Zoom pages carry a ZoomAmbientSoundObj: every few seconds (and whenever
// the view scrolls) it loops the wave of the SoundHotspot under the centre
// of the view, switching when a different region comes into view.
void CastleEngine::updateAmbientSound(uint32 now, bool force) {
	if (!force && now < _ambientNext)
		return;
	_ambientNext = now + 4000;
	Common::String name;
	LivePanel *zoomPanel = nullptr;
	if (_basePage) {
		const Common::Array<LivePanel *> &panels = _basePage->getPanels();
		for (uint i = 0; i < panels.size() && !zoomPanel; i++)
			for (uint k = 0; k < panels[i]->objects.size(); k++)
				if (panels[i]->objects[k].obj->cls == kObjZoomAmbientSoundObj) {
					zoomPanel = panels[i];
					break;
				}
	}
	if (zoomPanel) {
		Common::Point centre(zoomPanel->rect.left + zoomPanel->rect.width() / 2 + zoomPanel->scroll.x,
		                     zoomPanel->rect.top + zoomPanel->rect.height() / 2 + zoomPanel->scroll.y);
		for (int k = (int)zoomPanel->objects.size() - 1; k >= 0; k--) {
			const LiveObject &lo = zoomPanel->objects[k];
			if (lo.obj->cls == kObjSoundHotspot && !lo.obj->strs.empty() && lo.rect.contains(centre)) {
				name = lo.obj->strs[0];
				break;
			}
		}
	}
	if (name.equalsIgnoreCase(_ambientName))
		return;
	if (!_ambientName.empty())
		stopWaveChannel(-2, _ambientName);
	_ambientName = name;
	if (name.empty())
		return;
	debugC(1, kDebugSound, "Castle: ambient sound '%s'", name.c_str());
	playWaveChannel(_basePage->getDir(), name, -2, true);
}

LiveObject *CastleEngine::findHighlightObject(LivePage *page) {
	if (!page)
		page = _basePage;
	if (!page)
		return nullptr;
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++)
		for (uint k = 0; k < panels[i]->objects.size(); k++)
			if (panels[i]->objects[k].obj->cls == kObjHighlightingCastle)
				return &panels[i]->objects[k];
	return nullptr;
}

int CastleEngine::getBuiltinNumber(int id) const {
	return _db->getBuiltinNumber(id);
}

LiveObject *CastleEngine::findLiveObject(int id, LivePage *page) {
	LiveObject *lo = page ? page->findObject(id) : nullptr;
	if (lo)
		return lo;
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		lo = _popups[i]->findObject(id);
		if (lo)
			return lo;
	}
	if (_basePage)
		return _basePage->findObject(id);
	return nullptr;
}

bool CastleEngine::scriptShouldStop() const {
	return shouldQuit() || _pendingBase;
}

void CastleEngine::runScriptAction(const Action *a, Context &ctx) {
	runAction(a, ctx.page, ctx.object);
}

void CastleEngine::runCommand(const Action *a, LivePage *page, LiveObject *obj) {
	if (!a->script)
		return;
	Context ctx;
	ctx.page = page;
	ctx.object = obj;
	ctx.panel = obj ? obj->panel : (page && !page->getPanels().empty() ? page->getPanels()[0] : nullptr);
	Scope local;
	local.init(a->script->ext);
	ctx.scopes.push_back(&local);
	if (ctx.panel)
		ctx.scopes.push_back(&ctx.panel->scope);
	if (page)
		ctx.scopes.push_back(&page->getScope());
	for (int i = (int)_popups.size() - 1; i >= 0; i--)
		if (_popups[i] != page)
			ctx.scopes.push_back(&_popups[i]->getScope());
	if (_basePage && _basePage != page)
		ctx.scopes.push_back(&_basePage->getScope());
	_script->runScript(a->script, ctx);
}

void CastleEngine::setSpriteFrame(LiveObject *lo, int frame) {
	if (lo->obj->cls != kObjSprite && lo->obj->cls != kObjAmbientAnimation)
		return;
	if (frame < 1)
		frame = 1;
	if (lo->frameCount > 0 && frame > lo->frameCount)
		frame = lo->frameCount;
	lo->frame = frame;
	debugC(3, kDebugScript, "Castle: sprite %d frame -> %d", lo->obj->id, frame);
	if (!lo->obj->file.empty()) {
		Common::String name = lo->obj->cls == kObjSprite ? Common::String::format("%s%04d", lo->obj->file.c_str(), frame) : lo->obj->strs[MIN<uint>(frame - 1, lo->obj->strs.size() - 1)];
		Image *img = _res->loadImage(lo->panel->dir, name);
		if (img)
			lo->image = img;
	}
	_dirty = true;
}

// Runs the sprite scripts registered for a sprite event:
//   5 click, 9 frame reached (script->b is the 1-based frame),
//   0xd last frame reached, 0x10 loop restarted, 10 timer
// Jumping to a frame from a script also fires that frame's scripts
void CastleEngine::spriteGotoFrame(LiveObject *lo, int frame) {
	setSpriteFrame(lo, frame);
	LivePage *page = lo->panel ? lo->panel->page : nullptr;
	if (!page)
		return;
	runSpriteFrameScripts(page, lo, 9, lo->frame);
	if (lo->frame == lo->frameCount)
		runSpriteFrameScripts(page, lo, 0xd, lo->frame);
}

void CastleEngine::runSpriteFrameScripts(LivePage *page, LiveObject *lo, int event, int frame) {
	const GameObject *obj = lo->obj;
	for (uint i = 0; i < obj->scripts.size(); i++) {
		if (obj->scripts[i]->a != event)
			continue;
		if (event == 9 && obj->scripts[i]->b != frame)
			continue;
		Context ctx;
		ctx.page = page;
		ctx.object = lo;
		ctx.panel = lo->panel;
		Scope local;
		local.init(obj->scripts[i]->ext);
		ctx.scopes.push_back(&local);
		ctx.scopes.push_back(&lo->panel->scope);
		ctx.scopes.push_back(&page->getScope());
		if (_basePage && _basePage != page)
			ctx.scopes.push_back(&_basePage->getScope());
		debugC(2, kDebugScript, "Castle: sprite %d event %d frame %d script", obj->id, event, frame);
		_script->runScript(obj->scripts[i], ctx);
		if (scriptShouldStop())
			return;
	}
}

void CastleEngine::updateSprites(uint32 now) {
	Common::Array<LivePage *> pages;
	if (_basePage)
		pages.push_back(_basePage);
	for (uint i = 0; i < _popups.size(); i++)
		pages.push_back(_popups[i]);
	for (uint p = 0; p < pages.size(); p++) {
		LivePage *page = pages[p];
		const Common::Array<LivePanel *> &panels = page->getPanels();
		for (uint i = 0; i < panels.size(); i++) {
			for (uint k = 0; k < panels[i]->objects.size(); k++) {
				LiveObject &lo = panels[i]->objects[k];
				if (lo.obj->cls != kObjSprite || lo.frameCount <= 0)
					continue;
				// Only active (1), loaded (2) and running (4) sprites animate
				if (!lo.playing || (lo.spriteState & 7) != 7 || now < lo.nextFrameTime)
					continue;
				lo.nextFrameTime = now + MAX(25, lo.frameDelay);
				if (!advanceSprite(page, &lo))
					return;
			}
		}
	}
}

// One step of the sprite state machine (FUN_0040c370 in the original): the
// state bit 0x40 selects the direction and the loop mode decides what happens
// at either end: 1 loops, 2 bounces, 3 stops on the last frame, anything else
// ends the animation (running bit cleared, bit 8 set).
// Returns false when a script changed the page.
bool CastleEngine::advanceSprite(LivePage *page, LiveObject *lo) {
	const GameObject *obj = lo->obj;
	int mode = obj->ints.size() > 7 ? obj->ints[7] : 1;
	bool forward = (lo->spriteState & 0x40) != 0;
	int next = lo->frame;
	bool atEnd = forward ? lo->frame >= lo->frameCount : lo->frame <= 1;
	if (atEnd) {
		switch (mode) {
		case 1:
			if (!spriteLoopDone(page, lo))
				return !scriptShouldStop();
			next = forward ? 1 : lo->frameCount;
			break;
		case 2:
			lo->spriteState ^= 0x40;
			spriteLoopDone(page, lo);
			return !scriptShouldStop();
		case 3:
			lo->spriteFlags &= ~8;
			lo->playing = false;
			return true;
		default:
			spriteFinished(page, lo);
			return !scriptShouldStop();
		}
	} else {
		next += forward ? 1 : -1;
	}
	setSpriteFrame(lo, next);
	runSpriteFrameScripts(page, lo, 9, next);
	if (!scriptShouldStop() && next == lo->frameCount)
		runSpriteFrameScripts(page, lo, 0xd, next);
	return !scriptShouldStop();
}

// Counts down the remaining loops; false when the animation is over.
bool CastleEngine::spriteLoopDone(LivePage *page, LiveObject *lo) {
	if (lo->spriteLoops > 0)
		lo->spriteLoops--;
	if (lo->spriteLoops == 0) {
		spriteFinished(page, lo);
		return false;
	}
	runSpriteFrameScripts(page, lo, 0x10, lo->frame);
	return true;
}

void CastleEngine::spriteFinished(LivePage *page, LiveObject *lo) {
	lo->playing = false;
	lo->spriteState = (lo->spriteState & ~4) | 8;
}

void CastleEngine::setCursor(const Common::String &name) {
	if (name == _cursorName)
		return;
	Graphics::Cursor *cursor = _res->getCursor(name);
	if (!cursor)
		return;
	CursorMan.replaceCursor(cursor);
	_cursorName = name;
}

void CastleEngine::handleMouseMove(const Common::Point &p) {
	LivePage *page = nullptr;
	LiveObject *lo = nullptr;
	for (int i = (int)_popups.size() - 1; i >= 0 && !lo; i--) {
		lo = _popups[i]->objectAt(p, false);
		if (lo)
			page = _popups[i];
		else if (_popups[i]->getBounds().contains(p))
			break;
	}
	if (!lo && _basePage) {
		lo = _basePage->objectAt(p, false);
		page = _basePage;
	}
	// Roll-off-close pages (the drop-down navigation bar): in the original
	// each page is a child window, and this page type closes itself when the
	// mouse leaves its window for another page's window, provided it is still
	// the topmost page. The pointer therefore has to enter the page first.
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		LivePage *pop = _popups[i];
		if (pop->getType() != kPageRolloffClose)
			continue;
		if (pop->getBounds().contains(p)) {
			pop->setMouseEntered(true);
		} else if (pop->mouseEntered() && i == (int)_popups.size() - 1) {
			if (lo && page == pop) {
				lo = nullptr;
				page = nullptr;
			}
			closePopup(pop->getIndex());
		}
	}
	debugC(2, kDebugScript, "Castle: mouse %d,%d over %s%s", p.x, p.y, lo ? objectClassName(lo->obj->cls) : "nothing", lo == _hoverObject ? " (unchanged)" : "");
	if (lo == _hoverObject)
		return;
	if (_hoverObject) {
		if (_hoverObject->obj->cls == kObjNavRollOverButton) {
			_hoverObject->hovered = false;
			_dirty = true;
		}
		const Event *ev = _hoverObject->obj->findEvent(kEventRollOff);
		if (ev)
			runEvent(ev, _hoverPage, _hoverObject);
	}
	_hoverObject = lo;
	_hoverPage = page;
	if (lo && (lo->obj->cls == kObjScrollObject || lo->obj->cls == kObjWrapScrollObject)) {
		_scrollObject = lo;
		_scrollPage = page;
		_scrollStep = 1;
		_scrollNext = 0;
	} else {
		_scrollObject = nullptr;
	_pressedObject = nullptr;
	_dragging = false;
	_dragPage = nullptr;
		_scrollPage = nullptr;
	}
	if (lo && lo->obj->cls == kObjNavRollOverButton) {
		lo->hovered = true;
		_dirty = true;
	}
	setCursor(lo && !lo->obj->cursor.empty() ? lo->obj->cursor : _db->getDefaultCursor());
	if (lo) {
		const Event *ev = lo->obj->findEvent(kEventRollOn);
		if (ev)
			runEvent(ev, page, lo);
	}
}

void CastleEngine::playWave(const Common::String &dir, const Common::String &name, bool loop) {
	Common::SeekableReadStream *s = _res->openWave(dir, name);
	if (!s) {
		debugC(1, kDebugSound, "Castle: wave '%s' not found", name.c_str());
		return;
	}
	Audio::SeekableAudioStream *stream = Audio::makeWAVStream(s, DisposeAfterUse::YES);
	if (!stream)
		return;
	_mixer->stopHandle(_waveHandle);
	if (loop)
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &_waveHandle, Audio::makeLoopingAudioStream(stream, 0));
	else
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &_waveHandle, stream);
}

void CastleEngine::playVideo(const Common::String &dir, const Common::String &name, const Common::Rect &destIn) {
	if (ConfMan.hasKey("castle_skipvideo") && ConfMan.getBool("castle_skipvideo")) {
		debugC(1, kDebugGraphics, "Castle: skipping video '%s'", name.c_str());
		return;
	}
	Common::SeekableReadStream *s = _res->openVideo(dir, name);
	if (!s) {
		debugC(1, kDebugGraphics, "Castle: video '%s' not found", name.c_str());
		return;
	}
	Video::QuickTimeDecoder *qt = new Video::QuickTimeDecoder();
	if (!qt->loadStream(s)) {
		delete qt;
		return;
	}
	byte pal[768];
	_system->getPaletteManager()->grabPalette(pal, 0, 256);
	qt->setDitheringPalette(pal);
	Common::Rect dest = destIn;
	if (dest.width() <= 0 || dest.height() <= 0)
		dest = Common::Rect(0, 0, qt->getWidth(), qt->getHeight());
	if (dest.width() != qt->getWidth() || dest.height() != qt->getHeight()) {
		// centre the video in the destination
		int x = dest.left + (dest.width() - qt->getWidth()) / 2;
		int y = dest.top + (dest.height() - qt->getHeight()) / 2;
		dest = Common::Rect(x, y, x + qt->getWidth(), y + qt->getHeight());
	}
	debugC(1, kDebugGraphics, "Castle: playing video '%s' %dx%d at %d,%d", name.c_str(), qt->getWidth(), qt->getHeight(), dest.left, dest.top);
	qt->start();
	bool skip = false;
	int frames = 0;
	while (!shouldQuit() && !qt->endOfVideo() && !skip) {
		Common::Event event;
		while (_eventMan->pollEvent(event)) {
			if (event.type == Common::EVENT_LBUTTONDOWN || (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE))
				skip = true;
		}
		if (qt->needsUpdate()) {
			const Graphics::Surface *frame = qt->decodeNextFrame();
			if (frame) {
				if (frame->format.bytesPerPixel == 1) {
					Common::Rect clip = dest;
					clip.clip(Common::Rect(0, 0, _screen.w, _screen.h));
					if (!clip.isEmpty())
						_screen.copyRectToSurface(*frame, clip.left, clip.top, Common::Rect(0, 0, clip.width(), clip.height()));
					_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
				}
				// Test harness: keep the tenth frame and stop after a second
				frames++;
				if (!_dumpDir.empty()) {
					if (frames == 10)
						dumpSurface(_screen);
					if (frames >= 30)
						skip = true;
				}
			}
		}
		_system->updateScreen();
		_system->delayMillis(10);
	}
	qt->close();
	delete qt;
	_dirty = true;
}

void CastleEngine::stopWave() {
	_mixer->stopHandle(_waveHandle);
}

// Waves started on a numbered channel can be stopped again by channel (and
// optionally by name); the library pages use channel 0 for the read-aloud
// narration and restart it on every click.
void CastleEngine::playWaveChannel(const Common::String &dir, const Common::String &name, int channel, bool loop) {
	Common::SeekableReadStream *s = _res->openWave(dir, name);
	if (!s) {
		debugC(1, kDebugSound, "Castle: wave '%s' not found", name.c_str());
		return;
	}
	Audio::SeekableAudioStream *stream = Audio::makeWAVStream(s, DisposeAfterUse::YES);
	if (!stream)
		return;
	for (uint i = 0; i < _channels.size(); i++) {
		if (_channels[i].name.equalsIgnoreCase(name)) {
			_mixer->stopHandle(_channels[i].handle);
			_channels.remove_at(i);
			break;
		}
	}
	WaveChannel wc;
	wc.channel = channel;
	wc.name = name;
	if (loop)
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &wc.handle, Audio::makeLoopingAudioStream(stream, 0));
	else
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &wc.handle, stream);
	_channels.push_back(wc);
}

void CastleEngine::stopWaveChannel(int channel, const Common::String &name) {
	for (uint i = 0; i < _channels.size();) {
		if (_channels[i].channel == channel && (name.empty() || _channels[i].name.equalsIgnoreCase(name))) {
			_mixer->stopHandle(_channels[i].handle);
			_channels.remove_at(i);
		} else {
			i++;
		}
	}
}

void CastleEngine::playAnimation(const Common::String &dir, const Common::String &name, const Common::Point &pos) {
	Common::SeekableReadStream *s = _res->openAnimation(dir, name);
	if (!s) {
		debugC(1, kDebugGraphics, "Castle: animation '%s' not found", name.c_str());
		return;
	}
	delete _ani;
	_ani = new AniDecoder();
	if (!_ani->load(s)) {
		delete _ani;
		_ani = nullptr;
		return;
	}
	_aniPos = pos;
	_aniNextFrame = _system->getMillis();
	_aniBackground.free();
	_mixer->stopHandle(_aniAudioHandle);
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_aniAudioHandle, _ani->getAudioStream());
}

void CastleEngine::updateAnimation() {
	if (!_ani)
		return;
	uint32 now = _system->getMillis();
	if (now < _aniNextFrame)
		return;
	Common::Rect dirty;
	if (!_ani->decodeNextFrame(dirty) || _ani->getCurFrame() >= (int)_ani->getFrameCount()) {
		delete _ani;
		_ani = nullptr;
		_dirty = true;
		return;
	}
	_aniNextFrame = now + 1000 / _ani->getFrameRate();
	const Graphics::Surface &frame = _ani->getFrame();
	if (frame.getPixels()) {
		if (!_aniBackground.getPixels())
			_aniBackground.create(MAX<int>(dirty.right, 1), MAX<int>(dirty.bottom, 1), Graphics::PixelFormat::createFormatCLUT8());
		if (dirty.right > _aniBackground.w || dirty.bottom > _aniBackground.h) {
			Graphics::Surface bigger;
			bigger.create(MAX<int>(dirty.right, _aniBackground.w), MAX<int>(dirty.bottom, _aniBackground.h), Graphics::PixelFormat::createFormatCLUT8());
			bigger.copyRectToSurface(_aniBackground, 0, 0, Common::Rect(0, 0, _aniBackground.w, _aniBackground.h));
			_aniBackground.free();
			_aniBackground = bigger;
		}
		_aniBackground.copyRectToSurface(frame, dirty.left, dirty.top, Common::Rect(0, 0, frame.w, frame.h));
	}
	_dirty = true;
}

// --- Spy quest ---------------------------------------------------------

// The quest module publishes its state through document variables so that
// the page scripts can test them (spy type, scenario, new game flag)
void CastleEngine::afterQuestLoad(bool ok, bool fireEvent) {
	const DocumentTail &tail = _db->getTail();
	_script->setDocVariable(tail.getNewGameVar(), Value::logical(!ok));
	_script->setDocVariable(tail.getScenarioVar(), Value::number(_quest->getScenario()));
	setSpy(_quest->getSpy(), fireEvent);
}

void CastleEngine::setSpy(int spy, bool fireEvent) {
	_quest->setSpy(spy);
	_script->setDocVariable(_db->getTail().getSpyVar(), Value::number(spy));
	debugC(1, kDebugScript, "Castle: spy type %d", spy);
	if (_basePage)
		applyQuestObjects(_basePage, false);
	for (uint i = 0; i < _popups.size(); i++)
		applyQuestObjects(_popups[i], false);
	_dirty = true;
	if (fireEvent)
		fireSpyChanged();
}

// on_SpyChanged goes to every open page, topmost first
void CastleEngine::fireSpyChanged() {
	Common::Array<LivePage *> pages;
	for (int i = (int)_popups.size() - 1; i >= 0; i--)
		pages.push_back(_popups[i]);
	if (_basePage)
		pages.push_back(_basePage);
	for (uint p = 0; p < pages.size(); p++) {
		bool open = p == pages.size() - 1 ? pages[p] == _basePage : false;
		for (uint i = 0; i < _popups.size(); i++)
			if (_popups[i] == pages[p])
				open = true;
		if (!open)
			continue;
		runPageEvents(pages[p], kEventSpyChanged);
		if (scriptShouldStop())
			return;
	}
}

// Sets up the quest objects of a page: coins and evidence waiting to be
// found, the spy pictures of the hut, the chest icon of the chosen spy and
// the toggles of the options page
void CastleEngine::applyQuestObjects(LivePage *page, bool onOpen) {
	int spy = _quest->getSpy();
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++) {
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			LiveObject &lo = panels[i]->objects[k];
			const GameObject *o = lo.obj;
			switch (o->cls) {
			case kObjCoinBitmap: {
				bool present = o->ints.size() > 1 && _quest->getCoin(o->ints[1], o->ints[0]) == 1;
				lo.visible = spy != 0 && present;
				debugC(2, kDebugScript, "Castle: coin room %d slot %d state %d -> %s", o->ints.size() > 1 ? o->ints[1] : -1, o->ints.empty() ? -1 : o->ints[0],
				       o->ints.size() > 1 ? _quest->getCoin(o->ints[1], o->ints[0]) : -1, lo.visible ? "shown" : "hidden");
				lo.zOrder = present ? 3 : -30000;
				break;
			}
			case kObjCollectBitmap:
				lo.visible = spy == 2 && !o->ints.empty() && _quest->getItem(o->ints[0]) == 1;
				lo.zOrder = lo.visible ? o->d : -30000;
				if (lo.visible && lo.image) {
					lo.rect.right = lo.rect.left + lo.image->surface.w;
					lo.rect.bottom = lo.rect.top + lo.image->surface.h;
				}
				break;
			case kObjRandomMapBitmap:
				// The map item of the active scenario of its task
				lo.visible = spy != 0 && o->ints.size() > 1 && _quest->getTask(o->ints[0], o->ints[1]) == 1;
				lo.value = 0;
				debugC(2, kDebugScript, "Castle: map item '%s' task %d choice %d at %d,%d -> %s", o->file.c_str(), o->ints.size() > 1 ? o->ints[0] : -1, o->ints.size() > 1 ? o->ints[1] : -1, lo.rect.left, lo.rect.top, lo.visible ? "shown" : "hidden");
				break;
			case kObjSpyDitherBitmap:
				// The chosen spy has left the hut: only the other one stays
				lo.visible = o->ints.empty() || spy != o->ints[0];
				lo.zOrder = lo.visible ? 2 : -30000;
				break;
			case kObjBoxIconBitmap:
				lo.visible = !o->ints.empty() && spy == o->ints[0];
				lo.zOrder = lo.visible ? 2 : -30000;
				break;
			case kObjScrollTickBitmap:
				lo.visible = !o->ints.empty() && _quest->getScrollFlag(o->ints[0]) == 1;
				break;
			case kObjScrollEditBox:
				// The answer given earlier on this scroll
				if (o->ints.size() > 2 && o->ints[2] >= 0 && o->ints[2] < Quest::kScrolls)
					lo.text = _quest->getScrollText(o->ints[2]);
				lo.value = 0;
				lo.selStart = -1;
				break;
			case kObjToggleButton: {
				int code = toggleCodeOf(o->id);
				lo.value = code < 0 || toggleState(code) ? 1 : 0;
				break;
			}
			default:
				break;
			}
		}
		if (onOpen && panels[i]->panel->type == kPanelSpyChest)
			updateSpyChest(page);
	}
}

// The spy's chest (SpyChestPanel): its 31 object slots show the lid, the
// found evidence, the purse with the coin sprite frames and the coins
void CastleEngine::updateSpyChest(LivePage *page) {
	const Common::Array<LivePanel *> &panels = page->getPanels();
	const Panel *chest = nullptr;
	for (uint i = 0; i < panels.size() && !chest; i++)
		if (panels[i]->panel->type == kPanelSpyChest)
			chest = panels[i]->panel;
	if (!chest || chest->spy.size() < 31)
		return;
	LiveObject *slots[31];
	for (int i = 0; i < 31; i++)
		slots[i] = page->findObject(chest->spy[i]);
	int done = _quest->countTasksDone();
	int coins = _quest->countCoins();
	int spy = _quest->getSpy();
	debugC(1, kDebugScript, "Castle: spy chest: spy %d, %d coins, %d tasks done, stage %d", spy, coins, done, _quest->getStage());
	if (done == 4 && _quest->getStage() == 2) {
		if (slots[7])
			slots[7]->visible = true;
		if (slots[8])
			slots[8]->zOrder = 5;
		if (slots[0])
			slots[0]->visible = false;
		if (slots[1])
			slots[1]->zOrder = -30000;
	} else {
		static const int taskSlot[4] = { 2, 5, 3, 4 }; // task 0, 1, 2, 3
		for (int t = 0; t < 4; t++) {
			bool found = false;
			for (int ch = 0; ch < 3; ch++)
				if (_quest->getTask(t, ch) == 2)
					found = true;
			if (found && slots[taskSlot[t]])
				slots[taskSlot[t]]->visible = true;
		}
		if (_quest->getStage() == 2) {
			if (slots[6])
				slots[6]->visible = true;
			if (slots[0])
				slots[0]->visible = false;
			if (slots[1])
				slots[1]->zOrder = -30000;
		}
	}
	// The purse and drawer sprites are shown on the frame that matches the state
	if (spy == 2) {
		if (slots[9])
			showSpriteFrame(slots[9], coins + 1);
		static const int itemSlot[4] = { 13, 14, 12, 11 };
		for (int it = 0; it < 4; it++)
			if (slots[itemSlot[it]] && _quest->getItem(it) == 2)
				showSpriteFrame(slots[itemSlot[it]], 2);
	} else if (spy == 1 && slots[10]) {
		showSpriteFrame(slots[10], coins + 1);
	}
	for (int i = 0; i < 16; i++)
		if (slots[15 + i] && coins > i)
			slots[15 + i]->zOrder = 7;
	_dirty = true;
}

void CastleEngine::showSpriteFrame(LiveObject *lo, int frame) {
	setSpriteFrame(lo, frame);
	if (lo->image) {
		lo->visible = true;
		lo->spriteState |= 0x10;
		if (lo->rect.width() <= 0 || lo->rect.height() <= 0) {
			lo->rect.right = lo->rect.left + lo->image->surface.w;
			lo->rect.bottom = lo->rect.top + lo->image->surface.h;
		}
	}
}

LivePanel *CastleEngine::findSpyChest(LivePage **pageOut) {
	Common::Array<LivePage *> pages;
	for (int i = (int)_popups.size() - 1; i >= 0; i--)
		pages.push_back(_popups[i]);
	if (_basePage)
		pages.push_back(_basePage);
	for (uint p = 0; p < pages.size(); p++) {
		const Common::Array<LivePanel *> &panels = pages[p]->getPanels();
		for (uint i = 0; i < panels.size(); i++)
			if (panels[i]->panel->type == kPanelSpyChest) {
				*pageOut = pages[p];
				return panels[i];
			}
	}
	return nullptr;
}

// Something went into the chest: the chest sound plays
void CastleEngine::chestFlash() {
	playWaveChannel(Common::String(), "@chest1s", -1);
}

int CastleEngine::toggleCodeOf(int objectId) const {
	const Common::Array<ToggleDesc> &toggles = _db->getTail().toggles;
	for (uint i = 0; i < toggles.size(); i++)
		if (toggles[i].objectId == objectId)
			return toggles[i].code;
	return -1;
}

bool CastleEngine::toggleState(int code) const {
	return code >= 0 && code < (int)ARRAYSIZE(_toggles) ? _toggles[code] : true;
}

void CastleEngine::setToggleState(int code, bool on) {
	if (code < 0 || code >= (int)ARRAYSIZE(_toggles))
		return;
	_toggles[code] = on;
	applyToggles();
}

// Option 11 switches the sounds, 18 the page transitions
void CastleEngine::applyToggles() {
	_mixer->muteSoundType(Audio::Mixer::kSFXSoundType, !toggleState(0xb));
}

void CastleEngine::newGame() {
	_quest->reset();
	_quiz->reset();
	setSpy(0, true);
	_script->setDocVariable(_db->getTail().getNewGameVar(), Value::logical(true));
}

// "Start new game": a game in progress first asks whether to save it
void CastleEngine::startGame() {
	if (!_quest->isDirty()) {
		newGame();
		return;
	}
	const DocumentTail &tail = _db->getTail();
	int mode = _quest->getMode();
	openPopup(mode == 1 || mode == 2 ? tail.questPages[2] : tail.questPages[1]);
}

void CastleEngine::runOptionsAction(int code, LivePage *page, LiveObject *obj) {
	debugC(1, kDebugScript, "Castle: OptionsAction %d", code);
	const DocumentTail &tail = _db->getTail();
	switch (code) {
	case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
		// Printing and the Windows clipboard
	case 0xd: case 0xe: case 0x13:
		debugC(1, kDebugScript, "Castle: print/copy option %d is not supported", code);
		break;
	case 0xb:
	case 0x12:
		applyToggles();
		break;
	case 0xc:
		newGame();
		break;
	case 0xf:
		// "Open saved game"
		if (_quest->isDirty()) {
			openPopup(tail.questPages[0]);
		} else {
			bool ok = loadGameDialog();
			// The GUI dialogs replace the palette and the screen
			_paletteDirty = true;
			_dirty = true;
			if (ok)
				afterQuestLoad(true, true);
		}
		break;
	case 0x10:
		// "Save game": to the slot of the last save, else ask
		if (_saveSlot >= 0) {
			SaveStateDescriptor desc = getMetaEngine()->querySaveMetaInfos(_targetName.c_str(), _saveSlot);
			saveGameState(_saveSlot, desc.getDescription(), false);
		} else {
			saveGameDialog();
			_paletteDirty = true;
			_dirty = true;
		}
		break;
	case 0x11:
		saveGameDialog();
		_paletteDirty = true;
		_dirty = true;
		break;
	case 0x14:
		startGame();
		break;
	default:
		debugC(1, kDebugScript, "Castle: OptionsAction %d not implemented", code);
		break;
	}
}

// --- Savegames ---------------------------------------------------------

#define CASTLE_SAVE_TAG MKTAG('C', 'S', 'T', 'L')

Common::Error CastleEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	Common::OutSaveFile *f = _saveFileMan->openForSaving(getSaveStateName(slot));
	if (!f)
		return Common::kWritingFailed;
	f->writeUint32BE(CASTLE_SAVE_TAG);
	f->writeByte(1);
	f->writeUint32BE(_basePage ? _basePage->getIndex() : 0);
	f->writeByte(ARRAYSIZE(_toggles));
	for (uint i = 0; i < ARRAYSIZE(_toggles); i++)
		f->writeByte(_toggles[i] ? 1 : 0);
	_quest->saveToStream(*f);
	getMetaEngine()->appendExtendedSave(f, getTotalPlayTime() / 1000, desc, isAutosave);
	f->finalize();
	bool ok = !f->err();
	delete f;
	if (!ok)
		return Common::kWritingFailed;
	_saveSlot = slot;
	_quest->setDirty(false);
	debugC(1, kDebugGeneral, "Castle: saved game to slot %d", slot);
	return Common::kNoError;
}

Common::Error CastleEngine::loadGameState(int slot) {
	Common::InSaveFile *f = _saveFileMan->openForLoading(getSaveStateName(slot));
	if (!f)
		return Common::kReadingFailed;
	bool ok = f->readUint32BE() == CASTLE_SAVE_TAG;
	if (ok) {
		f->readByte(); // version
		_savedPage = f->readUint32BE();
		int n = f->readByte();
		for (int i = 0; i < n; i++) {
			bool on = f->readByte() != 0;
			if (i < (int)ARRAYSIZE(_toggles))
				_toggles[i] = on;
		}
		ok = _quest->loadFromStream(*f) && !f->err();
	}
	delete f;
	if (!ok) {
		warning("Castle: savegame slot %d could not be read", slot);
		_quest->randomize();
		return Common::kReadingFailed;
	}
	_saveSlot = slot;
	applyToggles();
	debugC(1, kDebugGeneral, "Castle: loaded game from slot %d (spy %d, page %u)", slot, _quest->getSpy(), _savedPage);
	return Common::kNoError;
}

// --- The spy's questions ----------------------------------------------

void CastleEngine::flushPendingPage() {
	if (!_pendingBase)
		return;
	_pendingBase = false;
	Common::Point scroll = _pendingScroll;
	_pendingScroll = Common::Point(0, 0);
	int transition = _pendingTransition;
	_pendingTransition = 0;
	// Page turns wipe the new page over the old one (option 18 switches it off)
	bool wipe = (transition == 4 || transition == 5) && toggleState(18) && _basePage && !_noScreenUpdate;
	Graphics::Surface old;
	if (wipe) {
		old.copyFrom(_screen);
		_noScreenUpdate = true;
	}
	openBasePage(_pendingBasePage);
	if (_basePage && (scroll.x || scroll.y)) {
		_basePage->setScroll(scroll);
		_dirty = true;
		updateTrailScroll();
	}
	if (wipe) {
		render();
		_noScreenUpdate = false;
		wipeTransition(old, transition);
		old.free();
		_dirty = true;
	}
	if (_pendingPopup) {
		uint popup = _pendingPopup;
		_pendingPopup = 0;
		openPopup(popup);
	}
}

// A room page opened: its scenario hotspot (mode 0) starts the question
// bound to it and the spy asks it
void CastleEngine::startRoomQuestions(LivePage *page) {
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++) {
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			LiveObject &lo = panels[i]->objects[k];
			const GameObject *o = lo.obj;
			if (o->cls != kObjRandomScenarioHotspot || o->ints.size() < 4)
				continue;
			debugC(2, kDebugScript, "Castle: scenario hotspot mode %d question %d name '%s'", o->ints[2], o->ints[3], o->strs.empty() ? "" : o->strs[0].c_str());
			if (o->ints[2] != 0)
				continue;
			lo.value = 0;
			_quiz->startQuestion(o->ints[3]);
			_quiz->fireEvent(Quiz::kEvtStart, 0);
			return;
		}
	}
}

static bool isEditBox(const GameObject *o) {
	return o->cls == kObjEditBox || o->cls == kObjRoomEditBox || o->cls == kObjScrollEditBox;
}

static LiveObject *findObjectOfClass(LivePage *page, int cls, bool editBoxes) {
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++)
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			LiveObject &lo = panels[i]->objects[k];
			if (lo.visible && (editBoxes ? isEditBox(lo.obj) : lo.obj->cls == cls))
				return &lo;
		}
	return nullptr;
}

// The edit box that receives the keyboard: the topmost page with one
LiveObject *CastleEngine::findEditBox(LivePage **pageOut) {
	// A clicked box keeps the keyboard while its page is on top
	if (_editFocus && _editFocusPage && (_popups.empty() ? _editFocusPage == _basePage : _editFocusPage == _popups.back())) {
		*pageOut = _editFocusPage;
		return _editFocus;
	}
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		LiveObject *lo = findObjectOfClass(_popups[i], 0, true);
		if (lo) {
			*pageOut = _popups[i];
			return lo;
		}
	}
	if (_basePage) {
		LiveObject *lo = findObjectOfClass(_basePage, 0, true);
		if (lo) {
			*pageOut = _basePage;
			return lo;
		}
	}
	return nullptr;
}

void CastleEngine::typeKey(int ascii, int keycode) {
	if (collageKey(ascii, keycode))
		return;
	LivePage *page = nullptr;
	LiveObject *edit = findEditBox(&page);
	if (!edit)
		return;
	if (keycode == Common::KEYCODE_RETURN || keycode == Common::KEYCODE_KP_ENTER) {
		// Enter presses the page's OK button
		LiveObject *ok = findObjectOfClass(page, kObjQuestionOKButton, false);
		if (!ok)
			ok = findObjectOfClass(page, kObjScrollQuestAnswerHtsp, false);
		if (ok)
			clickObject(ok, page);
		return;
	}
	if (keycode == Common::KEYCODE_TAB) {
		// Tab moves to the next edit box of the page
		const Common::Array<LivePanel *> &panels = page->getPanels();
		Common::Array<LiveObject *> boxes;
		for (uint i = 0; i < panels.size(); i++)
			for (uint k = 0; k < panels[i]->objects.size(); k++)
				if (panels[i]->objects[k].visible && isEditBox(panels[i]->objects[k].obj))
					boxes.push_back(&panels[i]->objects[k]);
		for (uint i = 0; i < boxes.size(); i++)
			if (boxes[i] == edit) {
				focusEditBox(boxes[(i + 1) % boxes.size()], page);
				break;
			}
		return;
	}
	if (keycode == Common::KEYCODE_BACKSPACE) {
		if (!edit->text.empty())
			edit->text.deleteLastChar();
	} else if (ascii >= 32 && ascii < 127 && edit->text.size() < 80) {
		edit->text += (char)ascii;
	} else {
		return;
	}
	_dirty = true;
}

// QuestionOKButton: checks the typed answer against the lists of the
// current question. A recognised misspelling keeps the popup open (the
// original beeps); otherwise the popup closes and the spy reacts.
void CastleEngine::answerClicked(LiveObject *lo, LivePage *page) {
	LiveObject *edit = findObjectOfClass(page, 0, true);
	if (!edit)
		return;
	int n = lo->obj->ints.size() > 2 ? lo->obj->ints[2] : 1;
	int result = _quiz->checkAnswer(n, edit->text);
	debugC(1, kDebugScript, "Castle: quiz: answer %d '%s' -> '%s' -> %d", n, edit->text.c_str(), _quiz->normalize(edit->text).c_str(), result);
	if (result == Quiz::kAnswerMisspelled)
		return;
	if (page->isPopup())
		closePopup(page->getIndex());
	_quiz->fireEvent(result == Quiz::kAnswerAccepted ? Quiz::kEvtCorrect : Quiz::kEvtWrong, 0);
	_quiz->answered();
	_dirty = true;
}

// The conversation videos play over the room view (the first panel of the
// room page); the names are relative to the room's directory
void CastleEngine::quizPlayVideo(const Common::String &name) {
	Common::Rect dest;
	Common::String dir;
	if (_basePage) {
		dir = _basePage->getDir();
		if (!_basePage->getPanels().empty())
			dest = _basePage->getPanels()[0]->rect;
	}
	playVideo(dir, name, dest);
}

void CastleEngine::quizPlayWave(const Common::String &name) {
	playWave(_basePage ? _basePage->getDir() : Common::String(), name, false);
}

// Page changes inside a conversation take effect at once, the objects
// that follow in the group run on the new page
void CastleEngine::quizChangePage(uint page, int transition) {
	Action a;
	a.type = kActChangePage;
	a.page = page;
	a.x = transition;
	runAction(&a, _basePage, nullptr);
	flushPendingPage();
}

void CastleEngine::quizOpenPopup(uint page) {
	openPopup(page);
}

void CastleEngine::quizSendMessage(int objectId, int msg) {
	Action a;
	a.type = kActSendMessage;
	a.p[0] = objectId;
	a.p[1] = msg;
	runAction(&a, _basePage, nullptr);
}

// The conversation state goes to the document variable the room scripts test
void CastleEngine::quizSetState(int state) {
	_script->setDocVariable(_db->getTail().vars[0], Value::number(state));
	_quest->setExtra(state);
}


// ---- Index and Trail lists ------------------------------------------------

// Fills the Trail list with the navigation history (the Index list is built
// from its record when the page opens)
void CastleEngine::setupCollages(LivePage *page) {
	LiveObject *lo = page->findCollage();
	if (!lo || !lo->collage || !lo->collage->tracker)
		return;
	Collage *c = lo->collage;
	c->items.clear();
	for (uint i = 0; i < _trail.size(); i++) {
		CollageItem it;
		it.text = _trail[i].title;
		it.full = it.text;
		it.icon = _trail[i].icon;
		it.entry = i;
		c->items.push_back(it);
	}
	c->selected = -1;
	c->scrollTo(c->maxScroll());
}

// The list that receives the keyboard: the topmost page with one
LiveObject *CastleEngine::findCollage(LivePage **pageOut) {
	for (int i = (int)_popups.size() - 1; i >= 0; i--) {
		LiveObject *lo = _popups[i]->findCollage();
		if (lo) {
			*pageOut = _popups[i];
			return lo;
		}
	}
	if (_basePage) {
		LiveObject *lo = _basePage->findCollage();
		if (lo) {
			*pageOut = _basePage;
			return lo;
		}
	}
	return nullptr;
}

// A click on the list selects the entry under the mouse; a double click
// goes to it
void CastleEngine::collagePress(LiveObject *lo, LivePage *page, const Common::Point &p) {
	Collage *c = lo->collage;
	if (!c)
		return;
	int item = c->itemAt(lo->rect, p);
	if (item < 0)
		return;
	uint32 now = _system->getMillis();
	bool doubleClick = !c->tracker && item == c->lastClickItem && now - c->lastClickTime < 400;
	c->lastClickTime = now;
	c->lastClickItem = item;
	if (c->select(item, 1))
		collageSelected(page, lo);
	if (doubleClick)
		activateCollageItem(page, lo);
}

// Releasing the mouse on a Trail entry goes back to it (the Tracker's
// event handler of the original activates on event 4)
void CastleEngine::collageRelease(LiveObject *lo, LivePage *page, const Common::Point &p) {
	Collage *c = lo->collage;
	if (!c || !c->tracker)
		return;
	int item = c->itemAt(lo->rect, p);
	if (item >= 0 && item == c->selected)
		goToTrailEntry(c->items[item].entry);
}

// The selection changed: the Index's edit box shows the entry's full text
void CastleEngine::collageSelected(LivePage *page, LiveObject *lo) {
	Collage *c = lo->collage;
	_dirty = true;
	if (c->tracker || c->selected < 0)
		return;
	c->typed.clear();
	LiveObject *edit = page->findObjectOfClass(kObjEditBox);
	if (edit) {
		edit->text = c->items[c->selected].full;
		edit->selStart = 0;
	}
}

// Text typed into the Index's edit box: the first entry sorting at or after
// it is selected and completed in the box, the completion selected so that
// the next key replaces it
void CastleEngine::collageTyped(LivePage *page, LiveObject *lo) {
	Collage *c = lo->collage;
	LiveObject *edit = page->findObjectOfClass(kObjEditBox);
	_dirty = true;
	if (c->typed.empty()) {
		if (edit) {
			edit->text.clear();
			edit->selStart = -1;
		}
		return;
	}
	int item = c->findPrefix(c->typed);
	if (item >= 0)
		c->select(item, 1);
	c->ensureVisible(c->selected);
	if (edit && c->selected >= 0) {
		edit->text = c->items[c->selected].full;
		edit->selStart = MIN<int>(c->typed.size(), edit->text.size());
	}
}

void CastleEngine::scrollCollage(LivePage *page, int delta) {
	LiveObject *lo = page ? page->findCollage() : nullptr;
	if (!lo)
		lo = findCollage(&page);
	if (!lo || !lo->collage)
		return;
	lo->collage->scrollBy(delta);
	_dirty = true;
}

// Goes to the selected entry: Index entries change the page (scrolling a
// zoom page to the entry's position) and open a popup (a library book, a
// glossary entry); Trail entries return to the recorded location
void CastleEngine::activateCollageItem(LivePage *page, LiveObject *lo) {
	Collage *c = lo->collage;
	if (!c || c->selected < 0 || c->selected >= c->count())
		return;
	CollageItem it = c->items[c->selected];
	if (c->tracker) {
		goToTrailEntry(it.entry);
		return;
	}
	debugC(1, kDebugScript, "Castle: index entry '%s' page %u popup %u at %d,%d", it.full.c_str(), it.page, it.popup, it.pt.x, it.pt.y);
	if (page->isPopup())
		closePopup(page->getIndex());
	// lo and c are gone with the popup
	if (it.page != 0xffffffff) {
		if (it.pt.x == 0 && it.pt.y == 0) {
			Action a;
			a.type = kActChangePage;
			a.page = it.page;
			runAction(&a, nullptr, nullptr);
		} else if (_basePage && _basePage->getIndex() == it.page) {
			_basePage->setScroll(it.pt);
			_dirty = true;
			updateTrailScroll();
		} else {
			_pendingBasePage = it.page;
			_pendingBase = true;
			_pendingScroll = it.pt;
		}
	}
	if (it.popup != 0xffffffff) {
		if (_pendingBase)
			_pendingPopup = it.popup;
		else
			openPopup(it.popup);
	}
}

// Geometry of the Index's scroll bar: the arrow buttons at both ends and
// the coin travelling along the bar between them
static void scrollBarGeometry(Resources &res, const LiveObject &lo, int &barTop, int &barBottom, int &coinH) {
	const GameObject *o = lo.obj;
	Image *up = o->strs.size() > 0 ? res.loadImage(Common::String(), o->strs[0]) : nullptr;
	Image *down = o->strs.size() > 1 ? res.loadImage(Common::String(), o->strs[1]) : nullptr;
	Image *coin = o->strs.size() > 2 ? res.loadImage(Common::String(), o->strs[2]) : nullptr;
	barTop = lo.rect.top + (up ? up->surface.h : 18);
	barBottom = lo.rect.bottom - (down ? down->surface.h : 18);
	coinH = coin ? coin->surface.h : 16;
}

void CastleEngine::scrollBarPress(LiveObject *lo, LivePage *page, const Common::Point &p) {
	int pos, maxPos, pageSize;
	if (!page->getScrollState(pos, maxPos, pageSize))
		return;
	int barTop, barBottom, coinH;
	scrollBarGeometry(*_res, *lo, barTop, barBottom, coinH);
	if (p.y < barTop) {
		page->setScrollPos(pos - 1);
		lo->value = 1;
	} else if (p.y >= barBottom) {
		page->setScrollPos(pos + 1);
		lo->value = 2;
	} else {
		int travel = barBottom - barTop - coinH;
		int y = (maxPos > 0 && travel > 0) ? travel * pos / maxPos : 0;
		int coinY = barTop + y;
		if (p.y >= coinY && p.y < coinY + coinH) {
			_scrollBarDrag = true;
			_scrollBarGrab = p.y - coinY;
		} else if (p.y < coinY) {
			page->setScrollPos(pos - pageSize);
		} else {
			page->setScrollPos(pos + pageSize);
		}
	}
	_dirty = true;
}

void CastleEngine::scrollBarDrag(LiveObject *lo, LivePage *page, const Common::Point &p) {
	int pos, maxPos, pageSize;
	if (!page || !page->getScrollState(pos, maxPos, pageSize))
		return;
	int barTop, barBottom, coinH;
	scrollBarGeometry(*_res, *lo, barTop, barBottom, coinH);
	int travel = barBottom - barTop - coinH;
	if (travel <= 0)
		return;
	int y = CLIP(p.y - _scrollBarGrab - barTop, 0, travel);
	int top = (y * maxPos + travel / 2) / travel;
	if (top != pos) {
		page->setScrollPos(top);
		_dirty = true;
	}
}

// Keys on a page with a list (the GroupCollage of the original): Enter
// goes to the selection, the cursor keys move it, characters search
bool CastleEngine::collageKey(int ascii, int keycode) {
	LivePage *page = nullptr;
	LiveObject *lo = findCollage(&page);
	if (!lo || !lo->collage)
		return false;
	Collage *c = lo->collage;
	switch (keycode) {
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		activateCollageItem(page, lo);
		return true;
	case Common::KEYCODE_UP:
		c->moveSelection(-1, -1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_DOWN:
		c->moveSelection(1, 1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_PAGEUP:
		c->moveSelection(-c->pageSize, -1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_PAGEDOWN:
		c->moveSelection(c->pageSize, 1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_HOME:
		c->moveSelection(-c->count(), 1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_END:
		c->moveSelection(c->count(), -1);
		collageSelected(page, lo);
		return true;
	case Common::KEYCODE_BACKSPACE:
		if (!c->tracker) {
			if (!c->typed.empty())
				c->typed.deleteLastChar();
			collageTyped(page, lo);
		}
		return true;
	default:
		break;
	}
	if (!c->tracker && ascii >= 32 && ascii < 127 && c->typed.size() < 80) {
		c->typed += (char)ascii;
		collageTyped(page, lo);
	}
	return true;
}

// Describes a page as a Trail location: the library books, the rooms and
// the Castle Guide carry their titles in the document; other pages carry
// theirs in their record. Pages without a title (the title page, the
// options) are not recorded.
bool CastleEngine::describeLocation(LivePage *page, bool popup, TrailEntry &e) {
	const DocumentTail &t = _db->getTail();
	PageRecord *rec = page->getRecord();
	if (!rec)
		return false;
	if (popup) {
		// The six library books, by the id of their popup page
		static const int kStrIndex[6] = { 0, 2, 1, 3, 5, 4 };
		static const int kIcons[6] = { 23, 25, 26, 21, 22, 24 };
		for (int k = 0; k < 6; k++) {
			if (rec->id == t.ints[1 + k]) {
				e.title = _db->getString(t.titleStrs[kStrIndex[k]]);
				e.icon = kIcons[k];
				e.page = _basePage ? _basePage->getIndex() : 0;
				e.popup = page->getIndex();
				return true;
			}
		}
		return false;
	}
	e.page = page->getIndex();
	e.popup = 0xffffffff;
	e.scroll = page->getScroll();
	for (int i = 0; i < 4; i++) {
		if (e.page == t.pages2[i]) {
			e.title = _db->getString(t.titleStrs[6 + i]);
			e.icon = 31 + i;
			return true;
		}
	}
	if (rec->id == t.ints[9]) {
		e.title = _db->getString(t.titleStrs[10]);
		e.icon = 30;
		return true;
	}
	e.title = rec->title;
	e.icon = rec->icon;
	return !e.title.empty() && e.icon > 0;
}

void CastleEngine::recordTrail(LivePage *page, bool popup) {
	if (_trailNavigating) {
		// Returning to an entry: the pages opened on the way are not new
		if (popup || !_pendingPopup)
			_trailNavigating = false;
		return;
	}
	TrailEntry e;
	if (!describeLocation(page, popup, e))
		return;
	if (!_trail.empty()) {
		const TrailEntry &last = _trail.back();
		if (last.page == e.page && last.popup == e.popup)
			return;
	}
	if (_trail.size() >= 60)
		_trail.remove_at(0);
	_trail.push_back(e);
	debugC(1, kDebugScript, "Castle: trail %u: '%s' icon %d page %u popup %u", _trail.size(), e.title.c_str(), e.icon, e.page, e.popup);
}

// Keeps the latest entry's view position up to date while on a zoom page
void CastleEngine::updateTrailScroll() {
	if (!_basePage || _trail.empty())
		return;
	TrailEntry &last = _trail.back();
	if (last.page == _basePage->getIndex() && last.popup == 0xffffffff)
		last.scroll = _basePage->getScroll();
}

void CastleEngine::goToTrailEntry(int entry) {
	if (entry < 0 || entry >= (int)_trail.size())
		return;
	TrailEntry e = _trail[entry];
	debugC(1, kDebugScript, "Castle: trail back to '%s' page %u popup %u", e.title.c_str(), e.page, e.popup);
	closeAllPopups();
	_trailNavigating = true;
	if (!_basePage || _basePage->getIndex() != e.page) {
		_pendingBasePage = e.page;
		_pendingBase = true;
		_pendingScroll = e.scroll;
		_pendingPopup = e.popup != 0xffffffff ? e.popup : 0;
	} else {
		_basePage->setScroll(e.scroll);
		_dirty = true;
		if (e.popup != 0xffffffff)
			openPopup(e.popup);
		else
			_trailNavigating = false;
	}
}


// ---- Chest scrolls ---------------------------------------------------------

void CastleEngine::focusEditBox(LiveObject *lo, LivePage *page) {
	_editFocus = lo;
	_editFocusPage = page;
}

// A page is about to close: the chest scroll popup saves its answers and,
// after its OK was pressed, marks the right ones
void CastleEngine::pageClosing(LivePage *page) {
	if (_editFocusPage == page) {
		_editFocus = nullptr;
		_editFocusPage = nullptr;
	}
	scrollPageClosing(page);
}

// The answer of a chest scroll against the scenario's answer lists
bool CastleEngine::scrollAnswerMatches(int i, const Common::String &text, bool misspelled) const {
	int scenario = _quest->getScenario();
	int index = (scenario - 1) * 4 + i;
	if (scenario < 1 || scenario > 3 || i < 0 || i >= 4 || index >= 12)
		return false;
	const AnswerList &list = _db->getTail().answers[index];
	Common::String norm = _quiz->normalize(text);
	const Common::Array<Common::String> &entries = misspelled ? list.misspelled : list.accepted;
	for (uint k = 0; k < entries.size(); k++)
		if (entries[k].equalsIgnoreCase(norm))
			return true;
	return false;
}

// The OK of a scroll (ScrollQuestAnswerHtsp): recognised misspellings are
// shown in red with the spelling popup; otherwise the scroll is judged when
// it closes
void CastleEngine::checkScrollAnswers(LiveObject *lo, LivePage *page) {
	const Common::Array<LivePanel *> &panels = page->getPanels();
	bool misspelt = false;
	for (uint i = 0; i < panels.size(); i++)
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			LiveObject &box = panels[i]->objects[k];
			if (box.obj->cls != kObjScrollEditBox || box.text.empty() || box.obj->ints.size() < 3)
				continue;
			if (scrollAnswerMatches(box.obj->ints[2], box.text, true)) {
				box.value = 1;
				misspelt = true;
			}
		}
	_dirty = true;
	if (misspelt) {
		lo->value = 0;
		if (_db->getTail().spellPage)
			openPopup(_db->getTail().spellPage);
	} else {
		lo->value = 1;
	}
}

void CastleEngine::scrollPageClosing(LivePage *page) {
	LiveObject *ok = page->findObjectOfClass(kObjScrollQuestAnswerHtsp);
	if (!ok)
		return;
	const Common::Array<LivePanel *> &panels = page->getPanels();
	for (uint i = 0; i < panels.size(); i++)
		for (uint k = 0; k < panels[i]->objects.size(); k++) {
			const LiveObject &box = panels[i]->objects[k];
			if (box.obj->cls != kObjScrollEditBox || box.obj->ints.size() < 3)
				continue;
			int idx = box.obj->ints[2];
			if (idx >= 0 && idx < Quest::kScrolls && box.text != _quest->getScrollText(idx))
				_quest->setScroll(idx, box.text, _quest->getScrollFlag(idx));
		}
	debugC(2, kDebugScript, "Castle: scroll closing, OK %d, answers '%s' '%s' '%s' '%s'", ok->value, _quest->getScrollText(0).c_str(),
	       _quest->getScrollText(1).c_str(), _quest->getScrollText(2).c_str(), _quest->getScrollText(3).c_str());
	if (ok->value != 1)
		return;
	for (int i = 0; i < Quest::kScrolls; i++)
		if (_quest->getScrollText(i).empty())
			return;
	// Every line answered: the spy reads the scroll
	bool all = true;
	for (int i = 0; i < Quest::kScrolls; i++) {
		bool good = scrollAnswerMatches(i, _quest->getScrollText(i), false);
		_quest->setScroll(i, _quest->getScrollText(i), good ? 1 : 0);
		all = all && good;
	}
	debugC(1, kDebugScript, "Castle: scroll judged: %s", all ? "all right" : "some wrong");
	Common::String dir = "\\chest\\";
	queueWave(dir, "con02");
	queueWave(dir, "conrustl");
	Action a;
	a.type = kActGeneralPurpose;
	a.x = 3;
	runAction(&a, nullptr, nullptr);
	if (all) {
		_quest->finishStage();
		a.x = 4;
		runAction(&a, nullptr, nullptr);
		queueWave(dir, "con04");
	} else {
		queueWave(dir, "con03");
	}
}

// Waves played one after the other (the original waits for each to end)
void CastleEngine::queueWave(const Common::String &dir, const Common::String &name) {
	_waveQueueDir = dir;
	_waveQueue.push_back(name);
}

void CastleEngine::updateWaveQueue() {
	if (_waveQueue.empty() || _mixer->isSoundHandleActive(_waveHandle))
		return;
	Common::String name = _waveQueue[0];
	_waveQueue.remove_at(0);
	playWave(_waveQueueDir, name, false);
}


// Wipes the new page (in _screen) over the old one: code 5 sweeps from the
// left edge, code 4 from the right
void CastleEngine::wipeTransition(const Graphics::Surface &from, int code) {
	Graphics::Surface frame;
	frame.copyFrom(from);
	const int duration = 350;
	uint32 start = _system->getMillis();
	bool dumped = false;
	for (;;) {
		int t = MIN<int>(_system->getMillis() - start, duration);
		int x = _screen.w * t / duration;
		for (int y = 0; y < _screen.h; y++) {
			int x0 = code == 5 ? 0 : _screen.w - x;
			memcpy(frame.getBasePtr(x0, y), _screen.getBasePtr(x0, y), x);
		}
		_system->copyRectToScreen(frame.getPixels(), frame.pitch, 0, 0, frame.w, frame.h);
		_system->updateScreen();
		if (!dumped && t >= duration / 2) {
			dumped = true;
			dumpSurface(frame);
		}
		if (t >= duration)
			break;
		Common::Event event;
		while (_eventMan->pollEvent(event))
			;
		_system->delayMillis(10);
	}
	frame.free();
}

} // End of namespace Castle
