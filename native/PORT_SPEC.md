# PORT_SPEC.md — porting web/*.js to native/core/*.c

Read this before writing a line. It extends `web/TRANSPILE_SPEC.md`, which is
still the authority on the *simulation semantics* (int32 wrapping, float32
rounding, the `+= (int)(...)` trap, preserved bugs). This document is only
about the JS→C mechanics and the native-platform runtime.

## 0. Two platforms, one core

This is a native C port with **two build targets sharing one codebase**:

- **Linux** (`native/platform/linux/`) — SDL2 + desktop OpenGL, buildable and
  runnable right here, no special hardware. This is the one to actually test
  against as modules get ported: run it, look at it, diff its numbers against
  `web/`.
- **PS Vita** (`native/platform/vita/`) — vitaGL over `sceGxm`, cross-compiled
  with VitaSDK. Needs hardware or an emulator (vita3k) to run, and needs
  VitaSDK to even build, neither of which exists in the environment that
  wrote this file.

Everything that is a genuine port of a `web/*.js` module lives in
`native/core/` and is **platform-agnostic C** — no `#ifdef`s for Linux vs
Vita, no SDL/vitaGL calls. Both platform backends link the same
`native/core/` sources. This split exists specifically so the simulation
math can be built and run on Linux *before* it ever has to work on a Vita —
see §8.

Only `native/platform/{linux,vita}/` are allowed to differ per platform:
window/context creation, input polling, the frame loop, asset paths. Both
currently draw the same placeholder scene in `main.c` so that once one of
them shows something wrong, the other is the reference to check it against.

## 1. Why JS, not Java, is the source for the `core/` port

`web/*.js` is already a verified, line-by-line, semantics-preserving
transcription of the decompiled Java (see `web/TRANSPILE_SPEC.md` §6 for how
it was checked against the real classes by reflection). It is closer to C
than Java is:

- both have real 32-bit `int` and `float` primitive types, no boxing
- both have explicit fixed-size arrays
- module-level free functions map directly to C functions; JS classes with
  plain-data fields map directly to C structs + functions taking `T *self`

**Port from `web/`, not from `decompilation/java-src/`.** If the JS looks
wrong, the fix belongs in `web/` first (with its own test suite), and the C
port picks up the fix after. Do not re-derive semantics from the Java bytecode
here — that work is already done and tested.

## 2. Numeric semantics in C

| JS (`java.js` helper) | C |
| --- | --- |
| `idiv(a, b)` | plain `a / b` — C's `/` on `int32_t` already truncates toward zero |
| `trunc(x)` | `(int32_t)x` — C float→int cast truncates toward zero, but does **not** saturate at INT32 bounds the way `java.js#trunc` does. Use the provided `jtrunc()` helper (`native/core/java_compat.h`) which replicates the saturating behaviour, for any value that can plausibly overflow (car coordinates do — see TRANSPILE_SPEC §2b). |
| `Math.imul(a, b)` / `i32(a + b)` | nothing to do — compile with `-fwrapv` (already set in every `CMakeLists.txt` here) so signed 32-bit overflow wraps exactly like Java/JS, instead of being UB. **Never build this project without `-fwrapv`.** |
| `fr(x)` | nothing to do — a C `float` IS Java's `float`; every C operation on two `float`s already rounds to float32 at each step, same as `Math.fround` does explicitly in JS. This is the one place C is less work than JS, not more. |
| `Int32Array` field | `int32_t arr[N]` |
| `Float32Array` field | `float arr[N]` |
| `jround(x)` | `floorf(x + 0.5f)` |
| `JavaRandom` | port `web/java.js`'s `JavaRandom` LCG verbatim — `native/core/java_compat.c`. Do not use libc `rand()`, it will desync anything compared against the JS/Java reference and breaks netplay determinism. |

## 3. File mapping

One `.c`/`.h` pair per `web/*.js` module, same base name, lowercased, in
`native/core/`: `web/ContO.js` → `native/core/cont_o.c` / `.h`,
`web/Plane.js` → `native/core/plane.c`, etc. Keep the original file name in a
comment at the top of each C file so it is greppable (`// ports web/ContO.js`).

Do not fold multiple JS modules into one C file even when small — the 1:1
mapping is what lets a reviewer diff JS and C side by side, same reason
`TRANSPILE_SPEC.md` §0 forbids restructuring JS against Java.

Port order (dependency order, pure-math/data first, rendering and platform
glue last):

