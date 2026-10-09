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
#include "graphics/font.h"
#include "graphics/surface.h"

#include "dkpenge/collage.h"
#include "dkpenge/detection.h"
#include "dkpenge/page.h"
#include "dkpenge/resources.h"

namespace DKPenge {

LivePage::LivePage() : _index(0), _rec(nullptr), _tmpl(nullptr), _mouseEntered(false), _changed(false), _paletteImage(nullptr), _paletteFixed(false) {
}

LivePage::~LivePage() {
	for (uint i = 0; i < _panels.size(); i++) {
		for (uint k = 0; k < _panels[i]->objects.size(); k++)
			delete _panels[i]->objects[k].collage;
		delete _panels[i];
	}
}

Common::String LivePage::frameName(const GameObject *obj, int frame) const {
	if (obj->cls == kObjSprite)
		return Common::String::format("%s%04d", obj->file.c_str(), frame);
	if (obj->cls == kObjAmbientAnimation) {
		if (frame >= 1 && frame <= (int)obj->strs.size())
			return obj->strs[frame - 1];
		return obj->file;
	}
	return obj->file;
}

void LivePage::layoutObjects(LivePanel *lp, Resources &res) {
	for (uint i = 0; i < lp->panel->objects.size(); i++) {
		GameObject *obj = lp->panel->objects[i];
		LiveObject lo;
		lo.obj = obj;
		lo.panel = lp;
		lo.cursor = obj->cursor;
		lo.rect = obj->rect;
		lo.rect.translate(lp->rect.left, lp->rect.top);

		lo.zOrder = obj->d;
		lo.visible = obj->c != 0;
		if (obj->cls == kObjSprite) {
			lo.frameCount = obj->spriteFrames;
			lo.frame = obj->ints.size() > 1 ? obj->ints[1] + 1 : 1;
			// A zero delay means the sprite never advances by itself (the
			// chest's purse and drawers are stepped by the quest logic)
			lo.frameDelay = obj->ints.size() > 2 ? (int)obj->ints[2] : 100;
			for (int k = 0; k < 5; k++)
				lo.counters[k] = obj->ints.size() > 5 + (uint)k ? obj->ints[5 + k] : 0;
			lo.spriteFlags = obj->ints.size() > 3 ? obj->ints[3] : 0;
			lo.spriteState = obj->ints.size() > 10 ? obj->ints[10] : 0;
			lo.spriteLoops = obj->ints.size() > 14 ? obj->ints[14] : -1;
			lo.limitRect = obj->rects.size() > 2 ? obj->rects[2] : Common::Rect();
			lo.vx = obj->ints.size() > 12 ? obj->ints[12] : 0;
			lo.vy = obj->ints.size() > 13 ? obj->ints[13] : 0;
			// Region scripts start with the flags of their c field: 0x10 counts
			// as already entered, 0x20 as already left
			for (uint sidx = 0; sidx < obj->scripts.size() && sidx < 32; sidx++) {
				const ScriptObject *sc = obj->scripts[sidx];
				if (sc->a == 7 && (sc->c & 0x10))
					lo.regionIn |= 1u << sidx;
				if (sc->a == 8 && (sc->c & 0x20))
					lo.regionOut |= 1u << sidx;
			}
			// Object flag 0x20 activates and starts the sprite when its panel
			// opens (state bits 1 and 4); whether it shows is bit 0x10 alone.
			if (obj->flags & 0x20)
				lo.spriteState |= 5;
			lo.visible = (lo.spriteState & 0x10) != 0 && obj->c != 0;
			lo.playing = (lo.spriteFlags & 8) != 0 && lo.frameDelay > 0;
			lo.spriteStartTime = g_system->getMillis();
			// The start point (record points[1]) is where activation puts the sprite
			lo.origin = obj->points.size() > 1 ? obj->points[1] : Common::Point(obj->rect.left, obj->rect.top);
			if ((lo.rect.width() <= 0 || lo.rect.height() <= 0) && obj->points.size() > 1) {
				lo.rect.left = lp->rect.left + obj->points[1].x;
				lo.rect.top = lp->rect.top + obj->points[1].y;
				lo.rect.right = lo.rect.left;
				lo.rect.bottom = lo.rect.top;
			}
			lo.nextFrameTime = g_system->getMillis() + MAX(25, lo.frameDelay);
			if (!obj->file.empty()) {
				lo.image = res.loadImage(lp->dir, frameName(obj, lo.frame));
				if (!lo.image)
					lo.image = res.loadImage(lp->dir, obj->file);
			}
		} else if (obj->cls == kObjAmbientAnimation) {
			lo.frameCount = obj->strs.size();
			lo.frame = 1;
			lo.image = res.loadImage(lp->dir, frameName(obj, 1));
			lo.nextFrameTime = g_system->getMillis() + 200;
		} else if (!obj->file.empty() && (obj->isBitmap() || obj->cls == kObjButton || obj->cls == kObjToggleButton ||
				obj->cls == kObjQuestionOKButton || obj->cls == kObjCollageButton || obj->cls == kObjNavRollOverButton ||
				obj->cls == kObjHighlightingCastle || obj->cls == kObjCollectBitmap || obj->cls == kObjScrollBitmap)) {
			lo.image = res.loadImage(lp->dir, obj->file);
		}

		if ((obj->cls == kObjToggleButton || obj->cls == kObjButton) && obj->strs.size() > 1 && !obj->strs[1].empty())
			lo.altImage = res.loadImage(lp->dir, obj->strs[1]);
		if (obj->cls == kObjCollectBitmap && obj->strs.size() > 1)
			lo.altImage = res.loadImage(lp->dir, obj->strs[1]);
		if (obj->cls == kObjToggleButton)
			lo.value = 1;
		if (obj->cls == kObjHighlightingCastle)
			lo.visible = false;
		if (lo.image) {
			// Objects without a stored rectangle take the image size
			if (lo.rect.width() <= 0 || lo.rect.height() <= 0) {
				lo.rect.right = lo.rect.left + lo.image->surface.w;
				lo.rect.bottom = lo.rect.top + lo.image->surface.h;
			}
			// The page palette comes from its largest full-palette image
			// (a PaletteBitmap object overrides that below)
			if (lo.image->palette.size() > 0 && (!_paletteImage ||
					(_paletteImage->palette.size() < 256 && lo.image->palette.size() > _paletteImage->palette.size()) ||
					(lo.image->palette.size() >= 256 && _paletteImage->palette.size() >= 256 && !_paletteFixed &&
					 lo.image->surface.w * lo.image->surface.h > _paletteImage->surface.w * _paletteImage->surface.h)))
				_paletteImage = lo.image;
			if (obj->cls == kObjPaletteBitmap && lo.image->palette.size() >= 256) {
				_paletteImage = lo.image;
				_paletteFixed = true;
			}
		}
		lp->objects.push_back(lo);
		if (!(obj->flags & kObjFlagCopyRect)) {
			lp->contentSize.x = MAX<int16>(lp->contentSize.x, obj->rect.right);
			lp->contentSize.y = MAX<int16>(lp->contentSize.y, obj->rect.bottom);
		}
	}
}

LivePanel *LivePage::addPanel(Panel *panel, const Common::Rect &rect, const Common::String &dir, Resources &res) {
	LivePanel *lp = new LivePanel();
	lp->panel = panel;
	lp->page = this;
	lp->rect = rect;
	lp->dir = dir;
	lp->scope.init(panel->ext);
	layoutObjects(lp, res);

	// Panels without a size (popups) grow to fit their contents
	if (lp->rect.width() <= 0 || lp->rect.height() <= 0) {
		Common::Rect bounds;
		bool first = true;
		for (uint i = 0; i < lp->objects.size(); i++) {
			const LiveObject &lo = lp->objects[i];
			if (lo.rect.width() <= 0 || lo.rect.height() <= 0)
				continue;
			if (first) {
				bounds = lo.rect;
				first = false;
			} else {
				bounds.extend(lo.rect);
			}
		}
		if (!first) {
			lp->rect.right = bounds.right;
			lp->rect.bottom = bounds.bottom;
		}
	}
	_panels.push_back(lp);
	return lp;
}

bool LivePage::open(Database &db, Resources &res, uint index, const Common::Point &origin) {
	_index = index;
	_rec = db.getRecord(index);
	if (!_rec) {
		warning("DKPenge: page %u does not exist", index);
		return false;
	}
	_origin = origin;
	_dir = _rec->dir;
	_scope.init(_rec->ext);
	_tmpl = _rec->isPage ? db.findTemplate(_rec->id) : nullptr;
	if (_rec->isPage && !_tmpl)
		debugC(1, kDebugGeneral, "DKPenge: page %u (type %d) has no template with id %d", index, _rec->type, _rec->id);

	Common::Point base = origin;
	if (_rec->isPage && (_rec->type == kPagePopup || _rec->type == kPageDragPopup || _rec->type == kPageRolloffClose))
		base += _rec->pos;

	// Panels stored with the record, placed according to the template
	for (uint i = 0; i < _rec->panels.size(); i++) {
		Panel *p = _rec->panels[i];
		Common::Rect rect(base.x, base.y, base.x, base.y);
		if (_tmpl) {
			for (uint k = 0; k < _tmpl->panels.size(); k++) {
				const PanelDesc &d = _tmpl->panels[k];
				if (d.id == p->id) {
					rect = Common::Rect(base.x + d.pos.x, base.y + d.pos.y, base.x + d.pos.x + d.size.x, base.y + d.pos.y + d.size.y);
					break;
				}
			}
		}
		addPanel(p, rect, _dir, res);
	}

	// Shared panels referenced by the template (navigation bars etc.)
	if (_tmpl) {
		for (uint k = 0; k < _tmpl->panels.size(); k++) {
			const PanelDesc &d = _tmpl->panels[k];
			if (!d.inStream)
				continue;
			bool present = false;
			for (uint i = 0; i < _rec->panels.size(); i++)
				if (_rec->panels[i]->id == d.id)
					present = true;
			if (present)
				continue;
			PageRecord *prec = db.getRecord(d.index);
			if (!prec || prec->isPage || prec->panels.empty())
				continue;
			Common::Rect rect(base.x + d.pos.x, base.y + d.pos.y, base.x + d.pos.x + d.size.x, base.y + d.pos.y + d.size.y);
			addPanel(prec->panels[0], rect, _dir, res);
		}
	}

	bool first = true;
	for (uint i = 0; i < _panels.size(); i++) {
		if (first) {
			_bounds = _panels[i]->rect;
			first = false;
		} else {
			_bounds.extend(_panels[i]->rect);
		}
	}
	// A popup without a position of its own (no record position, no panel
	// placement from its template) opens in the middle of the screen, as the
	// original's window creation centres it in its parent; the questions,
	// save prompts, zoom captions, glossary and help pages are such pages
	if (isPopup() && origin == Common::Point(0, 0) && _rec->pos == Common::Point(0, 0) && !_bounds.isEmpty() &&
			((_tmpl && _tmpl->centred) || (_bounds.left == 0 && _bounds.top == 0 && !(_tmpl && _tmpl->hasPos4c))))
		moveBy((640 - _bounds.width()) / 2 - _bounds.left, (480 - _bounds.height()) / 2 - _bounds.top);
	createCollages(db, res);
	debugC(1, kDebugGeneral, "DKPenge: opened page %u type %d template %d dir '%s' panels %u bounds %d,%d,%d,%d",
	       index, _rec->type, _rec->id, _dir.c_str(), _panels.size(), _bounds.left, _bounds.top, _bounds.right, _bounds.bottom);
	// Edit boxes draw with their text style (size and alignment)
	for (uint pi = 0; pi < _panels.size(); pi++)
		for (uint k = 0; k < _panels[pi]->objects.size(); k++) {
			LiveObject &lo = _panels[pi]->objects[k];
			if ((lo.obj->cls == kObjEditBox || lo.obj->cls == kObjRoomEditBox || lo.obj->cls == kObjScrollEditBox) && lo.obj->ints.size() > 1)
				lo.style = db.findStyle(lo.obj->ints[1]);
		}
	return true;
}

static void blitImage(Graphics::Surface &screen, const Image *img, const Common::Rect &dst, const Common::Rect &clipTo, int keyIndex = -1, bool dither = false) {
	Common::Rect r = dst;
	Common::Rect clip(0, 0, screen.w, screen.h);
	clip.clip(clipTo);
	r.clip(clip);
	if (r.isEmpty())
		return;
	int sx0 = r.left - dst.left;
	int sy0 = r.top - dst.top;
	for (int y = 0; y < r.height(); y++) {
		int sy = sy0 + y;
		if (sy >= img->surface.h)
			break;
		const byte *src = (const byte *)img->surface.getBasePtr(sx0, sy);
		byte *d = (byte *)screen.getBasePtr(r.left, r.top + y);
		int w = MIN((int)r.width(), (int)img->surface.w - sx0);
		if (dither) {
			// Every other pixel in a checkerboard, as the original's dither brush
			for (int x = ((r.left + r.top + y) & 1); x < w; x += 2)
				if (!(img->hasMask && img->mask[src[x]]) && !(img->hasTransparentColor && src[x] == img->transparentColor) && src[x] != keyIndex)
					d[x] = src[x];
		} else if (img->hasMask) {
			// Masked images may also use the key colour (book pages)
			for (int x = 0; x < w; x++)
				if (!img->mask[src[x]] && src[x] != keyIndex)
					d[x] = src[x];
		} else if (img->hasTransparentColor) {
			for (int x = 0; x < w; x++)
				if (src[x] != img->transparentColor && src[x] != keyIndex)
					d[x] = src[x];
		} else if (keyIndex >= 0) {
			for (int x = 0; x < w; x++)
				if (src[x] != keyIndex)
					d[x] = src[x];
		} else {
			memcpy(d, src, w);
		}
	}
}

// Brightens the screen pixels where the section mask has the given value
static void highlightSection(Graphics::Surface &screen, const Image *mask, const Common::Rect &dst, int section, Resources &res) {
	Common::Rect r = dst;
	r.clip(Common::Rect(0, 0, screen.w, screen.h));
	if (r.isEmpty() || section <= 0)
		return;
	const byte *remap = res.getHighlightTable();
	int sx0 = r.left - dst.left, sy0 = r.top - dst.top;
	for (int y = 0; y < r.height(); y++) {
		if (sy0 + y >= mask->surface.h)
			break;
		const byte *m = (const byte *)mask->surface.getBasePtr(sx0, sy0 + y);
		byte *d = (byte *)screen.getBasePtr(r.left, r.top + y);
		int w = MIN((int)r.width(), (int)mask->surface.w - sx0);
		for (int x = 0; x < w; x++)
			if (m[x] == section)
				d[x] = remap[d[x]];
	}
}

// The list of an Index or Trail popup: one row per entry from the first
// visible one, the selected entry on a highlight bar (Index) or with its
// highlighted icon (Trail)
static void drawCollage(Graphics::Surface &screen, Resources &res, const LiveObject &lo, const LivePanel &lp) {
	const Collage &c = *lo.collage;
	const Graphics::Font *font = res.getTextFont();
	if (!font)
		return;
	Common::Rect clip = lo.rect;
	clip.clip(lp.rect);
	clip.clip(Common::Rect(0, 0, screen.w, screen.h));
	if (clip.isEmpty())
		return;
	byte fg = res.findPaletteColor(c.fg[0], c.fg[1], c.fg[2]);
	byte hiBg = res.findPaletteColor(c.hiBg[0], c.hiBg[1], c.hiBg[2]);
	byte hiFg = res.findPaletteColor(c.hiFg[0], c.hiFg[1], c.hiFg[2]);
	for (int row = 0; row < c.pageSize; row++) {
		int item = c.scrollTop + row;
		if (item >= c.count())
			break;
		const CollageItem &it = c.items[item];
		int y = lo.rect.top + row * c.itemHeight;
		bool sel = item == c.selected;
		if (c.tracker) {
			const Common::Array<Common::String> &names = sel ? c.iconsHi : c.icons;
			int idx = it.icon - 1;
			if (idx >= 0 && idx < (int)names.size()) {
				Image *img = res.loadImage(lp.dir, names[idx]);
				if (img)
					blitImage(screen, img, Common::Rect(lo.rect.left, y, lo.rect.left + img->surface.w, y + img->surface.h), clip);
			}
			font->drawString(&screen, it.text, lo.rect.left + 40, y + 9, lo.rect.width() - 40, sel ? hiFg : fg);
		} else {
			if (sel) {
				Common::Rect bar(lo.rect.left, y, lo.rect.right, y + c.itemHeight);
				bar.clip(clip);
				if (!bar.isEmpty())
					screen.fillRect(bar, hiBg);
			}
			int x = lo.rect.left + (it.sub ? c.indent : 0);
			font->drawString(&screen, it.text, x, y, lo.rect.right - x, sel ? hiFg : fg);
		}
	}
}

// The scroll bar of the Index popup: arrow buttons at both ends (pressed
// artwork while held), the bar between them and the coin as the thumb
static void drawScrollBar(Graphics::Surface &screen, Resources &res, const LiveObject &lo, const LivePanel &lp) {
	const GameObject *o = lo.obj;
	if (o->strs.size() < 6)
		return;
	int pos = 0, maxPos = 0, pageSize = 1;
	if (lp.page)
		lp.page->getScrollState(pos, maxPos, pageSize);
	Common::Rect clip = lp.rect;
	clip.clip(Common::Rect(0, 0, screen.w, screen.h));
	Image *up = res.loadImage(lp.dir, o->strs[lo.value == 1 ? 4 : 0]);
	Image *down = res.loadImage(lp.dir, o->strs[lo.value == 2 ? 5 : 1]);
	Image *coin = res.loadImage(lp.dir, o->strs[2]);
	Image *bar = res.loadImage(lp.dir, o->strs[3]);
	int upH = up ? up->surface.h : 0, downH = down ? down->surface.h : 0;
	int barTop = lo.rect.top + upH, barBottom = lo.rect.bottom - downH;
	if (bar) {
		int x = lo.rect.left + (lo.rect.width() - bar->surface.w) / 2;
		blitImage(screen, bar, Common::Rect(x, barTop, x + bar->surface.w, barTop + bar->surface.h), clip);
	}
	if (coin) {
		int travel = barBottom - barTop - coin->surface.h;
		int y = (maxPos > 0 && travel > 0) ? travel * pos / maxPos : 0;
		int x = lo.rect.left + (lo.rect.width() - coin->surface.w) / 2;
		blitImage(screen, coin, Common::Rect(x, barTop + y, x + coin->surface.w, barTop + y + coin->surface.h), clip);
	}
	if (up)
		blitImage(screen, up, Common::Rect(lo.rect.left, lo.rect.top, lo.rect.left + up->surface.w, lo.rect.top + upH), clip);
	if (down)
		blitImage(screen, down, Common::Rect(lo.rect.left, barBottom, lo.rect.left + down->surface.w, barBottom + downH), clip);
}

void LivePage::draw(Graphics::Surface &screen, Resources &res) const {
	for (uint i = 0; i < _panels.size(); i++) {
		const LivePanel *lp = _panels[i];
		// Draw in Z order (stable for equal Z)
		Common::Array<const LiveObject *> order;
		for (uint k = 0; k < lp->objects.size(); k++)
			order.push_back(&lp->objects[k]);
		for (uint a = 1; a < order.size(); a++) {
			const LiveObject *x = order[a];
			int b = a;
			while (b > 0 && order[b - 1]->zOrder > x->zOrder) {
				order[b] = order[b - 1];
				b--;
			}
			order[b] = x;
		}
		for (uint k = 0; k < order.size(); k++) {
			const LiveObject &lo = *order[k];
			debugC(4, kDebugGraphics, "DKPenge: draw %s %d visible=%d image=%p z=%d rect=%d,%d,%d,%d", objectClassName(lo.obj->cls), lo.obj->id, lo.visible ? 1 : 0, (const void *)lo.image, lo.zOrder, lo.rect.left, lo.rect.top, lo.rect.right, lo.rect.bottom);
			if (lo.obj->cls == kObjEditBox || lo.obj->cls == kObjRoomEditBox || lo.obj->cls == kObjScrollEditBox) {
				// The typed text, vertically centred in the box
				const Graphics::Font *font = nullptr;
				Common::Rect r = lo.rect;
				r.clip(lp->rect);
				bool centred = false;
				if (lo.visible && !lo.text.empty()) {
					// The style's size is in tenths of a point; the box must fit it
					int px = 16;
					if (lo.style && lo.style->size > 0)
						px = CLIP<int>(lo.style->size * 96 / 720, 12, MAX<int>(12, r.height() - 2));
					centred = lo.style && lo.style->flags[0] != 0;
					font = res.getTextFont(px);
				}
				if (font) {
					int y = r.top + (r.height() - font->getFontHeight()) / 2;
					if (lo.selStart >= 0 && lo.selStart < (int)lo.text.size()) {
						// The auto-completed part of an index entry is selected
						int pw = font->getStringWidth(lo.text.substr(0, lo.selStart));
						int fw = font->getStringWidth(lo.text);
						Common::Rect selr(r.left + 2 + pw, y, MIN(r.left + 2 + fw, (int)r.right - 2), y + font->getFontHeight());
						selr.clip(r);
						if (!selr.isEmpty())
							screen.fillRect(selr, res.findPaletteColor(0x85, 0x86, 0xb2));
					}
					// Misspelt chest scroll answers are shown in red
					byte colour = lo.value == 1 ? res.findPaletteColor(0xc9, 0x0a, 0x0a) : res.findPaletteColor(0, 0, 0);
					font->drawString(&screen, lo.text, r.left + 2, y, r.width() - 4, colour, centred ? Graphics::kTextAlignCenter : Graphics::kTextAlignLeft);
				}
				continue;
			}
			if (lo.obj->cls == kObjCollage && lo.collage) {
				if (lo.visible)
					drawCollage(screen, res, lo, *lp);
				continue;
			}
			if (lo.obj->cls == kObjScrollBar) {
				if (lo.visible)
					drawScrollBar(screen, res, lo, *lp);
				continue;
			}
			if (lo.obj->cls == kObjScrollBitmap) {
				// The help text, scrolled by whole lines through its window
				if (lo.visible && lo.image) {
					int step = lo.obj->ints.size() > 2 && lo.obj->ints[2] > 0 ? lo.obj->ints[2] : 1;
					Common::Rect clip = lo.rect;
					clip.clip(lp->rect);
					int top = lo.rect.top - lo.value * step;
					blitImage(screen, lo.image, Common::Rect(lo.rect.left, top, lo.rect.left + lo.image->surface.w, top + lo.image->surface.h), clip);
				}
				continue;
			}
			// Sprites show only while active (state bit 1), as the original's draw routine
			if (lo.obj->cls == kObjSprite && !(lo.spriteState & 1))
				continue;
			if (!lo.visible || (!lo.image && lo.obj->cls != kObjZoomCaption))
				continue;
			// Ambient animations of other room nodes are switched off
			if (lo.obj->cls == kObjAmbientAnimation && lo.disabled)
				continue;
			Common::Rect r = lo.rect;
			if (!(lo.obj->flags & kObjFlagCopyRect))
				r.translate(-lp->scroll.x, -lp->scroll.y);
			if (lo.obj->cls == kObjZoomCaption) {
				// The caption shows the artwork painted by PaintZoomArea/Object
				if (lo.overlayA)
					blitImage(screen, lo.overlayA, Common::Rect(r.left, r.top, r.left + lo.overlayA->surface.w, r.top + lo.overlayA->surface.h), lp->rect);
				if (lo.overlayB)
					blitImage(screen, lo.overlayB, Common::Rect(r.left, r.top, r.left + lo.overlayB->surface.w, r.top + lo.overlayB->surface.h), lp->rect);
				continue;
			}
			if (lo.obj->cls == kObjHighlightingCastle) {
				highlightSection(screen, lo.image, r, lo.value, res);
				continue;
			}
			if (lo.obj->cls == kObjNavRollOverButton) {
				// The highlighted artwork only shows while the mouse is over the button
				if (lo.hovered)
					blitImage(screen, lo.image, r, lp->rect, lo.image->keyIndex);
				continue;
			}
			// Sprites, masks and the artwork of transparent panels are keyed
			// on pure green; plain bitmaps of opaque panels are copied as is.
			int key = -1;
			int pt = lp->panel->type;
			bool transparentPanel = pt == kPanelTransparent || pt == kPanelTransparentSprite || pt == kPanelTransparentText || pt == kPanelSpyChest;
			const Image *img = lo.image;
			// Toggle buttons show their off artwork when cleared, blinking
			// evidence alternates between its two pictures
			if (lo.obj->cls == kObjToggleButton && !lo.value && lo.altImage)
				img = lo.altImage;
			// Plain buttons show their second artwork while held (FUN_00482390)
			if (lo.obj->cls == kObjButton && lo.pressed && lo.altImage)
				img = lo.altImage;
			if (lo.obj->cls == kObjCollectBitmap && (lo.frame & 1) && lo.altImage)
				img = lo.altImage;
			if (lo.obj->cls != kObjBitmap || transparentPanel)
				key = img->keyIndex;
			blitImage(screen, img, r, lp->rect, key, lo.dithered);
		}
		for (uint k = 0; k < lp->overlays.size(); k++) {
			const Overlay &ov = lp->overlays[k];
			if (ov.image)
				blitImage(screen, ov.image, Common::Rect(ov.pos.x, ov.pos.y, ov.pos.x + ov.image->surface.w, ov.pos.y + ov.image->surface.h), lp->rect);
		}
	}
}

void LivePage::moveBy(int dx, int dy) {
	_bounds.translate(dx, dy);
	_origin += Common::Point(dx, dy);
	for (uint i = 0; i < _panels.size(); i++) {
		LivePanel *lp = _panels[i];
		lp->rect.translate(dx, dy);
		for (uint k = 0; k < lp->objects.size(); k++)
			lp->objects[k].rect.translate(dx, dy);
		for (uint k = 0; k < lp->overlays.size(); k++)
			lp->overlays[k].pos += Common::Point(dx, dy);
	}
}

bool LivePage::scrollBy(int dx, int dy) {
	bool moved = false;
	for (uint i = 0; i < _panels.size(); i++) {
		LivePanel *lp = _panels[i];
		if (lp->panel->type != kPanelZoomSprite && lp->panel->type != kPanelScroll)
			continue;
		Common::Point np(lp->scroll.x + dx, lp->scroll.y + dy);
		np.x = CLIP<int16>(np.x, 0, MAX<int16>(0, lp->contentSize.x - lp->rect.width()));
		np.y = CLIP<int16>(np.y, 0, MAX<int16>(0, lp->contentSize.y - lp->rect.height()));
		if (np != lp->scroll) {
			lp->scroll = np;
			moved = true;
		}
	}
	return moved;
}

void LivePage::setScroll(const Common::Point &p) {
	for (uint i = 0; i < _panels.size(); i++)
		if (_panels[i]->panel->type == kPanelZoomSprite || _panels[i]->panel->type == kPanelScroll)
			_panels[i]->scroll = p;
}

LiveObject *LivePage::findObjectOfClass(int cls) {
	for (uint i = 0; i < _panels.size(); i++)
		for (uint k = 0; k < _panels[i]->objects.size(); k++)
			if (_panels[i]->objects[k].obj->cls == cls)
				return &_panels[i]->objects[k];
	return nullptr;
}

LiveObject *LivePage::findCollage() {
	for (uint i = 0; i < _panels.size(); i++)
		for (uint k = 0; k < _panels[i]->objects.size(); k++)
			if (_panels[i]->objects[k].collage)
				return &_panels[i]->objects[k];
	return nullptr;
}

Common::Point LivePage::getScroll() const {
	for (uint i = 0; i < _panels.size(); i++)
		if (_panels[i]->panel->type == kPanelZoomSprite || _panels[i]->panel->type == kPanelScroll)
			return _panels[i]->scroll;
	return Common::Point(0, 0);
}

// A ScrollBitmap (the help texts) scrolls a tall bitmap through its
// rectangle by lines of a fixed height; ints[2] is the line height
static int scrollBitmapMax(const LiveObject &lo) {
	int step = lo.obj->ints.size() > 2 && lo.obj->ints[2] > 0 ? lo.obj->ints[2] : 1;
	int extra = lo.image ? lo.image->surface.h - lo.rect.height() : 0;
	return MAX(0, extra / step);
}

bool LivePage::getScrollState(int &pos, int &maxPos, int &pageSize) const {
	for (uint i = 0; i < _panels.size(); i++)
		for (uint k = 0; k < _panels[i]->objects.size(); k++) {
			const LiveObject &lo = _panels[i]->objects[k];
			if (lo.collage) {
				pos = lo.collage->scrollTop;
				maxPos = lo.collage->maxScroll();
				pageSize = lo.collage->pageSize;
				return true;
			}
			if (lo.obj->cls == kObjScrollBitmap) {
				int step = lo.obj->ints.size() > 2 && lo.obj->ints[2] > 0 ? lo.obj->ints[2] : 1;
				pos = lo.value;
				maxPos = scrollBitmapMax(lo);
				pageSize = MAX(1, lo.rect.height() / step);
				return true;
			}
		}
	return false;
}

void LivePage::setScrollPos(int pos) {
	for (uint i = 0; i < _panels.size(); i++)
		for (uint k = 0; k < _panels[i]->objects.size(); k++) {
			LiveObject &lo = _panels[i]->objects[k];
			if (lo.collage) {
				lo.collage->scrollTo(pos);
				return;
			}
			if (lo.obj->cls == kObjScrollBitmap) {
				lo.value = CLIP(pos, 0, scrollBitmapMax(lo));
				return;
			}
		}
}

// Builds the lists of the Collage objects. The Index entries come from the
// object record; the Trail entries are filled in by the engine from its
// navigation history.
void LivePage::createCollages(Database &db, Resources &res) {
	const Graphics::Font *font = res.getTextFont();
	int lineHeight = font ? font->getFontHeight() : 16;
	for (uint i = 0; i < _panels.size(); i++) {
		LivePanel *lp = _panels[i];
		for (uint k = 0; k < lp->objects.size(); k++) {
			LiveObject &lo = lp->objects[k];
			const GameObject *obj = lo.obj;
			if (obj->cls != kObjCollage)
				continue;
			Common::String cls = obj->collageA;
			cls.toLowercase();
			Collage *c = new Collage();
			lo.collage = c;
			const TextStyle *style = obj->ints.size() > 3 ? db.findStyle(obj->ints[3]) : nullptr;
			if (style) {
				memcpy(c->fg, style->rgb[1], 3);
				memcpy(c->hiBg, style->rgb[2], 3);
				memcpy(c->hiFg, style->rgb[3], 3);
			}
			if (cls == "tracker") {
				c->tracker = true;
				c->itemHeight = MAX(lineHeight, 42);
				// strs: prefix, 34 icons, 34 highlighted icons
				for (uint n = 1; n < obj->strs.size() && n <= 34; n++)
					c->icons.push_back(obj->strs[n]);
				for (uint n = 35; n < obj->strs.size(); n++)
					c->iconsHi.push_back(obj->strs[n]);
			} else {
				c->itemHeight = lineHeight + 1;
				c->indent = obj->u32s.size() > 1 ? (int)obj->u32s[1] : 23;
				// Entries: name, section, page, popup, point
				uint n = obj->strs.size() / 2;
				for (uint e = 0; e < n; e++) {
					CollageItem it;
					const Common::String &name = obj->strs[e * 2];
					const Common::String &sub = obj->strs[e * 2 + 1];
					it.sub = !sub.empty();
					it.text = it.sub ? sub : name;
					it.full = it.sub ? name + ", " + sub : name;
					if (e * 2 + 4 < obj->u32s.size()) {
						it.page = obj->u32s[e * 2 + 3];
						it.popup = obj->u32s[e * 2 + 4];
					}
					if (e < obj->points.size())
						it.pt = obj->points[e];
					it.header = it.page == 0xffffffff && it.popup == 0xffffffff;
					c->items.push_back(it);
				}
			}
			c->pageSize = MAX(1, lo.rect.height() / c->itemHeight);
			debugC(2, kDebugGraphics, "DKPenge: collage %s with %d items, %d rows of %d px", obj->collageA.c_str(), c->count(), c->pageSize, c->itemHeight);
		}
	}
}

LiveObject *LivePage::findObject(int id) {
	for (uint i = 0; i < _panels.size(); i++)
		for (uint k = 0; k < _panels[i]->objects.size(); k++)
			if (_panels[i]->objects[k].obj->id == id)
				return &_panels[i]->objects[k];
	return nullptr;
}

LiveObject *LivePage::objectAt(const Common::Point &p, bool hotspotsOnly) {
	for (int i = (int)_panels.size() - 1; i >= 0; i--) {
		LivePanel *lp = _panels[i];
		for (int k = (int)lp->objects.size() - 1; k >= 0; k--) {
			LiveObject &lo = lp->objects[k];
			// The castle of the Castle Guide is hit whether or not a section
			// of it is lit (visible only concerns the highlight), and a
			// cutaway cover (DitherBitmap) can be clicked back into place
			if (!lo.visible && lo.obj->cls != kObjHighlightingCastle && lo.obj->cls != kObjDitherBitmap)
				continue;
			if (hotspotsOnly && lo.disabled)
				continue;
			bool builtinClick = lo.obj->cls == kObjCoinBitmap || lo.obj->cls == kObjHighlightingCastle || lo.obj->cls == kObjDitherBitmap || lo.obj->cls == kObjCollectBitmap || lo.obj->cls == kObjToggleButton ||
				lo.obj->cls == kObjQuestionOKButton || lo.obj->cls == kObjRandomMapBitmap ||
				lo.obj->cls == kObjCollage || lo.obj->cls == kObjScrollBar || lo.obj->cls == kObjCollageButton ||
				lo.obj->cls == kObjEditBox || lo.obj->cls == kObjRoomEditBox || lo.obj->cls == kObjScrollEditBox || lo.obj->cls == kObjPageTurn ||
				(lo.obj->cls == kObjRandomScenarioHotspot && lo.obj->ints.size() > 3 && lo.obj->ints[2] == 0);
			if (hotspotsOnly && !builtinClick && !lo.obj->findEvent(kEventClick)) {
				bool clickScript = false;
				for (uint s = 0; s < lo.obj->scripts.size(); s++)
					if (lo.obj->scripts[s]->a == 5)
						clickScript = true;
				if (!clickScript)
					continue;
			}
			Common::Point q = p;
			if (!(lo.obj->flags & kObjFlagCopyRect))
				q += lp->scroll;
			if (!lo.rect.contains(q))
				continue;
			// Irregular hotspots carry a scanline mask within their rectangle
			if (lo.obj->mask && !lo.obj->mask->contains(q))
				continue;
			// Sprites are hit only while active (state bits 1 and 2) and when
			// their hit flag (0x80) is set; flag 1 asks for a pixel precise test
			if (lo.obj->cls == kObjSprite && ((lo.spriteState & 3) != 3 || !(lo.spriteFlags & 0x80)))
				continue;
			if (((lo.obj->cls == kObjSprite && (lo.spriteFlags & 1)) || lo.obj->cls == kObjDitherBitmap) && lo.image && lo.image->keyIndex >= 0) {
				int px = q.x - lo.rect.left, py = q.y - lo.rect.top;
				if (px < lo.image->surface.w && py < lo.image->surface.h) {
					byte pix = *(const byte *)lo.image->surface.getBasePtr(px, py);
					if (pix == lo.image->keyIndex || (lo.image->hasMask && lo.image->mask[pix]))
						continue;
				}
			}
			return &lo;
		}
	}
	return nullptr;
}

LiveObject *LivePage::hitTest(const Common::Point &p) {
	return objectAt(p, true);
}

void LivePage::update(uint32 now, Resources &res) {
	for (uint i = 0; i < _panels.size(); i++) {
		LivePanel *lp = _panels[i];
		for (uint k = 0; k < lp->objects.size(); k++) {
			LiveObject &lo = lp->objects[k];
			if (lo.obj->cls == kObjCollectBitmap && lo.visible && lo.altImage && now >= lo.nextFrameTime) {
				lo.frame++;
				lo.nextFrameTime = now + 500;
				_changed = true;
			}
			if (lo.obj->cls == kObjAmbientAnimation && lo.frameCount > 1 && now >= lo.nextFrameTime) {
				lo.frame = lo.frame % lo.frameCount + 1;
				Image *img = res.loadImage(lp->dir, frameName(lo.obj, lo.frame));
				if (img)
					lo.image = img;
				lo.nextFrameTime = now + 200;
				_changed = true;
			}
		}
	}
}

} // End of namespace DKPenge
