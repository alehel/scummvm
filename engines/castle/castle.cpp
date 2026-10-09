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
#include "audio/decoders/wave.h"
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

namespace Castle {

CastleEngine::CastleEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst), _gameDescription(gameDesc),
		_db(nullptr), _res(nullptr), _basePage(nullptr), _dirty(true), _paletteDirty(true), _pendingBasePage(0),
		_pendingBase(false), _ani(nullptr), _aniNextFrame(0) {
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

	uint start = _db->getStartPage();
	if (ConfMan.hasKey("boot_param"))
		start = ConfMan.getInt("boot_param");
	openBasePage(start);

	CursorMan.showMouse(true);

	// Debug harness: CASTLE_DUMP=<dir> writes a PNG of the screen after each
	// render, CASTLE_CLICKS="x,y;x,y;..." performs scripted clicks and quits.
	Common::String dumpDirStr = ConfMan.hasKey("castle_dump") ? ConfMan.get("castle_dump") : Common::String();
	const char *dumpDir = dumpDirStr.empty() ? nullptr : dumpDirStr.c_str();
	Common::Array<Common::Point> clicks;
	if (ConfMan.hasKey("castle_clicks")) {
		Common::StringTokenizer tok(ConfMan.get("castle_clicks"), ";");
		while (!tok.empty()) {
			Common::String t = tok.nextToken();
			int x = 0, y = 0;
			sscanf(t.c_str(), "%d,%d", &x, &y);
			clicks.push_back(Common::Point(x, y));
		}
	}
	uint clickIdx = 0;
	uint32 nextClick = _system->getMillis() + 1500;
	int dumpCount = 0;

	while (!shouldQuit()) {
		handleEvents();
		if (dumpDir && !clicks.empty() && _system->getMillis() >= nextClick) {
			if (clickIdx >= clicks.size()) {
				quitGame();
			} else {
				Common::Point pt = clicks[clickIdx++];
				LivePage *page = nullptr;
				LiveObject *lo = hitTest(pt, &page);
				debugC(1, kDebugScript, "Castle: scripted click %d,%d -> %s", pt.x, pt.y, lo ? objectClassName(lo->obj->cls) : "nothing");
				if (lo) {
					const Event *ev = lo->obj->findEvent(kEventClick);
					if (ev)
						runEvent(ev, page, lo);
				}
				nextClick = _system->getMillis() + 1500;
			}
		}
		if (_pendingBase) {
			_pendingBase = false;
			openBasePage(_pendingBasePage);
		}
		uint32 now = _system->getMillis();
		if (_basePage)
			_basePage->update(now, *_res);
		updateAnimation();
		bool wasDirty = _dirty || _paletteDirty;
		render();
		if (dumpDir && wasDirty && !_ani) {
			Common::DumpFile f;
			Common::Path path(Common::String::format("%s/castle%03d.png", dumpDir, dumpCount++), '/');
			if (f.open(path)) {
				byte pal[768];
				_system->getPaletteManager()->grabPalette(pal, 0, 256);
				::Image::writePNG(f, _screen, pal, 256);
				f.close();
			}
		}
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
				const Event *ev = lo->obj->findEvent(kEventClick);
				debugC(1, kDebugScript, "Castle: click on %s '%s' (id %d) at %d,%d", objectClassName(lo->obj->cls),
				       lo->obj->file.c_str(), lo->obj->id, event.mouse.x, event.mouse.y);
				if (ev)
					runEvent(ev, page, lo);
			} else if (_ani) {
				// Clicking skips a running animation
				delete _ani;
				_ani = nullptr;
				_dirty = true;
			}
			break;
		}
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
	if (img && img->palette.size() > 0)
		_system->getPaletteManager()->setPalette(img->palette.data(), 0, MIN<uint>(256, img->palette.size()));
	_paletteDirty = false;
}

void CastleEngine::render() {
	if (_paletteDirty)
		applyPalette();
	if (!_dirty)
		return;
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
}

void CastleEngine::openBasePage(uint index) {
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
			delete _popups[i];
			_popups.remove_at(i);
			_dirty = true;
			return;
		}
	}
}

void CastleEngine::closeAllPopups() {
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
	case kActNewPlayWave:
	case kActPlayWaveChannel:
		playWave(page ? page->getDir() : Common::String(), a->name, false);
		break;
	case kActStopWave:
	case kActStopWaveChannel:
		stopWave();
		break;
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
		// The script logic is not interpreted yet; run unconditional scripts only.
		if (a->script && a->script->ops.empty())
			runActions(a->script->actions, page, obj);
		else
			debugC(1, kDebugScript, "Castle: skipping scripted Command with %u ops", a->script ? a->script->ops.size() : 0);
		break;
	default:
		debugC(1, kDebugScript, "Castle: unimplemented action %s", actionName(a->type));
		break;
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
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_waveHandle, stream);
}

void CastleEngine::playVideo(const Common::String &dir, const Common::String &name, const Common::Rect &destIn) {
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
					_system->updateScreen();
				}
			}
		}
		_system->delayMillis(10);
	}
	qt->close();
	delete qt;
	_dirty = true;
}

void CastleEngine::stopWave() {
	_mixer->stopHandle(_waveHandle);
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
