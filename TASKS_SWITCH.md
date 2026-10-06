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

Diagnostics: `-DNFM_SVCLOG=ON` (stderr in the emulator's log). That build also
reads `sdmc:/switch/nfm/debug_env.txt` (KEY=VALUE lines) into the environment,
so game.c's headless hooks work on Switch, e.g. in Eden's SD folder:
`NFM_SCREENSHOT_PPM=sdmc:/switch/nfm/shot.ppm`, `NFM_SCREENSHOT_FRAME=300`,
`NFM_SCREENSHOT_MENU=settings` -- the game saves its own frame and exits.

Output: `native/build-switch/platform/switch/nfm_switch.nro` (~13 MB, every
asset inside its RomFS). Add `-DNFM_NXLINK=ON` to get `printf`/`stderr` on the
PC with `nxlink -s`.

## Install and run (Atmosphère)

1. Copy `nfm_switch.nro` to `sdmc:/switch/` on the SD card.
2. Open the Homebrew Menu **through a game with R held** (title override), not
   through the Album: the Album runs homebrew as an applet with much less memory.
3. Pick *Need for Madness*. Progress is saved in `sdmc:/switch/nfm/`.

## Controls (the Vita's scheme on the matching buttons; Joy-Cons or Pro Controller)

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
- [x] **Runs in Eden** (yuzu fork, v0.2.0-rc2, 2026-10-06): 60 FPS, menus and
      Instructions drawn right. The first build was black at 37 FPS: without
      `SDL_GL_CONTEXT_PROFILE_MASK` SDL's Switch backend creates an **OpenGL ES 3.2**
      context, where glBegin & co. resolve to no-ops. With the compatibility mask
      Mesa gives `4.3 (Compatibility Profile) Mesa 20.1.0 | NV120 | nouveau`.
      `-DNFM_SVCLOG=ON` prints stderr in the emulator's log (how this was found).
- [ ] **First run on hardware** -- untested. What to watch: the GL context
      (if Mesa refuses a legacy 2.1 context the game prints
      `SDL_GL_CreateContext failed` or `GL function not available: ...` with
      nxlink and exits); the picture filling the 1280x720 screen; sound;
      that every button does what the table says; the save file appearing.
- [x] Instructions screen in Switch buttons (checked in Eden): the help text
      (`KEY_*` in `game.c`, `NFM_TARGET_SWITCH`) and the button art,
      `data/switch/sw_*.png` from `tools/gen_switch_assets.py`, drawn to the
      keyboard art's footprints like the Vita's. The stage cards' "Press [ A ]"
      for the guidance arrow now says D-pad up (Vita too). The same names and
      places hold on the Joy-Cons and on the Pro Controller.
- [x] The .nro is rebuilt when an asset changes or is added (the RomFS copy is a
      tracked devkitPro asset target; a plain folder was not, and new files
      never reached the .nro).
- [x] **Settings (pause menu): Graphics, Screen Shake, Vibration** (2026-10-06, checked
      in Eden). Graphics: *Original* (800x450 stretched unfiltered, as the Vita
      draws it), *Smooth* (same, linear), *HD* (default on Switch: the game space
      drawn into a display-sized target -- `GfxGlRenderTarget.px_w/px_h` apart
      from its 800x450 game-space `width/height`; applied at once). Screen Shake
      off still draws the shake's two randoms, so the race's random sequence
      stays the original's. Vibration: libnx, on the Joy-Cons (handheld or
      paired) and the Pro Controller, as strong and long as the crash shake;
      `platform_rumble()` is a no-op on Linux/Vita, which do not show the row.
      Saved in `settings.txt` (`graphics=`, `screen_shake=`, `vibration=`).
- [x] **Settings from the main menu too**: a fourth row, *Settings*, in the slot
      the original's own fourth row used (y=351). Its label is options.png's style
      redrawn -- the Adventure face at 18 px, the size that gives the original
      labels their widths (`tools/gen_menu_label.py` -> `data/port/opsettings.png`;
      the font is not in the repo, DS-addons' appcore.jar has the game's copy).
      Opened there, the screen is letterboxed like the menu, over its background,
      and Back returns to it. `NFM_SCREENSHOT_MENU=mainsettings` captures it.
- [x] Docked at launch: the window is 1920x1080 (HD draws at that size);
      handheld 1280x720.
- [ ] Docking / undocking mid-game keeps the size the game started with.
- [ ] Vibration on real hardware (Eden forwards it to a PC pad; untested here).
- [ ] Touch screen in menus (taps as clicks), as the web port does.

## From nfm-master, later (small first)

- [ ] Raw "Recharged" stats in `car_define.c` (web `CarDefine.readRawStats`).
- [ ] `lightBrake` (light 3), 1000-piece cars, Re-Lit arrow stunts.
- [ ] Custom cars from the SD card; the Extended, R&R and Origins models.
- [ ] Extended Mode -- its own engine (~18k-line xtGraphics); a project of its own.
- [ ] Car and stage editors, touch and Joy-Con first.
