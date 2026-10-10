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

#ifndef FLAAKLYPA_ARCHIVE_H
#define FLAAKLYPA_ARCHIVE_H

#include "common/archive.h"
#include "common/hashmap.h"
#include "common/path.h"
#include "common/ptr.h"
#include "common/stream.h"

namespace Flaaklypa {

/**
 * Reader for the "DATA" container files (data/NAME.bin, lang/NAME.bin, ...)
 * used by Flåklypa Grand Prix.
 *
 * Layout (all little endian):
 *   char   magic[4]   "DATA"
 *   uint32 version    0x00000100
 *   uint32 count
 *   count * {
 *     char   name[260]  NUL-terminated, backslash separated, relative to the
 *                       scene directory (e.g. "animation\\idle.smk"). The bytes
 *                       after the terminator are uninitialised memory.
 *     uint32 size
 *     byte   data[size]
 *   }
 *
 * There is no central index; the table has to be walked once on open.
 * Entries are stored uncompressed.
 */
class DataArchive : public Common::Archive {
public:
	/**
	 * Opens an archive from a stream. Returns nullptr if the stream does not
	 * start with a valid DATA header.
	 */
	static DataArchive *open(Common::SeekableReadStream *stream, DisposeAfterUse::Flag dispose = DisposeAfterUse::YES);

	/** Opens an archive from a file in the search manager. */
	static DataArchive *open(const Common::Path &filename);

	bool hasFile(const Common::Path &path) const override;
	int listMembers(Common::ArchiveMemberList &list) const override;
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override;
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override;

	uint32 getVersion() const { return _version; }

private:
	struct Entry {
		Common::String name;
		uint32 offset;
		uint32 size;

		Entry() : offset(0), size(0) {}
		Entry(const Common::String &n, uint32 off, uint32 sz) : name(n), offset(off), size(sz) {}
	};

	typedef Common::HashMap<Common::Path, Entry, Common::Path::IgnoreCase_Hash, Common::Path::IgnoreCase_EqualTo> EntryMap;

	DataArchive(Common::SeekableReadStream *stream, DisposeAfterUse::Flag dispose, uint32 version)
		: _stream(stream, dispose), _version(version) {}

	static Common::Path normalize(const Common::Path &path);

	Common::DisposablePtr<Common::SeekableReadStream> _stream;
	EntryMap _entries;
	uint32 _version;
};

} // End of namespace Flaaklypa

#endif // FLAAKLYPA_ARCHIVE_H
