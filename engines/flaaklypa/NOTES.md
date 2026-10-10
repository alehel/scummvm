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

## Sub game `sockdrawer` ("Sokkeskapet", from `desk`)

A memory game: a cabinet with 5x4 hatches hides pairs of socks. Handler
`0x44b640` (jump table; 0x103 -> `FUN_0044b720`, 0x104 -> `44c0b0`, 0x105
-> `44c0e0`, 0x110 -> `44c190`, 0x111 -> `44c400`, 0x112 -> `44c560`, 0x116
-> `429320`, the generic help/exit button handler). Engine: `sockdrawer.cpp`.

* Data: 20 drawer records of 0x160 bytes at `0x680030`: has-sock (+0),
  closed (+4), hatch variant (+8, `rand() % 1`, always 0), the hatch
  animation struct (+0xc, name `drawer<variant+1>open` / `...close`,
  transparent clip at x = 189 + 114 col, y = 118 + 114 row, z 10), sock type
  (+0xb4) and the sock bitmap struct (+0xb8, `bitmap/Sock<type+1>.bmp` at
  hatch + (15, 13), z 0). Globals: score `0x680020`, level `0x680024`,
  state `0x680028` (0 idle, 1 closing hatches, 2 opening the empty ones,
  3 playing), level start time `0x67ee90`, hourglass frame `0x67ee94`,
  pending timer `0x681bb0`. Hotspots: 3 = the small "start" cabinet (the
  `start` / `points` clips at (33, 172)), 101..120 = hatches (mask).
* Level table at `0x49ef48`, 15 x {socks, kinds, ms}: {4,3,20000}
  {6,4,35000} {8,5,50000} {10,6,75000} {12,7,950000 (sic)} {14,8,110000}
  {16,9,110000} {18,10,90000} {20,12,80000} {20,14,70000} {20,16,60000}
  {20,18,60000} {20,20,50000} {20,22,50000} {20,24,40000}; the level stops
  at 14. Each pair picks a random kind among the first `kinds` entries of
  the order table `0x49effc` (0 1 2 6 7 9 10 13 16 18 5 8 11 12 14 17 3 4
  15 19 20 21 22 23) not already in the cabinet (100 tries, then the first
  free one) and two random empty drawers (100 tries, then the first free).
* Flow: init adds every hatch as the first frame of its `open` clip (the
  original passes `closed == 0`, so the hatches look closed), `start` (z
  -10, covers the score) and `points` (z -21), the title in `Amerigo BT_18_`
  centred in (335, 44)-(601, 86) and the score in `Amerigo BT_14_` in (55,
  192)-(122, 216), the hourglass bottom `timerbottom32`, music `subgame3`.
  Click on the cabinet in state 0: score/level 0, close all open hatches
  (sound `allclose`), state 1, play `start`. When no hatch clip plays
  (`FUN_0044bb80` after every hatch clip): state 1 -> remove socks, place
  the pairs, open the empty hatches (`allopen`), state 2 -> play
  `timerrotate` (z -20); when it ends: `timertop00` + `timerbottom02`,
  play `timermiddlestart` (z -21) then the looping `timermiddlerunning`,
  level clock starts, state 3.
* Rules (`FUN_0044c130` click, `44bfb0` hatch opened, `44c400` timer): a
  closed hatch opens (`oneopen`) while fewer than two socks show. Once two
  show, a 400 ms timer is set and an equal pair scores 25 at once (`points`
  clip, score text). At the timer: unequal -> both close (`oneclose`);
  equal -> both socks vanish (hatches stay open); all gone -> bonus
  `(int)(200 * level * remaining / time)`, level + 1, close all, state 1.
* Timer (`FUN_0044c5d0` every frame): frame = `(int)(30 / time * elapsed)`,
  `timertop<f>` at (45, 359) and `timerbottom<f+2>` at (46, 473) are
  re-added when it changes. elapsed > time: remove the running clip, bottom
  back to frame 32, play `timermiddleend`; when it ends (`44c380`): message
  box `sockdrawer:ENDMESSAGE` / `interfaceh:GAMEOVER`, high score
  registration `FUN_0041f9a0(0, score, {2500, 5000, 10000, 20000}, cb)`;
  the callback closes all hatches and re-adds `start`.
* Buttons `help_0/1` at (0, 0) and `exit_0/1` at (728, 0), hotspots 31/32.
* Engine notes: hatches, socks, hourglass frames and the four sound clips
  are `Anim` objects owned by the scene with mutable `AnimDef`s (the
  original sprintf()s into the name field); the sounds are played as single
  elements instead of `SCENE_PlayAnimClone` clones.
* Not done: message box, high score registration, help dialog, the
  "sub game in progress" flag (`FUN_0040d8a0`, abort confirmation on exit),
  tournament mode (`FUN_00419490`), `sound.ini` volumes.
## Sub game `hopscotch` ("Solan og Ludvig i Paradis", add-on set 1)

