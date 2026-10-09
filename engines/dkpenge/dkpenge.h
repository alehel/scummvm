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

#ifndef DKPENGE_DKPENGE_H
#define DKPENGE_DKPENGE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "engines/engine.h"
#include "audio/mixer.h"
#include "common/random.h"
#include "graphics/surface.h"

struct ADGameDescription;

namespace DKPenge {

class Database;
class Resources;
class LivePage;
class AniDecoder;
struct LiveObject;
struct LivePanel;
struct ScriptObject;
struct Action;
struct Event;
class ScriptVM;
struct Context;
class Quest;
class Quiz;
struct Value;
struct Collage;

class DKPengeEngine : public Engine {
public:
	DKPengeEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~DKPengeEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override { return true; }
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override { return true; }
	Common::Error loadGameState(int slot) override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;

	Database *getDatabase() { return _db; }
	Resources *getResources() { return _res; }
	Common::RandomSource &getRandom() { return _rnd; }
	LiveObject *findLiveObject(int id, LivePage *page);
	void runScriptAction(const Action *a, Context &ctx);
	bool scriptShouldStop() const;
	void setSpriteFrame(LiveObject *lo, int frame);
	void spriteGotoFrame(LiveObject *lo, int frame);
	void markDirty() { _dirty = true; }
	int getBuiltinNumber(int id) const;
	void runCommand(const Action *a, LivePage *page, LiveObject *obj);
	void updateSprites(uint32 now);
	bool advanceSprite(LivePage *page, LiveObject *lo);
	bool spriteLoopDone(LivePage *page, LiveObject *lo);
	void spriteFinished(LivePage *page, LiveObject *lo);
	void runSpriteFrameScripts(LivePage *page, LiveObject *lo, int event, int frame);
	void runSpriteScript(LivePage *page, LiveObject *lo, const ScriptObject *script, int event, int frame);
	// Region scripts (events 8 then 7) after a user driven move of the sprite
	void runSpriteRegionEvents(LivePage *page, LiveObject *lo);
	void spriteMoved(LiveObject *lo);
	// Moves a sprite (panel relative target) inside its limit rectangle as
	// the original's move routine: clamps, bounces or wraps by its motion
	// mode, fires the edge scripts (event 15) and, for user moves, the
	// region scripts
	void moveSpriteTo(LivePage *page, LiveObject *lo, int nx, int ny, bool user);
	void updateSpriteMotion(LivePage *page, LiveObject *lo, uint32 now);
	void handleMouseMove(const Common::Point &p);
	LiveObject *findHighlightObject(LivePage *page);
	int castleSectionAt(const LiveObject *hl, const Common::Point &p) const;
	LiveObject *findColourHotspot(LivePage *page, int section);
	void hoverCastleSection(LivePage *page, LiveObject *hl, int section);
	void updateNodeHotspots(LivePage *page, int node);
	void doTransition(LivePage *page, int mode, int spriteId, int async);
	void setCursor(const Common::String &name);
	// Scripts read the mouse position from a document variable
	void setMouseVar(const Common::Point &p, LivePanel *panel);
	LivePanel *panelAt(const Common::Point &p);
	// A script changed an object's cursor: shown at once while the mouse is on it
	void objectCursorChanged(LiveObject *lo);
	void clickObject(LiveObject *lo, LivePage *page);
	void pressObject(LiveObject *lo, LivePage *page, const Common::Point &p);
	void releaseMouse(const Common::Point &p);
	void dragTo(const Common::Point &p);
	LivePage *popupAt(const Common::Point &p);
	void updateDungeonTimer(uint32 now);
	void updateScrolling(uint32 now);
	void scrollStripBy(int st);
	void updateAmbientSound(uint32 now, bool force);
	LiveObject *findZoomCaption(int id, LivePage *page);
	Quest *getQuest() { return _quest; }
	Quiz *getQuiz() { return _quiz; }

	// Services of the spy's questions (see quiz.h)
	void quizPlayVideo(const Common::String &name);
	void quizPlayWave(const Common::String &name);
	void quizChangePage(uint page, int transition);
	void quizOpenPopup(uint page);
	void quizSendMessage(int objectId, int msg);
	void quizSetState(int state);

private:
	// Spy quest
	void setSpy(int spy, bool fireEvent);
	void fireSpyChanged();
	void applyQuestObjects(LivePage *page, bool onOpen);
	void updateSpyChest(LivePage *page);
	LivePanel *findSpyChest(LivePage **pageOut);
	void chestFlash();
	void showSpriteFrame(LiveObject *lo, int frame);
	void newGame();
	void startGame();
	void runOptionsAction(int code, LivePage *page, LiveObject *obj);
	void afterQuestLoad(bool ok, bool fireEvent);
	int toggleCodeOf(int objectId) const;
	bool toggleState(int code) const;
	void setToggleState(int code, bool on);
	void applyToggles();
	void startRoomQuestions(LivePage *page);
	void answerClicked(LiveObject *lo, LivePage *page);
	LiveObject *findEditBox(LivePage **pageOut);
	void typeKey(int ascii, int keycode);
	void flushPendingPage();

