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

#ifndef CASTLE_COLLAGE_H
#define CASTLE_COLLAGE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Castle {

// An entry of a collage list
struct CollageItem {
	Common::String text;    // text drawn in the list
	Common::String full;    // "name, section": searched and shown in the edit box
	bool sub;               // indented sub-entry of a heading
	bool header;            // heading of a group: never selected
	uint32 page;            // index: page to change to (0xffffffff = none)
	uint32 popup;           // index: popup to open afterwards (0xffffffff = none)
	Common::Point pt;       // index: scroll position of a zoom page
	int icon;               // trail: shield icon 1..34
	int entry;              // trail: history entry
	CollageItem() : sub(false), header(false), page(0xffffffff), popup(0xffffffff), icon(0), entry(-1) {}
};

/*
 * A Collage object: the scrollable list of the Word Search (Index) and Trail
 * popups. The original builds it from a "Linear" layout (one item per row)
 * and a data source, CastleIndex (the entries of the index) or Tracker (the
 * navigation history, one shield icon per section).
 */
struct Collage {
	bool tracker;
	Common::Array<CollageItem> items;
	int selected;           // -1 = none
	int scrollTop;          // first visible item
	int pageSize;           // visible rows
	int itemHeight;
	int indent;             // indent of sub-entries
	byte fg[3];             // text colour
	byte hiBg[3];           // highlight bar
	byte hiFg[3];           // highlighted text
	Common::String typed;   // prefix typed in the edit box (autocomplete)
	Common::Array<Common::String> icons;    // trail: icon names
	Common::Array<Common::String> iconsHi;  // trail: highlighted icon names
	uint32 lastClickTime;
	int lastClickItem;

	Collage();

	int count() const { return (int)items.size(); }
	int maxScroll() const { return MAX(0, count() - pageSize); }
	void scrollTo(int top);
	void scrollBy(int delta) { scrollTo(scrollTop + delta); }
	void ensureVisible(int item);
	// Selects an item; headings are skipped in the given direction
	// (dir < 0 upwards, otherwise downwards). True when the selection changed.
	bool select(int item, int dir);
	void moveSelection(int delta, int dir);
	// The item at a point, given the list rectangle on screen
	int itemAt(const Common::Rect &rect, const Common::Point &p) const;
	// The first item whose text sorts at or after the given prefix
	int findPrefix(const Common::String &text) const;
	static int compareText(const Common::String &a, const Common::String &b);
};

} // End of namespace Castle

#endif
