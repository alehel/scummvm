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
#ifndef FLAAKLYPA_CURSOR_H
#define FLAAKLYPA_CURSOR_H

#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Video {
class SmackerDecoder;
}

namespace Flaaklypa {

/**
 * The animated mouse cursors (common/cursors/<name>.smk).
 */
class Cursor {
public:
	Cursor();
	~Cursor();

	/** Shows common/cursors/<name>.smk; an empty name hides the cursor. */
	void set(const Common::String &name);
	const Common::String &current() const { return _name; }

	/** Advances the cursor animation. */
	void update();

private:
	void showFrame(const Graphics::Surface *frame);

	Common::String _name;
	Video::SmackerDecoder *_video;
	Graphics::ManagedSurface _frame;
};

} // End of namespace Flaaklypa

#endif
