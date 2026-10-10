# Flåklypa Grand Prix (PC, 2000-2002) - engine notes

Status: scene framework in place (see "Engine structure" below). The main
menu and the first story page (`yard`) work: backdrop, hotspot mask, cursor
tables, z-sorted Smacker/bitmap elements, animation sequences, the two
characters with idle/bored/reaction lists, timers, music, and Bink audio in
Smacker files (added to `video/smk_decoder.cpp`). The first sub game,
`puzzle` ("Solines smykkeskrin", see below), is playable. Not done: dialogs
(navigator, help, fact, award, message box), profiles and high scores, scene
index, other story pages, the other sub games, racing.

## The game

* Developer: Capricornus Computer Game Productions AS (Caprino Video Games),
  Norway. Editions: original (2000), Jubileumsutgave (2001), Gullutgave (2002).
  Norwegian, Swedish, Danish and English text exists in the language packs.
* Custom C++ engine. Win32, DirectDraw (`ddraw.dll`), DirectSound, WinMM,
  RAD Smacker (`smackw32.dll`), IJG libjpeg (used only by the 3D-glasses
  "anaglyph" add-on and screenshots). No scripting language: all scene and
  mini game logic is compiled into `FGP.exe` (about 1.1 MB unpacked).
* Screen: 800x600, 24-bit backdrops, 8-bit RLE hotspot masks.
* The retail disc uses Link Data Security (CD-Cops) copy protection
  (`CDCODE.KEY`, `FGP.W_X`, `FGP.QZ_`). This has no bearing on the data files.
* Configuration is stored in the registry under
  `HKLM\SOFTWARE\CapricornuS AS\Flåklypa Grand Prix\1.00.000`
  (`Data Path`, `Language`, `Layouts`, `Profiles`, `ScreenShot`, `Windowed`,
  `Debug Log`, ...).

## Install layout

The CDs are plain ISO 9660 (the archived `.mdf` images are raw 2448-byte
sectors, Mode 1: strip 16 bytes of sync/header and 384 bytes of EDC/ECC/sub-
channel per sector to get a mountable ISO).

```
<install>/bin/FGP.exe
<install>/data/<scene>.bin      48 containers, main game (all languages)
<install>/lang/<scene>.bin      48 containers, Norwegian text, fonts, dubbed clips
<install>/data1/<scene>.bin     CD1: add-on set 1 (subtitles, extra fonts, hopscotch...)
<install>/data2/<scene>.bin     CD1: add-on set 2 (balloonhunt, colorfill, mahjong, puzzle,
                                textinvader, whackamole, 2 extra race tracks)
<install>/data3/<scene>.bin     CD2: add-on set 3 (anaglyph 3D, gametrivia, mathlab, synonym)
<install>/lang1..3/<scene>.bin  CD2: language packs for the add-on sets
<install>/data/fence/tunes/*.tun  loose files (music editor tunes)
```

Each container holds one *scene*: a story page (`yard`, `desk`, `house`, ...),
a sub game (`beemaze`, `bugzzz`, `chess`, ...), an activity, or a shared pool
(`common`). `data/sceneindex.bin` + `scene.ini` list the story pages and games
and which cursor to show for each.

## `DATA` container format

See `archive.h`. 12-byte header (`"DATA"`, u32 version = 0x100, u32 count),
followed by `count` entries of `char name[260]` (uninitialised bytes after the
NUL), `u32 size`, `byte data[size]`. No index, no compression. Names use
backslashes and are relative to the scene (`animation\idle.smk`).

Totals for the Gold edition: 6264 files, about 1 GB uncompressed.

