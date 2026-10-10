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
#ifndef FLAAKLYPA_WHEELBARROW_H
#define FLAAKLYPA_WHEELBARROW_H

#include "common/rect.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Eplehøsten" (the wheelbarrow sub game, started from the "pee" story
 * page): apples ripen on the roof and roll down it; Solan runs left and
 * right along the bottom of the screen with his wheelbarrow (arrow keys)
 * and catches them. Rotten apples spoil the load. A full wheelbarrow (12
 * apples) is tipped into Ludvig's basket (up arrow next to it); when the
 * basket holds 24 apples Ludvig fetches it and a pie appears: the next
 * level. A good apple on the ground costs a life.
 *
 * Mirrors the handler at 0x44ed50 of the original executable.
 */
class WheelbarrowScene : public Scene {
public:
	WheelbarrowScene(FlaaklypaEngine *vm, const SceneDef *def);
	~WheelbarrowScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kMaxApples = 10,       ///< apple structures (0x68ca28, 0xc4 bytes each)
		kCounterSlots = 12,    ///< apples the wheelbarrow holds (0x68bbb0)
		kMaxPies = 10,         ///< pie bitmaps = levels shown (0x68c398)
		kSfxClones = 8,
		kHotspotHelp = 1,      ///< BUTTON module ids of the original
		kHotspotExit = 2,
		kHotspotStart = 3,     ///< hotspot of the start bitmap
		kTimerSpawn = 1
	};

	/** Solan's states: the second part of the clip name "<dir>_<state><fullness>". */
	enum SolanState {
		kNeutral = 0,          ///< bitmap
		kCycle = 1,            ///< walking, looping clip
		kStop = 2,             ///< unused by the original
		kDump = 3,             ///< tipping the wheelbarrow
		kBonk = 4              ///< unused by the original
	};

	/**
	 * An animation structure the original keeps in its data segment and
	 * renames at run time (sprintf into the name field), with the engine
	 * object that plays it.
	 */
	struct Element {
		AnimDef def;
		char name[64];
		Anim *anim;
		Element() : anim(nullptr) { memset(&def, 0, sizeof(def)); name[0] = 0; }
	};

	struct Apple {
		bool active;
		int type;              ///< 0 good, 1 rotten
		float x, y;            ///< centre of the sprite
		int stage;             ///< bounces done (index into the roof table), 4 = can be caught
		float vy;
		int state;             ///< 0 ripening (<type>_0), 1 falling (<type>_1 loop), 2 rotten splat (rotten_2)
		Element elem;
	};

	void initElement(Element &e, const char *name, bool smacker, bool visible, bool transparent, int x, int y, int z);
	void destroyElement(Element &e);

	// game flow
	void showStartScreen();
	void hideStartScreen();
	void startGame(int level);
	void gameOver();
	void setLevel(int level);
	void setSpawnTimer(int period);

	// Solan
	int fullness() const;
	void setSolan(int state);
	void moveSolan(float dt);
	void resumeFromKeys();
	void dumpBarrow();
	Common::Rect barrowRect() const;

	// apples
	void spawnApple();
	void setAppleState(Apple &a, int state);
	void removeApple(Apple &a);
	void clearApples();
	int appleZ(const Apple &a) const;
	bool updateApples(float dt);
	bool updateApple(Apple &a, const Common::Rect &barrow, float dt);
	bool moveApple(Apple &a, float dt);
	void bounceSound(const Apple &a, int action);
	bool loseLife();
	Apple *appleFor(Anim *anim);

	// score board
	void addScore(int points);
	void drawScore();
	void drawLives();
	void drawText(const char *area, const Common::String &text);
	void showCounter(int n);
	void showPies(int n);
	void showBasket(int n);

	void playSound(const char *base, int variants);
	void highlightButton(int hotspot);

	static const int kSpawnTable[kMaxApples][3];
	static const int kRoofTable[5][2];
	static const float kBounce[4];
	static const char *const kDirNames[2];
	static const char *const kStateNames[5];

	BitmapFont _font;
	Element _solan;
	Element _counter[kCounterSlots];
	Element _pies[kMaxPies];
	Element _basket;
	Element _sfx[kSfxClones];
	Apple _apples[kMaxApples];

	bool _running;             ///< 0x68ba28
	int _score;                ///< 0x68ba2c
	int _level;                ///< 0x68ba30 (pies)
	int _basketCount;          ///< 0x68ba34
	int _lives;                ///< 0x68ba38
	int _basketCapacity;       ///< 0x68bae8
	float _solanX;             ///< 0x68bb98
	int _dir;                  ///< 0x68bb9c: 0 left, 1 right
	float _speed;              ///< 0x68bba0
	int _count;                ///< 0x68bba4 apples in the wheelbarrow
	int _solanState;           ///< 0x68bba8
	uint32 _lastTick;          ///< 0x68c390
	int _spawnPeriod;          ///< repeat interval of the spawn timer, 0 = none
	bool _leftHeld, _rightHeld;
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
