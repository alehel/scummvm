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
#include "graphics/surface.h"

#include "castle/detection.h"
#include "castle/page.h"
#include "castle/resources.h"

namespace Castle {

LivePage::LivePage() : _index(0), _rec(nullptr), _tmpl(nullptr), _paletteImage(nullptr) {
}

LivePage::~LivePage() {
	for (uint i = 0; i < _panels.size(); i++)
		delete _panels[i];
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
		lo.rect = obj->rect;
		lo.rect.translate(lp->rect.left, lp->rect.top);

		lo.zOrder = obj->d;
		lo.visible = obj->c != 0;
		if (obj->cls == kObjSprite) {
			lo.frameCount = obj->spriteFrames;
			lo.frame = obj->ints.size() > 1 ? obj->ints[1] + 1 : 1;
			lo.frameDelay = obj->ints.size() > 2 ? MAX(25, (int)obj->ints[2]) : 100;
			for (int k = 0; k < 5; k++)
				lo.counters[k] = obj->ints.size() > 5 + (uint)k ? obj->ints[5 + k] : 0;
			lo.spriteFlags = obj->ints.size() > 3 ? obj->ints[3] : 0;
			lo.spriteState = obj->ints.size() > 10 ? obj->ints[10] : 0;
			lo.visible = (lo.spriteState & 0x10) != 0;
			lo.playing = (obj->flags & 0x20) != 0 && lo.frameCount > 0;
			if ((lo.rect.width() <= 0 || lo.rect.height() <= 0) && obj->points.size() > 1) {
				lo.rect.left = lp->rect.left + obj->points[1].x;
				lo.rect.top = lp->rect.top + obj->points[1].y;
				lo.rect.right = lo.rect.left;
				lo.rect.bottom = lo.rect.top;
			}
			lo.nextFrameTime = g_system->getMillis() + lo.frameDelay;
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
				obj->cls == kObjHighlightingCastle || obj->cls == kObjHatchBitmap || obj->cls == kObjCollectBitmap)) {
			lo.image = res.loadImage(lp->dir, obj->file);
		}

		if (obj->cls == kObjHighlightingCastle)
			lo.visible = false;
		if (lo.image) {
			// Objects without a stored rectangle take the image size
			if (lo.rect.width() <= 0 || lo.rect.height() <= 0) {
				lo.rect.right = lo.rect.left + lo.image->surface.w;
				lo.rect.bottom = lo.rect.top + lo.image->surface.h;
			}
			if (!_paletteImage && lo.image->palette.size() >= 256)
				_paletteImage = lo.image;
			if (obj->cls == kObjPaletteBitmap && lo.image->palette.size() >= 256)
				_paletteImage = lo.image;
		}
		lp->objects.push_back(lo);
	}
}

LivePanel *LivePage::addPanel(Panel *panel, const Common::Rect &rect, const Common::String &dir, Resources &res) {
	LivePanel *lp = new LivePanel();
	lp->panel = panel;
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
		warning("Castle: page %u does not exist", index);
		return false;
	}
	_origin = origin;
	_dir = _rec->dir;
	_scope.init(_rec->ext);
	_tmpl = _rec->isPage ? db.findTemplate(_rec->id) : nullptr;
	if (_rec->isPage && !_tmpl)
		debugC(1, kDebugGeneral, "Castle: page %u (type %d) has no template with id %d", index, _rec->type, _rec->id);

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
	debugC(1, kDebugGeneral, "Castle: opened page %u type %d template %d dir '%s' panels %u bounds %d,%d,%d,%d",
	       index, _rec->type, _rec->id, _dir.c_str(), _panels.size(), _bounds.left, _bounds.top, _bounds.right, _bounds.bottom);
	return true;
}

static void blitImage(Graphics::Surface &screen, const Image *img, const Common::Rect &dst, const Common::Rect &clipTo) {
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
		if (img->hasTransparentColor) {
			for (int x = 0; x < w; x++)
				if (src[x] != img->transparentColor)
					d[x] = src[x];
		} else {
			memcpy(d, src, w);
		}
	}
}

void LivePage::draw(Graphics::Surface &screen, Resources &res) const {
	for (uint i = 0; i < _panels.size(); i++) {
		const LivePanel *lp = _panels[i];
		for (uint k = 0; k < lp->objects.size(); k++) {
			const LiveObject &lo = lp->objects[k];
			if (!lo.visible || !lo.image)
				continue;
			Common::Rect r = lo.rect;
			r.translate(-lp->scroll.x, -lp->scroll.y);
			blitImage(screen, lo.image, r, lp->rect);
		}
		for (uint k = 0; k < lp->overlays.size(); k++) {
			const Overlay &ov = lp->overlays[k];
			if (ov.image)
				blitImage(screen, ov.image, Common::Rect(ov.pos.x, ov.pos.y, ov.pos.x + ov.image->surface.w, ov.pos.y + ov.image->surface.h), lp->rect);
		}
	}
}

void LivePage::setScroll(const Common::Point &p) {
	for (uint i = 0; i < _panels.size(); i++)
		if (_panels[i]->panel->type == kPanelZoomSprite || _panels[i]->panel->type == kPanelScroll)
			_panels[i]->scroll = p;
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
			if (!lo.visible)
				continue;
			if (hotspotsOnly && (lo.disabled || !lo.obj->findEvent(kEventClick)))
				continue;
			Common::Point q = p;
			q += lp->scroll;
			if (lo.rect.contains(q))
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
			if (lo.obj->cls == kObjAmbientAnimation && lo.frameCount > 1 && now >= lo.nextFrameTime) {
				lo.frame = lo.frame % lo.frameCount + 1;
				Image *img = res.loadImage(lp->dir, frameName(lo.obj, lo.frame));
				if (img)
					lo.image = img;
				lo.nextFrameTime = now + 200;
			}
		}
	}
}

} // End of namespace Castle