	// Index and Trail lists (see collage.h) and the navigation history
	struct TrailEntry {
		Common::String title;
		int icon;
		uint page;            // base page
		uint popup;           // library book popup, 0xffffffff when none
		Common::Point scroll; // zoom pages: position in view
		TrailEntry() : icon(0), page(0), popup(0xffffffff) {}
	};
	void setupCollages(LivePage *page);
	LiveObject *findCollage(LivePage **pageOut);
	void collagePress(LiveObject *lo, LivePage *page, const Common::Point &p);
	void collageRelease(LiveObject *lo, LivePage *page, const Common::Point &p);
	void collageSelected(LivePage *page, LiveObject *lo);
	void collageTyped(LivePage *page, LiveObject *lo);
	void scrollCollage(LivePage *page, int delta);
	void activateCollageItem(LivePage *page, LiveObject *lo);
	void scrollBarPress(LiveObject *lo, LivePage *page, const Common::Point &p);
	void scrollBarDrag(LiveObject *lo, LivePage *page, const Common::Point &p);
	bool collageKey(int ascii, int keycode);
	bool describeLocation(LivePage *page, bool popup, TrailEntry &e);
	void recordTrail(LivePage *page, bool popup);
	void updateTrailScroll();
	void goToTrailEntry(int entry);

	// Chest scrolls: four answers typed on a scroll popup
	void scrollPageClosing(LivePage *page);
	void checkScrollAnswers(LiveObject *lo, LivePage *page);
	bool scrollAnswerMatches(int i, const Common::String &text, bool misspelled) const;
	void queueWave(const Common::String &dir, const Common::String &name);
	void updateWaveQueue();
	void focusEditBox(LiveObject *lo, LivePage *page);
	void pageClosing(LivePage *page);

	// Page-turn wipes between base pages, screen dumps of the test harness
	void wipeTransition(const Graphics::Surface &from, int code);
	void composeScreen();
	void dissolveRect(const Common::Rect &r);
	void dumpSurface(const Graphics::Surface &surf);
	void updateRepeat(uint32 now);

	void handleEvents();
	void render();
	void applyPalette();

	void openBasePage(uint index, const Common::Point &scroll = Common::Point(0, 0));
	void openPopup(uint index);
	void closePopup(uint index);
	void closeAllPopups();
	LiveObject *hitTest(const Common::Point &p, LivePage **pageOut);

	void runEvent(const Event *ev, LivePage *page, LiveObject *obj);
	// Whether a page is still open (an action may have closed it)
	bool pageAlive(const LivePage *page) const;
	void runActions(const Common::Array<Action *> &actions, LivePage *page, LiveObject *obj);
	void runAction(const Action *a, LivePage *page, LiveObject *obj);

	void playWave(const Common::String &dir, const Common::String &name, bool loop);
	void playVideo(const Common::String &dir, const Common::String &name, const Common::Rect &dest);
	void runPageEvents(LivePage *page, int eventType);
	void stopWave();
	void playWaveChannel(const Common::String &dir, const Common::String &name, int channel, bool loop = false);
	void stopWaveChannel(int channel, const Common::String &name);
	void playAnimation(const Common::String &dir, const Common::String &name, const Common::Point &pos);
	void updateAnimation();

	Common::RandomSource _rnd;
	ScriptVM *_script;
	LiveObject *_hoverObject;
	int _castleSection;     // section of the Castle Guide's castle under the pointer, -1 = none
	LivePage *_hoverPage;
	Database *_db;
	Resources *_res;
	Graphics::Surface _screen;
	LivePage *_basePage;
	Common::Array<LivePage *> _popups;
	bool _dirty;
	bool _paletteDirty;
	uint _pendingBasePage;
	bool _pendingBase;
	Common::Point _pendingScroll;

	Common::String _cursorName;
	Common::String _dumpDir;
	int _dumpCount;
	Audio::SoundHandle _waveHandle;
	struct WaveChannel {
		int channel;
		Common::String name;
		Audio::SoundHandle handle;
		WaveChannel() : channel(0) {}
	};
	Common::Array<WaveChannel> _channels;
	Quest *_quest;
	Quiz *_quiz;
	bool _toggles[32];           // option toggles (sounds, transitions) by option code
	bool _spyChangedFlag;        // GeneralPurposeAction 7
	int _saveSlot;               // slot of the last save/load, -1 when none
	uint _savedPage;             // base page stored in a loaded savegame
	LiveObject *_pressedObject;  // object under the mouse button
	LivePage *_pressedPage;
	bool _dragging;              // the pressed sprite follows the mouse
	LivePage *_dragPage;         // drag popup being moved
	uint32 _dungeonTimerEnd;     // 0 when no dungeon timer runs
	Common::Point _dragOffset;
	Common::Point _mousePanelPt;  // last mouse position relative to its panel
	LiveObject *_scrollObject;   // scroll-edge object under the mouse
	LivePage *_scrollPage;
	uint32 _scrollNext;
	int _scrollStep;
	Common::String _ambientName;  // looping wave of the zoom page region in view
	uint32 _ambientNext;
	Audio::SoundHandle _aniAudioHandle;
	AniDecoder *_ani;
	Common::Point _aniPos;
	uint32 _aniNextFrame;
	Graphics::Surface _aniBackground;
	Common::Array<TrailEntry> _trail;
	bool _trailNavigating;       // returning to a trail entry: do not record it again
	uint _pendingPopup;          // popup to open once the pending base page is up
	bool _scrollBarDrag;         // the scroll bar's coin follows the mouse
	int _scrollBarGrab;          // offset of the mouse in the coin
	Common::Array<Common::String> _waveQueue; // waves to play one after the other
	Common::String _waveQueueDir;
	LiveObject *_editFocus;      // edit box receiving the keyboard
	LivePage *_editFocusPage;
	int _pendingTransition;      // transition code of the pending page change
	bool _noScreenUpdate;        // render() leaves the screen alone (wipes)
	uint32 _repeatNext;          // RepeatingHotspot held: next click
};

} // End of namespace DKPenge

#endif