| Type | Count | Notes |
|------|-------|-------|
| `.smk` | 1735 | Smacker SMK2/SMK4. Animations, cursors, music (`common/music/*.smk`), speech. 425 are 4x4 "audio only" clips. |
| `.bmp` | 3682 | 24-bit backdrops/buttons, 8-bit RLE `hotspots.bmp` masks, 8-bit fonts (`common/fonts/*.bmp`), race tracks (2000x1600 8-bit). |
| `.wav` | 516 | PCM 16-bit mono 22050/44100 Hz (`fence` music editor, `bugzzz`, racing). |
| `.ini` | 244 | Windows INI. `sound.ini` volumes, `help.ini` help text, `track.ini` / `car.ini` / `stage*.ini` race parameters, `scene.ini` scene index, `language.ini` all UI strings, `subtitle/*.ini` subtitles. |
| `.pfl` | 10 | Player profiles (save games). |
| `.tun` | 20 | Tunes for the `fence` music editor. |
| `.nod`, `.ai`, `.l`, `.map`, `.eff`, `.fff` | few | Binary race track collision/path data and `mountain`/`balloonhunt` level maps. Not yet analysed. |

INI files are Windows-1252. The executable also reads `*.lst` files (not found
in the data; probably runtime generated).

## How a scene works (from `FGP.exe` strings)

* `%s/%s/backdrop.bmp`, `%s/%s/hotspots.bmp`: static scene art and hotspot
  mask. Mask index 0 = nothing, 1..99 = animation hotspots, 100+ = other kinds
  (sub game entry, fact, music...). The mapping from index to action is in
  the executable, per scene.
* `%s/%s/animation/%s.smk` and `%s-o.smk` (overlay variant), `%s/%s/sound/%s.wav`,
  `%s/common/cursors/%s.smk`, `%s/common/fonts/%s.bmp`, `%s/common/music/%s.smk`,
  `%s/common/profiles/%s.pfl`, `%s/common/dialogue/<dlg>/...` for dialog boxes
  (`msgbox`, `awarddlg`, `highscore`, `navigate`, `calendar`, `roster`, ...).
* Modules seen in the binary: DRAW, WINDRAW, SOUND, PLAYER (Smacker playback),
  SCENE, TITLE, GAME, EVENT, RACE, CAR, TRACK, PATH, SCORE, HISCORE, AWARD,
  PROFILE, CONFIG, TIP, DESC, ACTIVITY, PRINT, JOY, BEEMAZE (C++ class).
* Racing is a 2-D top-down game: 2000x1600 track bitmap, `terrain.bmp` for
  surface types, `map.bmp` mini map, `path.ai` AI waypoints, `collision.nod`.
  `car.ini` has accel/grip/mass and AI bias values.

## Plan

1. Scene framework: load backdrop + hotspot mask, cursor Smacker, play
   Smacker animations at hotspot positions (ScummVM has `Video::SmackerDecoder`).
2. Language strings from `lang/common.bin:language.ini` via `Common::INIFile`.
3. Menu, scene index, story page navigation, `help.ini` dialogs.
4. Sub games one by one (each is a small hard-coded game).
5. Racing game (2-D).
6. Profiles (`.pfl`) as save games.

Reverse engineering of per-scene hotspot behaviour needs disassembly of
`FGP.exe` (the Gold v1.2 update replaces it, so work from the updated one).

## Detection

MD5 (first 5000 bytes) and sizes, Gold edition v1.2 (CD and installed copy
are identical):

```
data/sceneindex.bin   98f40e436b689e94407e60d993ae09da  3237616
lang/common.bin       0bb1e8f5105fa776e179633e1de0a2b7  27031
```

## Engine structure (from the disassembly of the Gold v1.2 `FGP.exe`)

The unpacked executable (`Crack/FGP.exe`, 1.1 MB, MD5
19e24fe177af95dfea49e50fdfb478f6) was analysed with Ghidra headless
(`flaaklypa-work/ghidra/`, decompiled C in `out/all.c`; `tools/fn.py ADDR`
prints a function, `tools/fnat.py REGEX` finds functions by body).

### Scene descriptors (55 in `.data`)

