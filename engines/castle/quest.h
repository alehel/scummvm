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
#ifndef CASTLE_QUEST_H
#define CASTLE_QUEST_H

#include "common/array.h"
#include "common/random.h"
#include "common/str.h"
#include "common/stream.h"

namespace Castle {

// The spy quest: the player picks one of two spies, collects coins and
// evidence in the castle rooms and answers the questions in the spy's
// chest. This mirrors the quest module of the original (document + 0x214).
class Quest {
public:
	enum {
		kRooms = 10,
		kSlots = 15,
		kItems = 4,
		kTasks = 4,
		kTaskChoices = 3,
		kScrolls = 4,
		kMaxCoins = 16,
		kSaveVersion = 6
	};

	Quest(Common::RandomSource &rnd);

	// Clears everything and lays out a fresh game (no spy chosen yet)
	void reset();
	// Places the coins, the evidence and picks the scenario at random
	void randomize();

	int getSpy() const { return _spy; }
	// Chooses the spy; returns true when the choice changed the game state
	bool setSpy(int spy);

	// Coin grid: 0 nothing, 1 coin waiting, 2 collected, 3 spent on a bribe
	int getCoin(int room, int slot) const;
	bool collectCoin(int room, int slot);
	int getBaseCoins() const { return _baseCoins; }
	// Coins in the purse: the starting ones plus every collected coin
	int countCoins() const;
	// Hands coins to the guard: collected coins first, then the purse
	void bribe(int coins);

	// Evidence items of the second spy: 1 waiting, 2 collected
	int getItem(int index) const;
	bool collectItem(int index);
	// Hands a collected item to the spy: it goes back to waiting
	bool useItem(int index);

	// Chest tasks: one of three alternatives is active (1) per task, 2 when done
	int getTask(int task, int choice) const;
	bool completeTask(int task, int choice);
	int countTasksDone() const;

	int getStage() const { return _stage; }
	void setStage(int stage) { _stage = stage; }
	void finishStage() { _stage = 2; _dirty = true; }
	int getScenario() const { return _scenario; }
	int getMode() const { return _mode; }
	void setMode(int mode);
	int getExtra() const { return _extra; }
	void setExtra(int v) { _extra = v; }

	// Scroll answers given in the chest
	const Common::String &getScrollText(int i) const { return _scrolls[i].text; }
	int getScrollFlag(int i) const { return _scrolls[i].flag; }
	void setScroll(int i, const Common::String &text, int flag);

	// A game is in progress when something changed since the last save
	bool isDirty() const { return _dirty; }
	void setDirty(bool d) { _dirty = d; }

	// The original "Game.cas" layout (big endian), used inside the savegames
	void saveToStream(Common::WriteStream &s) const;
	bool loadFromStream(Common::ReadStream &s);

private:
	Common::RandomSource &_rnd;
	int _grid[kRooms][kSlots];
	int _items[kItems];
	int _tasks[kTasks][kTaskChoices];
	int _stage;
	int _spy;
	int _extra;
	int _baseCoins;
	int _scenario;
	struct Scroll {
		Common::String text;
		int flag;
		Scroll() : flag(0) {}
	} _scrolls[kScrolls];
	bool _dirty;
	int _mode;
};

} // End of namespace Castle

#endif