A Simon says game on a hopscotch grid. Handler `0x43bbd0` (jump table, not a
function in the Ghidra project): 0x103 -> `FUN_0043bc90` init, 0x104 ->
`43c7a0`, 0x105 -> `43c7d0` mouse down, 0x110 -> `43c980` anim finished,
0x112 -> `43cbc0` frame tick, 0x116 -> `43c910` button. Engine:
`hopscotch.cpp`. Data: `data/hopscotch.bin` plus `data1/hopscotch.bin` (the
B/C/D tone sets and Solan's start poses), `lang/hopscotch.bin` (help.ini).

* Flow (`DAT_0066ea48` state): 0 idle, 1 Solan hops the visible part of the
  sequence (`FUN_0043c580`, one hop per 0x110 of his clip), then the
  hourglass is removed and `rotate` plays; when it ends the `timer` clip is
  added and the turn starts (state 3). The player clicks squares
  (`FUN_0043c800`, 75x75 rectangles at the square positions); Ludvig hops
  there (`ludjump<dir>`) or falls (`ludfall<dir>`) when it is the wrong one,
  which resets him to the start and the progress to 0. Each correct square
  gives 5 points; the last one 100 plus 5 per full second left on the
  hourglass (`FUN_0043cb90`), then `bird` plays and the next round starts
  (`FUN_0043cb60`). The start button becomes the hint button (state 2:
  Solan repeats the sequence while the hourglass keeps running).
* Levels (`0x49e788`, 16 x {base length, rounds, grid, max squares, time ms,
  symbols}): the sequence of a level is generated once with base + rounds
  squares (`FUN_0043c460`: random squares, each square used once before any
  repeats, `FUN_0040a150`); round r shows base + r of them, capped at base +
  rounds - 1 (`FUN_0043c5e0`). Levels 0..2 grid 1 (6 squares), 3..5 grid 2
  (9), 6..8 grid 3 (9), 9..11 grid 4 (12), 12..15 base lengths 8/10/12/14 on
  grids 1..4 again; level 15 repeats forever. Times 22..28 s. Without a
  profile the game starts at level 0 (with one: 3 x the profile difficulty).
* Screen: grid bitmaps `grid1..4` at (320,192) (373,200) (206,220) (251,195)
  (`0x49e628`), square bitmaps `number01..12` / `symbol01..12` at the offsets
  of `0x49e648..` z 2, jump clips 152x152 at square - 38 z 5, start positions
  `0x49e5e8` (Ludvig) / `0x49e608` (Solan). Facing from the hop vector,
  `hs_GetDeltaDirection` (`FUN_0043bfc0`): 0 up, 45, 90 right, ... 315;
  components above 0.5 decide. A hop moves the sprite linearly over 500 ms
  (`FUN_0043cce0`); on 0x110 the jump clip is re-added at the target as the
  standing pose and the square's tone `<prefix><n>.smk` (prefix "", B, C, D
  per grid) is played as a clone (`FUN_0043ca70`). The hourglass `timer` (33
  frames, z -10) shows frame round((frames - 1) / time * elapsed); past the
  last frame the game is over (`FUN_0043cc40`, `FUN_0043cd90`). Score box: a
  text area (315,2)-(506,31) z -10, `Amerigo BT_14_` centred; it shows the
  scene title in `Amerigo BT_10_` before a game and after game over.
* Buttons (BUTTON module, `FUN_00406ed0`): `help_0/1` at (0,0), `exit_0/1`
  at (728,0), start at (13,497) with no bitmap in the normal state, `start_1`
  pressed, `start_2` disabled, label `hopscotch:START` / `hopscotch:HINT` in
  the button font (`Amerigo BT_14_`) centred one z above. A press shows the
  pressed bitmap, the release anywhere fires 0x116. The buttons' pixels have
  their id as hotspot and the scene handler ignores clicks on 1, 2 and 10.
  The engine keeps this button logic inside `hopscotch.cpp`.
* Not done: the game over message box (`interfaceh:GAMEOVER` with
  `hopscotch:GAMEOVER`), high score registration (`FUN_0041f9a0`, medals
  3000/5000/7000 and 10000 at `0x49e918`), help dialog, the "abort the
  game?" question on exit (`DAT_0054bf98`), profile start level and the
  tournament "PLAYERREADY" box.
## Sub game `audiopairs` ("Reodors Lydmaskin", house)

A sound memory game on Reodor's cash register. Handler `0x428ae0` (a jump
table, not a function in the Ghidra project): 0x103 -> `FUN_00428bd0` init,
0x104 -> `4291b0` close, 0x105 -> `429200` mouse down, 0x10c and 0x112 ->
`432fd0` (empty), 0x110 -> `429340` anim finished, 0x111 -> `429600` timer,
0x116 -> `429320` buttons (1 help `41e580`, 2 exit `40cc30`). Engine:
`audiopairs.cpp`.

* Data: `hotspots.bmp` is all zero; every hotspot comes from an element.
  20 keys (`FUN_00428e20`, 0xb4 byte structs at `0x5bb038`: int active,
  Anim (hotspot 10 + index, positions at `0x4cb720`, 5 x 4), int state, int
  sound). The key bitmaps are `bitmap/<colour><row>.bmp`, colour = state
  (0 green untried, 1 yellow pressed, 2 red found, 3 black out of play), row
  = index / 5 (perspective); z = row, hit test mode 2 (rectangle,
  `FUN_00428ff0`). Machine parts (`FUN_00428d80`): `bitmap/v01..v14` (left
  machine, positions `0x49d660`) and `h01..h14` (right, `0x49d6d0`), z =
  index, counts at `DAT_005bbe58/5c`. `pig` at (373, 519) z -10, `lever`
  (560, 115) z 10, `start` (looping clip on the lever handle, hotspot 3,
  z 50; in the lang container), `left` (0, 84) z 14, `money` (234, 130)
  z 25, `right` (360, 0) z 14.
* Text (`FUN_00407a30` / `FUN_00407930`): labels `audiopairs:LIFE` and
  `audiopairs:POINTS` in `Amerigo BT_10_`, centred horizontally at the top
  of (300, 124)-(370, 139) and (433, 124)-(502, 139), z -11 (the 0x100 flag
  is left on the stack by MSVC, hence Ghidra shows it as a `LANG_Get`
  argument). Values in `Amerigo BT_18_` centred (0x101) in (300, 134)-
  (369, 163) and (433, 134)-(502, 163); the scene title centred in
  (243, 7)-(563, 61). Help/exit buttons via the BUTTON module at (0, 0) and
  (728, 0).
