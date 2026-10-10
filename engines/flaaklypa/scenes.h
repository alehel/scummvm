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
#ifndef FLAAKLYPA_SCENES_H
#define FLAAKLYPA_SCENES_H

#include "flaaklypa/scene.h"

namespace Flaaklypa {

/** Creates the scene object for a scene name (a generic Scene when the logic has not been written yet). */
Scene *createScene(FlaaklypaEngine *vm, const char *name);

/** The main menu. */
class MenuScene : public Scene {
public:
	MenuScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	struct Button {
		int hotspot;
		int x, y;
		const char *normal;    ///< bitmap shown when idle (may be nullptr: part of the backdrop)
		const char *highlight; ///< bitmap shown while the mouse is over the button
	};
	static const Button _buttons[];

	void highlight(int hotspot);
	void setAmp(int level);
	int _highlighted;
};

/** Story page 1: the yard (tunet). */
class YardScene : public Scene {
public:
	YardScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	enum {
		kFlagWeightLifted = 2,
		kFlagPumpUsed = 4,
		kStageMask = 0x30
	};

	int stage() const { return _flags & kStageMask; }
	void playIntro();
	void updateProps();
	void updateCursorTable();

	int _flags;
	int _toggleBench, _toggleWell, _toggleFence;
	static int _introCount, _introMask; ///< random pick without repeats, kept across visits like the original
};

/** Story page 3: Ludvig's bush (pee). */
class PeeScene : public Scene {
public:
	PeeScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum { kMaxPoints = 50 };

	int pickBush();
	void newFlight();
	static int heading(const Common::Point &p0, const Common::Point &p1);

	int _bushCount;              ///< clicks on the bush this visit
	static int _bushMask;        ///< variants played, kept across visits like the original

	// The flying butterfly
	Common::Point _points[kMaxPoints]; ///< flight path
	int _pointCount;
	float _progress;             ///< 0..1 along the path
	float _speed;                ///< path fraction per second
	int _colour;                 ///< index into the butterfly clip names
	uint32 _lastTick;
	Common::String _butterfly;   ///< clip currently shown
};

} // End of namespace Flaaklypa

#endif
