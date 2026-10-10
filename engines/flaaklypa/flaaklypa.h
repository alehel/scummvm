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
#include "common/keyboard.h"
#include "common/random.h"
#include "common/serializer.h"
#include "common/util.h"
#include "engines/engine.h"
#include "engines/savestate.h"
#include "common/events.h"
#include "graphics/framelimiter.h"
#include "graphics/screen.h"

#include "flaaklypa/detection.h"

namespace Common {
class INIFile;
}

namespace Flaaklypa {

class Cursor;
class Dialog;
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
	Common::INIFile *_language;

	struct AutoClick {
		uint32 time;
		int x, y;
		bool moveOnly;
		char mode;            ///< 0 click, 'm' move, 'd' press only, 'u' release only, 'r' right click
	};
	Common::Array<AutoClick> _autoClicks;
	struct AutoKey {
		uint32 time;
		Common::KeyCode key;
		uint16 ascii;
		bool down;
	};
	Common::Array<AutoKey> _autoKeys;
	uint32 _startTime = 0;
	Common::Point _mousePos;
	uint32 _autoshotTime = 0;
	Graphics::FrameLimiter *_limiter = nullptr;

	void switchScene();
	void dispatchEvent(const Common::Event &event);
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
	Dialog *_dialog = nullptr;     ///< the modal dialog receiving the input, if any

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
	/** Leaves the current scene for its parent in the scene table (sub game exit, navigator exit). */
	void endGame();

	/** Looks up "section:KEY" in lang/common/language.ini (Windows-1252), like the original's LANG_Get. */
	Common::String getString(const Common::String &key);

	/**
	 * The right click navigator of the story pages (FUN_00421560): a small
	 * dialog with previous/next page, help and exit buttons. Either page
	 * name may be nullptr (button disabled); the args are passed to
	 * changeScene() for the respective page.
	 */
	void showNavigator(const char *next, const char *prev, int nextArg = 0, int prevArg = 0);
	/**
	 * Modal message box (FUN_00421310 / FUN_00420f40) with a title line, a
	 * word wrapped text and the buttons given by the MessageBox::kButton*
	 * flags (0 = OK); "title"/"text" are language.ini keys or plain text.
	 * Returns the flag of the button pressed.
	 */
	int messageBox(const Common::String &title, const Common::String &text, int buttons = 0);

	Scene *scene() const { return _scene; }
	/** The mouse position of the last mouse event (the original's FUN_0040ca10). */
	Common::Point mousePos() const { return _mousePos; }
	/** One iteration of the main loop: input, scene update, drawing. Dialogs nest it. */
	void runFrame();

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
