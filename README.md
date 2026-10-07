# Need for Madness — native port for Nintendo Switch and Linux

**English** · [Español](README.es.md)

![icon](data/icon.png)

An unofficial port of **Need for Madness** (Radicalplay, 2015) to the
**Nintendo Switch** (homebrew) and **Linux**, written in C.

| Platform | Status |
|---|---|
| Nintendo Switch | Tested on hardware (Switch v1) and in the Eden emulator |
| Linux | Builds and runs; used to test every change |
| PS Vita | Inherited from the upstream port. The game code changed in this fork, but **these changes haven't been tested on the Vita** |

This repository is a fork of
[PedrelliMath/nfm-psvita-port](https://github.com/PedrelliMath/nfm-psvita-port),
the native PS Vita / Linux port. That port is itself built on
[radicalarchive/nfm](https://github.com/radicalarchive/nfm), the JavaScript/WebGL
port. This fork adds a Switch target (`native/platform/switch/`) and the fixes
and features listed below. All three targets share the same game code, so the
changes reach Linux and the Vita too.

The original source code was never released. The game was decompiled and
rewritten line by line, first in JavaScript/WebGL (`web/`) and then in C
(`native/`), with the aim of a faithful 1:1 port. The game assets (`data/`,
`stages/`, `mycars/`, `mystages/`, `music/`) are the originals, byte for byte.

---

## Download and install

You need a Switch running custom firmware (Atmosphère) with the Homebrew Menu.

1. Download `nfm_switch.nro` from the [latest release](../../releases/latest).
2. Copy it to `sdmc:/switch/` on the SD card.
3. Open the Homebrew Menu **through a game, holding R while it launches**
   (title override). Don't open it from the Album: from there, homebrew runs as
   an applet with much less memory.
4. Pick **Need for Madness**.

All the assets are inside the `.nro`; nothing else needs to be copied. Progress
and settings are saved in `sdmc:/switch/nfm/`.

### If the game freezes or crashes

The game writes a report beside the save so the problem can be found:

- `sdmc:/switch/nfm/freeze.txt` — the game stopped advancing for 8 seconds. It
  records what the game was doing (screen, stage, tick, car).
- `sdmc:/switch/nfm/crash.txt` — the game hit a fault and closed itself. It
  records registers, the fault address and a backtrace as offsets into the
  `.nro`; `aarch64-none-elf-addr2line -e nfm_switch.elf 0x…` turns these into
  file and line. You need the `.elf` from the same build.

Please attach the file when you open an issue.

---

## Controls

Joy-Cons (handheld or detached) or Pro Controller.

| Button | Action |
|---|---|
| ZR | accelerate |
| ZL | brake / reverse |
| Left stick | steer (menus: stick or D-pad) |
| B | handbrake; held in the air, the stunt key |
| B + left stick, in the air | stunts in 8 directions: forward/back = loops, sideways = rolls, diagonals = combined stunts |
| Right stick | look around |
| A / B (menus) | confirm / back |
| X | change camera |
| Y | mute music |
| − | mute sound effects |
| D-pad up | guidance arrow: track ↔ cars |
| D-pad down | radar (minimap + speedometer) |
| + | pause |

The defaults. Settings › Controls moves every race button and can steer and
stunt with the D-pad instead; the arrow and the radar then move to the left
stick.

---

## What the Switch port adds

**The Switch target**
- `.nro` with every asset in its RomFS, saves on the SD card, libnx controls,
  vibration (Joy-Cons and Pro Controller).
- 1920×1080 docked, 1280×720 handheld.
- A legacy OpenGL 2.1 context through SDL2 and Mesa. The 33 GL functions are
  loaded at runtime.
- The Instructions screen and stage cards show Switch button art.

**Smooth 60 fps**
- **Smooth Frames** (on by default): the race still ticks at 18.9 Hz like the
  original, but every display frame draws the cars and the camera blended
  between the two latest ticks. This includes each car's turn and flip angle,
  to a fraction of a degree. The simulation is untouched.
- A 60 Hz frame limiter that sleeps only what is left of each 16.7 ms frame.
- **Settings › Performance Test**: a 60-second AI-driven race on stage 9. It
  shows a summary on screen and writes a full report to
  `sdmc:/switch/nfm/benchmark.txt` (average and 1% low fps, percentiles, slow
  frames, work per frame).

**Pause menu**: Restart Race (same car and stage, from the loading card). A race
also pauses itself when you go to the HOME menu or put the console to sleep.

**Settings** (from the main menu and from the pause menu)
- **Graphics**: Image Quality (Original / Smooth / HD), Draw Distance, Scenery
  Detail, Shadows, Particles, Motion Blur, Smooth Frames.
- **Audio**: music and effects volume.
- **Interface**: FPS counter (off / FPS / detailed); car names in the race
  standings (off by default: the original's single player leaves them blank).
- **Gameplay**: screen shake, vibration.
- **Controls**: steer and stunt with the left stick or the D-pad; any button
  for each race action (a button used twice shows in red).

**Brought over from the original that the Vita port lacked**
- The original's **text**: Arial bold/plain at the original's sizes (as
  Liberation Sans, its metric-compatible open twin), antialiased, in mixed
  case with every symbol, and every string, font and position taken from the
  Java. The Vita port drew a 5×7 uppercase vector font.
- The stage backdrop: clouds, mountains, the stars on night stages and the
  ground patches around the track (`clouds(`, `mountains(`, `density(`,
  `fadefrom(`, `lightson`).
- The **dirt hills** (`pile(`) that 31 of the 32 stages place, 47 to 187 each,
  drawn and with collision.
- The AI's **route points** (`set(...)p`): the bots follow the track instead of
  aiming from gate to gate.
- Stunts in **8 directions** (the Vita read only 4, so diagonals did nothing).

**Car collisions as in the original**
- After a hit, a car moves by its wheel speeds *after* they are clamped, as
  Java does. The C port used the speeds from before the clamp, so a car that
  had just been hit drove on into the other one; in head-on tests the cars
  ended up overlapping a third less often after the fix.
- The heading, tilt, bumpy-ground wobble and bounce use Java's double
  arithmetic where the C port rounded to float. Head-on crashes now match the
  original exactly, tick for tick.

**Fixes ported from the web port** ([HopeAero/nfm](https://github.com/HopeAero/nfm))
- Brake and gear thresholds divide integers as Java does (`handb / 2`,
  `swits / 2`). Odd values braked and topped out 0.5 too high every tick.
- Wheel dust ages per tick, not per frame. At 60 fps it had vanished 3× too
  fast.
- A hang on heavily damaged cars, and a bounded AI waypoint loop.
- Stage validation as in Java: an unknown model, too many objects, or fewer
  than 2 checkpoints shows *ERROR LOADING STAGE* instead of hanging the race.
  Over-sized model faces no longer overflow the stack, and divisions by zero
  driven by stage data return 0.
- A readable HUD on dark skies: the HUD is recoloured to 4.5:1 contrast
  against the sky, as the web port does, instead of drawing boxes.
- Resuming from pause no longer runs 3 physics ticks in one burst.
- 64-bit projection (no signed overflow) and `-ffp-contract=off`, so the
  Switch's float math matches Java's.

**Diagnostics**
- Freeze and crash reports (see above).

---

## Building

### Nintendo Switch

With Docker and devkitPro's official image, from the repository root:

```sh
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64 bash -c \
  'cmake -S native -B native/build-switch -DNFM_PLATFORM=switch \
     -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake && \
   cmake --build native/build-switch -j'
```

On Windows (Git Bash), prefix the command with `MSYS_NO_PATHCONV=1` and mount
the drive path, e.g. `-v "D:/path/to/nfm-switch-port:/src"`.

The build output is `native/build-switch/platform/switch/nfm_switch.nro`
(about 13 MB), with `nfm_switch.elf` beside it.

Options:
- `-DNFM_SVCLOG=ON` sends stderr to the emulator's log (Eden, yuzu). This build
  also reads `sdmc:/switch/nfm/debug_env.txt` (`KEY=VALUE` lines) for the
  headless test hooks.
- `-DNFM_NXLINK=ON` sends `printf`/stderr to the PC with `nxlink -s`.

### Linux

The Linux target runs the same game with SDL2 and OpenGL, using the keyboard
(keys in [`CONTROLES.txt`](CONTROLES.txt)):

```sh
sudo apt install build-essential cmake libsdl2-dev libgl-dev zlib1g-dev
cmake -S native -B native/build-linux -DNFM_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-linux -j
native/build-linux/platform/linux/nfm_linux   # run from the repository root
```

Core tests (on the host, no window):

```sh
cmake -S native/tests -B native/tests/build && cmake --build native/tests/build -j
cd native/tests/build && ctest
```

### PS Vita

The Vita target is the upstream one: see
[PedrelliMath/nfm-psvita-port](https://github.com/PedrelliMath/nfm-psvita-port)
for VitaSDK, vitaGL and installing the `.vpk`. **Not tested in this fork:** the
shared game code has changed (everything listed above), and neither the Vita
build nor the game on the Vita has been checked.

---

## What's missing / known differences

**Compared to the original game**
- **No online multiplayer**: the original's lobby isn't ported. It exists only
  in the web port.
- **No user content**: only the 32 stock stages and the stock cars. Stages and
  cars made with the original's Stage Maker and Car Maker (`mystages/`,
  `mycars/`) can't be loaded yet.
- **Small numeric differences**: about 15 places still round in `float` where
  Java uses `double`, or the other way round. They cause rare one-unit
  differences in body tilt, bounces, AI timing and whether two cars touch.
- **Stages 28–32**: a few road pieces rotated ±90° cast a slightly different
  shadow (`loadnew` isn't set for those stages).
- **Two settings change the gameplay, not just the picture.** The defaults are
  faithful.
  - *Scenery Detail: Low* is the original's own low-detail mode, which also
    removes collisions with decoration.
  - *Draw Distance: Far/Max* can change whether a distant car's repair is
    animated.

**Platforms**
- Docking or undocking mid-game keeps the screen size the game started with.
- No touch screen in menus yet.
- Hardware testing so far is one person on a Switch v1. Vibration and the
  final performance numbers haven't been confirmed on hardware; the
  Performance Test report helps here.
- PS Vita: none of this fork's changes have been tested on the Vita.

**Priority**: a port as faithful to the original game as possible. Extra
content from the web port (Extended Mode, the editors, custom cars …) is not
part of this repository's plan; if there is demand, it may come later in a
separate branch or repository.

---

## Repository layout

| Folder | Contents |
|---|---|
| `native/` | **The native port.** `core/` is the platform-independent game (physics, rendering, audio, decoders). `platform/common/game.c` is the game loop, menus and settings; `platform/common/diag.c` holds the freeze/crash reports. `platform/switch/`, `platform/vita/` and `platform/linux/` are each target's thin layer. `tests/` holds the core tests. |
| `web/` | The JavaScript/WebGL port the C was translated from. |
| `decompilation/` | The decompiled Java (`java-src/`). A reading reference, not a build input. |
| `java/` | The original `Game.jar` patched for modern Java (`./start.sh`), plus the untouched original. |
| `data/`, `stages/`, `mycars/`, `mystages/`, `music/` | The original assets: **don't modify them**. `data/switch/` and `data/vita/` hold the button art; `data/port/` holds the port's own menu labels. |
| `tools/` | Helper scripts (button art, menu labels, music). |

## Documentation

- [`TASKS_SWITCH.md`](TASKS_SWITCH.md): the Switch port, what was done, how it
  was checked and what is left.
- [`native/PORT_SPEC.md`](native/PORT_SPEC.md): the native port's rules.
- [`native/TASKS_NATIVE.md`](native/TASKS_NATIVE.md): the native port's history.
- [`WORK.md`](WORK.md): discoveries and pitfalls, one per line.
- [`AGENTS.md`](AGENTS.md): how to run, measure and verify, and the invariants
  that must not break (no depth buffer; one draw call in submission order).
- [`web/TRANSPILE_SPEC.md`](web/TRANSPILE_SPEC.md): the Java → code translation
  contract (integer overflow, float32).
- [`CONTROLES.txt`](CONTROLES.txt): the Vita and Linux controls.

---

## Credits

- **Need for Madness**: Radicalplay (Omar Waly), the original game.
- [**radicalarchive/nfm**](https://github.com/radicalarchive/nfm): the
  decompilation and the JavaScript/WebGL port this work stands on.
- [**PedrelliMath/nfm-psvita-port**](https://github.com/PedrelliMath/nfm-psvita-port):
  the native C port for PS Vita and Linux this repository forks.
- [**HopeAero/nfm**](https://github.com/HopeAero/nfm): the web port the later
  fixes were ported from.
- [devkitPro](https://devkitpro.org), libnx, SDL2 and Mesa for the Switch
  toolchain and libraries.
- [Liberation Sans](https://github.com/liberationfonts/liberation-fonts)
  (SIL Open Font License 1.1, `data/port/LiberationSans-OFL.txt`) for the text.

*Need for Madness © Radicalplay. A non-commercial fan project, not affiliated
with Radicalplay or Nintendo.*
