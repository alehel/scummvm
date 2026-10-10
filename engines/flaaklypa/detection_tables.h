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

namespace Flaaklypa {

const PlainGameDescriptor flaaklypaGames[] = {
	{ "flaaklypa", "Flåklypa Grand Prix" },
	{ 0, 0 }
};

// Detection is keyed on the scene index container plus the language pack
// container, both of which live in sub-directories of the install folder:
//
//   <install>/data/*.bin    scene/game resources (shared by all languages)
//   <install>/lang/*.bin    language specific resources (text, dubbed smk)
//   <install>/data1..3/     add-on content shipped with later editions
//   <install>/lang1..3/     language packs for the add-on content
//
// The containers are identical on the CD and after installation, so a mounted
// CD image is detected as well.
const ADGameDescription gameDescriptions[] = {
	{
		// Flåklypa Grand Prix - Gullutgave (Gold edition), v1.2, Norwegian, 2 CDs
		"flaaklypa",
		"Gullutgave",
		AD_ENTRY2s("data/sceneindex.bin", "98f40e436b689e94407e60d993ae09da", 3237616,
		           "lang/common.bin",     "0bb1e8f5105fa776e179633e1de0a2b7", 27031),
		Common::NB_NOR,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO1(GUIO_NOMIDI)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Flaaklypa
