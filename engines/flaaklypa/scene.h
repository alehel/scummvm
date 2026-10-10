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
#ifndef FLAAKLYPA_SCENE_H
#define FLAAKLYPA_SCENE_H

#include "common/array.h"
#include "common/events.h"
#include "common/keyboard.h"
#include "common/list.h"
#include "common/str.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

class FlaaklypaEngine;
class Resources;

/** A NULL terminated list of animation names (an "anim list" of the original). */
typedef const char *const *AnimList;

/**
 * Base class of all screens: story pages, the menu, sub games.
 *
 * Mirrors the SCENE / EVENT / CHAR modules of the original: a backdrop with
 * an 8 bit hotspot mask, a z-sorted list of animations, a cursor table
 * mapping hotspot indices to cursors, animation sequences, characters with
 * idle/bored/reaction animation lists, and timers. Subclasses implement the
 * per scene logic in the on*() callbacks.
 */
class Scene {
public:
	Scene(FlaaklypaEngine *vm, const SceneDef *def);
	virtual ~Scene();

	const SceneDef *def() const { return _def; }
	const Common::String &name() const { return _name; }
	Resources *resources() const;

	/** Loads backdrop and hotspot mask. */
	virtual bool load();

	// Callbacks, in the order the original dispatches them
	virtual void onInit(int arg) {}
	virtual void onClose() {}
	virtual void onMouseDown(int hotspot, int x, int y) {}
	virtual void onRightClick(int x, int y) {}
	virtual void onMouseUp(int hotspot, int x, int y) {}
	virtual void onMouseMove(int hotspot, int x, int y) {}
	virtual void onKey(const Common::KeyState &key) {}
	virtual void onKeyUp(const Common::KeyState &key) {}
	virtual void onAnimStarted(Anim *anim) {}
	virtual void onAnimFinished(Anim *anim) {}
	virtual void onTimer(int id, int data) {}
	virtual void onSequenceDone() {}
	/** Called once per frame after the animations have been advanced. */
	virtual void onUpdate() {}
	/** Returns true when the scene consumes the key (the original's handler returning 1): the default space bar handling is skipped. */
	virtual bool handlesKey(const Common::KeyState &key) { return false; }

	void handleEvent(const Common::Event &event);
	void update();
	void draw(Graphics::ManagedSurface &dst);

	// ---- animations
	Anim *anim(const char *name);
	void addAnim(const char *name, int x = Anim::kDefaultPos, int y = Anim::kDefaultPos, int z = 0);
	void removeAnim(const char *name);
	void playAnim(const char *name);
	bool isAnimAdded(const char *name);
	bool isAnimPlaying(const char *name);
	/**
	 * Defines an element that has no entry in the scene tables (buttons and
	 * other bitmaps the original loads through other modules).
	 */
	Anim *defineAnim(const char *name, bool smacker, bool transparent, int hotspot, int x, int y, int z = 0, bool visible = true);
	/** Defines a procedurally drawn element, see Anim::createSurface(). */
	Anim *defineSurfaceAnim(const char *name, int w, int h, uint32 keyColor, int hotspot = 0);

	// ---- sequences (the "anim list" player of the CHAR module)
	/** Plays the animation as a one element sequence (cursor hidden while it runs). */
	void playSingle(const char *name);
	void playSequence(AnimList list);
	bool isSequencePlaying() const { return _seqList != nullptr; }
	/** Aborts the running sequence and resets the characters to idle (space bar). */
	void stopSequence();

	// ---- characters (CHAR module)
	void addCharacter(int id, int hotspot);
	void setCharacterZ(int id, int z);
	void setCharacterAnims(int id, AnimList idle, AnimList bored, AnimList reaction);
	/** Puts every character into its idle loop. */
	void resetCharacters();

	// ---- cursor
	void setCursorTable(const CursorEntry *table);
	void showWaitCursor();
	void hideCursor();
	void showCursor();

	// ---- timers
	void setTimer(int id, uint32 delayMs, int data = 0);
	void killTimer(int id);

	// ---- misc
	int hotspotAt(int x, int y) const;
	void playMusic(const char *name);

	// used by Anim
	void animAdded(Anim *anim);
	void animRemoved(Anim *anim);
	void animStarted(Anim *anim);

protected:
	FlaaklypaEngine *_vm;
	const SceneDef *_def;
	Common::String _name;

	Graphics::ManagedSurface *_backdrop;
	Graphics::Surface *_mask;

private:
	enum CursorMode { kCursorHidden, kCursorWait, kCursorNormal };

	enum {
		kListIdle = 1,
		kListBored = 2,
		kListReaction = 3,
		kListSequence = 5
	};

	/** A cyclic list of animations with a play position. */
	struct AnimQueue {
		AnimList list;
		int count;
		int pos;
		AnimQueue() : list(nullptr), count(0), pos(0) {}
		void set(AnimList l);
		const char *current() const { return count ? list[pos] : nullptr; }
		const char *previous() const;
		bool advance();
	};

	struct Character {
		int id;                 ///< bit in the animation group masks
		int hotspot;            ///< hotspot index of its pixels
		int z;
		AnimQueue lists[4];     ///< unused, idle, bored, reaction
		int state;              ///< 0 inactive, else the list to play next from (kList*)
		int mode;               ///< list the current clip came from
		Anim *current;
		uint32 boredTime;
		Character() : id(0), hotspot(0), z(0), state(0), mode(0), current(nullptr), boredTime(0) {}
	};

	struct Timer {
		int id;
		int data;
		uint32 fireTime;
	};

	Anim *findAnim(const char *name);
	void sortAnims();
	void updateCursor();
	void setCursorMode(CursorMode mode);
	void characterClicked(int hotspot);
	void characterAnimFinished(Character &c, Anim *anim);
	void characterPlayNext(Character &c, int list);
	void sequenceAnimFinished(Anim *anim);
	void sequenceClear();
	bool groupBusy(uint32 group) const;
	void setGroupState(uint32 group, int state);
	void takeAnim(Anim *anim, int mode);
	void scheduleBored(Character &c);
	void dispatchAnimFinished(Anim *anim);

	Common::Array<Anim *> _anims;        ///< all animation objects of the scene (owned)
	Common::Array<AnimDef *> _dynDefs;   ///< definitions created by defineAnim()
	Common::List<Common::String> _dynNames;
	const char *_singleList[2];
	Common::Array<Anim *> _active;       ///< added animations, sorted by z
	Common::Array<Character> _characters;
	Common::List<Timer> _timers;
	Common::Array<Anim *> _finished;     ///< animations that finished this frame

	const CursorEntry *_cursorTable;
	CursorMode _cursorMode;
	int _lastHotspot;

	AnimList _seqList;
	AnimQueue _seq;
};

} // End of namespace Flaaklypa

#endif