1. `trig.js` → `core/trig.c` — **done**, see below
2. `java.js` (the helpers, not a game class) → `core/java_compat.c` — **done**
3. `Plane.js`, `Medium.js`, `Trackers.js` — pure math, already Java-verified,
   no I/O. `Trackers.js` — **done** (`core/trackers.c`). `Plane.js` —
   **done** (`core/plane.c`), the whole file including both draw methods.
   `Medium.js` — **partially done** (`core/medium.c`): full field layout,
   plus `init`/`sin`/`cos`/`xs`/`ys`/`rot`/`random`/`follow`/`groundpolys`/`d`
   — everything `Plane.js` and a static scene need — not the other camera
   modes or the procedural generators (clouds/mountains/stars), which need
   `JavaRandom`-seeded generation this port hasn't reached yet.
4. `ContO.js`, `Wheels.js`, `Control.js` — physics, depend on (3).
   `Wheels.js` — **done** (`core/wheels.c`), the whole file. `ContO.js` —
   **partially done** (`core/cont_o.c`): the `#initBuf` `.rad`-file
   constructor (parser + the `loadnew` post-processing pass), the
   `#initCopy` clone constructor, and the `d()`/`rot()`/`xs()`/`ys()`/
   `electrify()` runtime methods, not `#initModel` or any other runtime
   method — see `cont_o.h`.
5. `CarDefine.js`, `Record.js`, `CheckPoints.js` — car/track data model.
   `CarDefine.js` — **not needed for M1** (its `loadcar()`'s actual
   geometry work is just `#initBuf`; the rest is gameplay stat tables that
   only matter once `Mad.js` reads them — see `TASKS_NATIVE.md`).
6. `Mad.js` — the simulation core, depends on everything above
7. `graphics.js`, `XtGraphics.js` — rendering; this is where WebGL calls
   become GL1.1 calls (§5) and the module stops being a 1:1 port
8. `GameSparker.js`, `Smenu.js`, `main.js` — game loop, menus, platform glue;
   `GameSparker.js` — **partially done** (`core/game_sparker.c`): a lean
   `loadbase()` (full) + `loadstage()` (real geometry/colours only, not
   the CheckPoints/XtGraphics/Control/Record-dependent parts — see
   `game_sparker.h`). Otherwise rewritten per-platform against
   `native/platform/{linux,vita}/`, not
   ported line-by-line

Steps 1–6 go in `native/core/`, are platform-agnostic, and should stay
mechanical, verifiable ports — build and check them on Linux (§8) long
before Vita ever enters the picture. Steps 7–8 are where actual
per-platform engineering happens (see §5, §6).

## 4. Class shape

```c
// ports web/Foo.js
#include "foo.h"

void foo_init(Foo *self, ...) {
  // field initialisers in the SAME ORDER the JS constructor lists them
}

void foo_some_method(Foo *self, int32_t a, int32_t b) { ... }
```

- JS `this.` → explicit `self->`
- no classes/inheritance in this codebase's hot path worth modelling with
  vtables; a plain struct + free functions is enough (matches what the JS
  already does — these are data classes, not polymorphic hierarchies)
- JS `objArray(n)` (null-filled `Object[]`) → depends on what it holds; most
  uses in this codebase are fixed-size arrays of a known struct type, so
  prefer `T arr[N]` with a `bool active[N]` / count, not a null-checked
  pointer array
- fixed 360-entry trig tables (`trig.js`) → `static const float` arrays,
  copied verbatim (see `native/core/trig.c` — same literals, do not
  regenerate from `sinf`/`cosf`, for the same lockstep-determinism reason the
  comment in `trig.js` gives)

## 5. Rendering: WebGL → GL1.1 immediate mode, on both platforms

`web/graphics.js` implements a `Graphics2D`-shaped shim (`fillPolygon`,
`drawPolygon`, `fillRect`, ...) on top of one WebGL draw call per frame,
because **the one invariant that must not break** (`AGENTS.md`) is: no depth
buffer, occlusion is submission order only, colour is a vertex attribute.

The renderer targets the GL1.1 immediate-mode subset
(`glBegin`/`glVertex`/`glColor`/`glEnd`) that both backends can run:

- **Linux**: real desktop OpenGL via SDL2, requesting a *compatibility*
  profile (`platform/linux/main.c` already does this) — core-profile GL
  removed `glBegin`, so this is not optional.
- **Vita**: [vitaGL](https://github.com/Rinnegatamante/vitaGL) (a GL1.1
  subset over `sceGxm`), not `vita2d` — vitaGL gives the same immediate-mode
  semantics, which map onto `fillPolygon` almost directly.

