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

#ifndef CASTLE_PAGE_H
#define CASTLE_PAGE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "castle/database.h"

namespace Graphics {
struct Surface;
}

namespace Castle {

class Resources;
struct Image;
struct LivePanel;

// An object instantiated on screen
struct LiveObject {
	GameObject *obj;
	LivePanel *panel;
	Common::Rect rect;      // screen rectangle
	Image *image;
	int frame;              // current animation frame (1-based), 0 = none
	int frameCount;
	uint32 nextFrameTime;
	bool visible;
	LiveObject() : obj(nullptr), panel(nullptr), image(nullptr), frame(0), frameCount(0), nextFrameTime(0), visible(true) {}
};

struct LivePanel {
	Panel *panel;
	Common::Rect rect;      // screen rectangle
	Common::String dir;
	byte rgb[3];
	bool fill;
	Common::Array<LiveObject> objects;
	LivePanel() : panel(nullptr), fill(false) { rgb[0] = rgb[1] = rgb[2] = 0; }
};

// A page opened on screen: a record laid out through its template
class LivePage {
public:
	LivePage();
	~LivePage();

	bool open(Database &db, Resources &res, uint index, const Common::Point &origin);
	void draw(Graphics::Surface &screen, Resources &res) const;
	LiveObject *hitTest(const Common::Point &p);
	LiveObject *objectAt(const Common::Point &p, bool hotspotsOnly);
	void update(uint32 now, Resources &res);

	uint getIndex() const { return _index; }
	int getType() const { return _rec ? _rec->type : kPageNull; }
	bool isPopup() const { return getType() == kPagePopup || getType() == kPageDragPopup || getType() == kPageRolloffClose; }
	const Common::String &getDir() const { return _dir; }
	Common::Rect getBounds() const { return _bounds; }
	const Common::Array<LivePanel *> &getPanels() const { return _panels; }
	const Image *getPaletteImage() const { return _paletteImage; }
	PageRecord *getRecord() const { return _rec; }

private:
	LivePanel *addPanel(Panel *panel, const Common::Rect &rect, const Common::String &dir, Resources &res);
	void layoutObjects(LivePanel *lp, Resources &res);
	Common::String frameName(const GameObject *obj, int frame) const;

	uint _index;
	PageRecord *_rec;
	const PageTemplate *_tmpl;
	Common::String _dir;
	Common::Point _origin;
	Common::Rect _bounds;
	Common::Array<LivePanel *> _panels;
	Image *_paletteImage;
};

} // End of namespace Castle

#endif
