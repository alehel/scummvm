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

/** Story page 6: the garage (verkstedet). */
class GarageScene : public Scene {
public:
	GarageScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;

private:
	int pickVariant(int &count, int &mask, int n);

	int _count008, _count009;            ///< picks of the two clip pairs this visit (reset on init)
	static int _mask008, _mask009;       ///< variants played since the last full round, kept across visits
	static bool _carPartFound;           ///< TODO: profile key 0 of the page
};

/** Story page 4: the house. */
class HouseScene : public Scene {
public:
	HouseScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	int pickRandom(int &count, int &mask, int n);

	int _countWindow, _countBench, _countDoor;   ///< picks made this visit (reset on init)
	static int _maskWindow, _maskBench, _maskDoor; ///< variants used since the last reset, kept across visits
	int _audiopairsHotspot;                      ///< DAT_005b877c
};

/** Story page 10: inside the sheik's tent (I sjeikens telt). */
class IntentScene : public Scene {
public:
	IntentScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	void setMusicVolume(float volume);
	/** Puts the idle/bored/reaction lists of the given characters (bit mask of ids) back to the defaults. */
	void restoreLists(int chars);
	int pickChessVariant();

	int _chessCount;         ///< clicks on the chess board this visit
	static int _chessMask;   ///< variants played, kept across visits like the original
	bool _wakeUpTimer;       ///< the trance wake up timer is pending
};

/** Story page 2: the desk ("Reodors stue"). */
class DeskScene : public Scene {
public:
	DeskScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onSequenceDone() override;
	void onTimer(int id, int data) override;

private:
	void updateClock();
	int pickWithoutRepeats(int &count, int &mask, int n);
	/** playSequence / playSingle, remembering that the intro is no longer running. */
	void startSequence(AnimList list);
	void startSingle(const char *name);

	int _pick101Count, _pick102Count, _pick110Count; ///< reset every visit
	static int _pick101Mask, _pick102Mask, _pick110Mask; ///< kept across visits like the original's globals
	static int _narrationCount; ///< profile key 1 in the original (TODO)
	bool _introRunning;
	int _longFrame, _shortFrame; ///< clock hand frames currently shown
};

/** Story page 12: the TV station (TV-stasjonen). */
class TvstationScene : public Scene {
public:
	enum { kMonitors = 4 };

	TvstationScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	void clearMonitor(int monitor);
	void showMonitorStill(int monitor);
	bool isMonitorBusy(int monitor);
	int monitorOfStill(const char *name) const;
	int pickFreeMonitor();

	Common::String _reelName;   ///< "reel_NN" bitmap of this visit
	static bool _carpartFound;  ///< TODO: profile; kept across visits like the original
};

/** Story page 7, "Solan på tokt": the morning before the ride down the mountain. */
class MorningScene : public Scene {
public:
	MorningScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onKey(const Common::KeyState &key) override;
	void onRightClick(int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onSequenceDone() override;
	void onTimer(int id, int data) override;

private:
	void rideDown();
};

} // End of namespace Flaaklypa

#endif
