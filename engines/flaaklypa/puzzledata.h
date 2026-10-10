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
#ifndef FLAAKLYPA_PUZZLEDATA_H
#define FLAAKLYPA_PUZZLEDATA_H

#include "common/scummsys.h"

namespace Flaaklypa {

/** A gem mesh of the puzzle sub game, extracted from the executable. */
struct GemMesh {
	int vertCount;
	int faceCount;
	const float *verts;   ///< x, y, z per vertex
	const uint8 *faces;   ///< three vertex indices per triangle
};

enum {
	kGemMeshGem0 = 0,     ///< colours 0, 6, 7 and the joker
	kGemMeshGem1,         ///< colours 1 and 2
	kGemMeshGem3,
	kGemMeshGem4,
	kGemMeshGem5,
	kGemMeshDisc,         ///< flat disc shown under a recoloured gem
	kGemMeshGold,
	kGemMeshSilver,
	kGemMeshCount
};

extern const GemMesh gemMeshes[kGemMeshCount];

} // End of namespace Flaaklypa

#endif
