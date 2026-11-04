<p align="center">
  <img src="screenshots/icon.png" alt="J2Vita" width="160"/>
</p>

<h1 align="center">J2Vita</h1>

<p align="center">
  Play your old Java ME (J2ME) phone games on the PS Vita.
</p>

---

J2Vita runs MIDP 2.0 / CLDC 1.1 games, the `.jar` games every phone had before smartphones, on
the PS Vita. It's built on PSPKVM, the Java ME virtual machine for the PSP, which itself is based
on Sun's open source phoneME Feature VM. The Java side is mostly left alone; the Vita side (video,
input, sound, menus) was rewritten for the Vita.

> [!IMPORTANT]
> **No games are included here or in the release.** Bring your own `.jar` files.

<p align="center">
  <img src="screenshots/launcher.png" alt="Game list with folders" width="400"/>
  <img src="screenshots/menu.png" alt="In-game menu" width="400"/>
</p>
<p align="center">
  <img src="screenshots/display.png" alt="Display settings with Auto resolution" width="265"/>
  <img src="screenshots/controls.png" alt="Controls settings" width="265"/>
  <img src="screenshots/keypad.png" alt="On-screen keypad settings" width="265"/>
</p>

The game compatibility list lives on the [project page](https://kiddrwxssj.github.io/j2vita/).

## Download & Install

Grab `j2vita.vpk` from the [Releases](../../releases) page and install it with VitaShell. You
need a Vita on HENkaku/Ensō and `libshacccg.suprx`, which most homebrew already needs (install it
with [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D/releases) if you don't have it).

Then add some games:

1. Copy your `.jar` files to `ux0:data/j2vita/games/`. Subfolders work too, and `.zip` files
   with JARs inside are unpacked for you.
2. Open J2Vita. New games are installed on their own and show up in the list, even when you copy
   them while J2Vita is open.
3. Pick a game and press Cross.

To update a game, overwrite its `.jar`. Deleting a game from the list also deletes its `.jar`.
Moving `.jar` files between folders, from J2Vita or from VitaShell, keeps their saves.

```
ux0:data/j2vita/
├── games/      your .jar files go here
├── config/     settings (default.cfg + one file per game)
└── log.txt     useful when something breaks
```

Games keep their saves in `ux0:data/j2vita/appdb/`, so updating J2Vita doesn't touch them.

## Controls

**Game list**

| Vita | Does |
|---|---|
| Cross | Play, open a folder |
| Circle | Back to the parent folder |
| Triangle / L | Options: move to folder, delete, new/rename/delete folder, settings, quit |
| SELECT | Settings |

You can also drag the list with your finger. Tap a game to select it and tap it again to start it.

**In game** (defaults; every game can have its own mapping)

| Vita | Phone key |
|---|---|
| D-Pad / Left stick | Arrows (or 2 4 6 8, see "D-pad sends") |
| Cross | Fire (5) |
| Circle / R | Right soft key |
| L / Start | Left soft key |
| Square / Triangle | `*` / `#` |
| Right stick | 2 / 8 / 4 / 6 |
| Rear touchpad left / right half | 1 / 3 |
| SELECT (tap) | Menu, the game pauses |
| SELECT (hold) | On-screen keypad |

The touchscreen works as the phone's touchscreen. Set a rear touchpad half to `None` in Controls
if you keep touching it by accident.

The **on-screen keypad** is there for cheat codes and anything that needs a key the Vita doesn't
have. Move with the D-Pad, press with Cross (it's held for as long as you hold Cross), Square moves
the keypad to the other side and Circle hides it. You can also just tap the keys.

## Settings

* **Global settings** (SELECT on the game list): theme, screen scaling, filter, phone key set,
  D-pad mode, controls and the performance overlay. Every game starts from these.
* **In-game menu** (SELECT while playing): the same options plus rotation and resolution. A game
  only remembers what you change inside it; everything else keeps following the global settings.
* **Themes**: White (default), Black, Blue and Green.
* **Performance overlay**: `FPS` shows the frame rate, `Full` adds how busy the Java VM is. Java
  games pick their own frame rate, so a low number isn't always a problem. `Game limit` means the
  game is waiting on its own timer and runs at the speed it was made for. `CPU limit` means the
  Vita is what holds it back, and `14/16 FPS` shows the rate the game would like to reach.
* **Adjust screen**: drag, pinch and twist to move, zoom and rotate the picture, the on-screen
  keypad and the button HUD. Each game keeps its own layout.
* **Button HUD**: a small bar that shows which phone key each Vita button sends, the rear touchpad
  halves included (and the screen borders in `Buttons` mode).
* **Touch screen**: `Pointer` passes touches to the game, `Buttons` turns the screen borders
  around the picture into two extra buttons (set them in Controls).
* **Auto-detect keys**: on a game's first start J2Vita looks at its code and sets up the soft keys,
  2/4/6/8 and the four extra buttons (Square, Triangle and the rear touchpad halves) with the keys
  the game uses most.
* **On-screen keypad**: size and opacity can be changed, `Always` keeps it up for playing with
  touch while the buttons still go to the game.
* **Resolution** is `Auto` by default: J2Vita starts with a guess from the game's name, manifest
  and pictures, then switches to the size the game actually draws at (480x800 included). Portrait
  games taller than the Vita screen are turned on their side. You can still pick a fixed size.
* **Menus**: the in-game menu is split into Display, Controls and On-screen keypad.
* **Text entry**: open the on-screen keypad and pick **Text** to type with the Vita keyboard.

## What works

* MIDP 2.0 games, including Nokia UI extras.
* 3D games made with **MascotCapsule Micro3D v3** or **M3G (JSR-184)**. Both are software
  renderers written for J2Vita, so 3D games run but are not as fast as 2D ones.
* MIDI, WAV, tones and AMR sound.
* Not there yet: networking and Bluetooth (games that ask for them keep running without).

## AI Assistance

Parts of the code were written or improved with the assistance of AI (Claude, by Anthropic). That
includes most of the Vita side: the video and input layer, the menus and launcher, the themes, the
on-screen keypad and the performance overlay, and a good part of the bug fixing. All AI-assisted
code was reviewed, tested, and manually validated on a real PS Vita, with the overall
implementation and debugging process remaining under human supervision.

## Credits

* **Sun Microsystems** for phoneME Feature (CLDC, MIDP and javacall), released as open source
  under the GPL v2.
* **The PSPKVM team** (Sleepper, M@x, Anweifeng, AJ Milne and Jurgen Konings) for PSPKVM, which
  J2Vita is built on. Their original readme and changelog are kept in `Readme.txt` and
  `CHANGELOG.TXT`.
* **The earlier PSPKVM Vita port** (v0.1.86), the first attempt at bringing PSPKVM to the Vita,
  which J2Vita started from.
* **vitaGL** by Rinnegatamante.
* **SDL2** and **SDL2_mixer** by the SDL team, with its TiMidity code for MIDI.
* **FreePats** by the Free Software Foundation, the instrument set used for MIDI music.
* **FreeType**, **libpng** and **zlib**.
* **FFmpeg** for the AMR audio decoders.
* **J2ME-Loader**, whose source was a handy reference for the MascotCapsule file formats.
* **VitaSDK** and the Vita homebrew scene.

Java and the Java coffee cup logo are trademarks of Oracle and/or its affiliates. PlayStation and
PS Vita are trademarks of Sony Interactive Entertainment. J2Vita isn't affiliated with or endorsed
by either of them, or by the makers of any game shown here. Game screenshots belong to their
respective owners.

## License

J2Vita is free software under the **GNU General Public License, version 2**, the same license as
phoneME Feature and PSPKVM. See [LICENSE](LICENSE). Third-party parts keep their own licenses:
vitaGL (LGPL v3), SDL2 and SDL2_mixer (zlib), FreePats (GPL v2 or later, with an exception for
music made with it), FFmpeg's AMR decoders (LGPL v2.1 or later), FreeType (FTL), libpng and
zlib.

## Building from source

You need [VitaSDK](https://vitasdk.org) with these vdpm packages: `vitaGL vitaShaRK libmathneon
SceShaccCgExt sdl2 sdl2_mixer freetype libpng zlib bzip2` (plus SDL2_mixer's own codec
dependencies), a **JDK 8**, and a host compiler that can build 32-bit binaries (`g++ -m32`), which
the ROM generator needs.

```bash
JDK_DIR=/path/to/jdk8 ./build.sh
```

`HOST32_BIN` can point at gcc/g++ wrappers if your `-m32` setup lives somewhere else. The VPK ends
up in `vita/j2vita.vpk`. The build fetches a pinned vitaGL (without the boot splash) and the
FreePats instruments the first time, and builds a tiny FFmpeg with only the AMR decoders.
