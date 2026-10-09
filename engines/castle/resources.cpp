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

#include "common/compression/deflate.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/formats/winexe_pe.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "graphics/wincursor.h"
#include "image/bmp.h"
#include "image/png.h"

#include "castle/detection.h"
#include "castle/resources.h"

namespace Castle {

static const char *const kImageExtensions[] = { ".png", ".dib", ".bmp", ".sbm", nullptr };
static const char *const kWaveExtensions[] = { ".wav", nullptr };
static const char *const kAniExtensions[] = { ".ani", nullptr };
static const char *const kVideoExtensions[] = { ".mov", nullptr };

Resources::Resources() : _exe(nullptr) {
	for (int i = 0; i < 256; i++)
		_highlight[i] = i;
}

void Resources::buildHighlightTable(const byte *palette) {
	for (int i = 0; i < 256; i++) {
		int tr = MIN(255, palette[i * 3] + 80);
		int tg = MIN(255, palette[i * 3 + 1] + 80);
		int tb = MIN(255, palette[i * 3 + 2] + 40);
		int best = i, bestDist = 0x7fffffff;
		for (int k = 0; k < 256; k++) {
			int dr = palette[k * 3] - tr, dg = palette[k * 3 + 1] - tg, db = palette[k * 3 + 2] - tb;
			int dist = dr * dr + dg * dg + db * db;
			if (dist < bestDist) {
				bestDist = dist;
				best = k;
			}
		}
		_highlight[i] = best;
	}
}

Resources::~Resources() {
	for (Common::HashMap<Common::String, Image *>::iterator it = _imageCache.begin(); it != _imageCache.end(); ++it)
		delete it->_value;
	for (uint i = 0; i < _cursorGroups.size(); i++)
		delete _cursorGroups[i];
	for (uint i = 0; i < _ownedCursors.size(); i++)
		delete _ownedCursors[i];
	delete _exe;
}

bool Resources::init() {
	_exe = new Common::PEResources();
	if (!_exe->loadFromEXE(Common::Path("CASTLE.EXE"))) {
		warning("Castle: could not load resources from CASTLE.EXE");
		delete _exe;
		_exe = nullptr;
	}
	return true;
}

Common::String Resources::makePath(const Common::String &dir, const Common::String &name) {
	Common::String n = name;
	Common::String d = dir;
	if (n.hasPrefix("~"))
		n = n.substr(1);

	Common::String path;
	if (n.hasPrefix("\\")) {
		path = n.substr(1);
	} else if (n.hasPrefix("&")) {
		path = n.substr(1);
	} else if (n.contains('\\')) {
		// Names with a directory part are relative to the data root
		path = n;
	} else {
		if (d.hasPrefix("\\"))
			d = d.substr(1);
		if (!d.empty() && !d.hasSuffix("\\"))
			d += "\\";
		path = d + n;
	}
	// Convert Windows separators
	for (uint i = 0; i < path.size(); i++)
		if (path[i] == '\\')
			path.setChar('/', i);
	return path;
}

Common::SeekableReadStream *Resources::openFile(const Common::String &dir, const Common::String &name, const char *const *extensions) {
	Common::String path = makePath(dir, name);
	for (int i = 0; extensions[i]; i++) {
		Common::Path p(path + extensions[i], '/');
		Common::File *f = new Common::File();
		if (f->open(p))
			return f;
		delete f;
	}
	return nullptr;
}

Common::SeekableReadStream *Resources::openExeBitmap(const Common::String &name) {
	if (!_exe)
		return nullptr;
	Common::String id = name;
	id.toUppercase();
	Common::SeekableReadStream *res = _exe->getResource(Common::kWinBitmap, Common::WinResourceID(id));
	if (!res)
		return nullptr;
	// Resource bitmaps lack the BITMAPFILEHEADER; synthesize one so that the
	// regular BMP decoder can read them.
	uint32 size = res->size();
	byte *buf = (byte *)malloc(size + 14);
	res->read(buf + 14, size);
	delete res;
	uint32 headerSize = READ_LE_UINT32(buf + 14);
	uint16 bpp = READ_LE_UINT16(buf + 14 + 14);
	uint32 colors = READ_LE_UINT32(buf + 14 + 32);
	if (colors == 0 && bpp <= 8)
		colors = 1 << bpp;
	uint32 dataOffset = 14 + headerSize + colors * 4;
	buf[0] = 'B';
	buf[1] = 'M';
	WRITE_LE_UINT32(buf + 2, size + 14);
	WRITE_LE_UINT32(buf + 6, 0);
	WRITE_LE_UINT32(buf + 10, dataOffset);
	return new Common::MemoryReadStream(buf, size + 14, DisposeAfterUse::YES);
}

Common::SeekableReadStream *Resources::openExeWave(const Common::String &name) {
	if (!_exe)
		return nullptr;
	Common::String id = name;
	id.toUppercase();
	return _exe->getResource(Common::WinResourceID("WAVE"), Common::WinResourceID(id));
}

// Decodes an 8-bit Windows bitmap, including the RLE8 compressed ones that
// the generic decoder does not handle.
// The original draws rollover artwork with pure green as the colour key
static void findKeyIndex(Image *img) {
	img->keyIndex = -1;
	for (uint i = 0; i < img->palette.size(); i++) {
		byte r, g, b;
		img->palette.get(i, r, g, b);
		if (r == 0 && g == 255 && b == 0) {
			img->keyIndex = i;
			return;
		}
	}
}

// Decodes a BMP RLE8 stream into a rectangle of a surface. Rows are
// bottom-up unless topDown is set.
static void decodeRLE8(Common::SeekableReadStream *stream, Graphics::Surface &surf, int x0, int y0, int width, int height, bool topDown) {
	int x = 0, y = 0;
	while (!stream->eos()) {
		byte count = stream->readByte();
		byte value = stream->readByte();
		if (stream->eos())
			break;
		if (count > 0) {
			for (int i = 0; i < count; i++) {
				if (x < width && y < height) {
					int dy = topDown ? y : height - 1 - y;
					if (x0 + x < surf.w && y0 + dy < surf.h)
						*((byte *)surf.getBasePtr(x0 + x, y0 + dy)) = value;
				}
				x++;
			}
		} else if (value == 0) {
			x = 0;
			y++;
		} else if (value == 1) {
			break;
		} else if (value == 2) {
			x += stream->readByte();
			y += stream->readByte();
		} else {
			for (int i = 0; i < value; i++) {
				byte px = stream->readByte();
				if (x < width && y < height) {
					int dy = topDown ? y : height - 1 - y;
					if (x0 + x < surf.w && y0 + dy < surf.h)
						*((byte *)surf.getBasePtr(x0 + x, y0 + dy)) = px;
				}
				x++;
			}
			if (value & 1)
				stream->readByte();
		}
	}
}

// The segmented bitmaps (RIFF "SBMP" files) hold a large 8-bit image as
// RLE8-compressed 256x256 segments: "bmi " carries the BITMAPINFO,
// "indx" a table of (segment, file offset, packed position) entries and
// the numbered chunks the segments themselves.
static Image *decodeSegmentedBitmap(Common::SeekableReadStream *stream) {
	stream->seek(0);
	byte magic[4];
	stream->read(magic, 4);
	if (memcmp(magic, "RIFF", 4) != 0)
		return nullptr;
	uint32 riffSize = stream->readUint32LE();
	stream->read(magic, 4);
	if (memcmp(magic, "SBMP", 4) != 0)
		return nullptr;
	uint32 end = MIN<uint32>(riffSize + 8, stream->size());
	int32 width = 0, height = 0;
	Image *img = new Image();
	img->palette.resize(256, false);
	Common::Array<uint32> index;
	while ((uint32)stream->pos() + 8 <= end) {
		stream->read(magic, 4);
		uint32 size = stream->readUint32LE();
		uint32 next = stream->pos() + size + (size & 1);
		if (memcmp(magic, "bmi ", 4) == 0) {
			uint32 hdr = stream->readUint32LE();
			width = stream->readSint32LE();
			height = stream->readSint32LE();
			stream->readUint16LE();
			uint16 bpp = stream->readUint16LE();
			stream->skip(hdr - 16);
			if (bpp != 8 || width <= 0 || height <= 0) {
				delete img;
				return nullptr;
			}
			for (uint i = 0; i < 256; i++) {
				byte b = stream->readByte(), g = stream->readByte(), r = stream->readByte();
				stream->readByte();
				img->palette.set(i, r, g, b);
			}
		} else if (memcmp(magic, "indx", 4) == 0) {
			index.resize(size / 4);
			for (uint i = 0; i < index.size(); i++)
				index[i] = stream->readUint32LE();
		}
		stream->seek(next);
	}
	if (!width || index.empty()) {
		delete img;
		return nullptr;
	}
	findKeyIndex(img);
	img->surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	memset(img->surface.getPixels(), 0, img->surface.pitch * height);
	for (uint i = 0; i + 2 < index.size(); i += 3) {
		if (index[i] != i / 3)
			break;
		uint32 offset = index[i + 1];
		int sx = (index[i + 2] >> 8) & 0xffff;
		int sy = index[i + 2] >> 24;
		if (offset + 8 > end)
			break;
		stream->seek(offset + 4);
		uint32 size = stream->readUint32LE();
		Common::SeekableReadStream *seg = stream->readStream(size);
		decodeRLE8(seg, img->surface, sx * 256, sy * 256, MIN(256, width - sx * 256), MIN(256, height - sy * 256), false);
		delete seg;
	}
	return img;
}

// Decodes an 8-bit paletted, non-interlaced PNG keeping its indices; the
// generic decoder would expand images with several transparent palette
// entries to RGBA. Returns nullptr for anything else.
static Image *decodePalettedPNG(Common::SeekableReadStream *stream) {
	stream->seek(8);
	int32 width = 0, height = 0;
	byte bitDepth = 0, colorType = 0, interlace = 0;
	Common::Array<byte> idat;
	Image *img = new Image();
	bool gotHeader = false;
	while (stream->pos() + 8 <= stream->size()) {
		uint32 len = stream->readUint32BE();
		char tag[4];
		stream->read(tag, 4);
		if (stream->eos() || (uint32)stream->pos() + len > (uint32)stream->size())
			break;
		if (memcmp(tag, "IHDR", 4) == 0) {
			width = stream->readSint32BE();
			height = stream->readSint32BE();
			bitDepth = stream->readByte();
			colorType = stream->readByte();
			stream->readByte();
			stream->readByte();
			interlace = stream->readByte();
			gotHeader = true;
			if (bitDepth != 8 || colorType != 3 || interlace != 0 || width <= 0 || height <= 0) {
				delete img;
				return nullptr;
			}
		} else if (memcmp(tag, "PLTE", 4) == 0) {
			uint n = MIN<uint>(256, len / 3);
			img->palette.resize(256, false);
			for (uint i = 0; i < n; i++) {
				byte r = stream->readByte(), g = stream->readByte(), b = stream->readByte();
				img->palette.set(i, r, g, b);
			}
			stream->skip(len - n * 3);
		} else if (memcmp(tag, "tRNS", 4) == 0) {
			uint n = MIN<uint>(256, len);
			for (uint i = 0; i < n; i++)
				if (stream->readByte() == 0) {
					img->mask[i] = 1;
					img->hasMask = true;
				}
			stream->skip(len - n);
		} else if (memcmp(tag, "IDAT", 4) == 0) {
			uint old = idat.size();
			idat.resize(old + len);
			stream->read(&idat[old], len);
		} else if (memcmp(tag, "IEND", 4) == 0) {
			break;
		} else {
			stream->skip(len);
		}
		stream->readUint32BE(); // crc
	}
	if (!gotHeader || idat.empty()) {
		delete img;
		return nullptr;
	}
	uint32 stride = width + 1;
	unsigned long rawLen = stride * height;
	byte *raw = (byte *)malloc(rawLen);
	if (!Common::inflateZlib(raw, &rawLen, &idat[0], idat.size()) || rawLen < stride * height) {
		free(raw);
		delete img;
		return nullptr;
	}
	img->surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	byte *prev = nullptr;
	for (int y = 0; y < height; y++) {
		byte filter = raw[y * stride];
		byte *row = raw + y * stride + 1;
		for (int x = 0; x < width; x++) {
			int a = x > 0 ? row[x - 1] : 0;
			int b = prev ? prev[x] : 0;
			int c = (prev && x > 0) ? prev[x - 1] : 0;
			switch (filter) {
			case 1: row[x] += a; break;
			case 2: row[x] += b; break;
			case 3: row[x] += (a + b) / 2; break;
			case 4: {
				int pp = a + b - c, pa = ABS(pp - a), pb = ABS(pp - b), pc = ABS(pp - c);
				row[x] += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
				break;
			}
			default: break;
			}
		}
		memcpy(img->surface.getBasePtr(0, y), row, width);
		prev = row;
	}
	free(raw);
	// A single transparent index can use the plain colour-key path
	int count = 0, single = -1;
	for (int i = 0; i < 256; i++)
		if (img->mask[i]) {
			count++;
			single = i;
		}
	if (count == 1) {
		img->hasTransparentColor = true;
		img->transparentColor = single;
		img->hasMask = false;
	}
	findKeyIndex(img);
	return img;
}

static Image *decodeBitmap8(Common::SeekableReadStream *stream) {
	stream->seek(0);
	if (stream->readByte() != 'B' || stream->readByte() != 'M')
		return nullptr;
	stream->readUint32LE();
	stream->readUint32LE();
	uint32 dataOffset = stream->readUint32LE();
	uint32 headerSize = stream->readUint32LE();
	int32 width = stream->readSint32LE();
	int32 height = stream->readSint32LE();
	stream->readUint16LE();
	uint16 bpp = stream->readUint16LE();
	uint32 compression = stream->readUint32LE();
	stream->readUint32LE();
	stream->readUint32LE();
	stream->readUint32LE();
	uint32 colors = stream->readUint32LE();
	if (bpp != 8 || width <= 0 || height == 0) {
		debugC(1, kDebugGraphics, "Castle: bitmap bpp %d size %dx%d compression %u not handled here", bpp, width, height, compression);
		return nullptr;
	}
	debugC(2, kDebugGraphics, "Castle: bitmap %dx%d bpp %d compression %u colors %u", width, height, bpp, compression, colors);
	if (colors == 0 || colors > 256)
		colors = 256;
	bool topDown = height < 0;
	if (topDown)
		height = -height;
	stream->seek(14 + headerSize);
	Image *img = new Image();
	img->palette.resize(256, false);
	for (uint i = 0; i < colors; i++) {
		byte b = stream->readByte(), g = stream->readByte(), r = stream->readByte();
		stream->readByte();
		img->palette.set(i, r, g, b);
	}
	findKeyIndex(img);
	img->surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	memset(img->surface.getPixels(), 0, img->surface.pitch * height);
	stream->seek(dataOffset);
	if (compression == 0) {
		int stride = (width + 3) & ~3;
		byte *row = (byte *)malloc(stride);
		for (int y = 0; y < height; y++) {
			stream->read(row, stride);
			int dy = topDown ? y : height - 1 - y;
			memcpy(img->surface.getBasePtr(0, dy), row, width);
		}
		free(row);
	} else if (compression == 1) {
		decodeRLE8(stream, img->surface, 0, 0, width, height, topDown);
	} else {
		delete img;
		return nullptr;
	}
	return img;
}

Image *Resources::decodeImage(Common::SeekableReadStream *stream) {
	byte magic[4];
	stream->read(magic, 4);
	stream->seek(0);
	::Image::ImageDecoder *decoder;
	if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
		Image *pal = decodePalettedPNG(stream);
		if (pal)
			return pal;
		stream->seek(0);
		decoder = new ::Image::PNGDecoder();
	} else if (memcmp(magic, "RIFF", 4) == 0) {
		return decodeSegmentedBitmap(stream);
	} else {
		Image *bmp = decodeBitmap8(stream);
		if (bmp)
			return bmp;
		stream->seek(0);
		decoder = new ::Image::BitmapDecoder();
	}
	if (!decoder->loadStream(*stream)) {
		delete decoder;
		return nullptr;
	}
	Image *img = new Image();
	const Graphics::Surface *src = decoder->getSurface();
	img->surface.copyFrom(*src);
	img->palette = decoder->getPalette();
	img->hasTransparentColor = decoder->hasTransparentColor();
	img->transparentColor = decoder->getTransparentColor();
	findKeyIndex(img);
	debugC(3, kDebugGraphics, "Castle: decoded %dx%d palette %u transparent %d (%u)", img->surface.w, img->surface.h, img->palette.size(), img->hasTransparentColor ? 1 : 0, img->transparentColor);
	delete decoder;
	return img;
}

