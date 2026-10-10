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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/archive.h"
#include "flaaklypa/console.h"
#include "flaaklypa/detection.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/debug-channels.h"
#include "common/events.h"
#include "common/scummsys.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/framelimiter.h"
#include "graphics/pixelformat.h"
#include "image/bmp.h"

namespace Flaaklypa {

FlaaklypaEngine *g_engine;

FlaaklypaEngine::FlaaklypaEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Flaaklypa") {
	g_engine = this;
}

FlaaklypaEngine::~FlaaklypaEngine() {
	delete _screen;
}

uint32 FlaaklypaEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String FlaaklypaEngine::getGameId() const {
	return _gameDescription->gameId;
}

DataArchive *FlaaklypaEngine::openScene(const Common::String &dir, const Common::String &scene) {
	Common::Path path(dir + "/" + scene + ".bin", '/');
	DataArchive *arc = DataArchive::open(path);
	if (!arc)
		warning("Could not open container %s", path.toString().c_str());
	return arc;
}

bool FlaaklypaEngine::drawBitmap(DataArchive *arc, const Common::Path &name, int x, int y) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(arc->createReadStreamForMember(name));
	if (!stream) {
		warning("Bitmap %s not found in container", name.toString().c_str());
		return false;
	}

	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(*stream)) {
		warning("Could not decode bitmap %s", name.toString().c_str());
		return false;
	}

	const Graphics::Surface *src = decoder.getSurface();
	debug(1, "Loaded bitmap %s (%dx%d, %d bpp)", name.toString().c_str(), src->w, src->h, src->format.bpp());
	Graphics::Surface *converted = src->convertTo(_screen->format, decoder.getPalette().data());
	_screen->blitFrom(*converted, Common::Point(x, y));
	converted->free();
	delete converted;
	return true;
}

Common::Error FlaaklypaEngine::run() {
	// The game draws 24-bit backdrops; ask for a 32-bit screen and let the
	// backend pick the closest supported format.
	Graphics::PixelFormat format(4, 8, 8, 8, 8, 24, 16, 8, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	_screen = new Graphics::Screen(kScreenWidth, kScreenHeight, g_system->getScreenFormat());

	// Set the engine's debugger console
	setDebugger(new Console());

	// Proof of concept: open the main menu container and show its backdrop.
	// Everything else (hotspot masks, Smacker animations, the sub games and
	// the racing game) still has to be written.
	Common::ScopedPtr<DataArchive> menu(openScene("data", "menu"));
	if (!menu)
		return Common::kNoGameDataFoundError;

	drawBitmap(menu.get(), "backdrop.bmp", 0, 0);
	_screen->update();

	Common::Event e;
	Graphics::FrameLimiter limiter(g_system, 60);
	while (!shouldQuit()) {
		while (g_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_KEYDOWN && e.kbd.keycode == Common::KEYCODE_ESCAPE)
				quitGame();
		}

		limiter.delayBeforeSwap();
		_screen->update();
		limiter.startFrame();
	}

	return Common::kNoError;
}

Common::Error FlaaklypaEngine::syncGame(Common::Serializer &s) {
	// Saving is not implemented yet. The original keeps per player state in
	// common/profiles/*.pfl inside the data directory.
	return Common::kUnknownError;
}

} // End of namespace Flaaklypa
