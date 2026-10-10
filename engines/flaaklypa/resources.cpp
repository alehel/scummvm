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
#include "common/formats/ini-file.h"
#include "common/textconsole.h"
#include "image/bmp.h"
#include "video/smk_decoder.h"

#include "flaaklypa/archive.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

static const char *const kDataDirs[] = { "lang", "lang1", "lang2", "lang3", "data", "data1", "data2", "data3", nullptr };

Resources::Resources() {
}

Resources::~Resources() {
	for (auto &it : _archives)
		delete it._value;
}

DataArchive *Resources::archive(const Common::String &dir, const Common::String &scene) {
	Common::String key = dir + "/" + scene;
	ArchiveMap::iterator it = _archives.find(key);
	if (it != _archives.end())
		return it->_value;

	DataArchive *arc = DataArchive::open(Common::Path(key + ".bin", '/'));
	if (arc)
		debug(1, "Resources: opened %s.bin", key.c_str());
	_archives[key] = arc;
	return arc;
}

void Resources::resolve(Common::String &scene, Common::String &file) {
	file.replace('\\', '/');
	// "../../common/animation/blank1" -> scene "common", file "animation/blank1"
	while (file.hasPrefix("../../")) {
		Common::String rest = file.substr(6);
		uint slash = rest.findFirstOf('/');
		if (slash == Common::String::npos)
			break;
		scene = rest.substr(0, slash);
		file = rest.substr(slash + 1);
	}
}

Common::SeekableReadStream *Resources::open(const Common::String &sceneIn, const Common::String &fileIn) {
	Common::String scene = sceneIn, file = fileIn;
	resolve(scene, file);
	Common::Path path(file, '/');
	for (const char *const *dir = kDataDirs; *dir; dir++) {
		DataArchive *arc = archive(*dir, scene);
		if (arc && arc->hasFile(path))
			return arc->createReadStreamForMember(path);
	}
	return nullptr;
}

bool Resources::exists(const Common::String &sceneIn, const Common::String &fileIn) {
	Common::String scene = sceneIn, file = fileIn;
	resolve(scene, file);
	Common::Path path(file, '/');
	for (const char *const *dir = kDataDirs; *dir; dir++) {
		DataArchive *arc = archive(*dir, scene);
		if (arc && arc->hasFile(path))
			return true;
	}
	return false;
}

Graphics::ManagedSurface *Resources::loadBitmap(const Common::String &scene, const Common::String &file, const Graphics::PixelFormat &format) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream) {
		warning("Resources: bitmap %s/%s not found", scene.c_str(), file.c_str());
		return nullptr;
	}
	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(*stream)) {
		warning("Resources: could not decode bitmap %s/%s", scene.c_str(), file.c_str());
		return nullptr;
	}
	const Graphics::Surface *src = decoder.getSurface();
	Graphics::Surface *conv = src->convertTo(format, decoder.hasPalette() ? decoder.getPalette().data() : nullptr);
	Graphics::ManagedSurface *result = new Graphics::ManagedSurface();
	result->copyFrom(*conv);
	conv->free();
	delete conv;
	return result;
}

Graphics::Surface *Resources::loadMask(const Common::String &scene, const Common::String &file) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream)
		return nullptr;
	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(*stream)) {
		warning("Resources: could not decode mask %s/%s", scene.c_str(), file.c_str());
		return nullptr;
	}
	const Graphics::Surface *src = decoder.getSurface();
	if (src->format.bytesPerPixel != 1) {
		warning("Resources: mask %s/%s is not 8 bit", scene.c_str(), file.c_str());
		return nullptr;
	}
	Graphics::Surface *copy = new Graphics::Surface();
	copy->copyFrom(*src);
	return copy;
}

Video::SmackerDecoder *Resources::openSmacker(const Common::String &scene, const Common::String &file) {
	Common::SeekableReadStream *stream = open(scene, file);
	if (!stream) {
		warning("Resources: clip %s/%s not found", scene.c_str(), file.c_str());
		return nullptr;
	}
	Video::SmackerDecoder *decoder = new Video::SmackerDecoder();
	if (!decoder->loadStream(stream)) {
		warning("Resources: could not open clip %s/%s", scene.c_str(), file.c_str());
		delete decoder;
		return nullptr;
	}
	return decoder;
}

bool Resources::loadIni(const Common::String &scene, const Common::String &file, Common::INIFile &ini) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream)
		return false;
	ini.allowNonEnglishCharacters();
	return ini.loadFromStream(*stream);
}

} // End of namespace Flaaklypa
