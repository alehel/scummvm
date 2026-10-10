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
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/font.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

BitmapFont::BitmapFont() : _bitmap(nullptr), _keyColor(0), _height(0) {
}

BitmapFont::~BitmapFont() {
	delete _bitmap;
}

bool BitmapFont::load(const Common::String &name) {
	delete _bitmap;
	_glyphs.clear();
	_bitmap = g_engine->_resources->loadBitmap("common", Common::String::format("fonts/%s.bmp", name.c_str()), g_engine->_screen->format);
	if (!_bitmap) {
		warning("Font %s not found", name.c_str());
		return false;
	}
	_height = _bitmap->h;

	// The original (FONT module) walks the top row: a run of pixels that
	// differ from the top left one is a glyph.
	uint32 separator = _bitmap->getPixel(0, 0);
	bool inGlyph = false;
	Glyph g = { 0, 0 };
	for (int x = 0; x < _bitmap->w && _glyphs.size() < 256; x++) {
		bool sep = _bitmap->getPixel(x, 0) == separator;
		if (!sep && !inGlyph) {
			g.x0 = x;
			inGlyph = true;
		} else if (sep && inGlyph) {
			g.x1 = x;
			_glyphs.push_back(g);
			inGlyph = false;
		}
	}
	if (_glyphs.empty())
		return false;
	_keyColor = _bitmap->getPixel(_glyphs[0].x0, 0);
	return true;
}

int BitmapFont::stringWidth(const Common::String &text) const {
	int w = 0;
	for (uint i = 0; i < text.size(); i++) {
		byte c = (byte)text[i];
		if (c < _glyphs.size())
			w += _glyphs[c].x1 - _glyphs[c].x0;
	}
	return w;
}

void BitmapFont::drawString(Graphics::ManagedSurface &dst, int x, int y, const Common::String &text) const {
	if (!_bitmap)
		return;
	for (uint i = 0; i < text.size(); i++) {
		byte c = (byte)text[i];
		if (c >= _glyphs.size())
			continue;
		const Glyph &g = _glyphs[c];
		dst.transBlitFrom(*_bitmap, Common::Rect(g.x0, 0, g.x1, _height), Common::Point(x, y), _keyColor);
		x += g.x1 - g.x0;
	}
}

void BitmapFont::drawStringCentred(Graphics::ManagedSurface &dst, const Common::Rect &r, const Common::String &text) const {
	int w = stringWidth(text);
	drawString(dst, r.left + (r.width() - w) / 2, r.top + (r.height() - _height) / 2, text);
}

} // End of namespace Flaaklypa
