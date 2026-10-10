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
#ifndef FLAAKLYPA_H
#define FLAAKLYPA_H

#include "common/scummsys.h"
#include "common/system.h"
#include "common/error.h"
#include "common/fs.h"
#include "common/hash-str.h"
#include "common/random.h"
#include "common/serializer.h"
#include "common/util.h"
#include "engines/engine.h"
#include "engines/savestate.h"
#include "graphics/screen.h"

#include "flaaklypa/detection.h"

namespace Common {
class INIFile;
}

namespace Flaaklypa {

class Cursor;
class Music;
class Resources;
class Scene;

struct FlaaklypaGameDescription;

// The original runs in an 800x600 24-bit DirectDraw surface.
enum {
	kScreenWidth = 800,
	kScreenHeight = 600
};

class FlaaklypaEngine : public Engine {
private:
	const ADGameDescription *_gameDescription;
	Common::RandomSource _randomSource;

	Scene *_scene;
	Common::String _nextScene;
	int _nextSceneArg;
	Common::String _returnScene;   ///< story page a sub game returns to
	Common::INIFile *_language;

	struct AutoClick {
		uint32 time;
		int x, y;
		bool moveOnly;
	};
	Common::Array<AutoClick> _autoClicks;
	struct AutoKey {
		uint32 time;
		Common::String keys;
	};
	Common::Array<AutoKey> _autoKeys;

	void switchScene();
	void parseAutoClicks();
	void parseAutoKeys();

protected:
	// Engine APIs
	Common::Error run() override;

public:
	Graphics::Screen *_screen = nullptr;
	Resources *_resources = nullptr;
	Cursor *_cursor = nullptr;
	Music *_music = nullptr;

public:
	FlaaklypaEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~FlaaklypaEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	/** Leaves the current scene for another one at the end of the frame (GAME_Start). */
	void changeScene(const Common::String &name, int arg = 0);
	/** Starts a sub game or activity from a story page. */
	void startGame(const Common::String &name);
	/** Returns from a sub game to the page it was started from. */
	void endGame();

	/** Looks up "section:KEY" in lang/common/language.ini (Windows-1252), like the original's LANG_Get. */
	Common::String getString(const Common::String &key);

	bool hasFeature(EngineFeature f) const override {
		return
		    (f == kSupportsReturnToLauncher);
	};

	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override {
		return false;
	}
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override {
		return false;
	}

	/**
	 * Uses a serializer to allow implementing savegame
	 * loading and saving using a single method
	 */
	Common::Error syncGame(Common::Serializer &s);

	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override {
		Common::Serializer s(nullptr, stream);
		return syncGame(s);
	}
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override {
		Common::Serializer s(stream, nullptr);
		return syncGame(s);
	}
};

extern FlaaklypaEngine *g_engine;
#define SHOULD_QUIT ::Flaaklypa::g_engine->shouldQuit()

} // End of namespace Flaaklypa

#endif // FLAAKLYPA_H
