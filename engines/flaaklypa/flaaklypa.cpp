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
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/console.h"
#include "flaaklypa/cursor.h"
#include "flaaklypa/detection.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/debug-channels.h"
#include "common/events.h"
#include "common/scummsys.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "engines/util.h"
#include "common/file.h"
#include "common/formats/ini-file.h"
#include "graphics/framelimiter.h"
#include "graphics/pixelformat.h"
#include "image/png.h"

namespace Flaaklypa {

FlaaklypaEngine *g_engine;

FlaaklypaEngine::FlaaklypaEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Flaaklypa"), _scene(nullptr), _nextSceneArg(0), _language(nullptr) {
	g_engine = this;
}

FlaaklypaEngine::~FlaaklypaEngine() {
	delete _scene;
	delete _language;
	delete _music;
	delete _cursor;
	delete _resources;
	delete _screen;
}

uint32 FlaaklypaEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String FlaaklypaEngine::getGameId() const {
	return _gameDescription->gameId;
}

void FlaaklypaEngine::changeScene(const Common::String &name, int arg) {
	_nextScene = name;
	_nextSceneArg = arg;
}

// Sub games and activities that have a scene class (see createScene()).
// Starting one of the others would strand the player on a backdrop without
// an exit button.
static const char *const kImplementedGames[] = {
	"puzzle",
	"sockdrawer",
	"textinvader",
	"hopscotch",
	"audiopairs",
	"whackamole",
	"butterfly",
	"wheelbarrow",
	"lettersort",
	"hustle",
	"buildabike",
	nullptr
};

static bool isGameImplemented(const Common::String &name) {
	for (const char *const *g = kImplementedGames; *g; g++)
		if (!scumm_stricmp(name.c_str(), *g))
			return true;
	return false;
}

void FlaaklypaEngine::startGame(const Common::String &name) {
	const SceneDef *def = findSceneDef(name.c_str());
	if (!def || !isGameImplemented(name)) {
		// TODO: the other sub games and activities
		warning("Sub game '%s' is not implemented yet", name.c_str());
		return;
	}
	if (_scene)
		_returnScene = _scene->name();
	changeScene(name, 0);
}

void FlaaklypaEngine::endGame() {
	Common::String target = _returnScene;
	if (target.empty() && _scene && _scene->def()->parent)
		target = _scene->def()->parent;
	if (target.empty())
		target = "menu";
	_returnScene.clear();
	changeScene(target, 1);
}

Common::String FlaaklypaEngine::getString(const Common::String &key) {
	if (!_language) {
		_language = new Common::INIFile();
		if (!_resources->loadIni("common", "language.ini", *_language))
			warning("common/language.ini not found");
	}
	uint colon = key.findFirstOf(':');
	if (colon == Common::String::npos)
		return key;
	Common::String value;
	if (_language->getKey(key.substr(colon + 1), key.substr(0, colon), value))
		return value;
	return key;
}

// Development aid: "autoclick=t:x,y;t:x,y" clicks at the given times (ms
// after start); "t:x,y,m" only moves the mouse there, "t:x,y,d" only
// presses the button and "t:x,y,u" only releases it (drags).
void FlaaklypaEngine::parseAutoClicks() {
	if (!ConfMan.hasKey("autoclick"))
		return;
	Common::StringTokenizer tok(ConfMan.get("autoclick"), ";");
	while (!tok.empty()) {
		Common::String item = tok.nextToken();
		AutoClick c;
		char mode = 0;
		int n = sscanf(item.c_str(), "%u:%d,%d,%c", &c.time, &c.x, &c.y, &mode);
		c.moveOnly = mode == 'm';
		c.mode = mode;
		if (n >= 3)
			_autoClicks.push_back(c);
	}
}

// Development aid: "autokey=t:key[:hold];..." presses a key at t ms after
// start and releases it hold ms later (default 100). key is left, right, up,
// down, space, esc, tab, return, a single character, or a string of
// characters typed one after the other ('~' stands for Escape).
void FlaaklypaEngine::parseAutoKeys() {
	if (!ConfMan.hasKey("autokey"))
		return;
	Common::StringTokenizer tok(ConfMan.get("autokey"), ";");
	while (!tok.empty()) {
		Common::StringTokenizer parts(tok.nextToken(), ":");
		Common::String t = parts.nextToken(), name = parts.nextToken(), hold = parts.nextToken();
		if (t.empty() || name.empty())
			continue;
		static const struct { const char *name; Common::KeyCode code; } named[] = {
			{ "left", Common::KEYCODE_LEFT }, { "right", Common::KEYCODE_RIGHT },
			{ "up", Common::KEYCODE_UP }, { "down", Common::KEYCODE_DOWN },
			{ "space", Common::KEYCODE_SPACE }, { "esc", Common::KEYCODE_ESCAPE },
			{ "tab", Common::KEYCODE_TAB }, { "return", Common::KEYCODE_RETURN },
			{ nullptr, Common::KEYCODE_INVALID }
		};
		AutoKey k;
		k.time = (uint32)atoi(t.c_str());
		k.key = Common::KEYCODE_INVALID;
		k.ascii = 0;
		for (int i = 0; named[i].name; i++)
			if (name == named[i].name)
				k.key = named[i].code;
		uint32 holdMs = hold.empty() ? (name.size() == 1 || k.key != Common::KEYCODE_INVALID ? 100 : 0) : (uint32)atoi(hold.c_str());
		if (k.key != Common::KEYCODE_INVALID) {
			k.down = true;
			_autoKeys.push_back(k);
			k.time += holdMs;
			k.down = false;
			_autoKeys.push_back(k);
			continue;
		}
		for (uint i = 0; i < name.size(); i++) {
			char c = name[i];
			k.key = c == '~' ? Common::KEYCODE_ESCAPE : (Common::KeyCode)tolower((byte)c);
			k.ascii = c == '~' ? 0 : (uint16)(byte)c;
			k.down = true;
			_autoKeys.push_back(k);
			k.time += holdMs;
			k.down = false;
			_autoKeys.push_back(k);
		}
	}
}