```
struct SceneDesc {        // e.g. yard at 0x4cb6c8
  int type;               // 1 system screen, 2 story page, 3 sub game, 4 activity
  char *name, *parent;    // "yard", "menu"
  char *title;            // language.ini key "yard:SCENENAME"
  CursorEntry *cursors;   // {hotspot, cursorName} pairs, (-1, "default") terminated
  int unused[2];
  int width, height;      // 800, 600
  void (*handler)(Event*);// per scene event handler
  int flags;
};
```

`tools/gen_scenedata.py` extracts these, all animation structures and all
cursor tables into `scenedata.cpp`; `scenelists.txt` lists the animation
lists (sequences) of each scene for hand transcription.

### Animation structures (0xa8 bytes each)

```
struct Anim {
  char name[64];          // "S01AN-LUD-001" -> animation/<name>.smk, or bitmap/<name>.bmp
  int isSmacker;          // +0x40
  int visible;            // +0x44 0 for audio only clips
  int transparent;        // +0x48 colour key = pixel (0,0) of the frame
  int loop;               // +0x4c
  int hotspot;            // +0x50 hotspot index of the element's opaque pixels
  int x, y;               // +0x54, +0x58 default position
  int unused;             // +0x5c
  uint32 group;           // +0x60 character bits shown by the clip
  int zOrder;             // +0x64 default z (used by SCENE_PlayAnim)
  int hasOverlay;         // +0x68 "<name>-o.smk" (unused in the Gold edition)
  ... runtime fields (position, z, playing, added, player handles, rects)
};
```

Up to 500 elements are active; they are drawn backdrop first, then sorted by
z (`qsort`), then the cursor. Hotspot lookup walks the elements top down
(pixel exact for transparent ones) before the mask.

### Original API, as mirrored by `Scene`

| Original | Engine |
|----------|--------|
| `SCENE_AddAnim(anim, x, y, z)` (-1,-1 = default pos) | `Anim::add` / `Scene::addAnim` |
| `SCENE_RemoveAnim`, `SCENE_PlayAnim`, `IsAdded` (+0x7c), `IsPlaying` (+0x78) | `remove`, `play`, `isAnimAdded`, `isAnimPlaying` |
| `FUN_0040a6f0(list)` play an anim list as a sequence; `FUN_0040a770(anim)` single | `playSequence`, `playSingle` (cursor hidden meanwhile) |
| `FUN_0040a2b0(id, hotspot)` create character, `FUN_0040a4b0(id, z)`, `FUN_0040a470(id, idle, bored, reaction)` | `addCharacter`, `setCharacterZ`, `setCharacterAnims` |
| `FUN_0040a880` reset characters to idle (space bar) | `resetCharacters` / `stopSequence` |
| `FUN_0040ef80(table)` set cursor table; `FUN_0040c370("")`/`("wait")`/`(0)` | `setCursorTable`, `hideCursor`, `showWaitCursor`, `showCursor` |
| `FUN_0040b1f0(name, 0)` music common/music/<name>.smk | `playMusic` |
| `FUN_0040c480(time, 0, id)` timer | `setTimer` |
| `FUN_0040cd70(name, 0)` start scene/sub game | `changeScene` / `startGame` |

Characters: state 1 idle (plays the idle list cyclically), 2 bored (one clip
of the bored list every 15-25 s), 3 reaction (next clip of the reaction list
after the current one ends, wait cursor meanwhile), 5 sequence. A clip whose
group bits include a character replaces that character's current clip; a
character whose current clip has not ended blocks clips of its group.

### Events dispatched to the scene handlers

0x103 init(arg), 0x104 close, 0x105 mouse down(x, y, hotspot), 0x106 mouse
up, 0x107 right button, 0x108 mouse move (x, y, hotspot; every move), 0x109
mouse move with button held (0x10a drag), 0x10c key, 0x10f anim started,
0x110 anim finished (anim), 0x111 timer(id, data), 0x112 frame tick, 0x113
hotspot change, 0x116 button pressed (button id, from the BUTTON module),
0x11d double click, 0x11f sequence done. `menu` acts on mouse up, story pages
on mouse down. The producers are `FUN_0040c590` (0x105) .. `FUN_0040c770`
(0x106) in the EVENT module.

