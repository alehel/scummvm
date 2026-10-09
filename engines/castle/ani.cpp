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
#include "common/memstream.h"
#include "common/textconsole.h"
#include "audio/decoders/adpcm.h"
#include "audio/decoders/raw.h"

#include "castle/ani.h"
#include "castle/detection.h"

namespace Castle {

AniDecoder::AniDecoder() : _stream(nullptr), _frameCount(0), _fps(0), _prerollMul(0), _formatTag(0), _channels(0),
		_sampleRate(0), _blockAlign(0), _bitsPerSample(0), _audioChunkSize(0), _curFrame(-1), _audio(nullptr) {
}

AniDecoder::~AniDecoder() {
	close();
}

void AniDecoder::close() {
	delete _stream;
	_stream = nullptr;
	_frame.free();
	if (_audio) {
		_audio->finish();
		// The mixer owns the stream once it was queued; if it was never
		// handed out we leak-free it here.
		_audio = nullptr;
	}
	_curFrame = -1;
}

bool AniDecoder::load(Common::SeekableReadStream *stream) {
	close();
	_stream = stream;
	uint16 version = stream->readUint16LE();
	if (version != 1) {
		warning("Castle: unsupported ANI version %d", version);
		return false;
	}
	_frameCount = stream->readUint32LE();
	_fps = stream->readUint16LE();
	_prerollMul = stream->readUint16LE();
	uint32 fmtSize = stream->readUint32LE();
	uint32 fmtStart = stream->pos();
	_formatTag = stream->readUint16LE();
	_channels = stream->readUint16LE();
	_sampleRate = stream->readUint32LE();
	stream->readUint32LE(); // avg bytes per sec
	_blockAlign = stream->readUint16LE();
	_bitsPerSample = stream->readUint16LE();
	stream->seek(fmtStart + fmtSize);
	stream->readUint16LE(); // 1
	_audioChunkSize = stream->readUint32LE();
	stream->skip(16);

	if (_fps == 0)
		_fps = 10;

	_audio = Audio::makeQueuingAudioStream(_sampleRate, _channels == 2);

	// Pre-roll: audio-only chunks
	uint preroll = _fps * _prerollMul;
	for (uint i = 0; i < preroll; i++) {
		uint32 audioSize, rawSize, compSize;
		Common::Rect rect;
		if (!readChunkHeader(audioSize, rect, rawSize, compSize))
			return false;
		queueAudio(audioSize);
		stream->skip(compSize);
	}
	debugC(1, kDebugGraphics, "Castle: ANI %u frames, %u fps, audio tag %u %uHz, preroll %u", _frameCount, _fps, _formatTag, _sampleRate, preroll);
	return true;
}

bool AniDecoder::readChunkHeader(uint32 &audioSize, Common::Rect &rect, uint32 &rawSize, uint32 &compSize) {
	if (_stream->eos() || _stream->pos() + 28 > _stream->size())
		return false;
	uint16 type = _stream->readUint16LE();
	if (type != 1) {
		warning("Castle: bad ANI chunk type %d at %d", type, (int)_stream->pos());
		return false;
	}
	audioSize = _stream->readUint32LE();
	rect.left = _stream->readSint16LE();
	rect.top = _stream->readSint16LE();
	rect.right = _stream->readSint16LE();
	rect.bottom = _stream->readSint16LE();
	rawSize = _stream->readUint32LE();
	compSize = _stream->readUint32LE();
	_stream->skip(6); // "funky!"
	return true;
}

void AniDecoder::queueAudio(uint32 size) {
	if (!size || !_audio)
		return;
	byte *buf = (byte *)malloc(size);
	_stream->read(buf, size);
	Common::MemoryReadStream *ms = new Common::MemoryReadStream(buf, size, DisposeAfterUse::YES);
	Audio::AudioStream *as = nullptr;
	if (_formatTag == 1) {
		byte flags = 0;
		if (_bitsPerSample == 16)
			flags |= Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN;
		else
			flags |= Audio::FLAG_UNSIGNED;
		if (_channels == 2)
			flags |= Audio::FLAG_STEREO;
		as = Audio::makeRawStream(ms, _sampleRate, flags, DisposeAfterUse::YES);
	} else if (_formatTag == 2) {
		as = Audio::makeADPCMStream(ms, DisposeAfterUse::YES, size, Audio::kADPCMMS, _sampleRate, _channels, _blockAlign);
	} else {
		warning("Castle: unsupported ANI audio format %d", _formatTag);
		delete ms;
		return;
	}
	_audio->queueAudioStream(as, DisposeAfterUse::YES);
}

void AniDecoder::decodeRLE(const byte *src, uint32 srcSize, byte *dst, uint32 dstSize) {
	const byte *end = src + srcSize;
	byte *dend = dst + dstSize;
	while (src < end && dst < dend) {
		byte ctrl = *src++;
		if (ctrl & 0x80) {
			uint n = ctrl & 0x7f;
			while (n-- && src < end && dst < dend)
				*dst++ = *src++;
		} else {
			if (src >= end)
				break;
			byte v = *src++;
			uint n = ctrl;
			while (n-- && dst < dend)
				*dst++ = v;
		}
	}
}

bool AniDecoder::decodeNextFrame(Common::Rect &dirty) {
	uint32 audioSize, rawSize, compSize;
	if (!readChunkHeader(audioSize, dirty, rawSize, compSize))
		return false;
	queueAudio(audioSize);

	int w = dirty.width();
	int h = dirty.height();
	if (w <= 0 || h <= 0) {
		_stream->skip(compSize);
		_curFrame++;
		return true;
	}
	int stride = (w + 1) & ~1;
	byte *comp = (byte *)malloc(compSize);
	_stream->read(comp, compSize);
	byte *raw = (byte *)calloc(stride * h, 1);
	decodeRLE(comp, compSize, raw, stride * h);
	free(comp);

	_frame.free();
	_frame.create(w, h, Graphics::PixelFormat::createFormatCLUT8());
	for (int y = 0; y < h; y++)
		memcpy(_frame.getBasePtr(0, y), raw + y * stride, w);
	free(raw);
	_curFrame++;
	return true;
}

} // End of namespace Castle