Image *Resources::loadImage(const Common::String &dir, const Common::String &name) {
	if (name.empty())
		return nullptr;
	Common::String key = name.hasPrefix("@") ? name : makePath(dir, name);
	key.toLowercase();
	debugC(3, kDebugGraphics, "Castle: loadImage '%s' key '%s' cached=%d", name.c_str(), key.c_str(), _imageCache.contains(key) ? 1 : 0);
	if (_imageCache.contains(key))
		return _imageCache[key];

	Common::SeekableReadStream *stream;
	if (name.hasPrefix("@"))
		stream = openExeBitmap(name.substr(1));
	else
		stream = openFile(dir, name, kImageExtensions);

	Image *img = nullptr;
	if (stream) {
		img = decodeImage(stream);
		delete stream;
	} else {
		debugC(1, kDebugGraphics, "Castle: no file for '%s'", key.c_str());
	}
	if (!img)
		debugC(1, kDebugGraphics, "Castle: image '%s' (dir '%s') not found or undecodable", name.c_str(), dir.c_str());
	if (img && ConfMan.hasKey("castle_dumpimage") && name.equalsIgnoreCase(ConfMan.get("castle_dumpimage"))) {
		Common::DumpFile f;
		if (f.open(Common::Path(ConfMan.get("castle_dump") + "/image.png", '/'))) {
			byte pal[768];
			img->palette.grab(pal, 0, 256);
			::Image::writePNG(f, img->surface, pal, 256);
		}
	}
	_imageCache[key] = img;
	return img;
}

