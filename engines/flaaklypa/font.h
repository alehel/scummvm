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
#ifndef FLAAKLYPA_FONT_H
#define FLAAKLYPA_FONT_H

#include "common/array.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Flaaklypa {

/**
 * A bitmap font (common/fonts/<name>.bmp).
 *
 * The bitmap is a strip of 256 glyphs, one per Windows-1252 code. The top
 * row marks the glyph columns: pixels equal to the top left pixel separate
 * the glyphs. The colour of the first glyph's top left pixel is the colour
 * key; the glyph pixels keep their own colours.
 */
class BitmapFont {
public:
	BitmapFont();
	~BitmapFont();

	bool load(const Common::String &name);

	int height() const { return _height; }
	int stringWidth(const Common::String &text) const;
	void drawString(Graphics::ManagedSurface &dst, int x, int y, const Common::String &text) const;

	/** Draws the text centred in the rectangle. */
	void drawStringCentred(Graphics::ManagedSurface &dst, const Common::Rect &r, const Common::String &text) const;

private:
	struct Glyph {
		int x0, x1;
	};

	Graphics::ManagedSurface *_bitmap;
	Common::Array<Glyph> _glyphs;
	uint32 _keyColor;
	int _height;
};

} // End of namespace Flaaklypa

#endif
