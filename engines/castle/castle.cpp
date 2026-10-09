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
#include "common/tokenizer.h"
#include "image/png.h"

#include "castle/ani.h"
#include "castle/castle.h"
#include "castle/database.h"
#include "castle/detection.h"
#include "castle/page.h"
#include "castle/resources.h"
#include "castle/vm.h"

namespace Castle {

CastleEngine::CastleEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc),
		_rnd("castle"), _script(nullptr), _hoverObject(nullptr), _hoverPage(nullptr), _db(nullptr), _res(nullptr), _basePage(nullptr), _dirty(true), _paletteDirty(true), _pendingBasePage(0),
		_pendingBase(false), _dumpCount(0), _ani(nullptr), _aniNextFrame(0), _spyType(0), _pressedObject(nullptr), _pressedPage(nullptr), _dragging(false), _dragPage(nullptr), _dungeonTimerEnd(0), _dungeonState(0), _scrollObject(nullptr), _scrollPage(nullptr), _scrollNext(0), _scrollStep(1), _ambientNext(0) {
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
	delete _res;
	delete _db;
}

bool CastleEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
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

	uint start = _db->getStartPage();
	if (ConfMan.hasKey("boot_param"))
		start = ConfMan.getInt("boot_param");
	if (ConfMan.hasKey("castle_dump"))
		_dumpDir = ConfMan.get("castle_dump");
	openBasePage(start);

	setCursor(_db->getDefaultCursor());
	CursorMan.showMouse(true);

	// Debug harness: CASTLE_DUMP=<dir> writes a PNG of the screen after each
	// render, CASTLE_CLICKS="x,y;x,y;..." performs scripted clicks and quits.
	Common::String dumpDirStr = ConfMan.hasKey("castle_dump") ? ConfMan.get("castle_dump") : Common::String();
	const char *dumpDir = dumpDirStr.empty() ? nullptr : dumpDirStr.c_str();
	// Each entry is "x,y" for a click, "m:x,y" for a mouse move, "p:x,y"
	// for a button press and "r:x,y" for a release.
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
	uint32 nextClick = _system->getMillis() + 1500;
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
					if (_dragging || _dragPage)
						dragTo(pt);
					else
						handleMouseMove(pt);
				} else if (kind == 'r') {
					releaseMouse(pt);
				} else if (lo) {
					pressObject(lo, page, pt);
					if (kind == 'c')
						releaseMouse(pt);
				}
				nextClick = _system->getMillis() + 1500;
			}
		}
		if (_pendingBase) {
			_pendingBase = false;
			Common::Point scroll = _pendingScroll;
			_pendingScroll = Common::Point(0, 0);
			openBasePage(_pendingBasePage);
			if (_basePage && (scroll.x || scroll.y)) {
				_basePage->setScroll(scroll);
				_dirty = true;
			}
		}
		uint32 now = _system->getMillis();
		if (_basePage)
			_basePage->update(now, *_res);
		updateSprites(now);
		updateScrolling(now);
		updateAmbientSound(now, false);
		updateDungeonTimer(now);
		updateAnimation();
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
		case Common::EVENT_MOUSEMOVE:
			if (_dragging || _dragPage)
				dragTo(event.mouse);
			else
				handleMouseMove(event.mouse);
			break;
		case Common::EVENT_KEYDOWN:
			if (event.kbd.keycode == Common::KEYCODE_ESCAPE && _ani) {
				delete _ani;
				_ani = nullptr;
				_dirty = true;
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
	_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, _screen.w, _screen.h);
	_system->updateScreen();
	_dirty = false;
	if (!_dumpDir.empty() && !_ani) {
		Common::DumpFile f;
		Common::Path path(Common::String::format("%s/castle%03d.png", _dumpDir.c_str(), _dumpCount++), '/');
		debugC(1, kDebugGraphics, "Castle: dump %s", path.toString().c_str());
		if (f.open(path)) {
			byte pal[768];
			_system->getPaletteManager()->grabPalette(pal, 0, 256);
			::Image::writePNG(f, _screen, pal, 256);
			f.close();
		}
	}
}

void CastleEngine::openBasePage(uint index) {
	_hoverObject = nullptr;
	_scrollObject = nullptr;
	_pressedObject = nullptr;
	_dragging = false;
	_dragPage = nullptr;
	_hoverPage = nullptr;
	closeAllPopups();
	delete _basePage;
	_basePage = new LivePage();
	if (!_basePage->open(*_db, *_res, index, Common::Point(0, 0))) {
		delete _basePage;
		_basePage = nullptr;
		return;
	}
	_dirty = true;
	_paletteDirty = true;
	render();
	runPageEvents(_basePage, kEventOpen);
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
	for (uint i = 0; i < _popups.size(); i++)
		delete _popups[i];
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
			if (page && page->isPopup())
				closePopup(page->getIndex());
			openPopup(a->page);
		} else {
			_pendingBasePage = a->page;
			_pendingBase = true;
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
	case kActQuit:
		quitGame();
		break;
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
		_spyType = a->x;
		break;
	case kActPlayResponse: {
		// Three wave/animation pairs, one per spy character
		int idx = _spyType == 1 ? 0 : _spyType == 2 ? 1 : 2;
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
		case 1:
			playWave(Common::String(), "@rot1s", true);
			break;
		case 2:
		case 0x10:
			stopWave();
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
		case 8: _dungeonState = 0; break;
		case 9: _dungeonState = 1; break;
		case 10: _dungeonState = 3; break;
		case 0xb: _dungeonState = 2; break;
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
		lo->playing = lo->frameDelay != 0;
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
	clickObject(lo, page);
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
				lo.nextFrameTime = now + lo.frameDelay;
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

} // End of namespace Castle