// Cursor names registered by the original executable and their resource ids
// (ids >= 32512 are standard Windows cursors).
static const struct { const char *name; uint16 id; } kCursorTable[] = {
	{ "Arrow", 32512 }, { "Normal", 32512 }, { "Watch", 32514 }, { "Wait", 32514 },
	{ "Hand", 4000 }, { "Up", 4001 }, { "Down", 4002 }, { "Left", 4003 }, { "Right", 4004 },
	{ "UpLeft", 4005 }, { "UpRight", 4006 }, { "DownLeft", 4007 }, { "DownRight", 4008 },
	{ "Magnify", 4009 }, { "Magnifier", 4009 }, { "Demagnify", 4010 }, { "Point", 4014 },
	{ "Grab", 4013 }, { "Palm", 4012 }, { "3DForward", 126 }, { "3DRight", 128 }, { "3DLeft", 127 },
	{ "3DBack", 125 }, { "3DHand", 4000 }, { "3DDrag", 4013 }, { "3DUp", 4001 }, { "3DDown", 4002 },
	{ "PageTurn1", 134 }, { "PageTurn2", 133 }, { "PageTurn3", 132 }, { "IBeam", 32513 },
	{ nullptr, 0 }
};

Graphics::Cursor *Resources::getCursor(const Common::String &nameIn) {
	Common::String name = nameIn;
	name.toLowercase();
	if (name.empty())
		name = "arrow";
	if (_cursorCache.contains(name))
		return _cursorCache[name];
	uint16 id = 0;
	for (int i = 0; kCursorTable[i].name; i++) {
		Common::String n = kCursorTable[i].name;
		n.toLowercase();
		if (n == name) {
			id = kCursorTable[i].id;
			break;
		}
	}
	Graphics::Cursor *cursor = nullptr;
	if (id == 32514) {
		cursor = Graphics::makeBusyWinCursor();
		_ownedCursors.push_back(cursor);
	} else if (id >= 32512 || id == 0 || !_exe) {
		cursor = Graphics::makeDefaultWinCursor();
		_ownedCursors.push_back(cursor);
	} else {
		Graphics::WinCursorGroup *group = Graphics::WinCursorGroup::createCursorGroup(_exe, Common::WinResourceID(id));
		if (group && !group->cursors.empty()) {
			_cursorGroups.push_back(group);
			cursor = group->cursors[0].cursor;
		} else {
			delete group;
			debugC(1, kDebugGraphics, "Castle: cursor '%s' (id %d) not found", nameIn.c_str(), id);
			cursor = Graphics::makeDefaultWinCursor();
			_ownedCursors.push_back(cursor);
		}
	}
	_cursorCache[name] = cursor;
	return cursor;
}

Common::SeekableReadStream *Resources::openWave(const Common::String &dir, const Common::String &name) {
	if (name.hasPrefix("@"))
		return openExeWave(name.substr(1));
	return openFile(dir, name, kWaveExtensions);
}

Common::SeekableReadStream *Resources::openAnimation(const Common::String &dir, const Common::String &name) {
	return openFile(dir, name, kAniExtensions);
}

Common::SeekableReadStream *Resources::openVideo(const Common::String &dir, const Common::String &name) {
	return openFile(dir, name, kVideoExtensions);
}

} // End of namespace Castle
