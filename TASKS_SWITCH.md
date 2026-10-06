# Nintendo Switch port — tasks

This repository starts from the PS Vita / Linux native port (first commit,
`049c2ae`) and adds a Switch target. The plan: first the Vita port as it is,
running on Switch; then, a piece at a time, what the web port in `nfm-master`
gained (Extended Mode, the car and stage editors, raw "Recharged" stats,
community cars, lightBrake, 1000-piece cars, touch controls ...).

## Build

```sh
# from the repository root, with Docker (devkitPro's official image)
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64 bash -c \
  'cmake -S native -B native/build-switch -DNFM_PLATFORM=switch \
     -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake && \
   cmake --build native/build-switch -j'
```

Windows (Git Bash): prefix with `MSYS_NO_PATHCONV=1` and mount the drive path,
e.g. `-v "D:/platica/nfm-switch-port:/src"`.

Output: `native/build-switch/platform/switch/nfm_switch.nro` (~13 MB, every
asset inside its RomFS). Add `-DNFM_NXLINK=ON` to get `printf`/`stderr` on the
PC with `nxlink -s`.

## Install and run (Atmosphère)

1. Copy `nfm_switch.nro` to `sdmc:/switch/` on the SD card.
2. Open the Homebrew Menu **through a game with R held** (title override), not
   through the Album: the Album runs homebrew as an applet with much less memory.
3. Pick *Need for Madness*. Progress is saved in `sdmc:/switch/nfm/`.

## Controls (the Vita's scheme on the matching buttons)

| Joy-Con | Action |
|---|---|
| ZR / ZL | accelerate / brake, reverse |
| Left stick | steer (menus: stick or D-pad) |
| B | handbrake; held in the air, the stunt key |
| B + left stick, in the air | stunts: forward/back = loops, sideways = rolls |
| Right stick | look around |
| A / B (menus) | confirm / back |
| X | change camera |
| Y | mute music |
| − | mute sound effects |
| D-pad up / down (race) | guidance arrow / radar |
| + | pause |

## Status

- [x] `native/platform/switch/`: SDL2 window, legacy GL 2.1 context, the 33 GL
      functions loaded at runtime (`gl_include.h`: no libGL, devkitPro's glad is
      core-only), romfs assets, SD-card save, libnx pads. **Builds** (`23a6e5e`).
- [ ] **First run on hardware** -- untested. What to watch: the GL context
      (if Mesa refuses a legacy 2.1 context the game prints
      `SDL_GL_CreateContext failed` or `GL function not available: ...` with
      nxlink and exits); the picture filling the 1280x720 screen; sound;
      that every button does what the table says; the save file appearing.
- [ ] Instructions screen: Joy-Con labels and button art (`game.c` has the
      Vita's under `NFM_TARGET_VITA`; Switch still names the keyboard's keys).
- [ ] Docked vs handheld: the window is a fixed 1280x720.
- [ ] Touch screen in menus (taps as clicks), as the web port does.

## From nfm-master, later (small first)

- [ ] Raw "Recharged" stats in `car_define.c` (web `CarDefine.readRawStats`).
- [ ] `lightBrake` (light 3), 1000-piece cars, Re-Lit arrow stunts.
- [ ] Custom cars from the SD card; the Extended, R&R and Origins models.
- [ ] Extended Mode -- its own engine (~18k-line xtGraphics); a project of its own.
- [ ] Car and stage editors, touch and Joy-Con first.
