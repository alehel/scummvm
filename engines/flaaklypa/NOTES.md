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

## Development aids (config keys in the `[flaaklypa]` section)

`start_scene`, `autoshot` / `autoshot_delay` / `autoshot_quit` (screenshot),
`autoclick=t:x,y;t:x,y,m` (synthetic clicks, `m` = move only, t in ms after
start), `autokey=t:keys;t:keys` (synthetic key presses, one per character,
`~` = Escape), `random_seed` (deterministic boards).

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
| | wheelbarrow | Eplehøsten | pee | data |
| | hopscotch | Solan og Ludvig i Paradis | house | data |
| | audiopairs | Reodors Lydmaskin | house | data |
| x | textinvader | Ordspillet | goodbye | data2 (message box, high score, help, profiles open) |
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