* Sounds: five groups `sound`, `music`, `birds`, `farm`, `horn`
  (`0x4cbc60`), group = level % 5, clips `animation/<group>01..10.smk`
  (4x4 audio only). They are played through the scene's one spare
  animation struct (`0x4cbbb8`, the `" "` entry; renamed with `"%s%02d"`,
  removed when done, `FUN_004292a0`). Parts play `animation/part01..15.smk`
  as clones (`FUN_00412c20`); part n is played when part n is added or
  removed (`FUN_00428f00`).
* State (`DAT_005bbe48`, `FUN_00428f80`): 0 idle, 1 playing, 2 two keys
  pressed (one shot timer 650 ms = `DAT_005bbe60`), 3 machine clips, 4
  left parts vanishing, 5 all parts vanishing (periodic timer 150 ms, see
  the asm: `FUN_0040c480(now + 150, 150, 0)`; `FUN_0040c450` is the
  time, not a random number).
* Rules: init shows all 28 parts and all keys green but inactive, plays
  `start`. Start (hotspot 3, `FUN_004290a0`): score 0, 15 tries, level 0
  (+ 2 x profile difficulty), state 5: one part (right side first) vanishes
  per tick until none are left, then a board. Board (`FUN_00429450`): all
  keys black and inactive, N pairs (`0x49d798`: level 0: 3, 1: 4, ... 6:
  9, 7+: 10) of random unused sounds on random free keys, `lever` plays.
  Key press in state 1 (`FUN_00429230`): plays the sound, yellow; the
  second press starts the 650 ms timer. Timer (`FUN_00429890`): both red
  when equal, else green. Miss: tries - 1, game over at 0. Hit
  (`FUN_00429700`): score + 80; if the left machine already has 14 parts
  (the count is tested before the part is added, so the machine runs on
  the 15th pair): level + 1, score + 25 x tries (added after the text was
  drawn), state 3, `left` plays; else a left part is added, and when no
  green key is left: a new board and tries + 3. Likewise `money` tests the
  right count before adding, so `right` runs on the 15th left machine. `left` done -> `money`; `money` done: if the right
  machine has 14 parts `right` plays, else `left` is removed, a right part
  is added and state 4 removes the left parts one per tick; tries + 5.
  `right` done: everything removed, counts 0, state 1, new board. Game
  over: state 0, message box `audiopairs:ENDMESSAGE` /
  `interfaceh:GAMEOVER`, high score `FUN_0041f9a0(0, score, 0x49d7d8 =
  {2000, 6000, 10000, 14000}, 0)`, `start` plays again unless a profile is
  active (then the game started automatically after
  `tournament:PLAYERREADY`).
* Engine notes: the keys are surface elements (`key00..19`) because several
  keys share one bitmap; the engine hit tests their opaque pixels where the
  original tests the rectangle. `Scene::defineAnim()` got a `visible`
  parameter (default true) for the audio only clips, which have no palette
  to convert a frame with.
* Not done: help dialog, game over message box, high score / award
  registration, profile difficulty, the GAME module "game in progress"
  flag (`FUN_0040d8a0`), `sound.ini` volumes.
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
## Sub game `lettersort` ("Postsorteringsmaskinen", yard)

Letters arrive through the post office window and are dragged into mail
bags on the conveyor belt above. Handler `0x43db30` (a jump table, not a
function in the Ghidra project; 0x103 -> `FUN_0043dc30`, 0x104 ->
`43e1e0`, 0x105 -> `43e230`, 0x10b drag release -> `43e390`, 0x110 anim
finished -> `43ea80`, 0x111 the 250 ms timer -> `43f750`, 0x112 ->
`43f3c0`, 0x116 buttons -> `43e370`). Engine: `lettersort.cpp`.

* Data: 30 letter structs (0xc0 bytes at `0x6745b0`: active, x, y, country,
  picture 0..11, gold, animation struct with hotspot 100 + index), 10 bag
  structs (0x168 bytes at `0x672578`: active, x, y, country, fill, gold,
  bag bitmap, flag bitmap), 9 claw structs (0xc0 bytes at `0x671eb0`:
  active, x, y, bag, state, gold, clip). Every bitmap and clip name is
  sprintf'ed into the struct while it is off screen (`ltr<Country>%02d`,
  `ltrGold01`, `Tiny <Country>` while carried, `bag%d[gold]`, `flg<Country>`,
  `claw{grab,retry,brown,gold}`, `bagout[gold]`, `level%02d`); the engine
  does the same with its own `Anim` objects (`Sprite`).
* Countries: `country.ini` sections in file order (scandinavia, europe,
  world), each shuffled with `FUN_0040a150` at every game start; level n
  plays with the first `countries` of the table at `0x4db5a0` (10 levels:
  bags to lift 5/12/20/29/39/49/59/69/79/-, seconds between bags
  14/12/11/10/9/8/8/7/7/6, between letters 4/4/4/3/3/2/2/2/2/2, countries
  3..12; `FUN_0043f2a0` never finishes level 10).
* Flow: start button (`s1_1_start`, hotspot 1 from the clip, the mask is
  empty) -> `FUN_0043dea0` -> the clip plays, then `FUN_0043ef00(0)`: level
  stamp at (626, 63), `stamp` sound, and Solan's delivery clip (`solpost1`
  at (294, 241); later levels pick one of `solpost1`, `soldrop1` (260, 225),
  `solopp`, `soldown` (294, 242)). When it ends ten letters drop on the
  floor (`dropall` sound) and a clip ending in "1" continues with its "2"
  part. `soljump` / `solrocket` idle clips every 120 s.
