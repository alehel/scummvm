# Flåklypa Grand Prix (PC, 2000-2002) - engine notes

Status: scene framework in place (see "Engine structure" below). The main
menu and the first story page (`yard`) work: backdrop, hotspot mask, cursor
tables, z-sorted Smacker/bitmap elements, animation sequences, the two
characters with idle/bored/reaction lists, timers, music, and Bink audio in
Smacker files (added to `video/smk_decoder.cpp`). Not done: dialogs
(navigator, help, fact, award), profiles, scene index, other story pages, sub
games, racing.

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

0x103 init(arg), 0x104 close, 0x105 mouse down(x, y, hotspot), 0x107 right
button, 0x108 mouse up, 0x10c key, 0x10f anim started, 0x110 anim finished
(anim), 0x111 timer(id, data), 0x113 hotspot change (mouse move), 0x11f
sequence done. `menu` acts on mouse up, story pages on mouse down.

### Hotspot index ranges (story pages)

1..3 sub game/activity entries, 55..99 animation hotspots, 100..150 misc,
151..199 fact pages (`fact.ini` `hotspot=`), 251/252 the characters,
254/255 hidden 3D glasses / car part bitmaps.

### Audio

Nearly all clips with sound use Bink RDFT audio inside Smacker (flag bit 27),
22050 Hz 16 bit, with a one second prebuffer in frame 0. Music and narration
are 4x4 pixel clips. `electric`, `wind` and the like are sound effects
played as one element sequences.
