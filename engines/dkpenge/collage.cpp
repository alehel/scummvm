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

#include "common/util.h"

#include "dkpenge/collage.h"

namespace DKPenge {

Collage::Collage() : tracker(false), selected(-1), scrollTop(0), pageSize(1), itemHeight(1), indent(0), lastClickTime(0), lastClickItem(-1) {
	fg[0] = fg[1] = fg[2] = 0;
	hiBg[0] = 0x85; hiBg[1] = 0x86; hiBg[2] = 0xb2;
	hiFg[0] = hiFg[1] = hiFg[2] = 0;
}

void Collage::scrollTo(int top) {
	scrollTop = CLIP(top, 0, maxScroll());
}

void Collage::ensureVisible(int item) {
	if (item < 0 || item >= count())
		return;
	if (item < scrollTop)
		scrollTo(item);
	else if (item >= scrollTop + pageSize)
		scrollTo(item - pageSize + 1);
}

bool Collage::select(int item, int dir) {
	if (item < 0 || item >= count())
		return false;
	// Group headings pass the selection on to a neighbour (CastleIndex
	// event handler of the original)
	while (item >= 0 && item < count() && items[item].header) {
		if (dir < 0) {
			if (item == 0)
				return false;
			item--;
		} else {
			if (item == count() - 1)
				return false;
			item++;
		}
	}
	if (item < 0 || item >= count() || item == selected)
		return false;
	selected = item;
	return true;
}

void Collage::moveSelection(int delta, int dir) {
	int item = selected < 0 ? (delta < 0 ? count() - 1 : 0) : selected + delta;
	item = CLIP(item, 0, count() - 1);
	select(item, dir); // a heading at the edge leaves the selection alone
	ensureVisible(selected);
}

int Collage::itemAt(const Common::Rect &rect, const Common::Point &p) const {
	if (!rect.contains(p) || itemHeight <= 0)
		return -1;
	int row = (p.y - rect.top) / itemHeight;
	if (row >= pageSize)
		return -1;
	int item = scrollTop + row;
	if (item < 0 || item >= count())
		return -1;
	if (tracker) {
		// The original hits the icon (35x40) or the text line to its right
		int x = p.x - rect.left, y = p.y - rect.top - row * itemHeight;
		if (!(x < 35 && y < 40) && !(x >= 40 && y >= 9 && y < 22 + 9))
			return -1;
	}
	return item;
}

// The original's compare treats text case-insensitively and sorts the
// comma that separates an entry from its section before any letter
int Collage::compareText(const Common::String &a, const Common::String &b) {
	uint i = 0;
	for (;; i++) {
		if (i >= a.size() || i >= b.size())
			break;
		byte ca = tolower((byte)a[i]), cb = tolower((byte)b[i]);
		if (ca == ',')
			ca = 0x1f;
		if (cb == ',')
			cb = 0x1f;
		if (ca != cb)
			return ca < cb ? -1 : 1;
	}
	if (a.size() == b.size())
		return 0;
	return a.size() < b.size() ? -1 : 1;
}

int Collage::findPrefix(const Common::String &text) const {
	int n = count();
	if (n == 0)
		return -1;
	if (compareText(text, items[n - 1].full) > 0)
		return n - 1;
	int lo = -1, hi = n - 1;
	while (lo + 1 < hi) {
		int mid = (lo + hi) / 2;
		if (compareText(text, items[mid].full) <= 0)
			hi = mid;
		else
			lo = mid;
	}
	return hi;
}

} // End of namespace DKPenge
