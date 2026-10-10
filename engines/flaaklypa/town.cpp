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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The town page (FUN_00426f30) is a short transition without hotspots or
// cursor. Started with arg 0 (from `morning`, FUN_00425f40, when the player
// rides down to the village) Solan rides through the town on the bike and
// the game goes on to "outtent". Started with arg 2 (the "next page" of the
// navigator of `intent`, FUN_00425af0) Ben Redic Fy Fazan walks by, the
// screen is dimmed and the film "reel1" plays in a frame before the game
// goes on to "buildacar". Escape and space skip to the same next page.

enum {
	kArgFilm = 2,   ///< init arg: play the Ben / film reel variant
	kMeshZ = 5
};

TownScene::TownScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _arg(0), _mesh(nullptr) {
}

void TownScene::onInit(int arg) {
	// FUN_00426fc0
	playMusic("subgame10");
	hideCursor();
	_arg = arg;
	if (arg != kArgFilm)
		playAnim("bike");
	else
		playAnim("ben");
}

void TownScene::onClose() {
	// FUN_00407210: take the dimming mesh away (and reset the update clip
	// rectangle, FUN_00413630(0), which the engine does not need).
	removeMesh();
}

void TownScene::leave() {
	if (_arg != kArgFilm)
		_vm->changeScene("outtent");
	else
		_vm->changeScene("buildacar");
}

bool TownScene::handlesKey(const Common::KeyState &key) {
	// The original handler returns 1 for escape only.
	return key.keycode == Common::KEYCODE_ESCAPE;
}

void TownScene::onKey(const Common::KeyState &key) {
	// FUN_00427010: escape and space both skip to the next page.
	if (key.keycode == Common::KEYCODE_ESCAPE || key.keycode == Common::KEYCODE_SPACE)
		leave();
}

void TownScene::onAnimFinished(Anim *a) {
	// FUN_00427050
	const Common::String n(a->name());
	if (n == "bike") {
		_vm->changeScene("outtent");
	} else if (n == "ben") {
		removeAnim("ben");
		addAnim("frame", Anim::kDefaultPos, Anim::kDefaultPos, 7);
		playAnim("reel1");   // z 10 from its table entry
		_vm->_music->stop();
		addMesh(kMeshZ);    // FUN_00407110(0, 5)
	} else if (n == "reel1") {
		_vm->changeScene("buildacar");
	}
}

// FUN_00407110 / FUN_00407160: a full screen bitmap element with a one pixel
// checkerboard of transparent (pure green, the colour key) and dark
// (COLORREF 0x402020, i.e. r 0x20 g 0x20 b 0x40) pixels that darkens
// everything below its z. The original shares one such element between the
// pages and the dialogs (dialog.cpp draws the same pattern).
void TownScene::addMesh(int z) {
	if (!_mesh) {
		const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
		const uint32 dark = _vm->_screen->format.RGBToColor(0x20, 0x20, 0x40);
		_mesh = defineSurfaceAnim("mesh", kScreenWidth, kScreenHeight, green);
		Graphics::ManagedSurface *s = _mesh->surface();
		for (int y = 0; y < kScreenHeight; y++)
			for (int x = (y & 1) ? 0 : 1; x < kScreenWidth; x += 2)
				s->setPixel(x, y, dark);
	}
	if (!_mesh->isAdded())
		_mesh->add(Anim::kDefaultPos, Anim::kDefaultPos, z);
}

void TownScene::removeMesh() {
	if (_mesh && _mesh->isAdded())
		_mesh->remove();
}

} // End of namespace Flaaklypa