### Hotspot index ranges (story pages)

1..3 sub game/activity entries, 55..99 animation hotspots, 100..150 misc,
151..199 fact pages (`fact.ini` `hotspot=`), 251/252 the characters,
254/255 hidden 3D glasses / car part bitmaps.

### Audio

Nearly all clips with sound use Bink RDFT audio inside Smacker (flag bit 27),
22050 Hz 16 bit, with a one second prebuffer in frame 0. Music and narration
are 4x4 pixel clips. `electric`, `wind` and the like are sound effects
played as one element sequences.

## Sub game `puzzle` ("Solines smykkeskrin", add-on set 2)

An Atomica clone (the executable even asserts `ATOMICA_spawnAtoms() != NULL`).
Handler `0x454fb0` (not a function in the Ghidra project; dispatches 0x103 ->
`FUN_00455090`, 0x104 -> `455320`, 0x105 -> `4553e0`, 0x108 -> `455360`,
0x110 -> `455bc0`, 0x112 -> `455890`, 0x116 -> `432d10`). Engine:
`puzzle.cpp`, renderer `gem3d.cpp`, meshes `puzzledata.cpp` (generated by
`flaaklypa-work/tools/gen_puzzledata.py`).

* Board: 9x8 cells, 20 units apart, at world (-80.42 + 20c, -69.61 + 20r,
  43.44) (table at `0x49f26c`, 72 x 3 floats, reordered by `FUN_00452720`).
  Everything from the executable goes through the 3D module's coordinate swap
  (x, y, z) -> (x, -z, y). Camera at (-0.369, -208.73, 422.08) looking at
  (0.085, 44.62, 24.40); projection x = 400 + f X/Z, y = 300 + f Y/Z with
  f = 400 tan(1.2043) = 1042 (`FUN_00452da0`).
* Clicks map to cells through `bitmap/mask.bmp` (200x150 = quarter
  resolution): the cell regions are flood filled with their index from the
  projected cell centre (+17.5 on the swapped y) at init, see `load()`.
* Gems: `0x104` byte structs in a linked list plus a 72 slot grid. Each gem
  has its own 64x64 3D scene (`FUN_0045a950`), two objects (full size scale
  0.8, "small" scale 0.4 with half brightness) and an embedded animation
  struct whose surface is the sprite, placed at the projected centre minus 32
  at z 100. Types 0..7 colours, 8 joker (colour cycles), 9 gold, 10 silver
  (`0x501ff0`: r, g, b words per type). Meshes at `0x4e6fa8` (types 0, 6, 7,
  8), `0x4ecfb8` (1, 2), `0x4effc0` (3), `0x4f2fc8` (4), `0x4f5fd0` (5),
  `0x4fbfe0` (gold), `0x4fefe8` (silver), `0x4f8fd8` (a disc): `{int16
  nverts, int16 nfaces}`, doubles x, y, z from +8, triangle indices as doubles
  from +0x1808.
* Renderer (`FUN_00458bf0` objects, `FUN_00459ad0` render): vertex normals
  from the face normals, look-at camera, painter's algorithm, back face test
  in screen space, per face light `0.5 - 0.5 nz` (view space face normal, no
  light source), colour = base * light * kd + pow(light, shininess) * ks *
  255 clamped to 200 (kd 0.5, ks 1, shininess 50), saturating-added to the
  environment mapped `illum.bmp` (texel from the vertex normal, (n + 1) *
  32). Gold and silver average `illum` (spherical mapping from the vertex
  position) with `gold.bmp` / `silver.bmp` (environment mapped). 16 bit
  RGB565 in the original.
