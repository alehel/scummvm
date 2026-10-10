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

#ifndef FLAAKLYPA_SCENEDATA_H
#define FLAAKLYPA_SCENEDATA_H

#include "common/scummsys.h"

namespace Flaaklypa {

/**
 * Static description of an animation, mirroring the 0xa8 byte structures
 * the original keeps in its data segment (one per animation per scene).
 */
struct AnimDef {
	const char *name;   ///< file name without extension, relative to the scene's animation/ or bitmap/ directory
	int smacker;        ///< 1: animation/<name>.smk, 0: bitmap/<name>.bmp
	int visible;        ///< 0 for audio only clips (narration, sound effects, music)
	int transparent;    ///< 1: the colour of pixel (0, 0) is the colour key
	int loop;           ///< 1: the clip repeats until it is removed
	int hotspot;        ///< hotspot index reported when the mouse is over the (opaque) pixels, 0 for none
	int x, y;           ///< default screen position
	uint32 group;       ///< character bit mask: which characters this animation shows
	int zOrder;         ///< default draw order (lower first) when started by playAnim()
	int overlay;        ///< 1: an additional <name>-o.smk overlay exists (unused in the Gold edition)
};

struct CursorEntry {
	int hotspot;        ///< hotspot index, -1 terminates the table
	const char *cursor; ///< common/cursors/<cursor>.smk
};

enum SceneType {
	kSceneSystem = 1,   ///< menu, scene index, score boards, ...
	kSceneStory = 2,    ///< story page
	kSceneSubGame = 3,
	kSceneActivity = 4
};

struct NamedCursorTable {
	const char *name;           ///< "<scene>_<n>" in address order, see scenelists.txt
	const CursorEntry *table;
};

struct SceneDef {
	const char *name;
	const char *parent;         ///< scene to return to
	const char *title;          ///< language.ini key "<scene>:<KEY>"
	int type;                   ///< SceneType
	const AnimDef *anims;       ///< nullptr terminated by a null name
	const CursorEntry *cursors; ///< default cursor table, may be nullptr
	int flags;
};

extern const SceneDef sceneDefs[];
extern const NamedCursorTable cursorTables[];

const CursorEntry *findCursorTable(const char *name);
const SceneDef *findSceneDef(const char *name);
const AnimDef *findAnimDef(const SceneDef *scene, const char *name);

} // End of namespace Flaaklypa

#endif
