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
#include "common/file.h"
#include "common/formats/winexe_pe.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "image/bmp.h"
#include "image/png.h"

#include "castle/detection.h"
#include "castle/resources.h"

namespace Castle {

static const char *const kImageExtensions[] = { ".png", ".dib", ".bmp", nullptr };
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
						*((byte *)img->surface.getBasePtr(x, dy)) = value;
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
						*((byte *)img->surface.getBasePtr(x, dy)) = px;
					}
					x++;
				}
				if (value & 1)
					stream->readByte();
			}
		}
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
		decoder = new ::Image::PNGDecoder();
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
	delete decoder;
	return img;
}

Image *Resources::loadImage(const Common::String &dir, const Common::String &name) {
	if (name.empty())
		return nullptr;
	Common::String key = name.hasPrefix("@") ? name : makePath(dir, name);
	key.toLowercase();
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
	_imageCache[key] = img;
	return img;
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
