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
- [x] **Settings redesigned** (2026-10-06, checked in Eden), in the launcher's look
      (beige stripes, black-bordered rows, the selected one black/yellow with a
      hazard stripe), pages of rows as data (`kSettingsPages` in game.c):
      - *Graphics*: Image Quality, **Draw Distance** (Original/Far/Max: the fog
        bands, and where objects stop drawing, at 100/150/200% --
        `medium_draw_distance`), **Scenery Detail** (High/Low: the original's own
        low-detail mode, `resdown` 2, re-applied after every stage load), **Shadows**,
        **Particles** (dust and sparks), Motion Blur (now a row like the rest).
        Shadows/Particles off still run their draw code (it advances particle
        state) and drop what it drew (`gfx_mark`/`gfx_rewind`).
      - *Audio*: **Music** and **Effects** volume, 0-100 (`radical_music_gain`,
        `audio_sfx_gain`; at 100 the music is bit-identical).
      - *Interface*: **Show FPS** Off / FPS / Detailed (the old `-DNFM_SHOW_FPS`
        overlay, now always measured; the screenshot hook dumps after it).
      - *Gameplay*: Screen Shake, Vibration. Plus Reset to Defaults.
      Drawing randoms are a separate stream (`nfm_set_draw_phase`), so drawing more
      or fewer objects never changes the race.
