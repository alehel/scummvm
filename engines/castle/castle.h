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

#ifndef CASTLE_CASTLE_H
#define CASTLE_CASTLE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "engines/engine.h"
#include "audio/mixer.h"
#include "common/random.h"
#include "graphics/surface.h"

struct ADGameDescription;

namespace Castle {

class Database;
class Resources;
class LivePage;
class AniDecoder;
struct LiveObject;
struct LivePanel;
struct Action;
struct Event;
class ScriptVM;
struct Context;
class Quest;
struct Value;

class CastleEngine : public Engine {
public:
	CastleEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~CastleEngine() override;

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
	void handleMouseMove(const Common::Point &p);
	LiveObject *findHighlightObject(LivePage *page);
	void updateNodeHotspots(LivePage *page, int node);
	void doTransition(LivePage *page, int mode, int spriteId, int async);
	void setCursor(const Common::String &name);
	void clickObject(LiveObject *lo, LivePage *page);
	void pressObject(LiveObject *lo, LivePage *page, const Common::Point &p);
	void releaseMouse(const Common::Point &p);
	void dragTo(const Common::Point &p);
	LivePage *popupAt(const Common::Point &p);
	void updateDungeonTimer(uint32 now);
	void updateScrolling(uint32 now);
	void updateAmbientSound(uint32 now, bool force);
	LiveObject *findZoomCaption(int id, LivePage *page);
	Quest *getQuest() { return _quest; }

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

	void handleEvents();
	void render();
	void applyPalette();

	void openBasePage(uint index);
	void openPopup(uint index);
	void closePopup(uint index);
	void closeAllPopups();
	LiveObject *hitTest(const Common::Point &p, LivePage **pageOut);

	void runEvent(const Event *ev, LivePage *page, LiveObject *obj);
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

	const ADGameDescription *_gameDescription;
	Common::RandomSource _rnd;
	ScriptVM *_script;
	LiveObject *_hoverObject;
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
	bool _toggles[32];           // option toggles (sounds, transitions) by option code
	bool _activityCompleted;     // ActivityCompleted action flag
	bool _spyChangedFlag;        // GeneralPurposeAction 7
	int _saveSlot;               // slot of the last save/load, -1 when none
	uint _savedPage;             // base page stored in a loaded savegame
	LiveObject *_pressedObject;  // object under the mouse button
	LivePage *_pressedPage;
	bool _dragging;              // the pressed sprite follows the mouse
	LivePage *_dragPage;         // drag popup being moved
	uint32 _dungeonTimerEnd;     // 0 when no dungeon timer runs
	Common::Point _dragOffset;
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
};

} // End of namespace Castle

#endif