* Rules (`FUN_004553e0` click, `FUN_00455670` move, `FUN_00453f70` matches):
  select a full gem, click a free cell (empty, small or dying gem) with a
  4-neighbour path over free cells. Every 2x2 block of one colour (specials
  match anything; the check has quirks with empty cells, mirrored) is grown
  to the largest uniform rectangle and removed. No match: all small gems grow,
  then a new set spawns: one small gem per colour in play (level n = n
  colours) times 4/3/2/1 sets for levels 1/2/3/4+; every 35..49 spawns a
  random special. Match: score += gems (shown times 7), level points pass
  the thresholds 45, 55, 75, 80, 100, 150, 150, 150 (level 8 never ends);
  gold recolours all full gems to a random colour in play, silver kills about
  a third. Game over when fewer than 5 cells are empty or hold a movable gem;
  when no movable gem is left a full size set spawns.
* Timing: the tick works in 10 ms units: small gem appears over 75, growth
  and death take 100, game over message after 120. Selected gem wobbles with
  the noise functions `FUN_00452840`/`FUN_00452880`/`FUN_00452ab0`.
* Screen: `lid` (800x600 clip, first frame = closed box, z 120) opens on a
  click in the box; `lid2` closes it at game over, after which init runs
  again. `bottle` at (688, 94) is a 101 frame progress meter (frame =
  level points / threshold * 101). Score and level text in `Amerigo BT_10_`
  centred in (266, 43)-(380, 78) and (418, 43)-(533, 78). Path preview on a
  460x320 overlay at (170, 172) z 98: a Kochanek-Bartels spline through the
  path cells (engine: Catmull-Rom). `rotate` is the looping sound while a
  gem is selected, faded in and out (out three times faster). Buttons
  `help_0/1` at (2, 1) and `exit_0/1` at (726, 2). Music `track20`.
* Not done: the game over message box (`puzzle:GAMEOVER` / `puzzle:LOOSE`),
  high score registration (`FUN_0041f9a0`), help dialog, `sound.ini`
  volumes.

## Sub game `wheelbarrow` ("Eplehøsten", started from `pee`)

Catch the apples that ripen on the roof. Handler `0x44ed50` (not a function
in the Ghidra project; the jump table dispatches 0x103 -> `FUN_0044ee50`
init, 0x104 -> `44f5f0` close, 0x105 -> `44f630` mouse down, 0x10c ->
`44f640` key down, 0x10d -> `44f730` key up, 0x110 -> `44f780` anim
finished, 0x111 -> `450270` timer, 0x112 -> `44fb70` tick, 0x116 ->
`429320` buttons: 1 help `FUN_0041e580`, 2 exit `FUN_0040cc30`). Engine:
`wheelbarrow.cpp`. The hotspot mask is empty; the only hotspots are the
`start` bitmap (3) and the two buttons.

