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
#ifndef CASTLE_QUIZ_H
#define CASTLE_QUIZ_H

#include "common/array.h"
#include "common/random.h"
#include "common/str.h"

namespace Castle {

class CastleEngine;
class Database;
class Quest;
struct Question;
struct QuestionStep;

// The spy's questions, asked in the 3D rooms: the conversation module of
// the original (document + 0x574). Every question of a spy has a few
// scripted variants ("steps", picked at random); each step names, per
// event, the group of answer objects to run: videos and waves to play,
// pages to change to, the popup with the edit box, jumps to other steps.
class Quiz {
public:
	enum {
		kEvtStart = 0,      // the spy asks (room opened)
		kEvtAsk = 1,        // the spy is clicked again: param = 1..3
		kEvtHint = 2,       // the map item is clicked: param = 1..3
		kEvtTimeout = 3,    // nothing happened for a minute
		kEvtCorrect = 4,
		kEvtWrong = 5
	};
	enum {
		kAnswerWrong = 0,
		kAnswerAccepted = 1,
		kAnswerMisspelled = 2
	};

	Quiz(CastleEngine *vm, Database *db, Quest *quest, Common::RandomSource &rnd);

	// Forgets every conversation (new game)
	void reset();
	// Makes the question current and picks its step
	void startQuestion(int q);
	// Runs the group of the current step for the event; false when the
	// step has nothing for it
	bool fireEvent(int evt, int param);
	int getQuestion() const { return _question; }

	// Set once the player has looked at the zoom page of the room: the map
	// item then completes its task without the hints
	void setFlag(bool f) { _flag = f; }
	bool getFlag() const { return _flag; }

	// Compares the typed text with the answer list n (1 or 2) of the
	// current question
	int checkAnswer(int n, const Common::String &text) const;
	// After an answer the next visit picks a fresh variant
	void answered();

	// Idle timer started by the spy's click
	void setIdle(uint32 now) { _idleStart = now; }
	void clearIdle() { _idleStart = 0; }
	bool idleExpired(uint32 now) const { return _idleStart != 0 && now >= _idleStart + 60000; }

	// Normalises an answer the way the original does: lower case, without
	// the common words, the remaining words sorted and joined
	Common::String normalize(const Common::String &text) const;

private:
	struct Record {
		int lastStep;
		int curStep;
		Common::Array<int> stepState;   // 0 never, 1 available, 2 used
		bool started;
		Record() : lastStep(-1), curStep(-1), started(false) {}
	};

	Record &record(int q);
	const Question *question(int q) const;
	int stepIndex(const Question &qu, int id) const;
	void advance(Record &rec, const Question &qu);
	int pickStep(Record &rec, const Question &qu);
	int lookupGroup(const QuestionStep &st, int evt, int param) const;
	bool runGroup(int q, Record &rec, const Question &qu, int group, int &next);

	CastleEngine *_vm;
	Database *_db;
	Quest *_quest;
	Common::RandomSource &_rnd;
	Record _recs[3][4];
	int _question;
	bool _flag;
	uint32 _idleStart;
	int _depth;
};

} // End of namespace Castle

#endif