* Per second (`FUN_0043f750`, only while running): a bag every
  `bagInterval` s at (-57, 126) plus a pusher stroke; a letter every
  `letterInterval` s while no delivery clip plays; one queued full bag gets
  a claw. Letters land at random on the floor (28, 311)-(772, 573) (nominal
  size 161x104, bitmaps are 150x97), never under the mouse; the country is
  random among those in play with fewer than 11 letters on the floor; a
  bag's country has fewer than 4 bags on the belt, or, with 22 or more
  letters down, more than 2 letters waiting. Letter 100 and every 120th
  after it is gold (fills a bag alone, fits any bag); every 20th bag is
  gold (a full gold bag queues every bag for the claw).
* Pusher (`FUN_0043f670`, frame tick): bitmap 164x73 at y 115, x from -221
  to -23 in one second and back to -137 in the next; `FUN_0043f4f0` shoves
  the bags (84x62) so each starts at the previous one's right edge. The
  rightmost bag past x 614 falls off (`bagout` clip at (614, 112)), the game
  stops, `s1_1_bell` loops for 3 s, then the game over message box and the
  high score registration (`FUN_0041f9a0`, medals at 2500/5000/10000/20000)
  whose callback `LAB_0043fd50` resets and shows the start button again.
* Drag (`FUN_0043e2d0` / `FUN_0043e390`): the letter becomes its "Tiny"
  bitmap centred on the mouse at z 61. Released below y 188 it is put down
  centred there (clamped to the floor with +-2 jitter, `drop` sound); above,
  the bag whose x range holds the mouse x takes it when the flags match or
  the letter is gold (`drop`, +10, `bag%d` bitmap), otherwise `wrong` and
  the letter goes back. Letter z order counts 0..59, then all letters drop
  by 60 and the bell goes just under the lowest. A full bag is queued;
  the claw (`clawgrab` at (bag x, 48), z -10) retries (`clawretry`) when the
  bag moved meanwhile, else removes the bag, +100, and lifts (`clawbrown` /
  `clawgold`). Levels advance when `bagsCleared` reaches the table value.
* Screen: help (0, 0) and exit (728, 0) buttons, title `lettersort:LONGNAME`
  centred in (154, 3)-(641, 46) in `Amerigo BT_18_`, score in (46, 232)-
  (154, 281), `lettersort:POINTS` in (45, 276)-(154, 295) in `Amerigo BT_10_`,
  `borderleft` (0, 99), `s1_1_frame` (116, 48), bell (677, 324) z -45. z:
  bags -30 (`0x672574`), flags -15, bagout -31, pusher/stamp -45, Solan
  -44/-46, letters 0..59. Music `subgame8`.
* Not done: help dialog, game over message box and high score
  registration, tournament mode (`FUN_00419490`: immediate start at level
  difficulty * 3), the "game in progress" abort warning (`FUN_0040d8a0`),
  the registry value `FUN_0040b940` writes at init, `sound.ini` volumes. A
  delivery clip that is still playing when the next level starts is
  restarted (the original only renames the struct).
## Sub game `hustle` ("Emanuels utfordring")

A shell game. Emanuel the monkey hides a red ball under one of three golden
cups and shuffles them; the player bets bananas on where it ended up. A
correct guess doubles the stake, a wrong one loses it. Handler `0x43ce20`
(not a function in the Ghidra project; a jump table: 0x103 -> `FUN_0043cf20`,
0x104 -> `43d110`, 0x105 -> `43d880`, 0x10c returns 1 for the space bar so the
shuffle cannot be skipped, 0x110 -> `43d160`, 0x111 -> `43d950`, 0x116 ->
`43d980`, 0x117 (slider moved) -> `43d850`). Engine: `hustle.cpp`.

* Data: `data/hustle` has no backdrop; `lang/hustle` has it (plus
  `start_0.smk`, the clock with the "Start" text, and `bet_1.bmp`, the "Sats"
  coin), so `Scene::load()` finds it through the language container.
  `bitmap/cups.bmp` is all green (transparent) and only carries the hit mask
  `bitmap/cupshs.bmp` (hit mode 4 of `FUN_00413670`): cups are hotspots 3, 4,
  5 (also in `hotspots.bmp`), 6 is the clock, 7 the bet button area. Emanuel
  is character 1 (hotspot 20): idle `em_bo1`, `em_bo4`; bored `grooming`,
  `look-00`, `em_bo2`, `em_bo3`; reaction `bongo`. Music `subgame11`.
* State `DAT_00670e7c`: 0 idle (click the clock -> `start_0` plays, then
  `FUN_0043d560` starts: 100 bananas, clock from now, bet button 7 created,
  `clockloop` audio), 1 betting, 2 shuffling, 3 choosing.
* Scales (`FUN_0043d320` / `FUN_0043d3c0`): the banana bar and the stake
  slider use a 90 step scale, 18 steps per decade: level L <-> stake
  `5 * 10^(L/18) * (L%18 + 2)` (10, 15, ..., 95, 100, 150, ...); bananas b map
  to the largest level with stake <= b (0 below 11). PROGRESS "bar" at
  (11, 244): the bottom `level/90` of `bar.bmp` is shown. SLIDER 21: track at
  x 75, 15 wide, bottom 521, height `level/90 * 278` (rebuilt by
  `FUN_0043d440` whenever the bananas change, keeping the knob's relative
  position), range 0..level, value 0 at the top = the whole stock; knob
  `scroll.bmp` centred on the track, dragged or placed by a click on the
  track (`FUN_004092d0`). Text fields (15, 529)-(89, 546) stake and
  (15, 559)-(89, 576) bananas, `Amerigo BT_10_` centred.
