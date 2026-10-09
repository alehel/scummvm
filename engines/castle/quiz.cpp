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
#include "castle/quiz.h"
#include "castle/castle.h"
#include "castle/database.h"
#include "castle/detection.h"
#include "castle/quest.h"

#include "common/debug.h"
#include "common/algorithm.h"

namespace Castle {

// Step record fields, in the order of the file
enum {
	kStepId = 0,
	kStepNext = 1,          // step to continue with on the next visit, -1 = pick again
	kStepEnabled = 2,
	kStepStart = 3,         // group for the start event
	kStepAsk1 = 4,          // groups for the 1st..3rd click on the spy
	kStepHint1 = 7,         // groups for the 1st..3rd click on the map item
	kStepCorrect = 10,
	kStepWrong = 11,
	kStepTimeout = 12
};

// Answer object types
enum {
	kObjMovie = 0,
	kObjVideo = 1,          // a .MOV of the room ("playvideoInd")
	kObjWave = 2,
	kObjChangePage = 3,
	kObjMessage = 4,        // SendMessage 1 to an object
	kObjGoto = 5,           // continue with another step
	kObjGotoIfItem = 6,     // ... when the spy's evidence for the question is held
	kObjSetState = 7,       // conversation state variable
	kObjOpenPopup = 8,      // the popup with the edit box
	kObjVideoIfTask = 9,    // video for the active scenario of the question
	kObjNextStep = 10,
	kObjDocFlag = 11
};

Quiz::Quiz(CastleEngine *vm, Database *db, Quest *quest, Common::RandomSource &rnd)
	: _vm(vm), _db(db), _quest(quest), _rnd(rnd), _question(0), _flag(false), _idleStart(0), _depth(0) {
}

void Quiz::reset() {
	for (int s = 0; s < 3; s++)
		for (int q = 0; q < 4; q++)
			_recs[s][q] = Record();
	_question = 0;
	_flag = false;
	_idleStart = 0;
}

Quiz::Record &Quiz::record(int q) {
	int spy = CLIP(_quest->getSpy(), 0, 2);
	return _recs[spy][CLIP(q, 0, 3)];
}

const Question *Quiz::question(int q) const {
	int spy = CLIP(_quest->getSpy(), 0, 2);
	if (q < 0 || q > 3)
		return nullptr;
	return &_db->getTail().questions[spy][q];
}

int Quiz::stepIndex(const Question &qu, int id) const {
	for (uint i = 0; i < qu.steps.size(); i++)
		if (qu.steps[i].v[kStepId] == id)
			return i;
	return -1;
}

// Picks a step at random among the ones not used yet; once all have been
// used they become available again
int Quiz::pickStep(Record &rec, const Question &qu) {
	int avail = 0;
	for (uint i = 0; i < rec.stepState.size(); i++)
		if (rec.stepState[i] == 1)
			avail++;
	if (!avail) {
		for (uint i = 0; i < rec.stepState.size(); i++)
			if (rec.stepState[i] == 2)
				rec.stepState[i] = 1;
		for (uint i = 0; i < rec.stepState.size(); i++)
			if (rec.stepState[i] == 1)
				avail++;
	}
	if (!avail)
		return qu.steps.empty() ? -1 : qu.steps[0].v[kStepId];
	int r = _rnd.getRandomNumber(avail - 1);
	for (uint i = 0; i < rec.stepState.size(); i++) {
		if (rec.stepState[i] != 1)
			continue;
		if (r == 0) {
			rec.stepState[i] = 2;
			return qu.steps[i].v[kStepId];
		}
		r--;
	}
	return -1;
}

void Quiz::advance(Record &rec, const Question &qu) {
	if (rec.stepState.size() != qu.steps.size()) {
		rec.stepState.clear();
		for (uint i = 0; i < qu.steps.size(); i++)
			rec.stepState.push_back(qu.steps[i].v[kStepEnabled] ? 1 : 0);
	}
	int cur = -2;
	if (rec.lastStep != -1 && rec.started) {
		int idx = stepIndex(qu, rec.lastStep);
		if (idx >= 0 && qu.steps[idx].v[kStepNext] != -1)
			cur = qu.steps[idx].v[kStepNext];
	}
	if (cur == -2)
		cur = pickStep(rec, qu);
	rec.curStep = cur;
	rec.started = true;
	rec.lastStep = cur;
}

void Quiz::startQuestion(int q) {
	_flag = false;
	_question = q;
	const Question *qu = question(q);
	if (!qu)
		return;
	advance(record(q), *qu);
	debugC(1, kDebugScript, "Castle: quiz: spy %d question %d starts at step %d", _quest->getSpy(), q, record(q).curStep);
	_vm->quizSetState(0);
}

void Quiz::answered() {
	record(_question).started = false;
}

int Quiz::lookupGroup(const QuestionStep &st, int evt, int param) const {
	switch (evt) {
	case kEvtStart:
		return st.v[kStepStart];
	case kEvtAsk:
		return param >= 1 && param <= 3 ? st.v[kStepAsk1 + param - 1] : -1;
	case kEvtHint:
		return param >= 1 && param <= 3 ? st.v[kStepHint1 + param - 1] : -1;
	case kEvtTimeout:
		return st.v[kStepTimeout];
	case kEvtCorrect:
		return st.v[kStepCorrect];
	case kEvtWrong:
		return st.v[kStepWrong];
	default:
		return -1;
	}
}

bool Quiz::fireEvent(int evt, int param) {
	const Question *qu = question(_question);
	if (!qu || _depth > 8)
		return false;
	Record &rec = record(_question);
	// A question never started (no room entrance seen) sits on its first step
	if (rec.curStep < 0 && !qu->steps.empty())
		rec.curStep = qu->steps[0].v[kStepId];
	int idx = stepIndex(*qu, rec.curStep);
	if (idx < 0)
		return false;
	int group = lookupGroup(qu->steps[idx], evt, param);
	debugC(1, kDebugScript, "Castle: quiz: event %d(%d) at step %d -> group %d", evt, param, rec.curStep, group);
	if (group == -1)
		return false;
	int next = -1;
	_depth++;
	runGroup(_question, rec, *qu, group, next);
	if (next != -1) {
		rec.curStep = next;
		fireEvent(evt, param);
		rec.lastStep = -1;
	}
	_depth--;
	return true;
}

// The questions 0..3 ask for the evidence items 1, 2, 0 and 3
static int itemOfQuestion(int q) {
	static const int items[4] = { 1, 2, 0, 3 };
	return q >= 0 && q < 4 ? items[q] : -1;
}

bool Quiz::runGroup(int q, Record &rec, const Question &qu, int group, int &next) {
	for (uint i = 0; i < qu.objects.size(); i++) {
		const QuestionObject &o = qu.objects[i];
		if (o.value != group)
			continue;
		debugC(2, kDebugScript, "Castle: quiz: object type %d '%s' %u %d %d", o.type, o.str.c_str(), o.u, o.a, o.b);
		switch (o.type) {
		case kObjMovie:
		case kObjVideo:
			_vm->quizPlayVideo(o.str);
			break;
		case kObjWave:
			_vm->quizPlayWave(o.str);
			break;
		case kObjChangePage:
			_vm->quizChangePage(o.u, o.a);
			break;
		case kObjMessage:
			_vm->quizSendMessage(o.a, 1);
			break;
		case kObjGoto:
			next = o.a;
			break;
		case kObjGotoIfItem:
			if (_quest->useItem(itemOfQuestion(q)))
				next = o.a;
			break;
		case kObjSetState:
			_vm->quizSetState(o.a);
			break;
		case kObjOpenPopup:
			_vm->quizOpenPopup(o.u);
			break;
		case kObjVideoIfTask:
			if (_quest->getTask(q, o.a - 1) == 1)
				_vm->quizPlayVideo(o.str);
			break;
		case kObjNextStep:
			if (_quest->getTask(0, 1) != 1) {
				advance(rec, qu);
				next = rec.curStep;
			}
			break;
		case kObjDocFlag:
		default:
			break;
		}
		if (_vm->shouldQuit())
			return false;
	}
	return true;
}

Common::String Quiz::normalize(const Common::String &text) const {
	const Common::Array<Common::String> &common = _db->getTail().commonWords;
	Common::String lower = text;
	lower.toLowercase();
	Common::Array<Common::String> words;
	Common::String word;
	for (uint i = 0; i <= lower.size(); i++) {
		char c = i < lower.size() ? lower[i] : 0;
		if (c == 0 || c == ' ' || c == '\t' || c == '.' || c == ';' || c == '\'' || c == '-' || c == ',' || c == '_') {
			if (!word.empty()) {
				bool skip = false;
				for (uint k = 0; k < common.size(); k++)
					if (word.equalsIgnoreCase(common[k]))
						skip = true;
				if (!skip)
					words.push_back(word);
				word.clear();
			}
		} else {
			word += c;
		}
	}
	Common::sort(words.begin(), words.end());
	Common::String result;
	for (uint i = 0; i < words.size(); i++)
		result += words[i];
	return result;
}

int Quiz::checkAnswer(int n, const Common::String &text) const {
	const Common::Array<AnswerList> &lists = _db->getTail().scenarios;
	const AnswerList *list = nullptr;
	for (uint i = 0; i < lists.size(); i++)
		if (lists[i].ints[0] == _question && lists[i].ints[1] == _quest->getSpy() && lists[i].ints[2] == n)
			list = &lists[i];
	if (!list) {
		debugC(1, kDebugScript, "Castle: quiz: no answer list for question %d spy %d answer %d", _question, _quest->getSpy(), n);
		return kAnswerWrong;
	}
	Common::String norm = normalize(text);
	for (uint i = 0; i < list->accepted.size(); i++)
		if (norm.equalsIgnoreCase(list->accepted[i]))
			return kAnswerAccepted;
	for (uint i = 0; i < list->misspelled.size(); i++)
		if (norm.equalsIgnoreCase(list->misspelled[i]))
			return kAnswerMisspelled;
	return kAnswerWrong;
}

} // End of namespace Castle
