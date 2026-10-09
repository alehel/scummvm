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
struct Action;
struct Event;
class ScriptVM;
struct Context;

class CastleEngine : public Engine {
public:
	CastleEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~CastleEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;

	Database *getDatabase() { return _db; }
	Resources *getResources() { return _res; }
	Common::RandomSource &getRandom() { return _rnd; }
	LiveObject *findLiveObject(int id, LivePage *page);
	void runScriptAction(const Action *a, Context &ctx);
	bool scriptShouldStop() const;
	void setSpriteFrame(LiveObject *lo, int frame);
	void markDirty() { _dirty = true; }
	int getBuiltinNumber(int id) const;
	void runCommand(const Action *a, LivePage *page, LiveObject *obj);
	void updateSprites(uint32 now);
	void runSpriteFrameScripts(LivePage *page, LiveObject *lo, int frame);
	void handleMouseMove(const Common::Point &p);

private:
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

	Common::String _dumpDir;
	int _dumpCount;
	Audio::SoundHandle _waveHandle;
	Audio::SoundHandle _aniAudioHandle;
	AniDecoder *_ani;
	Common::Point _aniPos;
	uint32 _aniNextFrame;
	Graphics::Surface _aniBackground;
};

} // End of namespace Castle

#endif