* Bet (`FUN_0043d9c0`, button 7): level index = clamp(1 + L/9, 0, 11) (the 1
  is the tournament player's level otherwise), stake from L, round counter
  incremented. Moves (`FUN_0043da30`): `kMoveCount[idx]` = 3..17 moves, each
  `rand() % kMoveRange[idx]` (3..11) into the list anim_b, anim_e, anim_a,
  anim_c, anim_d, anim_f, anim_g, 2g, 2b, 2a2, 2a. Sequence (`FUN_0043da80`):
  `intro` (rounds 1-2) or `intro2a` + `intro2b` (rounds 3-6) or `intro2a`
  (7+), the moves, `intro3`. When the introduction clip ends the global
  `SmackFrameRate` is forced to `kFrameRate[idx]` = 13..27 fps for the clips
  opened afterwards (the moves and `intro3`, all 15 fps clips); `intro3`
  resets it and sets state 3. Engine: `Anim::setFrameRate()` from
  `onAnimStarted()`.
* Ball (`FUN_0043d920`): starts under cup 1; each move permutes it with the
  table at `0x49e9ec` (`kPermutation`). Choice (`FUN_0043d8c0`): right ->
  `win<cup>`; wrong -> `lose<chosen>`, `noits<ball>`. When the win/lose clip
  ends (`FUN_0043d160`): bananas +-= stake, state 1, game over if bananas < 10
  or the clock has stopped.
* Clock (`FUN_0043d5e0`, timer 0 every second): `hand_0` (60 frames, z -1)
  shows frame `round(60 * t / 300 s)`; every step plays one of `click1..4`;
  after 300 s the clock stops and, if the state is 1, the game is over
  (`FUN_0043d770`: "hustle:TIMEOUT" if bananas > 10 else "hustle:NOMONEY" in
  an "interfaceh:GAMEOVER" message box, high score `FUN_0041f9a0(0, bananas,
  {5000, 40000, 160000, 220000}, 0)`, back to state 0 with the Start clock).
* Buttons (BUTTON module, z 1, hotspot = id, fire on release): `help_0/1` at
  (0, 0), `exit_0/1` at (728, 0), 7 at (693, 404) with only a hover bitmap
  `bet_1`. The buttons highlight on the frame tick.
* Not done: message box, high score, help dialog, the "gamec:SUBGAMEABORT"
  confirmation when exiting a running game, tournament mode. The SmackGoto
  frame of the hand is `round(...) + 1` in the original; the port shows the
  rounded frame (0 based). Clips with Bink audio play slower than their
  frame rate in the engine (audio clock), which stretches the shuffle.

### Bitmap fonts (FONT module, `font.cpp`)

`common/fonts/<name>.bmp`, 24 bit, 256 glyphs in a strip. The top row marks
the glyphs: pixels equal to the top left pixel are separators, a run of other
pixels is one glyph (char code = glyph index). The colour key is the first
glyph's top left pixel. Glyphs keep their colours; the advance is the glyph
width. Text areas are animation structs with a surface filled with the key
colour 0x00ff00, flags 0x101 = centred.

## Sub game `textinvader` ("Ordspillet", add-on set 2)

A typing game started from the `goodbye` page. Words fall from the night sky
and are typed away before they reach the ground. Handler `0x44c640` (jump
table: 0x103 -> `FUN_0044c740`, 0x104 -> `44cf70`, 0x105 -> `44d720`,
0x10c -> `44cfe0` (key filter), 0x10e -> `44d030` (character), 0x110 ->
`44c920`, 0x111 -> `44cea0`, 0x112 -> `44d4f0`, 0x116 -> `44d4d0`). Engine:
`textinvader.cpp`.

* Data: `data2/textinvader` (backdrop, `fjell` hill bitmap at z 6, `julelys1`
  / `julelys2` light chains, `li.smk` 128x128 shooting star, the 4x4 sound
  clips `start`, `right`, `wrong`, `drop`, buttons), `lang2/textinvader`
  (`skilt.smk` the sign turning round and `skilt.bmp` the sign showing the
  score, both hotspot 4 at (0, 518); `wordlist/wordlist.txt`, 4035 words;
  `help.ini`). Fonts `Comic Sans MS_16_lightish_blue` / `_green` / `_yellow`
  / `_orange` / `_red` (data2/common/fonts) and `Comic Sans MS_10_` for the
  score. The hotspot mask is empty; the only hotspots are the sign element
  and the buttons. Music `track22`.
* Word list (`FUN_0044d7e0`): one word per line, sorted into buckets by
  length 1..31. `FUN_0044d770` picks a random length up to the current
  maximum (empty buckets are redrawn) and a random word of it.
* Screen: start sign = first frame of `skilt.smk` at z 7; clicking it
  (`FUN_0044d720`) plays the clip and the `start` sound while the chain of
  lights fills over 300 ms (`(now - clickTime) * 1/300`); when the clip ends
  (`FUN_0044c920`) the game starts (`FUN_0044c950`) and the bitmap sign
  with the score (`%d`, centred in (20, 546)-(105, 566), z 8) replaces it.
  Energy meter = PROGRESS module (`FUN_004089a0`/`FUN_00408b00`): the left
  `round(350 * energy / 100)` columns of `julelys1` at (165, 493), z 100,
  over the unlit `julelys2` (z 7). Buttons `help_0/1` (2, 1), `exit_0/1`
  (726, 2).
* Word slots (`FUN_0044ca50`, 10 structs of 600 bytes at `0x682c20`:
  active, done, bonus, word, two text areas, a copy of the `li` clip, x, y,
  star offsets, text extent, float y, rect): a slot gets a word whose first
  letter no other active word has, placed at `x = rand(640 - w) + 80`,
  `y = rand(150)`, retried up to 100 times until it overlaps no other word.
  Text area 1 (z 3) shows the word in blue (yellow/orange/red for bonus
  words), text area 2 (z 4) the typed prefix in green on top; the star
  plays once at (x + w/2 - 64, y + h/2 - 64), z 5, and is removed when done.
* Falling (`FUN_0044d4f0`, every frame): `y += slow * speed * dt` with dt
  in seconds, speed 8/10/12 px/s for difficulty 0/1/2 (`0x4de620`). A word
  that is not finished and reaches y >= 491 is lost: energy -= 5, `drop`
  sound, slot freed, a new word in `rand(500)` ms, typing reset if it was
  the target.
* Typing (`FUN_0044d030`): only `isalpha()` (C locale: ASCII letters) and
  `-` are accepted, lower cased. The first letter picks the first active,
  unfinished slot starting with it (`FUN_0044d3b0`); `FUN_0044d320` then
  returns ok (prefix, +3 points), wrong (longer than the word or a
  mismatch: energy -= 3, `wrong`, everything redrawn, typing reset) or done
  (+4 * length, `right`, the slot is marked done, removed after 500 ms by
  timer data 1000 + slot, new word `rand(500)` ms later; bonus applied).
  Words with æ/ø/å cannot be completed in the original either.
* Bonus words (`FUN_0044d220`): every 10/15/20 typed words
  (`0x4de644`) the next spawned word gets the next colour of a cycle
  (`(rand(2) + 1 + cur) % 3`). Yellow (1): all words vanish, every slot
  respawns in `rand(6000)` ms (the 3 * length points per word use a slot
  field that is never written: zero). Orange (2): speed factor 0.5 for
  15000/10000/6000 ms (timer data -2). Red (3): energy += 10, max 100.
* Levels (`FUN_0044d680`): level = removed words (typed, lost or cleared)
  / 12/10/8 (`0x4de650`); each new level multiplies the speed by
  1.02/1.03/1.04; odd levels add a slot (max 10, filled in `rand(500)` ms),
  even levels raise the maximum word length (max 32).
* Game over (`FUN_0044d450`) when energy <= 0: all slots cleared, message
  box `textinvader:GAMEOVER`, high score `FUN_0041f9a0(0, score, 0x49f0a0, 0)`,
  then the start sign again (without a profile).
* Difficulty comes from the profile (`FUN_004194a0`); without one it is 0.
  With a profile the original shows `tournament:PLAYERREADY` and starts at
  once. Not done: profiles/difficulty, help dialog, game over message box,
  high score, `sound.ini` volumes.
## Sub game `whackamole` ("Dra meg baklengs!", add-on set 2)

A whack-a-mole with birds: they peek out of seven bird houses and five nests
in an old tree and want the larva (the mouse cursor) before they disappear
again; a squirrel shows up too and must not be fed. Started from `house`.
Handler `0x44d980` (not a function in the Ghidra project; dispatches 0x103 ->
`FUN_0044e1f0`, 0x104 -> `44e300`, 0x105 -> `44da80`, 0x108/0x109 ->
`44ed00`, 0x110 -> `44e350`, 0x111 -> `44ecd0`, 0x112 -> `44e740`, 0x116 ->
`44ece0`). Engine: `whackamole.cpp`.

* Elements: 14 arrays of 12 animation structs (houses 1..7, nests 1..5,
  hotspot = index + 1): bird `a` (comes out), `b1`/`b2`/`b3` (waits), `c`
  (goes back in) at `0x4debb8`, `0x4df398`, `0x4dfb78`, `0x4e0358`,
  `0x4e0b38`; the mirrored bird `ar`..`cr` at `0x4e1318`..`0x4e3298`;
  `squirrel a`/`b1`/`c` at `0x4e3a78`, `0x4e4258`, `0x4e4a38`; the looping
  chirp `b1.s` at `0x4e5218`. The unmirrored bird clips of houses 1, 2, 6 and
  all nests have a `.lst` suffix in their file names. Then the clone scratch
  element `" "` (`0x4e5b60`, used to play `false` and `thank you on bird
  language1/2` as clones), `forest loop` (unused), `startbuttn` (bitmap,
  hotspot 34, (203, 238)), `cursor` (the looping larva, 32x32).
* Level table `0x4e59f8`, 18 x 5 ints `{birds, squirrels, waitRange, unused,
  duration ms}` (`kLevels`): 21..72 birds, 6..51 squirrels, waits 3/2/1,
  60/50/40 s. `FUN_0044df80`: level n uses entry (n - 1) % 18; from level
  19 on the duration is halved and rounded down to 5 s.
* Start (`FUN_0044df40`, click on hotspot 34 while not running): remove the
  sign, points = 0, level 1. Per level: bird interval = duration / (birds +
  1), squirrel interval = duration / (squirrels + 1), next bird at start +
  interval * shown + rand(interval), likewise squirrels; fed = appeared = 0;
  the system cursor is hidden (`FUN_0040ef90(0)`, `FUN_0040f140(0)`) and the
  larva clip (z 250) follows the mouse at (x - 25, y - 16).
* Tick (`FUN_0044e740`): redraw the time (`"%s: %02d:%02d"` of
  `whackamole:TIME`), end the level when start + duration has passed,
  otherwise spawn (`FUN_0044e830` / `FUN_0044ea30`): a free hole out of 10
  random tries, bird `a` or `ar` (50 %, `rand(1000) < 500`) or `squirrel a`
  at z 10, waits = rand(waitRange) + 1, birds count as appeared.
* Anim finished (`FUN_0044e350`): the clip is removed; `c`/`cr`/`squirrel c`
  clear the hole's busy flag; after `a`/`b*` a bird plays a random `b1`..`b3`
  (and the chirp through `SCENE_PlayAnim`) while waits > 0, else `c` (chirp
  removed); the squirrel plays `b1` or `c` the same way.
* Click on hole 1..12 while not busy (`FUN_0044da80`): squirrel present ->
  it goes to `squirrel c` (busy), sound `false`, appeared + 1 (a miss);
  bird present (also when it is already going back in) -> `c`/`cr` (busy),
  `thank you on bird language1/2`, fed + 1, points + 13, larva hidden for
  250 ms (`FUN_0044ec50`: the computed `(15000 - points) * 250 / 10000` is
  only sign tested; no hiding from 15000 points on; timer id 0 shows it
  again). A hit on a bird that is already in `c` from its own timeout still
  counts and does not restart the clip.
* Level end (`FUN_0044eb30`): percent = fed * 100 / appeared (`FUN_0044df10`);
  >= 70 (`7000 * 0.01`): message `whackamole:WELLDONE` / `NEXTLEVEL`, points
  += (int)percent * 29, next level. Else game over: message
  `interfaceh:GAMEOVER` / `whackamole:GAMEOVER`, high score
  `FUN_0041f9a0(0, points, {2500, 5000, 10000, 20000} at 0x49f0f8, 0)`, the
  start sign comes back (not in tournament mode).
* Screen: texts in `Amerigo BT_14_`, centred text areas (z 11) at
  (618, 175)-(753, 204) points, (619, 224)-(754, 248) percent, (622, 271)-
  (714, 288) level, (620, 297)-(717, 321) time, all `"%s: %d"`. Buttons
  `help out/in` at (2, 1) and `exit out/in` at (726, 2), z 200. Music
  `ambient10`. `sound.ini`: sound 0.449, speech 1, music 0.3.
* Engine notes: the framework drops clicks while its cursor is hidden, so a
  cursor table mapping every hotspot to the empty cursor (`kNoCursor`) is
  used during a level instead; the larva is positioned in `onUpdate()` since
  the framework only reports mouse moves on hotspot changes. The sound
  clones are defined with `defineAnim()` and made invisible (audio only
  clips have no palette and must not be decoded for drawing).
* Not done: the message boxes (next level starts at once), high score
  registration, help dialog, `SUBGAMEABORT` confirmation on leaving a
  running game (`FUN_0040d8a0` flag), tournament mode auto start,
  `sound.ini` volumes.
## Sub game `butterfly` ("Sommerfugler i magen") and its album `buttercol`

Catch butterflies with a net on a meadow; wasps and bees cost points. Handler
`0x43a410` (jump table; not a function in the Ghidra project): 0x103 ->
`FUN_0043a4f0` init, 0x104 -> `43a8c0` close (frees fonts and text areas),
0x105 -> `43a920` mouse down, 0x108 -> `43b3e0` mouse move, 0x110 ->
`43b990` anim finished, 0x112 -> `43b410` frame tick, 0x116 -> `43b3b0`
buttons (2 help `FUN_0041e580`, 3 exit `FUN_0040cc30`, 4 album -> scene
`buttercol`). Engine: `butterfly.cpp` (`ButterflyScene`, `ButtercolScene`).

* Tables: species at `0x49e078` (15 x 0x40 bytes: class 0/1 common, 2 rare;
  name; points 4..100; spawn weight, the weights add up to 100; then for the
  normal and the "scared" mode: min/max ms between turns, min/max speed in
  px/s, min/max turn in degrees), insects at `0x49e438` (wasp -10, bee -20
  points; the help text says 5 and 10), levels at `0x49e4b8` (11 x {time
  limit ms 120000..190000, points needed 250..7500, speed factor 0.7..1.2}),
  meadow rectangle (24, 88)-(702, 532) at `0x49e550`, text rectangles at
  `0x49e560`.., medal thresholds {1500, 3000, 5000} at `0x49e53c`.
* Entities: 30 butterfly slots at `0x66a1a8` and 30 insect slots at
  `0x667e40`, 0xd8 bytes each: active, insect flag, species, resting, mode,
  next turn time, float x/y (top left of the 32x32 sprite), heading (0 up,
  90 right), direction vector, speed, then an embedded animation struct whose
  name is `sprintf("%s%d", species, clip * 45)`: clip = ((heading + 22) mod
  360) / 45 for butterflies (`FUN_0043ae60`), ((heading + 45) mod 360) / 90 *
  2 for insects (`43ae10`, 4 clips). The clip is removed and re-added when it
  changes (`43acd0`), butterflies at z -100, insects at z 0.
* Spawning (`43b4c0`, `43b820`): a butterfly every 550 / 10 frames (/ 11 on
  level 11), an insect every 900 / 10; species by weight (`43b5d0`). They
  enter at the left edge (x = 24 - 32, heading 90) or the right edge (x =
  702, heading 270), y random in 88..500 (`43b550`), speed from the mode
  table, and are removed once the 32x32 rectangle no longer touches the
  meadow (`43ee30`).
* Turning (`43ac40`): at the next turn time, heading += random turn with a
  random sign; a butterfly in normal mode whose quadrant is "down" rests
  (clip stopped, `43af30`) one time in four (`43aef0`); next turn time =
  now + random interval. Movement (`43b790`): pos += speed * dir * dt *
  0.001 * level speed factor.
* Net (`43a950`): the "catcher" clip (124x124) follows the mouse at (x - 35,
  y - 35), z 250, while the cursor is hidden (cursor mode 3). A click plays
  it; insects within 35 px of the sprite centre (pos + 16, distance rounded)
  are "stung" (points subtracted, "scream" clip, the insects stay); otherwise
  butterflies within 35 px are caught (points, caught[species]++, removed,
  "catch" clip) and those within 150 px are scared into mode 1 ("miss" clip
  when nothing was caught). When the level points exceed the level's score
  after a catch: bonus = remaining time / time limit * 100 * (level + 1),
  then the next level (max 11, `43b220`): level points reset, background
  `level_<level mod 4>.bmp` at (20, 82) z -200, texts, timer restart.
* Timer (`43b920`): "timermove" (looping) at (728, 425 - elapsed * 322 /
  limit); when the limit passes, "timerend" plays at (728, 103) and the tick
  stops; when it has finished (`43b990`): net removed, message box
  `interfaceh:GAMEOVER` / `butterfly:ENDMESSAGE`, all entities removed, the
  caught species go into profile item 7 (`FUN_0041b740(7, i, 0x10, 0)`),
  high score `FUN_0041f9a0(0, score, medals)`. Start: "startanim" (hotspot 1,
  looping, z 150) is clicked -> "timerstart" plays -> game starts
  (`43b990`), `FUN_0040d8a0(1)` sets the "sub game running" flag (leaving
  asks `gamec:SUBGAMEABORT`), album button disabled.
* Screen: borders at z 100, icons of the species in the air in the top window
  (`43afc0`: bitmap/<species>.bmp at y 6, common ones from x 77 step 52,
  rare ones from x 581 step 61, in table order), labels COMMON/RARE/LEVEL/
  NEXTLEVEL in `Amerigo BT_10_`, score / next level score / level number in
  `Amerigo BT_18_` (rectangles (268,554)-(446,584), (553,545)-(613,577),
  (616,545)-(666,577)), all z 110. Buttons help (0, 0), exit (728, 0), album
  (24, 541) z 150. Music `subgame5`.
* `buttercol` (`0x43a230`: init `43a2a0`, mouse down `43a3b0`, buttons
  `432d10`): reads the 15 collected flags from profile item 7, adds the
  `*_coll` bitmaps of the collected species at z -10 with hotspot 20 + i
  (list `0x4d8bd0`, note that it starts small_white, large_white while the
  species table starts largewhite, smallwhite). A click on one opens the
  "info" fact page about it; exit returns to `butterfly`. Registers the
  cheat "LOVELYFLIES" (`FUN_0040ad50`) which marks all as collected.
* Engine notes: ticks are frames (60 fps limiter; the original's rate is
  unknown). The collected flags live in a static of `ButterflyScene` until
  profiles exist. After game over the start clip is shown again (the
  original stays in the end state behind its dialogs). `debug(3)` dumps all
  entity positions once a second.
* Not done: message box, help dialog, profile storage, high score / medals,
  the sub game abort confirmation, the "info" fact page, `sound.ini`
  volumes, the LOVELYFLIES cheat.

## Development aids (config keys in the `[flaaklypa]` section)

`start_scene`, `autoshot` / `autoshot_delay` / `autoshot_quit` (screenshot),
`autoclick=t:x,y;t:x,y,m` (synthetic clicks, `m` = move only, `d` = press
only, `u` = release only for drags, t in ms after start), `autokey=t:key[:hold];...` (key press at t held for hold ms, default
100; `left`, `right`, `up`, `down`, `space`, `esc`, `tab`, `return`, a
character, or a string of characters typed one after the other with `~` =
Escape), `random_seed` (deterministic boards).

## Mini game checklist

From `sceneindex/scene.ini` and `language.ini` (Gold edition). Tick off as
they get ported; each needs a scene class, `createScene()` and
`FlaaklypaEngine::startGame()`.

Sub games (`[subgame]`, score based):

| Done | Scene | Title | Started from | Data |
|---|---|---|---|---|
| x | lettersort | Postsorteringsmaskinen | yard | data (help, message box, high score open) |
| | bugzzz | Larveliv i leiren | yard | data |
| x | sockdrawer | Sokkeskapet | desk | data (message box, high score, help open) |
| | wheelbarrow | Eplehøsten | pee | data |
| x | hopscotch | Solan og Ludvig i Paradis | house | data + data1 (message box, high score, help open) |
| | sockdrawer | Sokkeskapet | desk | data |
| x | wheelbarrow | Eplehøsten | pee | data (message box, high score, help open) |
| | hopscotch | Solan og Ludvig i Paradis | house | data |
| | audiopairs | Reodors Lydmaskin | house | data |
| x | textinvader | Ordspillet | goodbye | data2 (message box, high score, help, profiles open) |
| | hopscotch | Solan og Ludvig i Paradis | house | data |
| x | audiopairs | Reodors Lydmaskin | house | data (help, message box, high score open) |
| | textinvader | Ordspillet | goodbye | data2 |
| | balloonhunt | Solans ballongjakt | outtent | data2 (no sceneDefs entry yet) |
| x | whackamole | Dra meg baklengs! | house | data2 (message boxes, high score, help open) |
| | beemaze | Ludvigs Labyrint | tvroom | data |
| | buildabike | Reodors sykkelverksted | garage | data |
| | mountain | (no title in language.ini) | morning | data (no sceneDefs entry yet) |
| | pipeline | Oljeeventyret | outtent | data |
| x | butterfly | Sommerfugler i magen | pee | data (message box, high score, profile, help, info page open) |
| | hustle | Emanuels utfordring | intent | data |
| | butterfly | Sommerfugler i magen | pee | data |
| x | hustle | Emanuels utfordring | intent | data (message box, high score, help open) |

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