On both, `GL_DEPTH_TEST` stays **disabled** — that is what preserves the
painter's-algorithm invariant; it is a one-line omission, not a rewrite.

Rules carried over unchanged from `TRANSPILE_SPEC.md` §4:
- never reorder, hoist, batch-by-material, or sort draw calls
- one polygon in JS → one `glBegin(GL_TRIANGLE_FAN)`/`glEnd()` in C, in the
  same place in the same order, on either platform
- `Plane.d()`'s per-object depth sort (`ContO.dist`) ports verbatim (step 4
  above, into `core/`); it is what feeds the submission order, the GL calls
  just consume it

Because the renderer only uses the shared GL1.1 subset, it belongs in
`native/core/` too, not per-platform — only context setup and the swap call
differ, and those already live in `platform/{linux,vita}/main.c`.

## 6. Platform glue (not a port — new code)

No JS equivalent to transcribe; these are genuinely new, written against
`web/main.js`/`GameSparker.js` for *behaviour* (what happens each frame, what
triggers what) but not their code. One version per platform:

- `platform/linux/` — SDL2 window/GL context init, `SDL_GL_SwapWindow` frame
  pump, SDL keyboard/gamepad input, replaces `requestAnimationFrame` +
  `web/vfs.js`'s browser-fetch path with local file reads
- `platform/vita/` — `sceGxm`/vitaGL context init, `vglSwapBuffers` frame
  pump, `SceCtrlData` (d-pad/analog/buttons) input, and asset loading from
  `ux0:app/<TITLEID>/` or a bundled `.vpk` asset dir — byte-identical files,
  no format changes (`AGENTS.md`: assets are **not to be modified**)
- audio (`web/audio.js`, `web/music.js`): Linux via SDL_audio, Vita via
  `SceAudioOut`/`pss` streaming; not started, see TASKS_NATIVE.md
- both input backends should map onto the **same logical input struct**
  (buttons/axes as an enum, not raw SDL or Sce types) so `core/` game code
  never sees a platform type

## 7. Verification

Same standard as `TRANSPILE_SPEC.md` §6, adapted: no Java reflection probe
available for a C build, so the oracle is **the already-verified JS**, run
under Node.

For each ported `core/` module, write `native/tests/<module>_test.c` using
fixed inputs, and generate the expected outputs by running the corresponding
`node --test web/<Module>.test.js` fixtures (or a small throwaway Node script
importing the module directly) — copy the JS's *literal* expected values into
the C test as comments noting they came from the JS run, same as
`TRANSPILE_SPEC.md` requires literals from the Java probe.

`native/tests/` builds and runs on the host (plain gcc/clang, `-fwrapv`, no
VitaSDK needed) — see `native/tests/CMakeLists.txt`. This is the fast, cheap
check and covers `core/` only. The Linux platform build
(`native/platform/linux/`) is the next cheapest thing that can actually be
*looked at* — build and run it before ever touching the Vita cross-build.
Cross-compiling with VitaSDK and running on hardware/vita3k is the expensive
step, and the one most likely to be broken by something the other two
checks would already have caught — do it last.

## 8. Build

```sh
# Host unit tests -- core/ only, no window, fastest feedback:
cmake -S native/tests -B native/tests/build
cmake --build native/tests/build
native/tests/build/java_compat_test

# Linux dev target -- SDL2 + OpenGL window, actually watch it run:
cmake -S native -B native/build-linux -DNFM_PLATFORM=linux
cmake --build native/build-linux
native/build-linux/platform/linux/nfm_linux

# PS Vita cross-build -- needs VitaSDK, NOT available in this environment,
# NOT build-tested by whoever wrote this file. Written against the standard
# vitasdk CMake toolchain layout; verify the vitaGL call shapes in
# platform/vita/main.c against a real toolchain before trusting them.
export VITASDK=/usr/local/vitasdk && export PATH=$VITASDK/bin:$PATH
cmake -S native -B native/build-vita -DNFM_PLATFORM=vita \
  -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build native/build-vita
```

Say explicitly, in any report, which of these three were actually run vs.
only written.

## 9. Do not touch

- `web/`, `data/`, `stages/`, `mycars/`, `mystages/`, `music/`, `java/` — the
  port reads/references these, never edits them
- `web/TRANSPILE_SPEC.md`'s numeric rules are binding here too; this file
  only adds the JS→C and platform-specific parts on top
