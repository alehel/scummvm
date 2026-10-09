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
#include "common/textconsole.h"

#include "castle/detection.h"
#include "castle/quest.h"

namespace Castle {

Quest::Quest(Common::RandomSource &rnd) : _rnd(rnd) {
	reset();
}

void Quest::reset() {
	_spy = 0;
	randomize();
	_baseCoins = 3;
	_extra = 0;
	_mode = 3;
	for (int i = 0; i < kItems; i++)
		_items[i] = 1;
	for (int i = 0; i < kScrolls; i++) {
		_scrolls[i].text.clear();
		_scrolls[i].flag = 0;
	}
	_dirty = false;
}

// The original remembers used positions in an INI file so that successive
// games differ; a plain random choice serves the same purpose here.
void Quest::randomize() {
	for (int r = 0; r < kRooms; r++) {
		for (int c = 0; c < kSlots; c++)
			_grid[r][c] = 0;
		for (int n = 0; n < 5; n++) {
			int slot = _rnd.getRandomNumber(kSlots - 1);
			while (_grid[r][slot] == 1)
				slot = (slot + 1) % kSlots;
			_grid[r][slot] = 1;
		}
	}
	_stage = 1;
	for (int t = 0; t < kTasks; t++) {
		for (int c = 0; c < kTaskChoices; c++)
			_tasks[t][c] = 0;
		_tasks[t][_rnd.getRandomNumber(kTaskChoices - 1)] = 1;
	}
	_scenario = _rnd.getRandomNumber(2) + 1;
	debugC(1, kDebugGeneral, "Castle: quest laid out, scenario %d", _scenario);
}

bool Quest::setSpy(int spy) {
	bool changed = _spy != spy;
	if (spy == 0)
		_dirty = false;
	else if (changed)
		_dirty = true;
	_spy = spy;
	return changed;
}

int Quest::getCoin(int room, int slot) const {
	if (room < 0 || room >= kRooms || slot < 0 || slot >= kSlots)
		return 0;
	return _grid[room][slot];
}

bool Quest::collectCoin(int room, int slot) {
	if (getCoin(room, slot) != 1)
		return false;
	_grid[room][slot] = 2;
	_dirty = true;
	return true;
}

int Quest::countCoins() const {
	int n = 0;
	for (int r = 0; r < kRooms; r++)
		for (int c = 0; c < kSlots; c++)
			if (_grid[r][c] == 2)
				n++;
	return _baseCoins + n;
}

void Quest::bribe(int coins) {
	if (coins > 0)
		_dirty = true;
	for (int r = 0; r < kRooms; r++)
		for (int c = 0; c < kSlots; c++)
			if (_grid[r][c] == 2) {
				_grid[r][c] = 3;
				if (--coins < 1)
					return;
			}
	if (coins > 0) {
		_baseCoins -= coins;
		if (_baseCoins < 0)
			_baseCoins = 0;
	}
}

int Quest::getItem(int index) const {
	return index >= 0 && index < kItems ? _items[index] : 0;
}

bool Quest::collectItem(int index) {
	if (getItem(index) != 1)
		return false;
	_items[index] = 2;
	_dirty = true;
	return true;
}

int Quest::getTask(int task, int choice) const {
	if (task < 0 || task >= kTasks || choice < 0 || choice >= kTaskChoices)
		return 0;
	return _tasks[task][choice];
}

bool Quest::completeTask(int task, int choice) {
	if (getTask(task, choice) != 1)
		return false;
	_tasks[task][choice] = 2;
	_dirty = true;
	return true;
}

// The chest shows the tasks in the order 0, 2, 3, 1 but the count is the same
int Quest::countTasksDone() const {
	int n = 0;
	for (int t = 0; t < kTasks; t++)
		for (int c = 0; c < kTaskChoices; c++)
			if (_tasks[t][c] == 2) {
				n++;
				break;
			}
	return n;
}

void Quest::setMode(int mode) {
	_mode = mode;
	if (mode == 1 || mode == 2)
		_dirty = true;
}

void Quest::setScroll(int i, const Common::String &text, int flag) {
	if (i < 0 || i >= kScrolls)
		return;
	_scrolls[i].text = text;
	_scrolls[i].flag = flag;
	_dirty = true;
}

static void writeString(Common::WriteStream &s, const Common::String &str) {
	s.writeSint16BE(str.size());
	s.write(str.c_str(), str.size());
}

static Common::String readString(Common::ReadStream &s) {
	int len = s.readSint16BE();
	Common::String str;
	for (int i = 0; i < len && !s.eos(); i++)
		str += (char)s.readByte();
	return str;
}

void Quest::saveToStream(Common::WriteStream &s) const {
	s.writeSint16BE(kSaveVersion);
	s.writeUint32BE(_spy);
	s.writeSint16BE(_baseCoins);
	s.writeSint16BE(_scenario);
	for (int i = 0; i < kItems; i++)
		s.writeUint32BE(_items[i]);
	for (int i = 0; i < kScrolls; i++) {
		s.writeSint16BE(_scrolls[i].flag);
		writeString(s, _scrolls[i].text);
	}
	for (int r = 0; r < kRooms; r++)
		for (int c = 0; c < kSlots; c++)
			s.writeSint16BE(_grid[r][c]);
	for (int t = 0; t < kTasks; t++)
		for (int c = 0; c < kTaskChoices; c++)
			s.writeSint16BE(_tasks[t][c]);
	s.writeSint16BE(_stage);
	s.writeSint16BE(kSaveVersion);
}

bool Quest::loadFromStream(Common::ReadStream &s) {
	int version = s.readSint16BE();
	if (version != 5 && version != 6) {
		warning("Castle: unknown quest save version %d", version);
		return false;
	}
	_spy = s.readUint32BE();
	_baseCoins = s.readSint16BE();
	_scenario = s.readSint16BE();
	for (int i = 0; i < kItems; i++)
		_items[i] = s.readUint32BE();
	for (int i = 0; i < kScrolls; i++) {
		_scrolls[i].flag = s.readSint16BE();
		_scrolls[i].text = readString(s);
	}
	for (int r = 0; r < kRooms; r++)
		for (int c = 0; c < kSlots; c++)
			_grid[r][c] = s.readSint16BE();
	for (int t = 0; t < kTasks; t++)
		for (int c = 0; c < kTaskChoices; c++)
			_tasks[t][c] = s.readSint16BE();
	_stage = s.readSint16BE();
	if (version != 5 && s.readSint16BE() != kSaveVersion)
		return false;
	_dirty = false;
	return true;
}

} // End of namespace Castle
