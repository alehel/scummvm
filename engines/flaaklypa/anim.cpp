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
#include "common/system.h"
#include "common/textconsole.h"
#include "video/smk_decoder.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

Anim::Anim(Scene *scene, const AnimDef *def) : _scene(scene), _def(def),
	_video(nullptr), _frame(nullptr), _hasKey(false), _keyColor(0),
	_x(def->x), _y(def->y), _z(def->zOrder), _hotspot(def->hotspot), _group(def->group),
	_added(false), _playing(false), _removeWhenDone(false), _lastFrameTime(0), _lastFrameShown(false) {
}

Anim::~Anim() {
	unload();
}

void Anim::load() {
	Resources *res = _scene->resources();
	const Common::String &sceneName = _scene->name();
	if (_def->smacker) {
		_video = res->openSmacker(sceneName, Common::String::format("animation/%s.smk", _def->name));
		if (_video && _def->visible) {
			// Decode the first frame so the element can be drawn (and hit
			// tested) before it starts playing, like the original does.
			const Graphics::Surface *frame = _video->decodeNextFrame();
			if (frame)
				convertFrame(frame);
			_video->rewind();
		}
	} else {
		_frame = res->loadBitmap(sceneName, Common::String::format("bitmap/%s.bmp", _def->name), g_engine->_screen->format);
		if (_frame && _def->transparent) {
			_keyColor = _frame->getPixel(0, 0);
			_hasKey = true;
		}
	}
}

void Anim::unload() {
	delete _video;
	_video = nullptr;
	delete _frame;
	_frame = nullptr;
	_hasKey = false;
}

void Anim::convertFrame(const Graphics::Surface *frame) {
	const byte *palette = _video->getPalette();
	Graphics::Surface *conv = frame->convertTo(g_engine->_screen->format, palette);
	if (!_frame)
		_frame = new Graphics::ManagedSurface();
	_frame->copyFrom(*conv);
	conv->free();
	delete conv;
	if (_def->transparent && !_hasKey) {
		_keyColor = _frame->getPixel(0, 0);
		_hasKey = true;
	}
}

void Anim::add(int x, int y, int z) {
	if (_added)
		return;
	if (x == kDefaultPos && y == kDefaultPos) {
		x = _def->x;
		y = _def->y;
	}
	_x = x;
	_y = y;
	_z = z;
	_playing = false;
	_removeWhenDone = false;
	load();
	_added = true;
	_scene->animAdded(this);
	debug(2, "Anim %s added at %d,%d z=%d", _def->name, _x, _y, _z);
}

void Anim::remove() {
	if (!_added)
		return;
	stop();
	unload();
	_added = false;
	_scene->animRemoved(this);
	debug(2, "Anim %s removed", _def->name);
}

void Anim::play() {
	if (!_added)
		add(_def->x, _def->y, _def->zOrder);
	if (_playing)
		return;
	// A clip whose file is missing (some are absent from the Gold edition)
	// "plays" for zero time so sequences and characters carry on.
	if (_video) {
		_video->rewind();
		_video->start();
	}
	_playing = true;
	_lastFrameShown = false;
	_scene->animStarted(this);
	debug(2, "Anim %s started", _def->name);
}

void Anim::stop() {
	if (_video && _playing)
		_video->stop();
	_playing = false;
}

void Anim::setZ(int z) {
	_z = z;
	_scene->animAdded(this); // re-sort
}

bool Anim::update() {
	if (!_playing)
		return false;
	if (!_video) {
		_playing = false;
		return true;
	}

	if (_video->needsUpdate()) {
		const Graphics::Surface *frame = _video->decodeNextFrame();
		if (frame && _def->visible)
			convertFrame(frame);
	}

	if (_video->endOfVideo() && _def->loop) {
		_video->rewind();
		_video->start();
		return false;
	}

	if (_video->endOfVideo()) {
		// Hold the last frame for one frame period before reporting the end.
		if (!_lastFrameShown) {
			_lastFrameShown = true;
			_lastFrameTime = g_system->getMillis();
			return false;
		}
		uint32 frameMs = 1000 / MAX(1, _video->getFrameRate().toInt());
		if (g_system->getMillis() - _lastFrameTime < frameMs)
			return false;
		_video->stop();
		_playing = false;
		debug(2, "Anim %s finished", _def->name);
		return true;
	}
	return false;
}

void Anim::draw(Graphics::ManagedSurface &dst) {
	if (!_added || !_frame || !_def->visible)
		return;
	if (_hasKey)
		dst.transBlitFrom(*_frame, Common::Point(_x, _y), _keyColor);
	else
		dst.blitFrom(*_frame, Common::Point(_x, _y));
}

Common::Rect Anim::rect() const {
	if (!_frame)
		return Common::Rect(_x, _y, _x, _y);
	return Common::Rect(_x, _y, _x + _frame->w, _y + _frame->h);
}

int Anim::hitTest(int x, int y) const {
	if (!_added || !_frame || !_def->visible || _hotspot == 0)
		return -1;
	if (!rect().contains(x, y))
		return 0;
	if (_def->transparent && _hasKey) {
		if (_frame->getPixel(x - _x, y - _y) == _keyColor)
			return 0;
	}
	return _hotspot;
}

} // End of namespace Flaaklypa
