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

#ifndef DKPENGE_ANI_H
#define DKPENGE_ANI_H

#include "common/rect.h"
#include "common/stream.h"
#include "audio/audiostream.h"
#include "graphics/surface.h"

namespace DKPenge {

/*
 * DK Multimedia ".ANI" animation ("WINPICS").
 *
 *   u16 version (1)
 *   u32 frame count
 *   u16 frames per second
 *   u16 audio chunks per second of pre-roll (frames * this = pre-roll chunks)
 *   u32 size of WAVEFORMAT
 *   WAVEFORMAT(EX)
 *   u16 1
 *   u32 bytes of audio per chunk
 *   16 bytes reserved
 *   chunks...
 *
 * Each chunk is a 28 byte header followed by audio and then video data:
 *   u16 type (1)
 *   u32 audio size
 *   s16 left, top, right, bottom   (dirty rectangle)
 *   u32 raw pixel size
 *   u32 compressed size
 *   "funky!"
 * The first (fps * prerollMultiplier) chunks carry audio only.
 *
 * Video data is run-length encoded: a control byte with bit 7 set means a
 * literal run of (ctrl & 0x7f) bytes; otherwise the next byte is repeated
 * ctrl times. Rows are stored top-down, padded to an even width.
 */
class AniDecoder {
public:
	AniDecoder();
	~AniDecoder();

	bool load(Common::SeekableReadStream *stream);
	void close();

	uint getFrameCount() const { return _frameCount; }
	uint getFrameRate() const { return _fps; }
	int getCurFrame() const { return _curFrame; }
	bool endOfVideo() const { return _curFrame >= (int)_frameCount - 1; }

	// Decodes the next frame; returns the dirty rectangle and the decoded
	// pixels (8-bit, dirty rectangle sized) in _frame.
	bool decodeNextFrame(Common::Rect &dirty);
	const Graphics::Surface &getFrame() const { return _frame; }

	// Audio stream for the whole animation (queueable, 8-bit PCM or MS ADPCM)
	Audio::AudioStream *getAudioStream() { return _audio; }

private:
	bool readChunkHeader(uint32 &audioSize, Common::Rect &rect, uint32 &rawSize, uint32 &compSize);
	void decodeRLE(const byte *src, uint32 srcSize, byte *dst, uint32 dstSize);
	void queueAudio(uint32 size);

	Common::SeekableReadStream *_stream;
	uint32 _frameCount;
	uint16 _fps;
	uint16 _prerollMul;
	uint16 _formatTag;
	uint16 _channels;
	uint32 _sampleRate;
	uint16 _blockAlign;
	uint16 _bitsPerSample;
	int _curFrame;
	Graphics::Surface _frame;
	Audio::QueuingAudioStream *_audio;
};

} // End of namespace DKPenge

#endif
