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
#ifndef FLAAKLYPA_BUGZZZ_H
#define FLAAKLYPA_BUGZZZ_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/rect.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

class SoundPlayer;

/**
 * "Larveliv i leiren" (bugzzz): a Snake game for one to four players on a
 * tree trunk. Each player steers a worm with two keys (turn left / right)
 * and eats the ants and mushrooms that appear; every bite makes the worm
 * one segment longer. A woodpecker on the tree to the right is the timer:
 * when it has pecked through its branch the level is lost. Eating enough
 * ants before that scrolls the trunk to the next level.
 *
 * The original draws the worms, ants and mushrooms directly onto the
 * screen after the scene (event 0x119); here they are drawn into a full
 * screen overlay element on top of everything.
 */
class BugzzzScene : public Scene {
public:
	BugzzzScene(FlaaklypaEngine *vm, const SceneDef *def);
	~BugzzzScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kPlayers = 4,
		kMaxObjects = 40,
		kMaxSpits = 50,
		kMaxPopups = 20,
		kMaxSegments = 100,
		kDirections = 32,
		kSpriteSets = 32,
		kPanelGroups = 6,
		kSlideSteps = 30,
		kLevels = 9,
		kHotspotHelp = 31,
		kHotspotExit = 32
	};

	/** Object (ant / mushroom) types. */
	enum ObjectType {
		kAntGreen = 1,
		kAntRed = 2,
		kAntBrown = 3,
		kMushroomSpeed = 4,
		kMushroomSlow = 5,
		kMushroomDeath = 6,
		kMushroomSpit = 7,
		kMushroomStop = 8,
		kLarva = 9
	};

	/** Object states (the draw state machine of the original). */
	enum ObjectState {
		kStateAppear = 0,
		kStateWalk = 1,
		kStateTurn = 2,
		kStateFrozen = 3,
		kStateExplode = 4
	};

	/** A sprite sheet: frames across, directions down (bitmap/<name>.bmp). */
	struct SpriteDef {
		const char *name;
		int frames;
		int size;
		bool once;        ///< stops at the last frame instead of looping
		bool directions;  ///< the sheet has direction rows
		float speed;      ///< frames per tick
		int shift;        ///< direction index >> shift = row
	};

	struct Sprite {
		const SpriteDef *def;
		Graphics::ManagedSurface *surface;
		uint32 key;
		Sprite() : def(nullptr), surface(nullptr), key(0) {}
	};

	/** A worm segment: a knot position (x2, y2) and the interpolated draw position. */
	struct Segment {
		float angle, angle2;
		int dir;
		float x, y;
		float x2, y2;
		const Sprite *sprite;
	};

	struct Worm {
		float speed;
		float x, y, angle;
		Segment head;
		Segment body[kMaxSegments];
		int color;
		const Sprite *headSprite;
		Common::KeyCode keyLeft, keyRight, keySpit;
		bool eating, dying, deadDone;
		int dieTick, eatTick;
		int length;             ///< body segments drawn, 7..97
		// per game
		int score, eaten, lives;
		bool boost;
		int boostSecond;
		int spits;
		int lastSpitTick;
		int frags, deaths;
		bool poisoned;
		bool invulnerable;
		int invulnerableEnd;
	};

	struct Object {
		int type;
		bool pending;           ///< waits for appearSecond
		int appearSecond;
		bool active;
		bool isAnt;
		float x, y;
		float angle, angle2;
		int state;
		const Sprite *spriteA, *spriteB, *spriteC, *spriteD;
		int startTick;
		int pause;              ///< ticks left standing still after a turn
		int size;
		bool bounded;           ///< bounces off the play area edges instead of roaming off screen
		bool mayTurn;
		int treeCount;
	};

	struct Spit {
		bool active;
		float x, y, vx, vy;
		int owner;
		float distance, speed;
	};

	struct Popup {
		bool active;
		int x, y, startTick;
		const Sprite *sprite;
	};

	/** The sliding leaf panel of a player at the bottom of the screen. */
	struct Panel {
		int state;              ///< 0 sliding in, 1 shown, 2 sliding out
		const char *anim;
		const char *next;
		uint32 time;
	};

	struct LevelDef {
		int ants[3];            ///< green, red, brown
		int mushrooms[5];       ///< speed, slow, death, spit, stop
		int time;               ///< seconds
		int required;           ///< ants to eat
		float wormSpeed;        ///< half the worm speed
		float antSpeed;
	};

	static const SpriteDef kSpriteDefs[kSpriteSets];
	static const char *const kSoundNames[25];
	static const LevelDef kLevels_[kLevels];
	static const float kStartPos[kPlayers][3];
	static const int kBodySprite[kPlayers];
	static const char *const kColorNames[kPlayers];
	static const int kPanelFields[kPanelGroups][4];

	// ---- helpers mirroring the original's small functions
	int dirAnt(float angle) const;      ///< FUN_00434e20
	int dirWorm(float angle) const;     ///< FUN_00435a80
	float randomAngle(float lo, float hi);  ///< FUN_00434e80
	int randomRange(int lo, int hi);    ///< FUN_0040c550
	int requiredAnts() const;
	float levelWormSpeed() const;       ///< FUN_00433bb0
	float antSpeed(int type) const;     ///< FUN_00434a90
	bool blocked(int x, int y) const;   ///< FUN_004355a0
	bool onTree(int x, int y) const;    ///< FUN_00434ef0
	bool offScreen(int x, int y) const; ///< FUN_00434ec0
	static bool rectsOverlap(const Common::Rect &a, const Common::Rect &b);  ///< FUN_00434820
	static bool pointInRect(int x, int y, const Common::Rect &r);            ///< FUN_00435560
	static Common::Rect squareAround(float x, float y, int half);
	void playSound(int index);
	void setText(Anim *a, const BitmapFont &font, const Common::String &text, int flags);
	const char *panelAnimName(int group, int player) const;
	const char *backgroundBelow() const;  ///< FUN_00434010
	const char *backgroundAbove() const;  ///< FUN_00434040

	// ---- sprites
	bool drawSprite(float x, float y, const Sprite *sprite, int startTick, int dir);  ///< FUN_004351d0
	void clearPlayfield();

	// ---- screen
	void removeEverything();            ///< FUN_00412eb0
	void buildMenu(bool full);          ///< FUN_00434060
	void addButtons();                  ///< FUN_00434240
	void addLevelTexts();               ///< FUN_004341d0
	void buildGameScreen();             ///< FUN_004342c0
	void addPanels();                   ///< FUN_00434320
	void updatePanels();                ///< FUN_00433c20
	void updatePanelTexts();            ///< FUN_00433db0
	void switchPanel(int player, const char *anim);  ///< FUN_00433bd0
	void setPanelMessages(int group, const Common::String &key);  ///< FUN_00436fe0
	void updateTexts();                 ///< FUN_00436080
	void highlightButton(int hotspot);
	void updateScroll();                ///< FUN_00433e40
	void startScroll();                 ///< FUN_00436ea0

	// ---- game flow
	void countPlayers();                ///< FUN_004338c0
	void pressStart();                  ///< FUN_00437ba0
	void checkStart();                  ///< FUN_004343e0
	void startGame();                   ///< FUN_00433790
	void startLevel();                  ///< FUN_00433260
	void nextLevel();                   ///< FUN_00433220
	void setEndTime();                  ///< FUN_00433240
	int timeLeft() const;               ///< FUN_00435360
	bool checkTimeOut();                ///< FUN_00435f50
	bool updateDying();                 ///< FUN_00436d30
	bool levelComplete() const;         ///< FUN_00436fa0
	void levelDone();                   ///< FUN_00436db0
	bool anyoneAlive() const;           ///< FUN_00436c20
	void gameOver();                    ///< FUN_00434530
	void backToMenu();                  ///< FUN_00436cd0
	void playerClicked(int player);     ///< FUN_00437a10
	void joinPlayer(int player);        ///< FUN_00437b50
	void updateDelta();                 ///< FUN_00436c70

	// ---- worms
	void initWorm(int i);               ///< FUN_004375e0
	void resetWorm(int i);              ///< FUN_00433650
	void resetPlayer(int i);            ///< FUN_00433890
	void setBodySprite(Worm &w, const Sprite *s);  ///< FUN_00433770
	void setSpeed(Worm &w, float speed);  ///< FUN_00434870
	bool wormAlive(const Worm &w) const { return w.lives > 0; }  ///< FUN_00435830
	bool isKeyDown(Common::KeyCode key) const;
	void handleInput(Worm &w, int index);  ///< FUN_004369b0
	void spit(Worm &w, int owner);      ///< FUN_00436a20
	void endBoost(Worm &w);             ///< FUN_00436be0
	void moveWorm(Worm &w);             ///< FUN_004367e0
	void shiftKnots(Worm &w);           ///< FUN_00436980
	void followKnots(Worm &w, float t); ///< FUN_00436900
	void updateWorms();                 ///< FUN_00436710
	bool wormCollides(int i);           ///< FUN_00435850
	bool wormHitsWallOrSelf(const Worm &w);  ///< FUN_00435940
	bool noseInHead(const Worm &a, const Worm &b);  ///< FUN_00435ae0
	bool noseInBody(const Worm &a, const Worm &b);  ///< FUN_00435db0
	bool spitInHead(const Worm &w, int wormIndex, const Spit &s);  ///< FUN_00435bd0
	bool spitInBody(const Worm &w, const Spit &s);  ///< FUN_00435ca0
	void killWorm(Worm &w);             ///< FUN_00435ee0
	void drawWorm(Worm &w);             ///< FUN_00435710
	void drawSegments(Worm &w, int tick);  ///< FUN_004357f0
	void drawSegment(const Segment &s, int tick);  ///< FUN_004357b0
	void collideAndDrawWorms();         ///< FUN_00435690
	void eatObjects(int i);             ///< FUN_004362d0
	void addPoints(Worm &w, int type);  ///< FUN_004365e0
	void addPopup(const Worm &w, int points);  ///< FUN_00436640
	void checkExtraLife();              ///< FUN_00435370
	void endInvulnerability();          ///< FUN_004353c0
	void nosePoint(const Worm &w, int &x, int &y) const;

	// ---- objects
	void spawnObject(int type, int delay);  ///< FUN_004338f0
	bool objectsOverlap(const Object &a, const Object &b) const;  ///< FUN_00434720
	bool objectTouchesWorm(const Object &o, const Worm &w) const;  ///< FUN_00434f40
	void updateObjects();               ///< FUN_004348a0
	void steerAnt(Object &o);           ///< FUN_00434af0
	void drawObjects();                 ///< FUN_00435070
	void updateSpits();                 ///< FUN_00435400
	void drawSpits();                   ///< FUN_00435600
	void drawPopups();                  ///< FUN_00435640
	void updateBranch();                ///< FUN_004352c0

	// ---- development aid
	void parseAutoKeys();
	void runAutoKeys();

	Sprite _sprites[kSpriteSets];
	Graphics::Surface *_treeMask;
	byte _treeFree;
	float _cosTable[kDirections], _sinTable[kDirections];
	int _slideTable[kSlideSteps];
	BitmapFont _fontSmall, _font10, _font18;
	SoundPlayer *_sounds;
	Anim *_playfield;
	uint32 _playfieldKey;
	int _textPos[2][2][2];  ///< level / time text copies: x, y (they scroll with the trunk)
	Common::HashMap<Common::String, Common::String> _lastText;
	Common::HashMap<Common::String, int> _lastTextFlags;

	Worm _worms[kPlayers];
	Object _objects[kMaxObjects];
	Spit _spits[kMaxSpits];
	Popup _popups[kMaxPopups];
	Panel _panels[kPlayers];
	bool _selected[kPlayers];
	bool _active[kPlayers];
	int _lastSelected;
	int _playerCount;

	bool _inMenu;
	bool _menuBuilt;
	bool _running;
	bool _timeOut;
	bool _gameOverDone;
	bool _startPressed;
	uint32 _startTime;
	int _level, _startLevel;
	int _endTime;
	int _branch;
	int _nextLifeScore;
	bool _scrolling;
	uint32 _scrollStart;
	int _scrollParity;
	bool _loaded;
	int _highlighted;
	bool _deathmatch;      ///< the original's unused "frags" mode flag
	bool _multiSpit;       ///< the original's unused five way spit flag

	int _requiredOverride;  ///< development aid, -1 = the level table

	// time
	uint32 _timeBase;
	int _ticks, _seconds, _spitTicks;
	float _delta;
	bool _resetDelta;
	uint32 _lastFrameTime;

	bool _keyDown[Common::KEYCODE_LAST];

	struct AutoKey {
		uint32 time;
		Common::KeyCode key;
		uint16 ascii;
		bool up;
	};
	Common::Array<AutoKey> _autoKeys;
};

} // End of namespace Flaaklypa

#endif
