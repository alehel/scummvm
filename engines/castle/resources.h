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

#ifndef CASTLE_RESOURCES_H
#define CASTLE_RESOURCES_H

#include "common/hashmap.h"
#include "common/path.h"
#include "common/str.h"
#include "common/stream.h"
#include "graphics/palette.h"
#include "graphics/surface.h"

namespace Common {
class PEResources;
}

namespace Castle {

// A decoded 8-bit image with its palette
struct Image {
	Graphics::Surface surface;
	Graphics::Palette palette;
	bool hasTransparentColor;
	uint32 transparentColor;
	Image() : palette(0), hasTransparentColor(false), transparentColor(0) {}
	~Image() { surface.free(); }
};

/*
 * Resolves and loads the game's resources.
 *
 * Resource names in the database use a few prefixes:
 *   '@'  -> embedded in CASTLE.EXE as a Windows BITMAP/WAVE resource
 *   '&'  -> relative to the data directory
 *   '\'  -> absolute path from the CD root
 *   else -> relative to the directory of the current page
 * Bitmaps are stored as "name.png" (really PNG) or "name.dib" (BMP).
 */
class Resources {
public:
	Resources();
	~Resources();

	bool init();

	// Returns a stream for the given resource name (without extension), or
	// nullptr. The caller owns the stream.
	Common::SeekableReadStream *openFile(const Common::String &dir, const Common::String &name, const char *const *extensions);

	// Loads a bitmap (cached). Returns nullptr if missing.
	Image *loadImage(const Common::String &dir, const Common::String &name);

	Common::SeekableReadStream *openWave(const Common::String &dir, const Common::String &name);
	Common::SeekableReadStream *openAnimation(const Common::String &dir, const Common::String &name);
	Common::SeekableReadStream *openVideo(const Common::String &dir, const Common::String &name);

	static Common::String makePath(const Common::String &dir, const Common::String &name);

	// Builds a palette remapping table that brightens colours (used for the
	// castle section highlight). Call whenever the palette changes.
	void buildHighlightTable(const byte *palette);
	const byte *getHighlightTable() const { return _highlight; }

private:
	Image *decodeImage(Common::SeekableReadStream *stream);
	Common::SeekableReadStream *openExeBitmap(const Common::String &name);
	Common::SeekableReadStream *openExeWave(const Common::String &name);

	Common::PEResources *_exe;
	byte _highlight[256];
	Common::HashMap<Common::String, Image *> _imageCache;
};

} // End of namespace Castle

#endif
