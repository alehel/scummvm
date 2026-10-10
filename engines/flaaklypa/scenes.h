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

#include "common/formats/ini-file.h"

#include "flaaklypa/font.h"
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

/** Story page 14: goodbye (avslutning). Fireworks and stars, then the narrator sends the player on to the outro. */
class GoodbyeScene : public Scene {
public:
	GoodbyeScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	int pickFree(AnimList list, int count, int randomRange);
	void narrationFinished(const char *name);

	static int _fireworkCount; ///< DAT_0055d000: fireworks launched, reset when the page is entered afresh
};

/** The ending page (epilog): Reodor, Solan and Ludvig watch the fireworks, then the credits from credits.ini scroll up. */
class OutroScene : public Scene {
public:
	OutroScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum { kMaxLines = 48 }; ///< text elements at DAT_005783e0..0x57a360

	int sceneTime() const;
	bool getCredit(const char *key, int index, Common::String &value) const;
	int getCreditInt(const char *key, int index, int def) const;
	void nextCredit(int now);
	int addText(const Common::String &text, int font, int startTime);
	void addLine(const Common::String &line, int font, int startTime);
	void playFirework();
	void award();

	Common::INIFile _credits;
	bool _creditsLoaded;
	BitmapFont _fonts[2];       ///< DAT_0057a360 (10 pt, headings), DAT_0057b390 (14 pt)
	Anim *_lines[kMaxLines];
	int _lineStart[kMaxLines];  ///< DAT_0057831c: scene time at which the line starts scrolling
	int _creditIndex;           ///< DAT_00578318
	uint32 _startTime;
};

} // End of namespace Flaaklypa

#endif
