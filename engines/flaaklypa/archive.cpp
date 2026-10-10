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
#include "common/substream.h"
#include "common/textconsole.h"

#include "flaaklypa/archive.h"

namespace Flaaklypa {

static const uint32 kDataMagic = MKTAG('D', 'A', 'T', 'A');
static const uint32 kNameFieldSize = 260; // MAX_PATH

Common::Path DataArchive::normalize(const Common::Path &path) {
	// Entries use backslashes; callers may use either separator.
	Common::String s = path.toString('/');
	s.replace('\\', '/');
	return Common::Path(s, '/');
}

DataArchive *DataArchive::open(const Common::Path &filename) {
	Common::File *f = new Common::File();
	if (!f->open(filename)) {
		delete f;
		return nullptr;
	}
	DataArchive *arc = open(f, DisposeAfterUse::YES);
	if (!arc)
		delete f;
	return arc;
}

DataArchive *DataArchive::open(Common::SeekableReadStream *stream, DisposeAfterUse::Flag dispose) {
	stream->seek(0);
	if (stream->readUint32BE() != kDataMagic)
		return nullptr;

	uint32 version = stream->readUint32LE();
	uint32 count = stream->readUint32LE();
	if (version != 0x100)
		warning("DataArchive: unexpected version 0x%x", version);

	DataArchive *arc = new DataArchive(stream, dispose, version);

	char nameBuf[kNameFieldSize + 1];
	for (uint32 i = 0; i < count; i++) {
		if (stream->read(nameBuf, kNameFieldSize) != kNameFieldSize) {
			warning("DataArchive: truncated entry table at entry %u of %u", i, count);
			break;
		}
		nameBuf[kNameFieldSize] = 0;
		uint32 size = stream->readUint32LE();
		uint32 offset = stream->pos();
		if (stream->eos() || offset + size > (uint32)stream->size()) {
			warning("DataArchive: entry %u ('%s') runs past end of file", i, nameBuf);
			break;
		}

		Common::String name(nameBuf);
		arc->_entries[normalize(Common::Path(name, '\\'))] = Entry(name, offset, size);
		stream->seek(offset + size);
	}

	debug(1, "DataArchive: opened with %u entries", arc->_entries.size());
	return arc;
}

bool DataArchive::hasFile(const Common::Path &path) const {
	return _entries.contains(normalize(path));
}

int DataArchive::listMembers(Common::ArchiveMemberList &list) const {
	for (const auto &e : _entries)
		list.push_back(Common::ArchiveMemberList::value_type(new Common::GenericArchiveMember(e._key, *this)));
	return _entries.size();
}

const Common::ArchiveMemberPtr DataArchive::getMember(const Common::Path &path) const {
	Common::Path p = normalize(path);
	if (!_entries.contains(p))
		return nullptr;
	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(p, *this));
}

Common::SeekableReadStream *DataArchive::createReadStreamForMember(const Common::Path &path) const {
	Common::Path p = normalize(path);
	if (!_entries.contains(p))
		return nullptr;
	const Entry &e = _entries.getVal(p);
	// The parent stream is shared between all members, so use the safe
	// substream which re-seeks the parent on every read.
	return new Common::SafeSeekableSubReadStream(_stream.get(), e.offset, e.offset + e.size, DisposeAfterUse::NO);
}

} // End of namespace Flaaklypa