void FlaaklypaEngine::switchScene() {
	Common::String name = _nextScene;
	int arg = _nextSceneArg;
	_nextScene.clear();

	if (_scene) {
		_scene->onClose();
		delete _scene;
		_scene = nullptr;
	}
	_cursor->set("");

	Scene *scene = createScene(this, name.c_str());
	if (!scene) {
		warning("Unknown scene '%s'", name.c_str());
		return;
	}
	if (!scene->load()) {
		delete scene;
		return;
	}
	debug(1, "Entering scene %s (arg %d)", name.c_str(), arg);
	_scene = scene;
	_scene->onInit(arg);
	_scene->showCursor();
}

Common::Error FlaaklypaEngine::run() {
	// The game draws 24-bit backdrops; ask for a 32-bit screen and let the
	// backend pick the closest supported format.
	Graphics::PixelFormat format(4, 8, 8, 8, 8, 24, 16, 8, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	_screen = new Graphics::Screen(kScreenWidth, kScreenHeight, g_system->getScreenFormat());

	setDebugger(new Console());

	_resources = new Resources();
	_cursor = new Cursor();
	_music = new Music();

	changeScene(ConfMan.hasKey("start_scene") ? ConfMan.get("start_scene") : "menu", 0);
	if (ConfMan.hasKey("random_seed"))
		_randomSource.setSeed(ConfMan.getInt("random_seed"));
	parseAutoClicks();
	parseAutoKeys();
	uint32 startTime = g_system->getMillis();

	// Development aid: "autoshot=<file>" with "autoshot_delay=<ms>" writes a
	// PNG of the screen after the delay; "autoshot_quit=true" then exits.
	uint32 autoshotTime = 0;
	if (ConfMan.hasKey("autoshot"))
		autoshotTime = g_system->getMillis() + (ConfMan.hasKey("autoshot_delay") ? ConfMan.getInt("autoshot_delay") : 5000);

	Common::Event e;
	Graphics::FrameLimiter limiter(g_system, 60);
	while (!shouldQuit()) {
		if (autoshotTime && g_system->getMillis() >= autoshotTime) {
			autoshotTime = 0;
			Common::DumpFile out;
			if (out.open(Common::Path(ConfMan.get("autoshot"), '/'))) {
				Image::writePNG(out, *_screen->surfacePtr());
				out.close();
				debug(1, "Wrote screenshot %s", ConfMan.get("autoshot").c_str());
			}
			if (ConfMan.getBool("autoshot_quit"))
				quitGame();
		}

		if (!_nextScene.empty())
			switchScene();
		if (!_scene)
			return Common::kNoGameDataFoundError;

		while (g_system->getEventManager()->pollEvent(e))
			_scene->handleEvent(e);

		while (!_autoClicks.empty() && g_system->getMillis() - startTime >= _autoClicks[0].time) {
			AutoClick c = _autoClicks[0];
			_autoClicks.remove_at(0);
			debug(1, "Auto click at %d,%d", c.x, c.y);
			g_system->warpMouse(c.x, c.y);
			e.mouse = Common::Point(c.x, c.y);
			e.type = Common::EVENT_MOUSEMOVE;
			_scene->handleEvent(e);
			if (c.moveOnly)
				continue;
			if (c.mode != 'u') {
				e.type = Common::EVENT_LBUTTONDOWN;
				_scene->handleEvent(e);
			}
			if (c.mode != 'd') {
				e.type = Common::EVENT_LBUTTONUP;
				_scene->handleEvent(e);
			}
		}

		for (uint i = 0; i < _autoKeys.size();) {
			if (g_system->getMillis() - startTime < _autoKeys[i].time) {
				i++;
				continue;
			}
			AutoKey k = _autoKeys[i];
			_autoKeys.remove_at(i);
			debug(1, "Auto key %d '%c' %s", k.key, k.ascii ? (char)k.ascii : ' ', k.down ? "down" : "up");
			e.type = k.down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
			e.kbd = Common::KeyState(k.key, k.ascii);
			_scene->handleEvent(e);
		}

		_scene->update();
		_music->update();
		_scene->draw(*_screen);

		limiter.delayBeforeSwap();
		_screen->update();
		limiter.startFrame();
	}

	return Common::kNoError;
}

Common::Error FlaaklypaEngine::syncGame(Common::Serializer &s) {
	// Saving is not implemented yet. The original keeps per player state in
	// common/profiles/*.pfl inside the data directory.
	return Common::kUnknownError;
}

} // End of namespace Flaaklypa
