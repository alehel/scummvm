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

#ifndef DKPENGE_PAGE_H
#define DKPENGE_PAGE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "dkpenge/database.h"
#include "dkpenge/vm.h"

namespace Graphics {
struct Surface;
}

namespace DKPenge {

class Resources;
struct Image;
struct LivePanel;
struct Collage;

// An object instantiated on screen
struct LiveObject {
	GameObject *obj;
	LivePanel *panel;
	Common::Rect rect;      // screen rectangle
	Image *image;
	int frame;              // current animation frame (1-based), 0 = none
	int frameCount;
	int frameDelay;         // ms between frames
	uint32 nextFrameTime;
	bool playing;
	bool visible;
	bool disabled;
	bool hovered;           // mouse is over the object (rollover buttons)
	bool pressed;           // mouse button held on the object (buttons show their pressed artwork)
	int zOrder;
	int value;
	int counters[5];
	int extra[9];
	int spriteFlags;        // sprite boolean properties (+0x11c)
	int spriteState;        // sprite state bits (+0x138)
	int spriteLoops;        // remaining loops (-1 = forever)
	uint32 spriteStartTime;
	bool spriteStarted;
	Image *overlayA;        // zoom caption: painted zoom area artwork
	Image *overlayB;        // zoom caption: painted zoom object artwork
	Image *altImage;        // second artwork: toggle off, blinking item, open chest icon
	bool flashed;           // chest icon: shows its open-lid artwork
	Common::Point altOffset; // chest icon: where the open-lid artwork sits relative to the icon
	bool dithered;          // spy pictures of the hut: drawn through a checkerboard
	Common::Point origin;   // sprites: start position (panel relative) taken when activated
	uint32 regionIn, regionOut; // sprites: region scripts (events 7/8) already fired, one bit per script
	Common::Rect limitRect; // sprites: limit rectangle (panel relative, record rects[2], property 0x3e)
	int vx, vy;             // sprites: velocity in pixels per second (record p[11], p[12])
	uint32 motionT0x, motionT0y, nextMotionTime; // sprites: timing of the self propelled motion
	Common::String cursor;  // cursor name shown over the object (scripts may change it)
	const TextStyle *style; // edit boxes: text style (size, alignment)
	Common::String text;    // edit boxes: the typed text
	int selStart;           // edit boxes: start of the selected (auto-completed) text, -1 = none
	Collage *collage;       // Collage objects: the list (owned by the page)
	LiveObject() : obj(nullptr), panel(nullptr), image(nullptr), frame(0), frameCount(0), frameDelay(100), nextFrameTime(0),
			playing(false), visible(true), disabled(false), hovered(false), pressed(false), zOrder(0), value(0), spriteFlags(0), spriteState(0), spriteLoops(-1), spriteStartTime(0), spriteStarted(false), overlayA(nullptr), overlayB(nullptr), altImage(nullptr), flashed(false), dithered(false), regionIn(0), regionOut(0), vx(0), vy(0), motionT0x(0), motionT0y(0), nextMotionTime(0), style(nullptr), selStart(-1), collage(nullptr) {
		memset(counters, 0, sizeof(counters));
		memset(extra, 0, sizeof(extra));
	}
};

struct Overlay {
	Image *image;
	Common::Point pos;
};

class LivePage;

struct LivePanel {
	Panel *panel;
	LivePage *page;
	Scope scope;
	Common::Array<Overlay> overlays;
	Common::Point scroll;   // content offset (zoom panels)
	Common::Point contentSize; // extent of the scrollable content
	Common::Rect rect;      // screen rectangle
	Common::String dir;
	byte rgb[3];
	bool fill;
	Common::Array<LiveObject> objects;
	LivePanel() : panel(nullptr), page(nullptr), fill(false) { rgb[0] = rgb[1] = rgb[2] = 0; }
};

// A page opened on screen: a record laid out through its template
class LivePage {
public:
	LivePage();
	~LivePage();

	bool open(Database &db, Resources &res, uint index, const Common::Point &origin);
	void setScroll(const Common::Point &p);
	// Moves the whole page on screen (drag popups)
	void moveBy(int dx, int dy);
	// Scrolls the zoom panels by a delta, clamped to their content
	bool scrollBy(int dx, int dy);
	void draw(Graphics::Surface &screen, Resources &res) const;
	LiveObject *hitTest(const Common::Point &p);
	LiveObject *objectAt(const Common::Point &p, bool hotspotsOnly);
	// Advances the page's own animations; true when something changed
	void update(uint32 now, Resources &res);
	bool takeChanged() { bool c = _changed; _changed = false; return c; }

	uint getIndex() const { return _index; }
	int getType() const { return _rec ? _rec->type : kPageNull; }
	bool isPopup() const { return getType() == kPagePopup || getType() == kPageDragPopup || getType() == kPageRolloffClose; }
	const Common::String &getDir() const { return _dir; }
	Common::Rect getBounds() const { return _bounds; }
	bool mouseEntered() const { return _mouseEntered; }
	void setMouseEntered(bool b) { _mouseEntered = b; }
	const Common::Array<LivePanel *> &getPanels() const { return _panels; }
	const Image *getPaletteImage() const { return _paletteImage; }
	PageRecord *getRecord() const { return _rec; }
	Scope &getScope() { return _scope; }
	LiveObject *findObject(int id);
	LiveObject *findObjectOfClass(int cls);
	// The page's list object (Index and Trail popups)
	LiveObject *findCollage();
	// Content offset of the zoom panels
	Common::Point getScroll() const;
	// The page's scrollable list or bitmap: position, maximum and rows per page
	bool getScrollState(int &pos, int &maxPos, int &pageSize) const;
	void setScrollPos(int pos);

private:
	LivePanel *addPanel(Panel *panel, const Common::Rect &rect, const Common::String &dir, Resources &res);
	void layoutObjects(LivePanel *lp, Resources &res);
	void createCollages(Database &db, Resources &res);
	Common::String frameName(const GameObject *obj, int frame) const;

	uint _index;
	PageRecord *_rec;
	const PageTemplate *_tmpl;
	Common::String _dir;
	Common::Point _origin;
	Common::Rect _bounds;
	bool _mouseEntered;     // roll-off-close pages: the pointer has been inside the page
	bool _changed;          // an animation advanced since the last draw
	Common::Array<LivePanel *> _panels;
	Image *_paletteImage;
	bool _paletteFixed;     // a PaletteBitmap chose the palette
	Scope _scope;
};

} // End of namespace DKPenge

#endif