- [x] **Smooth Frames** (Graphics row, default on; `smooth_frames=`), the web
      port's INTERPOLATE: the race still ticks at 18.9 Hz, but every display frame
      draws the cars and camera blended between the two latest ticks
      (`smooth_capture/apply/restore` in game.c), then puts the tick's values back.
      The camera keeps the angle's fraction (`Medium.fxz/fzy`, zero otherwise, so
      off is bit-identical); car angles stay whole degrees. Between ticks the HUD
      is the last tick's draw list replayed (`GfxClip`: its drawing advances blink
      and banner timers), the shake holds (its randoms are the race's), the trail
      decays by frame length (`keep^(dt/53ms)`), and every object's `dist` is put
      back (checkstat reads it). Checked headless (Linux/Xvfb, frames between ticks
      show the replayed HUD); not yet on hardware.
- [x] The camera ran once per display frame, outside the tick loop: the fly-by
      orbit, the orbit view and the camera eases ran ~3x fast at 60 Hz. It runs
      in the tick now, as in the Java. Confirm inside the tick (hold card's
      continue, fly-by skip) is latched, so a press between ticks is not lost.
- [x] `NFM_SCREENSHOT_COUNT=n`: the screenshot hook dumps n consecutive frames
      (`path.0`, `path.1`, ...) and logs whether each one ticked.
- [x] **The stage backdrop** (2026-10-06, checked headless on stages 1, 2, 8, 20):
      clouds, mountains, the stars of the `lightson` stages, and the ground
      patches around the track (`newpolys`: groundpolys was ported but had no
      data). Ported from web/Medium.js into medium.c with the stage lines that
      feed them -- `clouds(`, `density(`, `fadefrom(`, `mountains(`, `lightson`
      (game_sparker.c). Clouds draw from the race's random stream at load, as
      the original; stars twinkle per tick. `fadefrom(` sets the stage's own fog
      bands and Draw Distance now scales those (`Medium.fade_base/fade_pct`).
- [x] **Stunts in 8 directions**: B + left stick reads eight 45-degree sectors;
      a diagonal presses two arrows at once (the original's combined stunts).
      It was snapped to 4 (the Vita's choice), so diagonals did nothing.
- [x] **The AI's route points** (`set(...)p` lines, typ 0/-1..-4): game_sparker.c
      read only `chk`, so control_preform's waypoint code never had a waypoint
      and the bots aimed from gate to gate. Same fixed seed, 3 ticks a frame,
      1800 ticks (`NFM_SIM_TICKS_PER_FRAME=3`, deterministic): stage 10 bots at
      10/8/5 checkpoints against 3/4/5 before; stage 1's at 2/2/4 against 0/0.
- [x] Projection in 64 bits (`medium_xs/ys`, `cont_o_xs/ys`): identical to the
      Java wherever its int product does not wrap; removes C's signed-overflow UB.
      (The stage-select dive did not show the web launcher's far-camera
      distortion -- that one was the launcher's own overhead view.)
- [x] Screenshot hook logs each car's cleared checkpoints.
- [x] **Fixes from the first hardware test** (Switch v1, 2026-10-06):
      - ~47 fps with ~6 ms of work a frame: a flat `platform_delay_ms(16)` after
        every swap (the Switch's swap does not wait for vsync). Now a 60 Hz
        limiter that sleeps only the rest of the 16.7 ms frame.
      - The repair tint stuck on a fixed car: `cont_o_fixit` advanced `fcnt` per
        DRAW as well as `cont_o_step_fix` per tick; with smooth frames it ran
        past the 7/8 that mad_drive reads, so the car was never rebuilt. Only
        the tick advances it now (the replays step it per replay tick).
      - The starting grid scattered: a race's first frame drawn before its
        first tick turned smooth frames on with the LAST race's snapshots and
        restored those positions into the cars. Smooth frames wait for a tick
        of the current race (`smooth_captured`).
- [x] **Settings > Performance Test** (main-menu Settings only): Free Play on
      stage 9 with Dr Monstaa (hard to wreck), seed 9001, the AI driving your
      car, measured for 60 s from the green light. Summary on screen; full report
      in `benchmark.txt` beside the save (`sdmc:/switch/nfm/benchmark.txt`):
      settings and resolution, average / 1% low fps, median / p95 / p99 / worst
      frame, late frames (over 20 ms, over 33 ms), work per frame split logic /
      draw / gl / swap, faces and vertices, fps per second, the 8 slowest frames.
      Headless: `NFM_SCREENSHOT_MENU=bench NFM_BENCH_SECONDS=15`.
- [x] **Freeze on hardware (game stopped, music went on)**: smooth frames'
      interpolated passes replay the tick's draw randoms in a cycle
      (`medium_random`), and `plane_d`'s embos-16 spark re-rolled
      `while (pa == pb)` -- with a one-value recording, forever. A heavily
      damaged car could hang the game. Bounded now, as web/Plane.js already
      was ("the page froze on a wasted car's replay"); `plane_test`'s
      `test_d_spark_reroll_terminates` hangs without the fix. The AI's skip
      loop (control.c) is bounded to one trip round the route as well.
- [x] **Freeze and crash reports** (platform/common/diag.c): the loop leaves
      breadcrumbs (phase, screen, stage, tick, the AI car being thought about);
      a watchdog thread writes `freeze.txt` beside the save when the loop
      stops for 8 s; on Switch, `__libnx_exception_handler` writes `crash.txt`
      (registers, fault address, pc/lr and a frame-pointer backtrace as offsets
      into the .nro -- `aarch64-none-elf-addr2line -e nfm_switch.elf 0x...`)
      and closes the game. `NFM_DEBUG_FREEZE_FRAME=n` hangs on purpose.
- [ ] Missing vs the web port: the Arial font, custom cars, Rivals.
- [ ] Performance not measured on hardware. The web port's known heavy item is
      the per-plane ground shadow (`Plane.s` for every plane of a nearby car);
      Settings > Shadows off is the lever until it is measured.
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

- [x] **Web fixes the native port lacked, batch 1** (audit against nfm-master):
      brake and gear thresholds divide `handb`/`swits` as ints like Java
      (`swit_speed`/`acel_step` in mad.c); wheel dust (`cont_o_pdust`) rolls,
      drifts, grows and ages on ticks only, and `scx *= n3` is the float decay
      the bytecode does; the horizon pitch test sees the fractional `fzy`;
      leaving pause/Settings/pause replay drops the paused time instead of
      running 3 catch-up ticks; `game_sparker_loadstage` rejects a stage like
      Java's `stage = -3` (model id out of range, Trackers full, under 2
      checkpoints, 16000+ ground cells) -- the stage list shows ERROR LOADING
      STAGE and a race on it returns to the menu; face points cap at 100.
- [ ] Batch 2: `pile(` rock piles (procedural `ContO.#initModel`, 31 stages).
- [ ] Batch 3: visual-only low detail (not `resdown=2`, which drops scenery
      collisions) and the web's lightweight intro; draw distance keeps cars'
      `dist` neutral.
- [ ] Float/double rounding sites from the web's later float audit (mad.c
      bounce `- 0.3`, `tilt/1.5`, `gr += abs(n*1.5)`, ...); stages 28-32
      `loadnew`.

## From nfm-master, later (small first)

- [ ] Raw "Recharged" stats in `car_define.c` (web `CarDefine.readRawStats`).
- [ ] `lightBrake` (light 3), 1000-piece cars, Re-Lit arrow stunts.
- [ ] Custom cars from the SD card; the Extended, R&R and Origins models.
- [ ] Extended Mode -- its own engine (~18k-line xtGraphics); a project of its own.
- [ ] Car and stage editors, touch and Joy-Con first.