* State (`.bss`): running `0x68ba28`, score `ba2c`, level `ba30`, apples in
  the basket `ba34`, lives `ba38`, basket capacity `bae8` (24), Solan's
  animation struct `baf0` with x `bb98` (float, start 324), direction
  `bb9c` (0 left, 1 right), speed `bba0` (300 px/s), apples in the
  wheelbarrow `bba4`, Solan's state `bba8`; 12 apple counter bitmaps at
  `bbb0` (`apple` at (208 + 30 i, 3), z 100), 10 pie bitmaps at `c398`
  (`pie` at (732, 546 - 10 i), z i - 100), spawn timer handle `c394`, 10
  apple structs at `ca28` (0xc4 bytes: active, type 0 good / 1 rotten, x,
  y floats = sprite centre, bounce stage, vy, clip state, animation
  struct). The engine keeps these as `Anim` objects of its own with
  mutable `AnimDef`s (the original sprintf's the clip names into them).
* Solan (`FUN_0044f300`): clip `<left|right>_<neutral|cycle|stop|dump|bonk>
  <fullness>` at (round(x), 478) z 100, fullness = apples * 5 / 12 (0..5).
  `neutral` is a bitmap, `cycle` loops while an arrow key is held (x +=
  300 dt, clamped to 55..578), `dump` plays once. `stop` and `bonk` exist
  as files but nothing in the executable starts them. Keys (`FUN_0044f640`,
  `44f730`): left/right start and stop walking, up tips the wheelbarrow
  when x <= 60, facing left, with apples and a basket present (sound
  `empty1`); after `dump` ends (`FUN_0044faf0`) the apples go into the
  basket (20 points each) and walking resumes if a key is still held
  (`FUN_0044f870`, GetAsyncKeyState -> `onKeyUp` tracking).
* Apples: the spawn timer fires 2 s after (re)start and then every 3750 -
  300 level ms (`FUN_0044f550`, period re-armed by the EVENT module; a
  negative period at level 13 would hang the original, clamped to 50 ms).
  `FUN_00450270` picks one of 10 roof spots (`0x49f158`: x, y, start
  stage), 10 % rotten, plays `<apple|rotten>_0` once (ripening, the apple
  stands still), then `_1` looping while falling (started by the first
  bounce). Physics `FUN_0044ff80` per tick: y += vy dt, vy += 600 dt; the
  sprite is centred on (x, y) (`FUN_00412cf0`). Roof table `0x49f1d0`:
  stage n bounces when y >= 273/300/335/362/570 with action 0/0/0/1/3,
  vy = -vy * {0.5, 0.8, 0.3, 0.2}[action], sounds `roof1-4` / `rotten1-3`
  for actions 0-1 and `ground1-2` / `splash1-4` for the ground (action 3).
  A good apple on the ground costs a life (`lifedown1`; game over when
  none left), a rotten one plays `rotten_2` and vanishes. Apples past
  y 620 are removed. Z = slot index + 10 stage.
* Catching (`FUN_0044fe50`): only apples in stage 4 (past the eaves); the
  union of the sprite's rectangles before and after the move is tested
  against the wheelbarrow, (x + 39, 520, 94 x 65) facing left or (x + 57,
  520, 112 x 65) facing right. Good apple: `cart1-3`, counter + 1, 10 + 2
  level points (`(level * 0.2 + 1) * 10` truncated); when the wheelbarrow
  holds 12 the apple drops through. Rotten: `splash`, the load is lost.
* Basket (`FUN_0044f280`): `basket<n>` at (54, 535) z 50 with n = apples *
  4 / 24 clamped to 3. Tipping 24 or more (`FUN_0044faf0`) stops the spawn
  timer, removes the basket and plays `pickup` (Ludvig carries it off) ->
  `pie` (the pie out of the window) -> level + 1, 500 points, pies
  redrawn, timer restarted -> `putdown` -> `basket0`.
* Score (`FUN_0044f8d0`): every 3000 points give a life (`lifeup1`). Start:
  3 lives, level 0 (with a profile the original starts at 3 x the
  profile's value and shows `tournament:PLAYERREADY`). The wheelbarrow
  counter is not reset by a new game (quirk of `FUN_0044f110`).
* Screen: `help_0/1` at (0, 0), `exit_0/1` at (728, 0), z 100; `start`
  (353, 267) z 100 with `wheelbarrow:START` in `Amerigo BT_14_` centred in
  (356, 296, 89 x 33) z 101; labels `wheelbarrow:SCORE` / `:LIFE` centred in
  (6, 85)-(85, 110) and (715, 85)-(795, 110), values below in (6, 110)-
  (85, 135) and (715, 110)-(795, 135), z 200. Music `subgame18`, ambient
  loop `birds`. `sound.ini` volumes are not applied.
* Game over (`FUN_00450180`): Solan neutral, pie/pickup/putdown removed,
  then (not done) the message box `wheelbarrow:GAMEOVER` via
  `interfaceh:GAMEOVER` and the high score registration `FUN_0041f9a0`
  (`wheelbarrow:LONGNAME`, medals 2500/5000/10000/15000); without a
  profile the start button returns and the apples are cleared. Also not
  done: the help dialog, the "abort the game?" box (`gamec:SUBGAMEABORT`,
  flag `FUN_0040d8a0`) when leaving a running game, Tab to the scene index.
* Testing: `autokey=t:key[:hold];...` (see "Development aids") presses the
  arrow keys; the start button is at (400, 300).

### Bitmap fonts (FONT module, `font.cpp`)

`common/fonts/<name>.bmp`, 24 bit, 256 glyphs in a strip. The top row marks
the glyphs: pixels equal to the top left pixel are separators, a run of other
pixels is one glyph (char code = glyph index). The colour key is the first
glyph's top left pixel. Glyphs keep their colours; the advance is the glyph
width. Text areas are animation structs with a surface filled with the key
colour 0x00ff00, flags 0x101 = centred.

## Development aids (config keys in the `[flaaklypa]` section)

`start_scene`, `autoshot` / `autoshot_delay` / `autoshot_quit` (screenshot),
`autoclick=t:x,y;t:x,y,m` (synthetic clicks, `m` = move only, t in ms after
start), `autokey=t:key[:hold];...` (key press at t held for hold ms, default
100; `left`, `right`, `up`, `down`, `space`, `esc`, `tab`, `return` or a
character), `random_seed` (deterministic boards).

## Mini game checklist

From `sceneindex/scene.ini` and `language.ini` (Gold edition). Tick off as
they get ported; each needs a scene class, `createScene()` and
`FlaaklypaEngine::startGame()`.

Sub games (`[subgame]`, score based):

| Done | Scene | Title | Started from | Data |
|---|---|---|---|---|
| | lettersort | Postsorteringsmaskinen | yard | data |
| | bugzzz | Larveliv i leiren | yard | data |
| | sockdrawer | Sokkeskapet | desk | data |
| x | wheelbarrow | Eplehøsten | pee | data (message box, high score, help open) |
| | hopscotch | Solan og Ludvig i Paradis | house | data |
| | audiopairs | Reodors Lydmaskin | house | data |
| | textinvader | Ordspillet | goodbye | data2 |
| | balloonhunt | Solans ballongjakt | outtent | data2 (no sceneDefs entry yet) |
| | whackamole | Dra meg baklengs! | house | data2 |
| | beemaze | Ludvigs Labyrint | tvroom | data |
| | buildabike | Reodors sykkelverksted | garage | data |
| | mountain | (no title in language.ini) | morning | data (no sceneDefs entry yet) |
| | pipeline | Oljeeventyret | outtent | data |
| | butterfly | Sommerfugler i magen | pee | data |
| | hustle | Emanuels utfordring | intent | data |

Activities (`[activity]`, open ended):

| Done | Scene | Title | Started from | Data |
|---|---|---|---|---|
| | gallery | Karakter galleri | tvroom | data |
| | bouquet | Ludvigs blomsterbinderi | desk | data |
| | fence | Musikkgjerdet | pee | data |
| | colorfill | Reodors tegnebord | desk | data2 |
| | jigsaw | Puslespillet | yard | data |
| | mahjong | Mah Jongg | outtent | data2 |
| | draughts | Damm | tvroom | data |
| x | puzzle | Solines Smykkeskrin | intent | data2 (message box, high score, help open) |
| | sliding | Skyvepusslespillet | garage | data |
| | chess | Sjakk | intent | data |
| | movieplayer | Filmfremviser | tvstation | data |
| | activity | Aktivitetssenteret | sceneindex | data |

Add-on set 3 (CD 2, fourth index group):

| Done | Scene | Title | Data |
|---|---|---|---|
| | gametrivia | Kjentmannsprøven | data3 |
| | anaglyph | 3D-titter | data3 |
| | mathlab | Reodors Tallmaskin | data3 |
| | synonym | Reodors Ordmaskin | data3 |

Not games: `buildacar` and `racing` are story pages, `buttercol`
(Sommerfugl kolleksjon) is the collection screen of `butterfly`.
