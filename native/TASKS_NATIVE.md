# TASKS_NATIVE.md — native C port status (Linux + PS Vita)

Companion to the repo-root `TASKS.md`/`WORK.md`, scoped to the `native/`
native port. Read `native/PORT_SPEC.md` first — it has the architecture and
porting rules this file assumes, including why there are two platform
targets sharing one `core/`.

## Done
- [x] Scaffold: top-level `native/CMakeLists.txt` (builds `nfm_core` then one
      of `platform/linux` or `platform/vita` via `-DNFM_PLATFORM=`), and
      `native/tests/CMakeLists.txt` (host build of `core/` only, no SDK/SDL
      needed).
- [x] `web/java.js` → `native/core/java_compat.c/.h` — the saturating-trunc,
      round-half-up, two-stream xorshift32, JDK-exact `JavaRandom`, and
      `Color.RGBtoHSB`/`HSBtoRGB` helpers. Host-tested against the same
      literals `web/java.test.js` uses (`native/tests/java_compat_test.c`,
      all passing).
- [x] `web/trig.js` → `native/core/trig.c/.h` — the 360-entry baked sin/cos
      tables, mechanically regenerated from the JS literals. Host-tested
      entry-for-entry.
- [x] `native/platform/linux/` — SDL2 + desktop OpenGL (compatibility
      profile). **Built and run in this environment**: opens an 800x450
      window, draws a hexagon whose vertices come from `core/trig.c` and
      whose colour comes from `core/java_compat.c`'s HSB conversion, no
      depth test. `apt-get install libsdl2-dev libgl1-mesa-dev` was needed
      and was done. This is the target to build against as more of `core/`
      gets ported — it is the only one of the two that can actually be
      watched run.
- [x] `native/platform/vita/` — vitaGL placeholder mirroring the Linux
      scene. **Not compiled anywhere** — no VitaSDK in this environment.
      Written against the documented vitaGL API from memory of its shape;
      treat the exact call signatures as unverified until a real VitaSDK
      build checks them.
- [x] `web/Trackers.js` → `native/core/trackers.c/.h` — the spatial grid
      (`devidetrackers`) and squared-distance helper (`py`). No
      `web/Trackers.test.js` exists, so the expected values were captured by
      running the real `web/Trackers.js` under Node with a synthetic scene
      (see `native/tests/trackers_test.c`'s header comment) rather than
      copied from a pre-existing fixture — PORT_SPEC.md §7's "JS is the
      oracle" rule, applied for the first time here. Host-tested, all
      passing, and separately run under ASan/UBSan (`-fsanitize=address,undefined`)
      to check the manual malloc/free grid — caught and fixed one real leak
      (`trackers_free_sect` wasn't freeing the per-cell arrays, only the row
      pointers) before it went anywhere near the Linux or Vita target.

- [x] `web/Medium.js` → `native/core/medium.c/.h` — **PARTIAL**. Declared the
      full field layout (every field the JS constructor sets, same order,
      including NULL/0 placeholders for the procedural-generation arrays
      nothing allocates yet) so later work is additive, not a reshape. Only
      implemented what `Plane.js` actually calls: `medium_init`,
      `medium_sin`/`cos` (with the fractional-index lerp), `medium_xs`/`ys`
      (perspective projection), `medium_rot`, and `medium_random` (the
      game's own correlated PRNG — separate from `java_compat`'s
      `nfm_random`, though it calls that as its underlying source).
      NOT ported: camera modes (`watch`/`aroundtrack`/`around`/`getaround`/
      `transaround`/`follow`/`getfollow`), the procedural generators
      (`newpolys`/`newclouds`/`newmountains`/`newstars`), their draw methods
      and `d()` itself (all need the GL bridge, PORT_SPEC.md §5), and the
      stage-load setters (`setsky`/`setcloads`/`setgrnd`/etc.) — none of
      these are needed yet.
      No `web/Medium.test.js` exists, so — same fallback as Trackers —
      expected values were captured by running the real JS under Node.
      Two real correctness risks were caught and fixed by comparing against
      the JS semantics directly, not against test failures (nothing failed
      until they were fixed correctly the first time — worth flagging
      because it means a less careful port would have shipped silently
      wrong physics/rendering math): (1) `nfm_random()` itself had never
      been directly tested against JS `random()` despite `medium_random()`
      depending on it entirely — added that check first, before trusting
      anything downstream; (2) the trig fractional-lerp and the `rand[trn]/10.0`
      division in `medium_random()` initially computed in native `float` at
      each step, which is "double rounding" relative to the JS (which
      computes the whole expression in double and rounds to float32 exactly
      once) — usually identical, not provably always identical, so both
      were rewritten to compute in `double` and cast once, matching the JS's
      actual rounding behaviour instead of the common case. Host-tested
      (`native/tests/medium_test.c`) and separately clean under
      `-fsanitize=address,undefined`.

## Track: path to a testable Linux build

This is the concrete, dependency-ordered checklist toward "run it on Linux
and actually see/drive the game" — not full feature parity. It mirrors a
scoping decision the JS port already made (`web/GameSparker.js`'s own header
comment: "Ported: loadbase(), loadstage(), ... and the fase == 0 race tick
... Everything else in that class is applet plumbing, menus, mouse handling,
cookies and multiplayer, all of which the port replaces or drops"). Same cut
here: menus, HUD polish, audio and netplay come after driving works, not
before. Traced from `web/main.js`'s own construction order (`Medium`,
`Trackers`, `CheckPoints`, `Control`, `Record`, `CarDefine`, `Mad`,
`GameSparker`, `XtGraphics`) — that's the real dependency graph, not a guess.

Vita-specific work (input mapping, asset packaging) is listed under its own
heading further down since none of it blocks Linux.

### M1 — a static scene renders (no physics, no input)
- [x] **Graphics bridge**. Split in two, matching how the JS itself
      separates headless-testable geometry from the WebGL calls:
      - `native/core/gfx.c/.h` — pure CPU-side vertex-list building (the
        geometry half of `web/graphics.js`'s `Graphics2D`: `fillPolygon`
        with both the convex fan and the concave/self-intersecting
        trapezoid-fill fallback, `drawPolygon`, `drawLine`, `fillRect`,
        `drawRect`, `fillOval`, `clearRect`, `setColor`, `setComposite`).
        Zero GL dependency, fully host-testable
        (`native/tests/gfx_test.c`). Caught one real behavioural quirk by
        checking against a Node oracle rather than assuming: `clearRect`
        sets r/g/b/a to black internally but the JS never calls `_pack()`
        afterward, so it actually fills with whatever colour was last set
        via `setColor` — not black. Ported that verbatim (commented why),
        per `web/TRANSPILE_SPEC.md` §3's "preserve the game's bugs" rule,
        instead of "fixing" it into an actual black fill.
      - `native/core/gfx_gl.c/.h` — `gfx_submit_gl()`, immediate-mode
        GL1.1 draw calls (`glBegin(GL_TRIANGLES)`/`glColor4ub`/
        `glVertex2f`/`glEnd`) over the vertex list `gfx.c` built. Not a
        port (no JS equivalent — the JS uploads a VBO and calls
        `gl.drawArrays` once; this uses immediate mode instead so the same
        source works against both desktop GL and vitaGL through a
        per-platform `gl_include.h`, see `platform/{linux,vita}/gl_include.h`
        and the `target_include_directories` trick in `native/CMakeLists.txt`).
        Not host-testable (needs a real GL context) — verified by rewiring
        `platform/linux/main.c` to draw its placeholder hexagon through
        this path instead of raw GL calls, and running it under Xvfb +
        `LIBGL_ALWAYS_SOFTWARE=1` again: clean exit, no SDL/GL errors. Not
        yet verified by actual pixel readback — that's the "first real
        screenshot milestone" a few items down, once there's a real scene
        (car + stage) worth screenshotting instead of a placeholder.
- [x] `Plane.js` → `core/plane.c/.h` — **the whole file**, not a slice:
      constructor, `deltafntyp`, `loadprojf`, `spy`, and both draw methods
      `d()` (the face itself — damage jitter, the shard/chip debris
      animation, the pinned-triangle damage decals, fog, headlight
      recolouring, the `flx`/`gr` special-case overlays) and `s()` (ground
      shadow, including the `Trackers`-based colour lookup for shadows cast
      onto a coloured surface). `rot()`/`xs()`/`ys()` turned out to be
      IDENTICAL math to `Medium`'s own versions (`Plane.rot()` calls
      `this.m.cos()`/`sin()`, exactly what `medium_rot()` already does) so
      they delegate to `medium_rot`/`medium_xs`/`medium_ys` instead of
      duplicating already-verified code.

      `web/Plane.test.js` covers `rot`/`xs`/`ys`/`spy`/`deltafntyp` with
      literals from a real Java probe — reused verbatim, free verification.
      `d()`/`s()` have no JS test coverage of their own, so those went
      through the same Node-oracle process as `Medium`'s draw methods: 5
      scenarios (an undamaged quad, a mid-damage jitter quad, a
      damage-shard/chip-debris quad, the wheel-roll/steer rotation gate,
      and a ground shadow with the `Trackers` colour-lookup branch
      exercised), all built with a real `Medium`/`Trackers`, not stubs.

      A real, user-reported bug was found and fixed here after M2 wiring
      made it visible for the first time: `plane_d`'s wheel-roll/steer
      rotation was gated on `n7 != 0`/`n6 != 0` (whether the car-wide
      `wzy`/`wxz` ANGLE happened to be nonzero) instead of the JS's own
      `this.wz !== 0`/`this.wx !== 0` (whether THIS PLANE belongs to a
      wheel at all). Every plane in the whole car — body panels included
      — was rotating around a wheel pivot any time the car had any speed
      or steering input, visibly warping the chassis ("looks like it
      turned into a tricycle"). Every existing `plane_d` oracle scenario
      passed `wxz=wzy=0` (a parked car), so none of them exercised this
      gate either way — M1's static screenshots never had a moving car to
      catch it, and this was invisible until real physics + real input
      were both wired up. Fixed; see `plane.c`'s comment at the call site
      and the new `test_d_wheel_rotation_gate` scenario above, which
      specifically exercises nonzero `wxz`/`wzy` against both a body
      plane (must stay unaffected) and a wheel plane (must rotate).

      **The oracle caught a real bug on the first attempt**, not a close
      call: the `cox`/`coy`/`coz` shard-jitter block in the JS computes
      `cox[1]`/`cox[2]` first, then `coy[1]`/`coy[2]`, then `coz[1]`/`coz[2]`
      — grouped by AXIS. The natural way to write that loop in C groups it
      by INDEX instead (x/y/z together per point) — which is what got
      written first, and which draws every subsequent frame of that shard
      from the wrong `medium_random()` values, since the draw order changes
      which random draw feeds which coordinate. The chip-debris oracle
      scenario exists specifically to exercise this path, and its vertex
      positions were visibly wrong under the wrong loop shape (matching
      colours, mismatched positions) before the fix. Left as a named test
      (`test_d_chip_debris`) rather than folded away, because it's exactly
      the failure mode that passes a build and still draws the wrong thing.

      Host-tested (`native/tests/plane_test.c`), clean under
      `-fsanitize=address,undefined`. `nfm_core` and the Linux target
      rebuilt and re-verified clean under Xvfb.
- [x] The rest of `Medium.js` needed to draw a frame: `medium_d` (world
      backdrop: ground/sky/fog gradient bands + 19 more atmosphere bands +
      ground polys), `medium_groundpolys`, and `medium_follow` (the default
      chase camera — takes the car's position as three ints rather than a
      `ContO*`, since `ContO.js` isn't ported yet and that's all `follow()`
      reads from it). Still NOT ported: the other camera modes, and
      `drawstars`/`drawmountains`/`drawclouds` — `medium_d` skips those
      three calls outright rather than stubbing them, which is exactly
      equivalent to calling them (their backing counts `noc`/`nmt`/`nst`
      stay 0 until `newclouds`/`newmountains`/`newstars` exist, at which
      point those three would loop zero times anyway) — verified, not
      assumed: the oracle comparison below used a fresh, unpopulated
      `Medium` on both sides and diffed exactly.

      This method has the densest float-rounding footgun surface ported so
      far — `d()`/`groundpolys()` mix, per expression, three different
      truncation shapes: (1) a single `fr()` wrapping one binary op, where
      native C `float` arithmetic is provably equivalent to "compute in
      double, round once" (a double holds any float±float or float×float
      product exactly before rounding); (2) one `fr()` wrapping a MULTI-op
      expression with no inner `fr()` splitting it (e.g. `fr((abs(y)-250)/
      (fade[0]*2))`), which native float chaining would round more often
      than the JS does; (3) `trunc()` applied directly to an `fr()`-rounded
      value combined with FURTHER un-wrapped arithmetic (e.g.
      `trunc(fr(ogpx*pvr) + cgpx - x)`), where only the `fr()`-marked part
      is float-rounded and the rest must stay in JS's native double
      precision. Cases 2 and 3 needed double precision to match the JS;
      added `jtrunc_d()` (`core/java_compat.c`, double-precision saturating
      truncation) for case 3, alongside the already-existing float
      `jtrunc()` for case 1. Every site in `medium.c` is commented with
      which case it is — that classification, not "run it and see if the
      test passes," is what the oracle comparisons below were checking.

      Verified against the real JS running headless (`web/graphics.js`'s
      `glCanvas === null` mode) for: `follow()` (3 scenarios — default,
      clamped-positive, clamped-negative bcxz), `groundpolys()` (a synthetic
      2×2 populated cell grid, since `newpolys()` isn't ported — 72
      vertices, exact match), and `d()` (4 scenarios: default/zero state,
      non-zero `zy` camera tilt, `resdown==2`, and the `lightn` flicker path
      which exercises `medium_random()` mid-draw — vertex counts from 198 to
      312, every one diffed byte-for-byte against the JS, not sampled,
      though the committed test only keeps a representative slice of that
      diff to avoid embedding ~1500 literals). All exact matches, first try
      after the case-1/2/3 classification above — nothing was fixed by
      trial and error against a failing test.
- [x] Asset loading (turned out to belong in `core/`, not per-platform —
      see below). `web/vfs.js`'s `readText`/`readLines`/`readBytes`/
      `readZip`/`parseZip`/`entryText` → `native/core/vfs.c/.h`. NOT
      ported: `detectFpath` — its `./` vs `../` HTTP probe has no meaning
      on a real filesystem; the platform sets the fpath directly instead.
      Zip reading (needed for `data/models.zip`, once `GameSparker.
      loadbase()` needed it) uses zlib's raw inflate (`inflateInit2` with
      `windowBits=-15`, matching the zip format's own raw DEFLATE streams
      and the JS's `DecompressionStream('deflate-raw')`) rather than a
      hand-rolled decompressor — zlib is a standard, well-tested library
      available on both Linux and (assumed, via VitaSDK's zlib portlib,
      not yet verified) Vita. Host-tested against the real
      `data/models.zip`: 84 entries, and a total uncompressed size of
      621172 bytes — the EXACT constant `GameSparker.js`'s own
      `loadbase()` checks the archive against, so this isn't just "the
      test passes", it's the same integrity check the real game performs
      landing on the same number.

      Realized while writing this that the earlier "per-platform
      `assets_linux.c`/`assets_vita.c`" split in this file was overcautious:
      both platforms have standard C `fopen`/`fread` (VitaSDK's newlib
      provides it against `ux0:`-style paths too), so the file-reading CODE
      is identical — only the fpath STRING differs per platform, which is
      config, not a reason to fork the implementation. `vfs.c` lives in
      `core/` accordingly; only the fpath value gets set from
      `platform/{linux,vita}/main.c`.

      One deliberate behaviour difference from the JS, not a bug: `vfs.c`
      reads bytes through unchanged (no encoding conversion), matching
      `java.io.DataInputStream.readLine()`'s actual behaviour (ISO-8859-1,
      byte-for-char) and `web/vfs.js`'s own `entryText()` — NOT
      `web/vfs.js`'s `readText()`, which decodes as UTF-8 via the browser
      `fetch` API and so can disagree with the Java on non-ASCII bytes. That
      mismatch is latent in the existing JS port; a native implementation
      has no `fetch()` to inherit it from, so it matches Java instead.

      Host-tested (`native/tests/vfs_test.c`): the `readLines` cases are
      copied verbatim from `web/vfs.test.js`'s own test, plus a real read of
      the repo's actual `stages/1.txt` checked against the same assertions
      `web/vfs.test.js`'s stage-1 test makes. Clean under
      `-fsanitize=address,undefined`. Also proven from the actual built
      Linux binary, not just the test suite: `platform/linux/main.c` now
      reads `stages/1.txt` at startup and logs its line count (129) before
      entering the render loop — ran under Xvfb, confirmed in the output.
- [x] `web/ContO.js`'s `#initBuf` constructor → `native/core/cont_o.c/.h` —
      the `.rad`-file parser (JS lines 126-829), not the whole file (see
      `cont_o.h`'s header comment for the full scope split: `#initModel`/
      `#initCopy`/every runtime method are M2/M3 material, not needed to
      load and see a car). Two parts: (a) the line-by-line text-command
      parser (`p(`/`gr(`/`fs(`/`c(`/`glass`/`w(`[wheels, via
      `wheels_make`]/`tracks`/`div(`/etc., ~330 lines) and (b) the
      `this.m.loadnew`-gated post-processing pass (~360 lines) that
      computes each face's bounding box, classifies which axis it's "thin"
      along, and flips `fs` (the decal/glass flip-side flag) by comparing
      against neighbouring faces that share an edge. (b) was initially
      scoped as a deferrable refinement, but real-file oracle testing
      against `mycars/Simple_Car.rad` (run with `m.loadnew = true`,
      matching `CarDefine.loadcar()`'s actual usage) showed every summary
      field already matched with (b) unported, but *every* plane's `fs`
      was wrong — the pass turned out to set `fs` for the majority of real
      faces, not an edge case — so it was ported in full rather than left
      out. Two multi-op-under-`fr()` misclassifications
      (`fr(fr(A*B)*C*D)` in the `p(`/`w(` handlers, wrongly chained as
      native `float` at first) were caught by re-deriving the
      parenthesization before ever compiling, not by a test failure.
      Host-tested (`native/tests/cont_o_test.c`) against the real
      `mycars/Simple_Car.rad` oracle — all 102 planes' `n`/`master`/`gr`/
      `fs`/`glass`/`c`/`road`/`light` plus every summary field
      (`npl`/`errd`/`maxR`/`grat`/`sprkat`/`wh`/`keyx`/`keyz`/`fcol`/
      `scol`/`colok`/`tnt`/`disline`/`shadow`/`decor`/`grounded`) matched
      the JS exactly. Clean under `-fsanitize=address,undefined`.
- [x] `web/ContO.js`'s `d()` runtime draw method (plus its `xs`/`ys`
      helpers) → `native/core/cont_o.c`. Re-scoped from M2 to M1 the same
      way `Wheels.js` was: `GameSparker.js`'s `#draw` (the real per-frame
      scene loop, not itself ported) calls `contO.d(g)` per object, so
      drawing even ONE static car for the first-screenshot milestone needs
      this, not just the geometry parser. `d()` also calls `lowshadow`/
      `electrify`/`fixit`/`pdust`/`dsprk`, all left genuinely unported
      (stubbed to `abort()` if reached) because every call site is gated on
      state (`co->fix`/`co->elec`/`co->stg[]`/`co->sprk_`, or the camera
      being >=2000 units away) that only a ported physics tick or a very
      distant camera can ever set — see `cont_o.h`'s doc comment on
      `cont_o_d` for the full argument. One real bug found this way: `dsprk`
      is called unconditionally every frame `co->shadow` is true (not
      gated at the call site like `pdust` is), so stubbing it to always
      abort was wrong; fixed by porting its real outer structure (the
      per-frame call is a no-op for zeroed `sprk_`/`rtg[]`) and only
      stubbing the inner spawn/trail bodies. Host-tested
      (`native/tests/cont_o_test.c`'s `test_d_ground_shadow`) against a
      real `web/ContO.js` run positioned with `Medium.follow()` (the real
      chase-camera setup) — instrumented both sides to record all 204
      `Plane.d`/`Plane.s` calls (index, colour, draw order, shadow mode)
      and diffed byte-for-byte, in both the default ground-shadow mode and
      `m.crs = true` mode; only summary counters are kept as permanent
      test literals since `plane_d`/`plane_s` themselves are already
      exhaustively covered by `plane_test.c`. Clean under
      `-fsanitize=address,undefined`.
- [x] `web/ContO.js`'s `#initCopy` constructor → `native/core/cont_o.c` —
      clones a base model (an `#initBuf`-parsed `ContO`, one of
      `loadbase()`'s 124) into a placed instance at a world position/
      rotation. This is how every stage-placed object actually enters the
      world — needed for the lean stage loader below. Along the way, found
      and fixed a real memory-layout problem: `ContO.p` had been a fixed
      `Plane p[CONT_O_MAX_PLANES]` (286-slot) array embedded in the
      struct, matching the JS's own `this.p = objArray(286)` — except JS
      arrays are sparse (286 `null`s is ~2KB), while a C struct array
      unconditionally allocates 286 full `Plane`s (~71KB) whether they're
      used or not. Fine for a handful of cars, not for the ~610 objects a
      stage places (~42MB). Changed `ContO.p` to a heap pointer,
      allocated to a `CONT_O_MAX_PLANES` scratch capacity during
      `#initBuf`'s parse (npl isn't known upfront) then `realloc()`-
      shrunk to exactly `npl` once it is; `#initCopy` allocates exactly
      `src->npl` slots directly, matching the JS's `objArray(contO.npl)`.
      Also retyped the shadow/dust/spark-trail placeholder fields
      (`sx`/`sy`/`sz`/.../`vrz`) from `void*` to their real types, since
      `#initCopy` (unlike `#initBuf`) actually allocates and writes them.
      Needed two new `Medium` pieces along the way: `medium_snapped`
      (the `(int)(v + v*(snap/100f))` colour-blend shape `#initCopy`'s
      `tracks(...)` colour transform shares with several stage-load
      setters) and `medium_setsnap` (trivial, but `#initCopy` reads
      `m->snap[]`). One real fr()-classification trap caught before
      compiling, not by a test failure: `t.xy`/`t.zy` and `t.x`/`t.z` LOOK
      like the same rotate-and-shift shape `medium_rot` already
      implements, but `t.x`/`t.z`'s JS is `trunc(this.x + fr(fr(A)-fr(B)))`
      — the integer centre is added BEFORE truncating, not after like
      `medium_rot`'s own `n + trunc(fr(...))` — a genuinely different
      computation (worked example in the code comment) that would have
      silently diverged for any rotation with a non-integer-truncating
      result. Host-tested (`native/tests/cont_o_test.c`'s `test_init_copy`)
      against a real `web/ContO.js` run cloning `road.rad` (pulled
      straight out of `data/models.zip`, a real track piece with
      `tracks(3` collision volumes so the tracker-copy path is actually
      exercised) at 4 different (x,y,z,angle) placements, including the
      `a===180` radx/radz special case and the `m.loadnew`-gated
      `grounded+=10000` branch — all matched exactly; one full scenario's
      per-plane data is kept as a permanent literal. Clean under
      `-fsanitize=address,undefined`.
- [x] `CarDefine.js` → `core/car_define.c`: `car_define_init()` (the 20
      built-in-car stat tables, literal transcription) plus
      `car_define_loadstat()`/`car_define_loadcar()` -- the `.rad` file
      `stat(`/`physics(`/`handling(` line parser (`cd_getvalue` +
      friends) and the full built-in-car interpolation ladder (`n12`/
      `n13`/`n14` for `swits`, `n16`/`n17`/`n18` for `acelf`, then every
      derived table: `airs`/`airc`/`powerloss`/`moment`/`maxmag`/
      `outdam`/`clrad`/`dammult`/`msquash`/`flipy`/`handb`/`turn`/`grip`/
      `bounce`/`lift`/`revlift`/`push`/`revpush`/`comprad`/`simag`/
      `cclass`/`dishandle`), transcribed verbatim with fr()-case comments
      per module. `loadcar()` adds the wheel-corner quadrant validation
      (`keyx`/`keyz` sign checks) then delegates to `loadstat`.
      Host-tested (`native/tests/car_define_test.c`) against a real
      `web/CarDefine.js` `loadcar('Simple Car', 16, text)` run (via
      `new ContO(bytes, m, t)` + `cd.loadcar(...)`, matching
      `CarDefine.js`'s own real call chain) on the actual
      `mycars/Simple_Car.rad` file -- every one of the ~25 derived stat
      fields for slot 16 matched exactly. Caught and fixed one real bug
      in the process: `cd_getvalue`'s comma-delimiter handling didn't
      match the JS's own `getvalue()` per-character order (it
      unconditionally skipped the character right after every delimiter
      instead of testing/appending it like the JS does), which silently
      dropped the first digit of every field after the first -- confirmed
      against a Node run of the real JS on `"stat(114,150,135,97,104)"`
      before fixing. Clean under `-fsanitize=address,undefined`. Wired
      into `main.c`: `mycars/Simple_Car.rad` now drives with its OWN
      interpolated stats (car slot 16), not built-in car slot 0's.
- [x] Stage loading: a LEAN `GameSparker.loadbase()`/`loadstage()` →
      `native/core/game_sparker.c/.h`, scope decision confirmed with the
      user rather than assumed (the real `loadstage()`, JS lines 93-333,
      pulls in `CheckPoints.js`/`XtGraphics.js`/`Control.js`/`Record.js`/
      `Medium`'s procedural generators/`ContO`'s `#initModel` -- most of
      M2's data model, for what a static scene doesn't need). `loadbase()`
      ported in full: unpacks `models.zip` into 124 base `ContO`s via
      `vfs_read_zip` + `cont_o_init_buf`, slotted by `CAR_NAMES`/
      `TRACK_NAMES` filename prefix, checked against the same 621172-byte
      total the JS itself validates against. `loadstage()` kept to
      `snap(`/`sky(`/`ground(`/`fog(` (→ `medium_set*`) and `set(`/`chk(`/
      `fix(`/`maxr`/`maxl`/`maxt`/`maxb` (→ `cont_o_init_copy` + `Trackers`
      writes, including the `fix(`-command's 5-placement cap via a plain
      local counter -- cheap to keep faithful even without `CheckPoints`).
      Explicitly skipped: `pile`, `nlaps`/`name`/`soundtrack`, `clouds(`/
      `texture(`/`polys(`/`density(`/`fadefrom(`/`lightson`/`mountains(`,
      the post-loop procedural-generator calls, and the CheckPoints/
      XtGraphics/Control/Record-dependent player-setup tail (see
      `game_sparker.h`'s header comment for the full list and why each is
      safe to skip for M1). Host-tested
      (`native/tests/game_sparker_test.c`) against a real
      `GameSparker.js#loadstage()` run (real `CheckPoints`/`XtGraphics`
      instances just so it doesn't throw -- their state isn't compared,
      this port doesn't have them) on a synthetic stage file built to
      exercise every kept command while avoiding `pile(` (which would
      shift every later object's array index between the two sides,
      since the JS still places piles via its own working `#initModel`
      and this port doesn't place them at all). Matched exactly on the
      first attempt: `loadbase`'s 621172-byte total, all 31 `Trackers`
      entries, `Medium`'s post-`snap`/`sky`/`ground`/`fog` colours, and
      all 16 placed objects' `baseIndex`/`npl`/`x`/`y`/`z`/`xz`/`elec`/
      `roted` (including `fix(`'s y/z field-swap). Clean under
      `-fsanitize=address,undefined`.
- [x] Rewrite `native/platform/linux/main.c`'s placeholder loop: loads
      `mycars/Simple_Car.rad` straight through `cont_o_init_buf` (skipping
      `CarDefine.js`, see above), loads a real stage
      (`data/models.zip` + `stages/1.txt`) via `game_sparker_loadbase`/
      `game_sparker_loadstage`, and drives the car in a straight line down
      the track every frame (no physics yet -- just enough motion to fly
      the chase camera past the stage's placed objects) via `medium_d` +
      `cont_o_d`, with a from-scratch port of `GameSparker.js`'s own
      `#draw` object-sort (painter's algorithm across many objects, not
      just one car -- see the "ORDERING" comment in `main.c`; this is glue
      code, not itself a `web/*.js` port target, but the algorithm IS the
      game's, reproduced faithfully). **M1 COMPLETE**: added a headless
      `NFM_SCREENSHOT_PPM=/path` env var (dumps the framebuffer via
      `glReadPixels` after N frames and exits, for verification without an
      interactive display) and actually looked at the output -- a real
      track (road surface, lane markings, "START" text, guardrails, a
      distant tree and mountain, sky gradient) renders correctly with the
      car parked at the start line. Verified under Xvfb.

      One real bug found this way, not by a test: the first stage-loaded
      run aborted immediately on `cont_o_d`'s `electrify` stub -- the
      `fix(` command sets `co->elec = true` on real stage objects (electric
      fences), which `cont_o_d` was NOT the "genuinely never happens for a
      fresh car" case `electrify` was scoped as. Ported `electrify` (the
      bolt-zigzag effect) and `ContO.rot()` (delegates to `medium_rot`,
      same as `Plane.rot()`) in full. Host-tested
      (`native/tests/cont_o_test.c`) against a real `web/ContO.js`
      `electrify()` run, 3 consecutive frames, both `roted=false` and
      `roted=true` -- every `fillPolygon`/`drawPolygon` call's colour and
      all 8 vertices matched exactly across all 3 frames, both scenarios
      (checked via a temporarily instrumented `cont_o.c`, same technique
      as `cont_o_d`'s own oracle test); the permanent test keeps the
      `elc[]`/`edl[]`/`edr[]`/`xy`/`zy` state after frame 3, which is what
      the per-bolt geometry is computed from. Caught and fixed one test-
      harness mistake before trusting it: the oracle script's first run
      reused one `Medium` (and its stateful `random()` stream) across both
      `roted` scenarios, making `roted=true`'s randoms depend on how many
      `roted=false` already consumed -- gave a misleading diff on `edl`/
      `edr` that had nothing to do with `roted` itself. Fixed by giving
      each scenario its own fresh seed/`Medium`, matching the C side's own
      one-process-per-scenario invocation. Clean under
      `-fsanitize=address,undefined`.

      Also: this is the first time `ContO.p`'s heap-pointer memory-layout
      fix (see the `#initCopy` entry above) has run at real scale -- 78
      placed objects + 124 base models loaded, drawn, and freed clean
      under ASan on every run, not just the earlier synthetic tests.

- [x] `web/Wheels.js` → `native/core/wheels.c/.h` — turned out to belong to
      the M1 track, not M2: `ContO.js`'s `.rad`-file constructor calls
      `Wheels.make()` directly to build each wheel's 19 `Plane`s (hub, 6 rim
      spokes, 12 tyre side-panels) while parsing a car, so it's needed to
      load a car at all, not just to drive one. `wheels_set_rims` and
      `wheels_make` are the whole file — nothing deferred. No
      `web/Wheels.test.js` exists; verified against a real `web/Wheels.js`
      run (one `setrims` + one `make` call, all 19 output `Plane`s' full
      `ox`/`oy`/`oz`/`master`/`gr`/`fs` checked, not sampled) — matched the
      JS exactly on the first attempt, unlike `Plane.js`'s chip-debris case.
      The file's own header comment already flags which literals (`8.66 *
      this.size` etc.) are double-precision with no `fr()`, which is exactly
      the classification this port's fr()-translation rules need — that
      made this file mechanical rather than another place to hunt for
      double-rounding bugs. Host-tested
      (`native/tests/wheels_test.c`), clean under
      `-fsanitize=address,undefined`.

### M2 — actually drivable (real physics tick + input)
- [x] `ContO.js` remainder → `core/cont_o.c` — **PARTIAL**: `dust`/`sprk`
      (seed-only halves) and their DRAWING halves `pdust`/`dsprk` are now
      all ported in full (see the "wire mad_drive() into the real game
      loop" work below for why `pdust`/`dsprk` turned out to be
      immediately reachable, not deferrable M3 material). `d`/`xs`/`ys`/
      `rot`/`electrify`/`#initBuf`/`#initCopy` were already done, see the
      M1 entries above. NOT ported: `lowshadow`'s real body (only
      reachable at camera distances >=2000 units, not exercised by the
      close-following camera implemented so far), `fixit`/`stepFix`
      (repair-pit animation, needs a car actually driving into a fix
      zone with `fixes != 0`), `py`/`getpy` (Mad.js has its own
      equivalents, used instead), and the `#initModel` constructor
      (procedural decoration generation -- the `pile` stage command,
      already dropped from game_sparker's lean `loadstage`, see
      game_sparker.h).
- [x] `CheckPoints.js` → `core/check_points.c/.h` — full field layout,
      `calprox`, `py`, and `checkstat` (per-tick race position/lap
      computation across every car, called once per tick from
      `GameSparker.js`'s own `simulate()`, same place `cont_o_step_fix()`
      is called from). Needed a real fix for a genuine header cycle
      first: `checkstat` takes `Mad**`/`ContO**`/`Record*`, but
      `check_points.h` can't `#include` `mad.h` (mad.h already
      `#include`s `check_points.h`, since `mad_drive`/`reseto`/`colide`
      all take a `CheckPoints*`) — fixed by giving `Mad`/`ContO`/`Record`
      real struct tags (they were anonymous `typedef struct { ... }`
      before, only reachable by their typedef name) so
      `check_points.h` can forward-declare `struct Mad`/`struct ContO`/
      `struct Record` cleanly; `check_points.c` itself `#include`s all
      three headers directly, where there's no cycle. Same bug CLASS as
      the earlier `car_define.h`/`cont_o.h` mismatch this session, fixed
      the general way this time rather than one-off. Also added
      `Record.closefinish` (a real field `checkstat` writes to on a
      photo finish; wasn't in the struct at all before -- `cotchinow`
      was already ported but stayed a no-op with this port's scope, see
      record.h). Host-tested (`native/tests/check_points_test.c`)
      against a real `web/CheckPoints.js` run: constructor defaults, a
      `calprox` scenario, a two-car ranking scenario (position by clear
      count), and a photo-finish/catch-up scenario (`catchfin`
      countdown), all exact. Wired into
      `native/platform/linux/main.c`'s tick loop right after
      `mad_drive()` (single car for now, `n=1`), matching
      `GameSparker.js`'s real ordering: `drive()` -> `checkstat()` ->
      `stepFix()`. Clean under `-fsanitize=address,undefined`.
- [x] `Control.js` → `core/control.c/.h` — **PARTIAL**: full field layout,
      `falseo`, `reset`, `py`/`pys`. NOT ported: `preform` (~1950 of the
      file's 2178 lines — the AI driver, computes left/right/up/down/handb
      from track geometry for AI-controlled cars). Checked `Mad.js`'s
      `drive()` first: it only ever READS `control.left/right/up/down/
      handb/steer/touchTrick*/wall/zyinv`, never calls `preform()` itself
      (that's `GameSparker.js`'s tick, for AI slots specifically) — so a
      human-driven car works fully without it. Host-tested
      (`native/tests/control_test.c`) against the real `web/Control.js`'s
      `reset()` across all 11 stage-number special cases (1 as a plain
      default, 16/17/18/20/21/22/24/25/26) × 3 values of `n` (33 total
      scenarios) with a synthetic `CheckPoints` (6 checkpoints, 3 fix
      points) — every `hold`/`revstart`/`statusque`/`fpnt[]` combination
      matched exactly on the first attempt, plus `falseo`'s three `n`-gated
      skip cases. Clean under `-fsanitize=address,undefined`.
- [x] `Record.js` → `core/record.c/.h` — **PARTIAL**: full field layout
      (minus the interpolated-draw history ring buffers), `recy`/`recx`/
      `recz` (live damage-dent recording, preserving the `nry`-not-`nrx`/
      `nrz` indexing quirk), `cotchinow` (gated correctly on `caught>=300`,
      but its body beyond the gate — a deep copy of ~300-tick replay
      history — is a no-op here since that history isn't ported; see
      record.h). NOT ported: `rec`/`play`/`playh` (interpolated-draw
      recording, only matters once drawing faster than the tick rate),
      `regy`/`regx`/`regz`/`chipx`/`chipz` (REPLAY-viewer damage decay —
      `Mad.js` has its own live copies of this logic, ported directly into
      `mad.c`).
- [x] `Mad.js` → `core/mad.c` (2343 lines) — **full field layout, `reseto`,
      `drive()`, `colide()`, and their `distruct`/`regy`/`regx`/`regz`/
      `rot`/`rpy`/`py` helpers all translated**, plus `cont_o_dust`/
      `cont_o_sprk` (the seed-only halves of `ContO.dust`/`sprk`, needed by
      `drive()`/`colide()` for particle-trigger state — drawing stays
      stubbed, see cont_o.h). `drive()`/`colide()` are only PARTIALLY
      oracle-verified: fourteen scenarios so far — free-fall onto flat
      trackless ground (coasting vs. full throttle, exercises the
      suspension-travel-limit ground-touch shortcut), free-fall onto a
      real flat-ground `Trackers` plane (with/without left-steering,
      exercises the actual `trackers.sect` collision loop, steering, and
      the skid/dust path), driving into a wall (`trackers.zy===-90`,
      30 ticks, exercises the crank/regz wall-bounce branch), two
      gear-cap scenarios on the same flat-ground rig run long enough to
      actually clear every `swits[cn]` threshold: forward top-gear cap
      (`n9===3`, tick 63) and reverse cap (`n8===2`, tick 32) — both
      previously-unexercised branches of the throttle/brake gear-curve
      lookup (the closed-form `fr(swits[cn][2]/2 + power*swits[cn][2]/196)`
      clamp, vs. the incremental `acelf[cn][n]` add every other scenario
      only ever hits) — one checkpoint-clearing scenario, a single
      REAL reachable `typ=1` checkpoint (every other scenario's
      checkpoint sits 100000+ units away, purely so the focus-search loop
      has something to iterate) that the car actually drives into and
      clears at tick 12, which (single-checkpoint track, so clearing it
      always also completes a lap) simultaneously exercises
      `mad->nlaps`/`m->checkpoint`/`m->lastcheck` updating on a real
      clear — a sloped-surface scenario (a single `Trackers` plane with
      `zy=30`, a genuine ramp rather than flat ground or a `±90` wall),
      exercising the `mad_rot`-based local-frame rotation branch
      (mad.c ~1365-1397) and its front/back-wheel contact split as the
      car climbs on — a multiple-trackers-in-one-sector scenario (a
      byte-for-byte duplicate ground tracker at the same position,
      testing the `array7[wheel]` "first tracker in sect[] order claims
      it" gate genuinely, unlike the wall scenario's ground+wall pair
      which satisfy DIFFERENT branches rather than competing for the
      same one) — and a car-vs-car `mad_colide()` scenario, the first
      exercise of that function at all: two `#initCopy` clones 150 units
      apart, one given nonzero forward speed/scz so it dominates and
      pushes the other, checked against the corner-pair push loop's
      exact scz/scx/scy deltas — every field now matches `web/Mad.js`
      exactly — a capsize/loop-trick scenario (holding the handbrake
      while airborne, then holding `down`, a genuine mid-air backflip)
      exercising the loop-trick `ucomp`/`dcomp`/`lcomp`/`rcomp`
      accumulation and the capsize DETECTION itself (mad.c ~641-665, the
      `zyinv`/`n3` wrap-and-compare against the previous tick's `pzy`) —
      and a damage/repair scenario exercising the `contO->fcnt===7||8`
      repair-completion branch — no remaining known gaps in what these
      fourteen scenarios cover. The damage/repair scenario surfaced a
      real, more serious gap than a missing test: `ContO.js`'s
      `stepFix()` (the repair-animation counter stepper -- SIMULATION
      despite driving a visual effect, per its own JS comment) had never
      been ported AT ALL, so `contO->fcnt` could never advance past 0
      and mad.c's `fcnt===7||8` repair branch was permanently dead code
      in every build up to now, not merely untested -- no damaged car
      could ever actually get repaired by driving through a fix zone.
      Ported as `cont_o_step_fix()` (native/core/cont_o.c/.h) and wired
      into `native/platform/linux/main.c`'s tick loop, called once per
      tick AFTER `mad_drive()`, matching `GameSparker.js`'s own
      `simulate()` ordering (drive() reads the PREVIOUS tick's fcnt;
      this tick's step advances it for the NEXT tick to read).
      The colide() scenario caught a real, previously-dormant bug:
      `xt_graphics_stub_human()` (native/core/xt_graphics.c) implemented
      `!isbot[n]`, but the real `XtGraphics.human(i)` is
      `humans ? humans.has(i) : i === this.im` — with no `humans` Set
      wired up in this single-player-only stub, "human" actually means
      "the locally-viewed car" (`n === xt.im`), nothing to do with
      `isbot`. Every scenario before colide_scenario() only ever drove
      ONE car at `im === xt.im`, where both formulas happen to agree, so
      this was invisible until a second car with a different `im`
      actually exercised it. Fixed to `n == xt->im`.
      The wall scenario caught a real bug: ticks 19-29 diverged by one
      `this.m.random()` draw per tick once sustained landing-wobble made
      `dust()` fire every tick — `cont_o_dust` was missing the public
      `dust()` wrapper's `setDrawPhase(true)` guard, so it silently ate a
      SIM-stream draw instead of a DRAW-stream one each time it fired.
      Fixed in `cont_o_dust` (native/core/cont_o.c); see mad.c's
      VERIFICATION STATUS comment. All scenario families drive an
      `#initCopy`-cloned instance rather than a raw `#initBuf` object,
      matching real gameplay — a real bug class was caught doing this:
      `#initBuf` never allocates the shadow/dust particle arrays even
      with `shadow=true`, so driving the raw base model crashes the
      instant a skid triggers a `dust()` call with nonzero lateral speed.
      No further known scenario gaps remain in `drive()`/`colide()` — see
      mad.c's own "VERIFICATION STATUS" comment above `mad_drive()`
      for the full list of what's covered. `preform()`-driven AI cars
      (Control.js, not ported) will exercise `drive()`'s AI-facing
      branches once that's done, but a human-driven car doesn't need it.
- [x] `mad_drive()` wired into `native/platform/linux/main.c`'s frame loop,
      replacing the M1 placeholder (`co.z += 40`, no physics). The
      player's car is now an `#initCopy` clone of the loaded base model
      (matching real gameplay, see mad.c's VERIFICATION STATUS comment),
      driven every frame against the real loaded stage's `Trackers`, with
      throttle hardcoded on (`control.up = true`) since real input
      mapping is still blocked on the control-scheme decision below.
      Verified via the headless screenshot hook across 2000+ frames,
      clean under ASan/UBSan: the car drives, steers under its own
      physics, hits the suspension/ground-contact/wall-collision paths,
      and comes to rest against track geometry without crashing.
      Surfaced (and fixed) two more "M1 never reaches this, M2 does"
      abort stubs in the process:
      - `ContO.pdust`/`dsprk` (the DRAWING halves of the dust/spark
        particles `cont_o_dust`/`cont_o_sprk` seed) → `core/cont_o.c`,
        now ported in full. `pdust` fires on nearly any tick a wheel's
        suspension travel differs from rest (not just skids), `dsprk` on
        any wall scrape — both hit within a few hundred ticks of real
        driving. See cont_o.h's top comment for the fr()/trunc()
        translation notes (several genuine case-2 spots, plus one exact
        native-float shortcut for chains only involving division by
        exactly 2.0/4.0).
      - Driving a raw `#initBuf` `ContO` (rather than an `#initCopy`
        clone) crashes on the first `dust()` call with nonzero lateral
        speed — `#initBuf` never allocates the shadow/dust particle
        arrays even with `shadow=true` (see ContO.js). Fixed by cloning
        via `cont_o_init_copy` before driving, matching how
        `GameSparker.js`'s own tick always drives a car.
- [x] `native/platform/linux/input_linux.c/.h`: SDL keyboard → `Control`,
      replacing the hardcoded `control.up = true`. Not a guessed binding
      scheme — the control-scheme "open question" resolved itself once
      the user asked for it directly: uses `web/main.js`'s own
      `installInput()` keymap verbatim (Arrow keys/WASD for up/down/
      left/right, Space for handbrake), polled once per frame via
      `SDL_GetKeyboardState`. Gamepad, analog touch-steer, and menu keys
      (enter/lookback) are not wired — out of scope for "is the physics
      port drivable".
- [x] Fixed a second sim/draw PRNG-stream bug found while re-testing after
      the `cont_o_dust` fix above: nothing wrapped `main.c`'s render
      section in `setDrawPhase`, so `cont_o_pdust`/`cont_o_dsprk`'s random
      draws (added when they were ported, see above) were ALSO landing on
      the sim stream instead of the draw stream. `GameSparker.js`'s own
      `draw()` wraps its entire `#draw` body (including every `ContO.d()`
      call) in `setDrawPhase(true)`/`(false)` for exactly this reason —
      main.c's frame loop now does the same around its `medium_d`/
      `cont_o_d` calls. No test regression (native/tests/ doesn't exercise
      rendering), verified by rerunning the wall-collision oracle scenario
      (which drives dust() every tick once grounded) after the fix and
      confirming main.c itself still runs 500+ frames clean under
      ASan/UBSan.
- [x] Fixed the chase camera never lagging behind the car: another real,
      user-reported bug ("in the original, moving the car shows a bit of
      its side; in ours it's always 100% aligned with the camera"). Root
      cause in `main.c`: `medium_follow()` (already correctly ported, see
      medium.c) was being called with `contO.xz` (the car's own
      INSTANTANEOUS heading) and a hardcoded `0` for its lookback
      parameter. The real call site (`GameSparker.js`'s race tick) passes
      `mad.cxz` — a heading `Mad.drive()` deliberately SMOOTHS toward the
      car's movement direction rather than snapping to it (see
      `mad_drive()`'s own `cxz` block, only adjusting past ~30 speed,
      easing by quarter-steps or fixed ±10 steps) — plus the real
      `control.lookback` value, not a hardcoded constant. Passing the
      raw, instantaneous heading made the camera re-center behind the car
      every single tick with zero lag, so a turn was invisible from the
      chase view. `main.c` now passes `mad.cxz`/`control.lookback`.
      Verified visually via the headless screenshot hook while forcing
      steering input: the car's side is now clearly visible mid-turn,
      closing back to dead-center once it straightens out.
- [x] Fixed-timestep physics loop in `main.c`, ports `web/main.js`'s own
      accumulator (see its "---- pacing ----" comment): `mad_drive()` now
      runs at a fixed 53ms/tick (~18.9 ticks/sec, `web/main.js`'s own
      `TICK_MS` default) via an accumulator clamped at 3 ticks/frame
      (matches its `MAX_CATCHUP`), decoupled from the render/vsync rate.
      Fixes a real, user-reported bug: the previous "once per rendered
      frame" placeholder ran the simulation at the display refresh rate
      (60Hz+) instead of 18.9Hz, ~3x too fast (every constant in
      `Mad.drive()` is tuned per-tick, so this wasn't cosmetic — the car
      visibly crossed the whole map too quickly). Rendering still happens
      once per real frame regardless of how many ticks it consumed (0 to
      3), i.e. tick-rate rendering — matches `web/main.js`'s own
      `?interp=0` mode, not its default blended-frame interpolation.
      Verified by temporary tick-vs-frame-count instrumentation (removed
      after confirming the ~53ms pacing), not by an oracle scenario (this
      is loop/timing plumbing in `main.c`, not a `web/*.js` port target).
      NOT done: draw-side interpolation between ticks for smoother motion
      at high refresh rates — real work belonging with `Record.js`'s
      `rec`/`play`/`playh` (deferred, see the `Record.js` entry above);
      tick-rate rendering already fixes the reported speed bug on its
      own.

### M3 — polish (not required to answer "does the port work")
- [x] ~~`CheckPoints.js` remainder (`checkstat`)~~ — done, see the M2 entry
      above (moved up once it turned out to be small and self-contained).
- [x] Text rendering: `core/vfont.c/.h` (NOT a port -- see its own top
      comment; `core/gfx.c` has no text/image support to translate from,
      only something to build). A small stroke/vector font (space,
      `0`-`9`, `A`-`Z`, basic punctuation) on a 5x7 grid, drawn through
      the EXISTING `gfx_draw_line` primitive rather than a new texture
      pipeline or an `SDL_ttf` dependency -- keeps parity with Vita's
      vitaGL backend, which submits the same kind of vertex list. Digits
      use a classic 7-segment layout; letters are hand-authored
      single/multi-stroke shapes in the same grid. `vfont_draw_string`/
      `vfont_text_width` are the only entry points; lowercase folds to
      uppercase, unsupported characters fall back to a blank cell rather
      than asserting. Host-tested (`native/tests/vfont_test.c`) against
      the module's own internal consistency (no oracle exists for this
      one -- see its test file's own top comment): known glyphs emit the
      expected vertex counts, lowercase folds correctly, unsupported
      chars and empty strings draw nothing, `vfont_text_width` matches
      `vfont_draw_string`'s real cursor advance. Caught one real bug in
      its own first draft before it ever shipped: `gfx.c`'s `segment()`
      silently drops zero-length lines (`len < 1e-6`), so the initial
      colon/exclamation-mark glyphs (encoded as a repeated identical
      point, meant as a "dot") rendered nothing -- fixed by encoding
      dots as short 1-unit segments instead, same trick the digit glyphs
      already used. Visually verified via the headless screenshot hook
      (a full alphabet/digit/punctuation string, legible at 2-3x scale).
- [x] Minimal HUD (speed/lap/position), in `native/platform/linux/main.c`.
      NOT a port of `graphics.js`'s remainder or `XtGraphics.js`'s real
      speedo/damage display (2578 lines of browser Canvas/DOM-specific
      HUD -- out of scope, same reasoning as the menu above) -- reads
      the same underlying simulation fields that display would
      (`mad.speed`, `checkPoints.clear`/`nlaps`/`nsp`/`pos`, all real
      state via `check_points_checkstat`, wired into the tick loop
      earlier this session), drawn with `core/vfont.c`. Visually
      verified via the headless screenshot hook, including a temporary
      forced-throttle check confirming the speed readout tracks
      `mad.speed` live (0 at rest, climbing as the car accelerates)
      before removing that check.
- [x] Menu: car select -> stage select -> race, in
      `native/platform/linux/main.c`. NOT a port of `Smenu.js` (written
      against the browser's own DOM/image-asset loading -- out of this
      port's scope, see PORT_SPEC.md §5) -- a native menu screen flow
      instead, same category as this whole file. Car select offers all
      16 built-in cars (`base_models[0..15]`, already loaded via
      `game_sparker_loadbase` for stage-piece decoration regardless, so
      there was no extra loading cost to exposing them) plus a 17th
      "Simple Car (custom)" entry using the existing `car_base`/
      `car_define_loadcar` flow from the M2 entry above; display names
      transcribed from `CarDefine.js`'s own `this.names` literal (menu-
      display only, not part of `core/car_define.c`'s ported field set).
      Stage select offers all 32 `stages/N.txt` files, reading each
      one's real `name(...)` line live via a small helper
      (`stage_read_name`) rather than needing the full
      `game_sparker_loadstage` geometry parse just to list names.
      LEFT/RIGHT cycles the current screen's selection (edge-detected
      against raw `SDL_GetKeyboardState`, not `Control` -- `Control`'s
      fields are a level-triggered continuous drive-input mapping, the
      wrong shape for discrete menu paging), ENTER/SPACE confirms. Car+
      stage loading, `ContO`/`CarDefine`/`Mad`/`CheckPoints` setup (the
      former M1/M2 hardcoded startup sequence) now happens exactly once,
      deferred until stage-select confirms, parameterised by whatever
      was picked instead of hardcoded car/stage 1. `NFM_SCREENSHOT_PPM`
      auto-skips the menu (car 0 / stage 1, the same defaults as
      before) since there's no way to simulate menu input under this
      sandbox's headless Xvfb (see the file's own comment on `xdotool`
      not working here) -- existing screenshot-based regression checks
      keep working unmodified. Verified three ways via the headless
      screenshot hook: both menu screens render correctly (car name /
      stage name + real live-parsed stage title), race mode still
      matches prior behaviour exactly with the defaults, and a
      temporary forced-selection test (car index 5 "MAX Revenge", stage
      7) proved the FULL transition/setup code path works end-to-end --
      a visibly different car model on a visibly different (desert-
      themed) stage rendered correctly, confirming the deferred
      loading, not just the menu screens' own rendering, actually works.
- [ ] Audio: `web/audio.js`, `web/music.js` → SDL_audio.
- [ ] netplay (`web/netpeer.js`, `web/netsync.js`): out of scope for a first
      playable build, separate research question from everything above.

### 1:1 original assets
User-requested follow-up: the menu/HUD above use a native placeholder
look (vfont text, flat-colour panels), not the real game's actual
GIF/PNG/JPG art (`data/images.zip`, 139 files, already in the repo and
unused until now) -- replace the placeholders with the real assets.
Requires building real image decoding + a texture pipeline from
scratch -- `core/gfx.c` has never drawn anything but flat-coloured
vector primitives, on either platform, and there's no browser API to
port this from (the browser reads these through its own built-in
`<img>`/OffscreenCanvas decoders). Confirmed while scoping this: the
HUD's actual NUMBERS (speed/lap count/etc) are drawn via
`XtGraphics.js`'s own `drawString` calls even in the original -- i.e.
system/canvas font text, not a bitmap digit font -- so `core/vfont.c`
staying in place for those specifically is not a fidelity regression;
only the panel/background GRAPHICS (damage, power, position, lap,
wasted, speedometer backdrop, rank badges, countdown) are real fixed
bitmap assets that need decoding.
  - [x] GIF decoder (`core/gif_decode.c/.h`) -- 122 of the 139 assets.
        NOT a port (see the file's own top comment) -- a from-scratch
        GIF87a/89a implementation (LZW decompression, global/local
        colour tables, single transparent colour index), scoped to
        what these specific files actually use: single-frame, standard
        variable-width LZW, sometimes interlaced. Host-tested
        (`native/tests/gif_decode_test.c`) against Python's PIL --an
        independent, trusted reference decoder standing in for the
        "oracle" role this session's JS-based tests use, since this
        isn't game logic but a well-defined external file format --
        sampled pixels (corners/center/off-center) across 11 real
        files covering every feature the format set actually needs:
        opaque, transparent, large (670x400, exercises LZW code-size
        growth to the 12-bit ceiling), tiny, and interlaced. ALSO
        verified via a full sweep (throwaway script, not checked in)
        decoding all 122 real GIFs in `data/images.zip` and comparing
        every pixel (MD5 of the full RGBA buffer) against PIL -- exact
        match on all 122. That sweep caught two real bugs the 9-file
        sample list alone had missed:
        - A KwKwK ("code used before it's defined") LZW edge case was
          implemented confusingly/wrong in the first draft (see the
          function's own history) -- rewritten cleanly around a single
          "walk the prefix chain into a scratch buffer, emit forward,
          then append the repeated byte if this was the KwKwK case"
          shape.
        - 6 files failed outright: 2 (`bob.gif`/`bot.gif`, 1x3 pixels)
          because their LZW stream omits the trailing end-code entirely
          once exactly `width*height` pixels have been produced --
          non-conformant strictly, but real encoders do this and
          reference decoders accept it, so "output buffer already full"
          is now an equally valid stop condition as "saw the end code".
          4 (`dome.gif`/`mycl.gif`/`myfr.gif`/`roomp.gif`) because they
          are genuinely interlaced -- an image-info dict check that
          looked authoritative (`PIL.Image.info['interlace']`) claimed
          NONE of the 122 real GIFs were interlaced, which turned out
          to be a dead end, not ground truth (reading the actual
          per-image packed byte directly off every file found these
          4). Implemented standard 4-pass interlaced row deinterleaving
          rather than skip them. Both bug classes now have permanent
          regression coverage in the test file. Clean under
          `-fsanitize=address,undefined`.
  - [x] PNG decoder (`core/png_decode.c/.h`) -- all 13 real PNG assets,
        including the countdown "dude" face (`d1.png`/`d2.png`/
        `d3.png`). All 13 turned out to be the simplest possible PNG
        variant (verified by inspecting every one of them): 8-bit
        depth, colour type 6 (truecolour+alpha, i.e. already-RGBA8
        pixels, no palette lookup or bit-unpacking needed),
        non-interlaced -- so the decoder is scoped to exactly that (one
        IHDR + one-or-more concatenated IDAT chunks, standard
        zlib-wrapped inflate reusing zlib -- already linked for
        `core/vfs.c`'s zip reader -- then per-scanline filter
        reconstruction: None/Sub/Up/Average/Paeth). Host-tested
        (`native/tests/png_decode_test.c`) against Python's PIL, same
        role/approach as the GIF decoder's test -- sampled pixels
        across all 13 real files, including partial-alpha gradients
        (the dude faces) and the largest asset (670x400, `br.png`).
        ALSO verified via a full sweep (throwaway script) decoding all
        13 real PNGs and comparing every pixel (MD5 of the full RGBA
        buffer) against PIL -- exact match on all 13, no bugs found
        (unlike the GIF decoder, which needed two real fixes after its
        own sweep). Clean under `-fsanitize=address,undefined`.
  - [x] Baseline JPEG decoder (`core/jpeg_decode.c/.h`) -- all 4 real
        JPG assets, all menu background photos (`bggo.jpg`/
        `bgmain.jpg`/`logomadbg.jpg`/`track.jpg`). Heaviest single
        piece of this whole effort (DCT, Huffman, chroma subsampling) --
        user chose to build it now rather than defer/substitute a flat
        colour. Implements ITU-T T.81's baseline sequential DCT process
        directly (Huffman table construction per spec Annex C,
        zigzag-order dequantization, a plain O(n^4)-per-block 2D IDCT --
        fine at asset-load time on 4 small images, not a hot path,
        chosen for correctness/clarity over a fast approximation),
        scoped to exactly what these 4 files use (verified by
        inspecting every one): SOF0 baseline, 8-bit, 3-component YCbCr,
        no restart markers, no progressive/arithmetic coding. 3 of the
        4 use 4:2:0 chroma subsampling; `logomadbg.jpg` uses 4:4:4 (no
        subsampling at all) -- both sampling-ratio paths are real,
        exercised, and tested, not just the common case. Host-tested
        (`native/tests/jpeg_decode_test.c`) against Python's PIL, same
        approach as the other two decoders' tests with one necessary
        difference: JPEG is LOSSY, so this decoder's own IDCT/rounding/
        chroma-upsampling choices don't have to bit-match libjpeg's
        (PIL's backend) to be correct, only close -- checks use a
        24/255 per-channel tolerance, well above the actual measured
        error (a full-image sweep against PIL, throwaway script, found
        mean absolute error under 0.4/255 and max under 16/255 across
        all 4 real files) so the test still catches a real decode
        break (wrong colours, garbage output, systematic shift) without
        being sensitive to ordinary inter-decoder rounding variance.
        Also visually verified (headless PPM dump, eyeballed against
        PIL's own decode of the same files) -- a sunset/storm-cloud sky
        and a dark night-sky/clouds background, both rendering
        correctly. Clean under `-fsanitize=address,undefined`.
  - [x] Textured-quad rendering in `core/gfx.c`/`core/gfx_gl.c` -- new
        capability, not a port (mirrors this section's own top-level
        framing). `gfx_draw_image()` (the `drawImage` equivalent) is
        the one new `gfx.c` entry point, and stays GL-free/host-testable
        like the rest of that file (see its own banner comment) by
        recording an OPAQUE `int32_t image_id` in a new interleaved
        draw-command list (`GfxDrawCmd`), rather than touching GL
        itself. Getting the interleaving right mattered: this renderer
        has no depth buffer at all (occlusion is submission order
        only, see gfx.h's own ordering banner), so a naive "batch all
        flat-coloured triangles, then draw all images" split would have
        silently broken draw order the instant an image needed to
        appear BETWEEN two pieces of scene geometry -- `gfx_draw_image`
        flushes whatever untextured vertices came before it into their
        own command, then appends its own, so `gfx_submit_gl` replays
        both kinds in exact submission order. The actual GL work
        (`glGenTextures`/`glTexImage2D` to upload, `glBindTexture` +
        a textured `GL_TRIANGLES` quad to draw, modulated by the
        composite alpha at call time -- `setComposite()` affects
        `drawImage` in the original too, e.g. the countdown "dude"
        face's fade-in) lives entirely in `gfx_gl.c`, matching that
        file's existing GL-only-code split from `gfx.c`. Host-tested
        (`native/tests/gfx_test.c`, no GL context needed) against the
        command-list bookkeeping itself (no oracle exists for genuinely
        new functionality): batch-then-image-then-batch produces the
        right 2 commands with the trailing batch correctly left
        unflushed for `gfx_submit_gl` to pick up, an image-only frame
        produces no spurious empty batch commands, and the composite
        alpha is captured at call time rather than read late. Visually
        verified via the headless screenshot hook: uploaded and drew
        the real `damage.gif` HUD panel (gif_decode.c's own output)
        directly over the running 3D scene -- the first real original
        game asset this port has ever actually put on screen, with
        correct transparency and draw ordering. Clean under
        `-fsanitize=address,undefined`.
  - [x] Port `web/images.js`'s `loadsnap()` (`core/hud_recolor.c/.h`) --
        the real per-pixel recolour step every one of these assets goes
        through before display: COLOURED pixels get tinted by
        `medium.snap[]` (the same per-stage palette shift that biases
        sky/track colours) and forced opaque; GREY pixels become BLACK
        with alpha derived from how far they sit below a reference
        white (how the original fakes antialiasing from a GIF with no
        alpha channel). One deliberate departure from the JS, not an
        oversight (see `hud_recolor.h`'s own long comment): the JS's
        own `cornerOpaque` special case works around a BROWSER CANVAS
        quirk (`getImageData` zeroes RGB under a GIF-transparent pixel,
        unlike Java's original `PixelGrabber`, which the JS's own
        comment says "hands back the palette RGB even for the
        transparent index") -- this port's `gif_decode.c`/
        `png_decode.c` already behave like `PixelGrabber`, never
        zeroing RGB just because alpha is 0, so the reference pixel's
        real value is always readable directly here, no fallback
        needed. Confirmed this is numerically equivalent to the JS on
        real data, not just architecturally cleaner, before relying on
        it: ran the FULL transcribed JS loadsnap loop (`cornerOpaque`
        branch included) against real `gif_decode.c` pixel dumps in
        Node and found `refR` resolves to 192 either way -- whether
        read directly or via the JS's hardcoded fallback -- on every
        real asset checked. Host-tested
        (`native/tests/hud_recolor_test.c`) against that same Node
        oracle on 3 real assets covering an opaque-corner image
        (`damage.gif`), a transparent-corner one (`8.gif`, a rank
        badge), and non-uniform-sign `snap[]` values (`position.gif`,
        negative R shift / positive G/B) to prove the tint formula's
        sign handling, not just one direction. Clean under
        `-fsanitize=address,undefined`.
  - [x] Wire the real panel images into `native/platform/linux/main.c`,
        replacing the vfont-only HUD placeholder, at `XtGraphics.js`'s
        own real screen positions (`dmg` 600,7 / `pwr` 600,27 / `lap`
        19,7 / `was` 92,7 / `pos` 42,27 / rank badge 110,28 / `sped`
        7,234), plus a full port of `drawstat()`'s two `fillPolygon`
        damage/power bars (JS lines 1812-1890, including the
        `dmcnt`/`dmflk` flicker-when-critical state machine and the
        per-channel `snap[]`-tinted bar colour formula) and the real
        speedometer (JS lines 2324-2339, km/h+mph computed from
        inter-frame car position delta via
        `sqrt(imul(dx,dx)+imul(dz,dz))`-based formula, `dy` genuinely
        computed-but-unused in the original -- preserved as a real
        quirk). `XtGraphicsStub` (`core/xt_graphics.h/.c`) gained the
        small bit of persistent state `drawstat`/the speedometer need
        across frames (`dmcnt`/`dmflk`/`auscnt`/`aflk`,
        `lcarx`/`lcary`/`lcarz`) -- `auscnt`/`aflk` default to the JS
        constructor's own idle values (45/false) and are never updated
        since the low-power warning flash that drives them lives in a
        large chunk of `XtGraphics.js`'s own tick this stub doesn't run
        (out of scope, same framing as the rest of that stub). Numbers
        (lap/wasted counts, speed) stay vfont-drawn text overlaid on
        the panel images, matching the original's own plain
        `drawString` calls at those same spots -- not a fidelity
        regression (see this section's own framing note above).
        Loading is deferred to the `STATE_RACING` transition, after
        `game_sparker_loadstage()` runs, since `medium.snap[]` only
        gets real per-stage values from the stage file's own `snap(...)`
        command. Visually verified via the headless screenshot hook:
        every panel's baked-in label art (`Lap:`, `Wasted:`,
        `Position:`, `Speed:`, `Damage`, `Power`) renders at the
        correct position with vfont numbers correctly overlaid
        (`1 / 1`, `0 / 0`, a `1st` rank badge, `0 KM/H` / `0 MPH` at
        race start), the damage bar empty and the power bar full
        (matching zero damage/full power at spawn), colours correctly
        `snap[]`-tinted. All 19 tests still pass; clean build.
  - [x] Real checkpoints/laps, the checkpoint arrow, and race messages --
        user follow-up to the HUD-assets work above: "lembre-se de estar
        de acordo com todos os gifs do hud durante o loop de gameplay. a
        seta que mostra o proximo checkpoint e as mensagens que o game
        informa durante a corrida" (the arrow pointing at the next
        checkpoint, and the messages the game shows during a race). Four
        parts:
        - **Real checkpoint/lap data** (`game_sparker_loadstage`, extended
          -- now takes a `CheckPoints*`): parses `chk(`/`fix(`/`nlaps(`
          stage-file lines into `cp->x/y/z/typ/n/nsp/pcs` and
          `cp->fx/fy/fz/roted/special/fn` (capped at 5, matching
          `checkPoints.nfix`) and `cp->nlaps` (clamped 1-15), matching
          `GameSparker.js` lines 195-243 -- replacing the placeholder
          single 100000-unit-away fake checkpoint M2 used to keep
          `mad_drive()`'s focus-search loop from spinning with zero real
          checkpoints. Also fixed the player's own start position/facing
          in `main.c` to `XtGraphics.js`'s real `xstart[0]`/`zstart[0]`
          (x=0, z=-760), not (0,0,0) -- found while wiring this up, since
          getting the FIRST checkpoint reachable at all depends on
          starting from the right spot. NOT ported: the `set(...)p`
          special-marker sub-case (typ 0/-1/-2/-3/-4 entries also written
          into `checkPoints.x/z/typ` by plain `set(` lines) -- confirmed
          harmless to skip, since every reader of `cp->typ` that matters
          here (`mad.c`'s own focus-search loop, `checkstat`'s tie-break
          search) already explicitly skips `typ<=0` entries. Host-tested
          (`native/tests/game_sparker_test.c`, extended with a `chk`/
          `fix`/`nlaps` line in its synthetic stage and matching
          `CheckPoints` assertions) and verified end-to-end against a
          REAL stage (`stages/1.txt`): a throwaway full-throttle-straight
          harness confirmed the car clears the real checkpoint at
          `chk(40,0,28000,0)` at the exact right position and `mad.focus`
          correctly advances to the second checkpoint afterward.
        - **Checkpoint arrow** (`draw_checkpoint_arrow` in `main.c`) --
          ports the `!arrace` branch of `XtGraphics.js`'s `arrow(n, n2,
          checkPoints, b)` (lines 1967-2159): a 7-vertex world-space
          polygon rotated toward `mad.point` via the ALREADY-PORTED
          `medium_rot`/`medium_xs`/`medium_ys` (same perspective the 3D
          scene itself uses -- XtGraphics.js's own `rot()`/`xs()`/`ys()`
          are the exact same formula, reused rather than reimplemented),
          filled+outlined with a `snap[]`-tinted flicker colour gated on
          `mad.missedcp`/`xt.cntan`. The `arrace` (radar car-lock) branch
          is genuinely out of scope: `resetstat()` defaults `arrace` to
          `false` and the only code that ever sets it `true` is gated on
          `nplayers !== 1`, permanently dead in this single-player-only
          port. Visually verified via the headless screenshot hook and by
          hand-computing the expected bearing (`k=129`) from real
          checkpoint/car coordinates and matching it exactly against the
          rendered `xt.ana`.
        - **Race messages** (`hud_say_draw`/`tick_missed_cp`/
          `hud_messages_tick` in `main.c`, new `XtGraphicsStub` fields
          `say`/`tcnt`/`wasay`/`tflk`/`cntovn`/`hud_clear`/`ana`/`flk`/
          `cntan`) -- ports `drawcs()`'s modes 0/2 (plain tint / tint+
          sky-blend; modes 1/3/4/5 are drop-shadow/menu/announcer-stunt
          variants this port's scoped message set never uses),
          `tickMissedCp()` (already simplified for single-player by the
          JS port itself, see its own comment), the generic `say`/`tcnt`
          display loop (lines 1505-1527), and 4 concrete triggers:
          "Checkpoint!" (`xt.hud_clear !== mad.clear`), "Car Fixed"
          (`mad.newcar`), "Checkpoint Missed!" (`mad.missedcp` in
          (15,50)), "Wrong Way!" (`xt.cntan>40` after increment, sustained
          off-bearing). `auscnt`/`exitm` gates simplify away to always-true
          (see `xt_graphics.h`). Deliberately out of scope: the
          announcer/stunt message subsystem ("Power Up X%", "Power To The
          MAX", "Power low, perform stunt!") -- a separate, larger feature
          (extra fields `asay`/`adj[]`/`exlm[]`/`skidup`/`crashup`, real
          per-stunt trigger logic) that reuses this same `say`/`tcnt`
          mechanism but wasn't part of what was asked for here. Visually
          verified via the headless screenshot hook for all 4 messages,
          including two temporary force-hooks (env-var gated, removed
          before commit) to reach "Checkpoint Missed!"/"Car Fixed" without
          a multi-thousand-tick drive session: "Checkpoint!" and "Wrong
          Way!" both occur naturally (the latter immediately, since the
          real stage 1 data happens to put the first focus target behind
          the start line). All 19 tests still pass; clean build, no
          warnings.
  - [x] Correction pass -- user follow-up: "vamos fazer uma etapa de
        correcao antes de continuar. Precisamos corrigir bugs que podem
        ter na fisica, e no loop do game. O numeros que representam a
        volta e wasted estao minusculos. precisamos utilizar todos os
        assests originais do games e deixar o game 1:1. Depois vamos
        criar os menus com os assests originais". Four parts:
        - **Physics/game-loop review**: full re-read of every gate/loop
          in the tick, focused on things the JS gates on that this
          port doesn't (`starcnt` intro countdown, `haltall` win/loss
          holds, `newedcar` underflow) -- ALL correctly matched already:
          starcnt intro (3-2-1-GO) and win-condition holds are
          deliberately unported (single-player never enters those
          branches without the announcer/finish overlay, which is out
          of this port's scope, same reasoning as XtGraphics.js's stub
          header); `newedcar` starts at 0 and only decrements when
          `!= 0`, safe; `checkPoints->nfix` is dead code in the JS too
          (never incremented anywhere in web/*.js after the constructor's
          `= 0`), matches. Only visible gap is the missing 3-2-1-GO
          intro and the missing "You Won" screen, both intentional
          scope-outs, not bugs -- documented here for future reference
          rather than fixed now.
        - **Bitmap font (`core/bitfont.c`/`.h`, generated via
          `scratchpad/bake_bitfont.py`)**: replaces the vfont-at-scale-1
          HUD numbers ("os numeros que representam a volta e wasted
          estao minusculos" -- 5x7 vector strokes were unreadably small
          vs the JS's own `12px Arial`). Bakes LiberationSans-Bold at
          14px (metric-compatible Arial-Bold substitute -- glyph shapes
          indistinguishable at this size) into a `uint8_t` bit array
          shipped INSIDE the binary as a C source file, plus a per-glyph
          `{ascii, advance, bit_offset}` table -- no runtime file
          dependency, no Vita asset packaging step. Covers digits +
          `/` + space + `:` + `.` + `-` + `%` (the exact set XtGraphics.js
          formats HUD numbers with), draws each set bit as a 1x1
          gfx_fill_rect quad -- no new source-rect draw API needed on
          top of what gfx.c already has. Wired into main.c's HUD block
          for the lap counter, wasted counter, and both speedometer
          numbers (km/h + mph); vfont still used for the labels
          themselves (`LAP:` / `SPEED:` etc) and for menu text, since
          it's a HUD-numbers-specific fix, not a wholesale font
          replacement. Visually verified: numbers went from
          nearly-invisible 5x7 strokes to legible bold Arial-Bold at
          the JS's own `~12px` visual weight.
        - **Menus with original assets**: replaces the previous flat-
          colour vfont-only car/stage-select screens with the real
          JS-shipped assets composited on top of `bggo.jpg` (the JS's
          800x450 game-over/menu backdrop photo). Draws `nfm.gif` +
          `madness.gif` as the top wordmark ("Need For Madness /
          MADNESS!"), `selectcar.gif` as the car-picker's real caption
          asset, and `back.gif`/`next.gif`/`play.gif` as real button
          affordances (bottom-left/right/center). Stage-picker uses
          vfont for "SELECT STAGE" -- no matching asset ships in the
          real pack for that specific caption (Smenu.js typesets it
          via Canvas). The dynamic label (currently-selected car/stage
          name) still uses vfont, matching the JS which itself uses
          Canvas drawString for those. Loaders now handle
          menu-appropriate GIF/JPEG paths (no hud_recolor tint pass,
          which is HUD-specific). `NFM_SCREENSHOT_MENU={car|stage}`
          added so the headless hook can dump the menu without needing
          keyboard input simulation. Not a full Smenu.js port -- Smenu
          is browser-DOM-driven and out of this port's scope (see M3
          entry above) -- but composites the SAME real assets onto a
          simpler keyboard-driven flow.
        - **Test suite**: all 19 tests still pass; clean build, zero
          warnings.
  - [x] Live 3D car/stage previews -- user follow-up: "Ainda nao esta
        certo. Nao renderiza os carros corretamente na hora da escolha e
        as pistas tambem nao". The menu screens above still only showed
        vfont text + a static backdrop photo for the actual car/stage
        being browsed -- the real game (both the Java applet's own in-
        game car-select screen, per `web/preview.js`'s own comments
        citing `xtGraphics.java:5058`'s camera constants, and the JS
        port's separate `preview.js`/launcher-page tool, which copies
        those exact constants) shows a REAL live-rendered 3D preview:
        a spinning car model, and an overhead render of the actual track
        geometry. `preview.js` isn't itself one of this port's normal
        web/*.js porting targets (it's launcher-page tooling, not part
        of the applet/game loop XtGraphics.js etc. cover) -- but it's the
        most faithful available reference for what those screens show,
        since it deliberately copies the real Java camera constants
        rather than inventing its own, so porting its approach is the
        right call, not a mismatched source.
        - **Car preview** (`draw_car_preview` in `main.c`): renders
          `base_models[car_index]` (or `car_base` for the custom-car
          slot) spinning in place through the SAME `cont_o_d()` the
          racing scene itself uses -- not a static image or a pre-
          rendered thumbnail. Camera constants copied exactly from
          `preview.js`'s own `drawCar()` (itself copied from
          `xtGraphics.java:5058`'s car-SELECT camera, distinct from the
          car-MAKER preview at `:6806` which looks level and shows the
          underside): `m.x=-400,y=-525,z=-50,zy=10,ground=495,
          focus_point=400,cx=400,cy=225,cz=50`, car parked at
          `(0,0,1000)`, `xz+=5`/`wzy-=10` per frame for the body/wheel
          spin. Shadow forced off for the draw (a floating preview car's
          shadow would land across the model, not beneath it) and
          restored after. Mutates the shared `base_models[car_index]`/
          `car_base` objects' position/rotation directly rather than a
          disposable copy -- confirmed safe: `cont_o_init_copy()` (used
          once the player actually starts a race) re-derives x/y/z/xz/
          xy/zy from its own explicit call parameters and never inherits
          them from the source object, so spin state left here by the
          preview can't leak into the actually-driven car. Visually
          verified via the headless screenshot hook at two different
          frames, confirming continuous rotation.
        - **Stage preview** (`draw_stage_preview`/`load_stage_objects` in
          `main.c`): an overhead render of the SAME real track geometry
          `game_sparker_loadstage` places for an actual race -- not a
          flat colour-coded block map. `load_stage_objects` (re)loads a
          stage's objects on demand, properly freeing whatever was there
          before (`cont_o_free` per old object, matching this port's
          existing leak-safety discipline), and is now shared by BOTH
          the stage-select screen's live preview (reloaded only when the
          browsed stage number actually changes) and the
          `STATE_RACING` transition (which still reloads once more for
          whichever stage was finally confirmed -- redundant in the
          common case but cheap, ~100-300 objects, and far simpler than
          threading an "already loaded" fast path through both call
          sites). Camera framing ported from `preview.js`'s own
          `drawStage3D()`: pointed straight down (`zy=90`), `trk=2`
          (keeps decoration visible and disables the racing camera's
          distance-fade/min-projected-size culls -- both gated on
          `m.trk !== 0` in `cont_o_d`, already correctly ported), depth
          computed from the stage's real bounds (`trackers.sx/sz/ncx/
          ncz`, already computed by `trackers_devidetrackers` inside
          `game_sparker_loadstage` -- not re-derived from object
          positions, which stray far-off decoration would skew) with a
          clearance term so the tallest geometry doesn't sit at or above
          the camera plane. No backdrop drawn (pointed straight down it
          would fill the frame with sky colour and wash the track out --
          achieved here simply by not calling `medium_d`, no stubbing
          trick needed since this port's draw call is explicit, unlike
          the JS's own shared `gs.draw()`). One deliberate simplification
          vs the JS: objects draw in load order rather than through the
          racing draw loop's own painter's-algorithm distance sort --
          that sort matters for a 3rd-person perspective view's near/far
          occlusion, much less so pointed straight down at a menu
          preview. Leak-safety verified with a standalone ASan/UBSan
          harness (not checked in) that ran `load_stage_objects` through
          6 reloads across 5 different real stages (1, 7, 9, 15, 3, then
          1 again, deliberately repeating one) -- clean, no leaks, no
          use-after-free. Visually verified via the headless screenshot
          hook across 4 real stages (1, 7, 9, 15) -- each renders its own
          actual track layout (a straight road, a hairpin switchback, a
          branching arcade-themed layout, a U-shaped underwater course),
          not a generic placeholder.
        - **Test suite**: all 19 tests still pass; clean build, zero
          warnings.
  - [x] Part 16: physics-triggered SFX -- `XtGraphics.js`'s `#crash`/
        `#skid`/`#scrape`/`#gscrape`/`#sparkeng`/`#playsounds`, previously
        no-op stubs, now real 1:1 ports (`native/core/xt_graphics.c/.h`).
        `crash`/`skid`/`scrape`/`gscrape` each have their own debounce
        counter and variant-rotation state (`bfcrash`/`crshturn`/
        `crashup`, `bfskid`/`skflg`/`dskflg`/`skidup`, `bfscrape`/
        `sturn0`/`sturn1`, `bfsc1`/`bfsc2`) and write which one-shot clip
        to play into a `pending_*` field rather than playing it directly
        -- kept separate from playback so `mad.c`'s multiple call sites
        (colide() and drive(), which can both fire in the same tick) never
        clobber each other, and so the actual `AudioLinux` handle stays
        out of `core/` (platform-agnostic, matching the rest of this
        layer). `#sparkeng`'s imperative "stop the old rev loop, start the
        new one" JS is ported as a plain desired-state assignment
        (`xt->pengs[j] = (n==j)`) -- `main.c` reconciles that against its
        own real channel indices, verified behaviourally identical since
        JS's `loop()` is itself idempotent and only the FINAL per-tick
        state is observable, except where the underlying asset changes
        (a `lcn` bank switch), which `main.c` detects explicitly and
        force-stops all engine channels for. `#playsounds`'s `n` (stage
        number) parameter is dropped -- verified unread in its JS body,
        same class of harmless quirk as `drawstat`'s unused `newcar`.
        `xt_graphics_stub_scrape` gained a `Medium*` parameter (needs
        `this.m.random()`, not `Math.random()`); its
        `this.m.random() > this.m.random()` is sequenced through named
        temporaries, same left-to-right pitfall as every other such
        pattern in this port. `Medium` and `Control`'s anonymous struct
        typedefs needed a real tag (`typedef struct Medium {...} Medium;`)
        to forward-declare, the same fix already applied to `Trackers`.
        `main.c` wires this in per tick, INSIDE the `starcnt==0` guard, at
        the START of which `pending_crash`/`pending_skid`/`pending_scrape`/
        `pending_gscrape` are reset before `mad_colide()`/`mad_drive()` run
        (tick-scoped, not frame-scoped -- the established pattern for any
        edge-triggered state in this port). Loads 49 additional WAV clips
        (5 engine bank x 5 revs, 6 air, crash/lowcrash/skid/dustskid/
        scrape x3, tires, wasted, firewasted) via a table-driven zip
        loader. Reconciliation against real playback channels (engine x5,
        air, wasted) happens once per tick by comparing `XtGraphicsStub`'s
        desired-state fields against tracked channel indices, starting/
        stopping only on mismatch; `stop_all_sfx_loops()` cuts all live
        loops at both `STATE_RACING -> STATE_POST_RACE` transitions
        (hold-card Continue, F10 manual abandon). The `mutem` mute-toggle
        sync is wired but dormant -- no key currently sets
        `control->mutem`, matching the existing `control->lookback`
        precedent of "wired, unreachable until a key exists".
        - **Test suite**: all 25 tests still pass; clean build, zero
          warnings. Verified via the headless screenshot hook (250 ticks,
          well past the countdown and into live racing with 7 cars) --
          no crash. `audio_linux_*` calls are safe no-ops when no ALSA
          device is available (this sandbox), confirmed by inspecting
          `audio_linux.c`'s `al->device == 0` guards rather than assuming.
  - [x] Part 17: wasted/elimination endings + hold-card `haltall` freeze.
        A dedicated investigation (see chat) found the simulation-level
        destruction/repair mechanic (`Mad.hitmag`/`cntdest`/`dest`,
        `ContO.fix`/`fcnt`, `CheckPoints.fx/fz/fy`/`dested`/`wasted`,
        stage-file `fix(` zones) was **already fully ported** in Parts
        14.3-16 -- `Record.wasted`/`whenwasted`/`hcaught`/`cotchinow` (the
        one piece that still looked unfinished) turned out to be a red
        herring: it's the wiring for `GameSparker.java:1340-1620`'s
        instant-replay orbit-camera cutscene, which `web/XtGraphics.js`
        never reads and `web/GameSparker.js` itself never ported -- not a
        prerequisite for elimination. The one real gap was narrower: the
        end-of-race hold-card only ever checked the normal finish-line
        condition, never the two elimination endings.
        Added, mirroring `xtGraphics.java:7683-7803`'s exact three-way
        priority order (`native/platform/linux/main.c`'s tick-loop
        end-of-race scan): **all-other-cars-wasted** (`cp.wasted ==
        BOTS_MAX_PLAYERS-1`) -> win, `youwastedem.gif` + "You Won, all
        cars have been wasted!"; **player-wasted** (`mad[0].dest &&
        xt.cntwis==8`, using the Part 16 `cntwis` ramp) -> lose,
        `yourwasted.gif`, no blink message (matches :7708-7722 having no
        `drawcs(120,...)` call, unlike its two siblings); then the
        existing finish-line scan. `youwastedem.gif`/`yourwasted.gif`
        aren't loaded anywhere in `web/XtGraphics.js` (its own "TODO not
        ported: asset not loaded" fallback at those `drawhi()` sites) --
        ported straight from `xtGraphics.java:129,134,809-816,9514,9517`
        instead, same recolor treatment as every other `loadsnap()`'d HUD
        glyph, confirmed present in `data/images.zip`.
        Also fixed two smaller fidelity gaps surfaced by reading
        `stat()`'s full source while placing the new branches: (1)
        `checkPoints.haltall` was never actually set true anywhere in
        this port, despite `mad_drive()` already correctly reading it
        (`mad.c:1098`, ported in Part 14.3) to coast every car to a stop
        once the race ends -- the existing win/lose comment even claimed
        "shown over the frozen-in-place racing scene", which wasn't true
        until now; wired `cp.haltall = true` into the all-wasted and
        finish-line branches (matching JS `:1077`,`:1189` -- the
        player-wasted branch deliberately does NOT set it, matching JS
        `:1081-1116` exactly, so other cars keep racing around the
        wrecked player). (2) the finish-line hold-card was missing the
        "Press [ Enter ] to continue" prompt every one of these branches
        draws in the source (`:7690`,`:7721`,`:7788`) -- added as a fixed
        black line at y=350, matching JS exactly (not a blink message).
        - **Test suite**: all 25 tests still pass; clean build, zero
          warnings. Verified via the headless screenshot hook at
          increasing tick counts (250 / 2500 / 6000 / 12000) with no
          player input (headless Xvfb has no real keyboard) -- no crash
          at any point; the 6000-tick screenshot visually confirms the
          HUD's "Wasted: 5/6" counter and a burning wrecked car on track,
          i.e. the destruction mechanic firing for real, one tick short
          of the new all-wasted win condition.
  - [x] Fix: `bots_sortcars()` crash on stages 28-32 (segfault in
        `cont_o_init_copy`, user-reported via a real interactive session --
        the first time this port's actual menu flow, not the
        `NFM_SCREENSHOT_PPM` hook, drove a race past stage 27). Root cause
        confirmed with `gdb`, then cross-checked against
        `web/XtGraphics.js:2367-2380`'s own `sortcars(n)`: that function
        clamps `n` to 27 both below (`n<0` -> Free Play -> 27, already
        ported) AND above (`n>27` -> 27) -- native `bots.c` only had the
        first clamp. The JS's own comment names this exact bug as a
        documented "PORT DIVERGENCE": stages 28-32 are multiplayer-only
        in the real Java (single-player stage select clamps at 27,
        `xtGraphics.java:1899,2597`), so the real game's `sortcars()`
        never sees `n>27` and never needed the second clamp -- but both
        the JS port and this native port let a solo player reach 28-32
        anyway, and without the clamp the car-index formulas run off the
        16-car roster (`n=30` forces opponent index 17), so
        `cont_o_init_copy` later reads a null/garbage `base_models[]`
        slot. Fixed by adding the same `if (n > 27) n = 27;` in
        `native/core/bots.c`, right after the existing `n<0` clamp.
        Added `NFM_CAR_INDEX`/`NFM_STAGE_NUM` env-var overrides to the
        screenshot hook (`main.c`) as a permanent diagnostic tool, since
        this bug was invisible to every prior automated test (which
        always used the hook's car-0/stage-1 defaults, never real stage-
        select navigation) -- swept all 32 stages x 3 cars (0/8/16) at
        150 ticks each after the fix, 0 failures, plus stages 28-32
        individually re-verified clean.
        - **Test suite**: all 25 tests still pass (added coverage
          implicitly via the sweep above, no dedicated bots_test case
          added since bots_test.c already exercises `bots_sortcars()`
          directly and this is a boundary-input fix, not new behavior).
  - [x] Part 18: extracted the shared game loop + wrote the full Vita
        platform backend. User request: "vamos converter todo main.c
        para o vita" (port all of main.c to Vita) -- clarified first that
        `native/core/` was already 100% platform-agnostic (nothing here
        touched it) and that the real work was giving Vita its own
        equivalent of `platform/linux/main.c`'s SDL/desktop-GL glue, not
        redoing physics/AI/etc.
        Investigated `platform/linux/main.c` first: its genuinely
        platform-specific surface (window/GL-context creation, the event
        pump, buffer swap, wall-clock ticks) turned out to be ~30 lines
        total, isolated at a handful of call sites, against ~3000 lines
        of menu/asset-loading/physics-loop code that never touched SDL
        directly. That ratio settled the architecture: extract the small
        real interface, share everything else, rather than duplicating
        the whole file into `platform/vita/main.c` and hand-translating
        it (which would leave two ~3000-line files to keep in sync by
        hand forever -- see this exact session's own stages-28-32 fix
        just above for what a missed-in-one-copy bug looks like).
        - **New shared layer** (`native/platform/common/`, built as its
          own `nfm_game` static library, linked against `nfm_core`):
          `game.c`/`game.h` -- the entire old `main()` body (menu state
          machine, asset loading, the fixed-timestep physics/race loop),
          renamed to `game_run()`, now called identically by both
          platforms' one-line `main()`. `platform.h` -- the six-function
          window/event/clock contract (`platform_init/shutdown/
          ticks_ms/delay_ms/swap_buffers/poll`) plus two path hooks
          (`platform_asset_prefix`/`platform_progress_path`, see below).
          `input.h` -- `input_poll(Control*)`, unchanged shape from the
          old `input_linux.h`. `buttons.h` -- a new `Button` enum
          (`BTN_UP/DOWN/LEFT/RIGHT/CONFIRM/CANCEL/ABANDON`) replacing the
          old code's direct `SDL_SCANCODE_*` use in menu-navigation edge
          detection (`KEY_EDGE()`); `CONFIRM`/`CANCEL` each collapse two
          desktop keys (Return+Space, Escape+Backspace) that were always
          checked together at every call site into one logical button.
        - **`platform/linux/`**: `audio_linux.h/.c` -> `audio.h/.c`
          (`AudioLinux` -> `Audio`, `audio_linux_*` -> `audio_*` --
          purely a rename, matching the shared contract's names).
          `input_linux.h/.c` -> `input.h/.c` (`input_linux_poll` ->
          `input_poll`; the header itself moved into common/, since its
          declaration doesn't vary by platform, only its .c does). New
          `platform.c` implementing the six-function contract via the
          exact SDL calls the old `main()` made inline. `main.c` shrank
          to a two-line `int main(void) { return game_run(); }`.
        - **`platform/vita/`** (written against documented vitaGL/
          sceCtrl/sceAudioOut/sceKernel APIs, NOT build-tested -- no
          VitaSDK in this environment, see this section's own note
          below): `input.c` -- sceCtrl D-pad -> `Control`, same shape as
          the Linux keymap. `audio.c`/`audio.h` -- the one genuinely hard
          part: `sceAudioOut` is a BLOCKING-output model (`sceAudioOut
          Output()` blocks until the previous grain finishes), unlike
          SDL's callback-driven one, so this backend runs its own
          dedicated `sceKernelCreateThread`'d output thread that renders
          into a grain-sized buffer and blocks on `sceAudioOutOutput()`
          for its own pacing, guarded by an `sceKernelCreateMutex` lock
          around every mixer/music mutation (mirrors
          `platform/linux/audio.c`'s `SDL_Lock/UnlockAudioDevice`
          discipline against a different primitive). Flagged inline: the
          exact sample rate `sceAudioOutOpenPort` will actually honor on
          real hardware is unverified (audio_mixer.c's own per-channel
          resampler makes this a quality question, not a correctness
          one, if the real port rate differs from the requested 44100).
          `platform.c` -- `vglInit`/`vglSwapBuffers` for the six-function
          contract, `platform_asset_prefix()` returns `"app0:"` (assets
          bundled into the `.vpk` itself), `platform_progress_path()`
          returns a fixed `ux0:data/NFMD00001/progress.bin` (`ux0:` is
          writable memory-card storage; `app0:` is the read-only app
          package the assets live in -- the two intentionally differ).
          `main.c` -- the same two-line `game_run()` hand-off, plus
          `sceUserMainThreadStackSize = 1MB` (up from newlib's 256KB
          default; game.c's menu/HUD helpers weren't sized against the
          old default, never measured on real hardware). `CMakeLists.txt`
          rewritten for the new 4-file backend + `nfm_game` link, and now
          globs `data/`, `stages/`, `music/`, and `mycars/Simple_Car.rad`
          into `vita_create_vpk`'s `FILE` list at configure time (previously
          undecided -- see the now-resolved "ship inside the .vpk or push to
          ux0: separately" question below; total real asset size is only
          ~5MB, comfortably small enough to just bundle).
        - **`core/progress.c` also generalized** (a real, if smaller, gap
          surfaced while doing the above): `game_progress_save_to_disk`/
          `load_from_disk` used to resolve `$XDG_DATA_HOME`/`$HOME`
          directly via `getenv()` -- POSIX/desktop-only logic baked into
          a file whose own top comment promises "fully platform-agnostic,
          host-testable". Both functions now take an explicit `path`
          argument instead; the caller (`game.c`) resolves it once via
          the new `platform_progress_path()` hook. `progress_test.c`
          updated to match (and got simpler: a scratch path built
          directly, no more `setenv(XDG_DATA_HOME)` indirection).
          `core/progress.c` itself never depends on `platform.h` --
          keeping that dependency out is exactly why the path became a
          parameter instead of progress.c growing a platform.h include,
          which would have broken `native/tests/`' own documented
          "no platform dependency" build (progress_test.c compiles
          progress.c directly, no platform backend linked in at all).
        - **Test suite**: all 25 tests still pass, clean build, zero
          warnings. Verified via the same 32-stage x 3-car headless
          sweep as the fix above (96 combinations, re-run after this
          refactor to catch any behavior change, not just a crash) --
          0 failures -- plus a dedicated `XDG_DATA_HOME`-redirected run
          confirming `platform_progress_path()`/`vfs_set_fpath()` wiring
          doesn't crash the real binary, not just the unit tests.
  - [x] Part 19: "motion blur" trail effect. User-reported: "o jogo
        original tem um certo motion blur... nosso port não tem e é bem
        mais nítido" (the original has a motion blur our port lacks and
        looks noticeably sharper). Confirmed real: `GameSparker.java`'s
        own `paint()` (decompilation/java-src/GameSparker.java:1798-1861)
        blits its offscreen-rendered frame onto the visible Canvas via
        `AlphaComposite` at `this.mvect/100` alpha and a small random
        jitter offset while `this.shaka` (screen-shake-from-crash) counts
        down -- onto a Canvas it deliberately does NOT clear between
        paints. `mvect` (default 100, i.e. fully opaque/no visible trail)
        drops toward 65 the faster the camera's heading is turning
        (`65 + abs(lastHeading - heading)/5*100`, clamped to 90) --
        letting the previous frame's pixels show through more with each
        additional degree of turn per tick is the entire "motion blur"
        the user was seeing, most visible mid-corner and right after an
        impact. **Not in web/*.js at all** -- grepped for "blur"/
        `mvect`/`AlphaComposite` equivalents across every `web/*.js`
        file, zero hits; same class of gap as the Part 17 investigation's
        instant-replay-camera finding (GameSparker.java has genuine Java-
        only features `web/*.js` itself never ported), so this was
        translated directly from the decompiled Java rather than an
        already-verified JS oracle.
        Implementation required one deliberate, documented exception to
        PORT_SPEC.md §5's "renderer only uses the shared GL1.1 subset"
        rule: framebuffer objects (`glGenFramebuffers`/
        `glFramebufferTexture2D`/etc., GL 3.0 core / `ARB_framebuffer_
        object`) are the only way to render a whole frame as ONE flat
        image first (matching Java's `offImage`) before compositing it
        over the previous frame at a single alpha -- doing the alpha
        reduction per-triangle instead (no intermediate target) would
        make each triangle blend against whatever partially-blended mess
        is already there from earlier in the SAME frame's own submission
        order, not a clean one-generation trail, which looked wrong on
        inspection before this design was chosen. `core/gfx_gl.c` gained
        `gfx_gl_render_target_init/free/bind` (an FBO + texture pair) and
        `gfx_gl_render_target_blit` (draws it back at an offset + alpha,
        deliberately NOT clearing the destination first -- see its own
        doc comment in `gfx_gl.h`); `platform/linux/gl_include.h` now
        also includes `<GL/glext.h>` with `GL_GLEXT_PROTOTYPES` defined,
        verified linking clean against this sandbox's Mesa/llvmpipe
        `libGL.so` directly (no `glXGetProcAddress` loading needed here --
        flagged inline as a per-driver assumption to double check on
        other desktop targets). Vita's `gl_include.h` already just
        `#include <vitaGL.h>` unconditionally, so whether these functions
        resolve there is unverified along with everything else in
        `platform/vita/`, no separate action taken.
        `game.c` gained `mvect`/`shaka`/`lmxz` state (GameSparker.java:
        80,152; `outshakedam` already existed, check_points.c's own
        checkstat already copies `mad->shakedam` into it every tick) --
        `mvect`/`lmxz` recomputed once per TICK in the exact spot the
        real Java's `medium.follow()`-equivalent call already lived
        (matching this port's established tick-vs-frame convention:
        camera-affecting state updates on ticks, gets consumed once per
        drawn frame); `shaka`'s jitter and countdown decrement happen
        once per FRAME, matching Java's own `paint()`. Outside
        `STATE_RACING`, both reset to Java's own "no effect" defaults
        (`mvect=100`, `shaka=0`) every frame -- collapsing the many
        individual `this.mvect=100` resets scattered across every
        non-gameplay Java `fase` this port doesn't model (chat, lobby,
        connecting, ...) into one blanket rule that matches all of them.
        **Caught and fixed one real bug before verifying further**: the
        first working build rendered the whole game upside down --
        rendering into a texture through the same y-down projection
        every other draw uses writes texture row 0 at the SCENE's bottom
        edge (OpenGL's rasterizer always fills framebuffer/texture row 0
        at NDC y=-1), the opposite of a normally-loaded image (whose row
        0 is already visually top by construction, from gif/png/
        jpeg_decode.c). Fixed by flipping the V texture coordinates in
        `gfx_gl_render_target_blit` specifically (not the general
        `gfx_draw_image` path, which samples normally-oriented images and
        stayed correct) -- confirmed via screenshot before and after.
        - **Test suite**: all 25 tests still pass, clean build, zero
          warnings (this feature has no `core/` unit-testable surface --
          it's pure rendering, verified visually instead). Re-ran the
          full 32-stage x 3-car headless sweep (96 combinations, 0
          failures) plus screenshots of all 6 menu screens (main,
          gamemode, car select, stage select, instructions, credits) to
          confirm the render-target path doesn't break anything outside
          racing either, since it's now unconditionally in the per-frame
          path for every screen, not gated to `STATE_RACING`.
  - [x] Part 20: fixed a real crash-on-load bug. User-reported: "o
        primeiro estágio esta crashando ao carregar (os outros estao
        normais)" (the first stage crashes on load, others are fine).
        The env-var screenshot hooks (which skip straight to
        `STATE_RACING`) never reproduced it; real menu navigation via a
        scripted X11/xdotool session did, but only intermittently --
        neither `gdb`, an ASan+UBSan build, nor `SDL_AUDIODRIVER=dummy`
        caught it, which pointed at something whose *inputs* vary between
        runs rather than a memory-corruption/race bug timing-sensitive to
        instrumentation. Root cause found via Valgrind (which doesn't
        perturb the game's own deterministic PRNG, unlike the above):
        `nfm_random()` uses a fixed seed (`nfm_set_seed(9001)`,
        `game.c:1312`), so its exact draw sequence at any call site
        depends only on how many prior calls happened -- which, for menu
        screens with animated per-frame jitter (car-select's spinning
        preview, etc.), depends on real elapsed dwell time before the
        player confirms. That's what made repro inconsistent: the exact
        `bots_sortcars()` roll it feeds varies with how long a human (or
        an xdotool script) lingers on each menu screen, not with stage
        number at all -- "stage 1" was a red herring, the actual trigger
        is **NFM1 mode** (any of its 10 stages), confirmed with a
        `fprintf` dump of `gmode`/`sc[]` right after `bots_sortcars()`
        plus a small standalone harness exercising `bots_sortcars()`
        directly across thousands of rolls.
        The real bug: `bots.c`'s `bots_sortcars()` already correctly
        implements `xtGraphics.java`'s NFM1 rule that a race only has 5
        cars, not 7 (`sc[5]`/`sc[6]` deliberately stay at their `-1`
        reset value for `GMODE_NFM1` -- see `n2`'s reduction in
        `bots_sortcars`), but `game.c`'s entire `STATE_RACING` setup and
        per-tick loop still hardcoded `BOTS_MAX_PLAYERS` (7) everywhere,
        so it read `base_models[sc[i]]` for `i=5,6` with `sc[i]=-1`,
        indexing 24 bytes before `base_models`'s `calloc`'d block and
        eventually segfaulting on a wild pointer read three field-copies
        later inside `cont_o_init_copy`. Root-caused precisely via
        Valgrind's memcheck against a `-g -O0` debug build: `Invalid read
        of size 8 at cont_o.c:81 (dst->m = src->m) ... Address is 24
        bytes before a block of size 192,448 alloc'd at game.c:1347`,
        which is exactly `sizeof(ContO)*(-1) + offsetof(ContO,m)`
        (confirmed via a tiny `offsetof`/`sizeof` probe: 1552 and 1528).
        Java's own `xtGraphics.java:2354-2358` (`loadstage()`) sets
        `xtGraphics.nplayers = 5` for NFM1 (moving car slot 4 to grid
        position `xstart[4]=0,zstart[4]=760`, the spot slot 6 normally
        occupies) and bounds EVERY per-race loop by `nplayers`, not a
        fixed 7 -- construction (`:2723`), collision/drive/checkstat
        (`:940-956`), the all-wasted and finish-line win checks
        (`:7683`,`:7737`), and the HUD's `wasted/(nplayers-1)` display
        (`:1397`). Added a `nplayers` variable to `game.c` (defaults to
        `BOTS_MAX_PLAYERS`, set to 5 + the grid-position override for
        `GMODE_NFM1` at the same point Java's `loadstage()` does, before
        its own stale-`sc[]` reset loop) and switched every one of those
        loop bounds from `BOTS_MAX_PLAYERS` to `nplayers`: the reset
        loop, the construction loop, `total_objs`/`all_objs` (the
        painter's-sort draw list -- co[5]/co[6] are uninitialized stack
        garbage for NFM1 and must never be drawn), `mad_ptrs`/`co_ptrs`,
        the collision/drive/checkstat/stepFix tick block, `control_
        preform()`, both win-condition checks, the HUD wasted-count
        string (this one was already half-fixed -- the code comment
        already said "Java: checkPoints.wasted / (nplayers-1)" but the
        actual expression still hardcoded `BOTS_MAX_PLAYERS-1`), and the
        final `cont_o_free()` cleanup loop.
        - **Verification**: rebuilt, then scripted the exact real menu
          sequence (Main -> Gamemode -> confirm on NFM1 -> Car Select ->
          confirm -> Stage Select stage 1 -> confirm -> loading ->
          racing) via Xvfb+xdotool 15 times in a row -- all 15 survived
          (previously this reproduced a crash), confirmed via screenshot
          that exactly 4 opponents render (5-car field) and the HUD's
          "Wasted: 0/4" is correct (was "0/6" before). Re-ran the same
          sequence for NFM2 and Free Play (3x each) to confirm the
          unaffected 7-car path still works. Full 25-test suite still
          passes, clean build, zero warnings.
  - [x] Part 21: car-select's car-switch transition. User-reported (after
        watching the original again): "os carros quando trocam de um pro
        outro acontece um efeito de blur ao redor do carro" (switching
        cars shows a blur-like effect around the car). Not a post-process
        blur filter -- `xtGraphics.java:6335-6425`'s `flipo`/`nextc` state
        machine: `carselect()` only runs its normal idle-spin-and-accept-
        input code inside `if (flipo == 0)`; a LEFT/RIGHT press instead
        sets `flipo = 20` and runs a 20-frame animation before accepting
        another press -- for `flipo` 20..11 the OLD car falls away
        tumbling (`y -= 100`, `zy += /-= 20` per frame depending on
        direction); at exactly `flipo == 10` the next/previous unlocked
        car index is picked (the same skip-locked-slot search) and placed
        1100 units above its resting height; for `flipo` 10..1 the NEW
        car rises back into place (`y += 100`/frame). The idle yaw spin
        (`xz`) and wheel spin (`wzy`) both freeze for the whole 20 frames
        -- they only advance in Java's own `flipo == 0` branch -- which is
        exactly what reads as a fast, tumbling "blur" at speed instead of
        an ordinary spin. This port previously swapped `car_index`
        instantly on the key press with no transition at all.
        Added `car_flipo`/`car_nextc`/`car_transition_y`/
        `car_transition_zy` state to `game.c`, gated LEFT/RIGHT to only
        arm a transition when `car_flipo == 0` (the actual index change
        now happens at the `flipo == 10` crossover, reusing the same
        skip-locked-slot loop the old instant-swap code used), and gave
        `draw_car_preview()` three new parameters (`y_offset`, `zy_value`,
        `freeze_spin`) so the transition can drive the car's height/tumble
        and suspend the idle spin without duplicating its camera/placement
        setup.
        While investigating this, also confirmed (re-reading
        `xtGraphics.java`'s own `stageselect()`) that the real applet's
        stage-select screen has NO live 3D track preview at all -- it's
        text + arrow navigation only, matching this port's own Part 6
        doc comment (`game.c`'s `draw_stage_preview` removal note) that a
        previous session already found and corrected the same thing. The
        user's recollection of a 3D stage preview is most likely a
        different tool (e.g. the stage-maker/editor) or the launcher
        page's `preview.js`, not the in-game applet -- flagged back to
        the user rather than re-adding it speculatively.
        - **Verification**: rebuilt clean, full 25-test suite still
          passes. Visually confirmed via the `NFM_SCREENSHOT_MENU=car`
          headless hook + a scripted RIGHT keypress, capturing 5 frames
          across the transition: the old car (Tornado Shark) sinks below
          the display "hole", the new car (Formula 7, name label already
          updated) rises into view tumbled/sideways partway through, then
          settles into its normal resting spin -- matching the Java
          source's fall-away/rise-in choreography.
  - [x] Part 22: shadow/ground colour bug. User-reported: "precisamos
        investigar a cor/intensidade das sombras". Root cause:
        `Medium.java`'s `setexture()`/`setpolys()` (lines 2021-2073) --
        the methods that recompute `cpol` (the ground polygon tint) and,
        from it, `crgrnd` (the shadow-blend tint every car/object's shadow
        is drawn with, see `cont_o.c`'s `plane_s()` calls) whenever a
        stage's `texture(...)`/`polys(...)` directive loads -- were never
        ported at all, and `game_sparker_loadstage()` never recognised
        either keyword in a stage file's line-by-line directive dispatch.
        31 of this game's 32 stages use `texture(r,g,b,strength)`, 1 uses
        `polys(r,g,b)` -- with both silently ignored, `m->texture` stayed
        at `medium_init`'s constructor default (`{0,0,0,50}`) for every
        single stage, so `cpol`/`crgrnd` (and therefore the ground's own
        tint and every shadow's colour) were always derived from the
        wrong texture value instead of each stage's real one.
        Added `medium_setexture()`/`medium_setpolys()` to `medium.c`
        (declared in `medium.h`), transcribed directly from
        `Medium.java:2021-2073` (same clamp-to-[20,60] on the texture
        strength, same `(ogrnd*n4+texcolor)/(1+n4)` blend before
        `medium_snapped()`, same final `crgrnd = trunc((cpol*0.99+cgrnd)/
        2.0)`), and wired `"polys"`/`"texture"` directive parsing into
        `game_sparker_loadstage()` alongside the existing `"ground"`/
        `"fog"` cases.
        - **Verification**: rebuilt clean, full 25-test suite still
          passes. Hand-computed `cpol`/`crgrnd` for several stages
          against the exact Java formula (stage 1's `texture(41,119,204,
          20)`, stage 21's `texture(0,0,0,60)`, stage 13's `texture(0,133,
          255,37)`, stage 8's `texture(204,0,255,20)`) and confirmed the
          port's new functions produce the identical values -- the actual
          per-stage shift versus the old always-default-texture behaviour
          is a modest few RGB units in most cases (the formula weights
          `ogrnd` far more heavily than the texture tint at these
          strength values), which is a property of the original formula
          itself, not a sign the fix is too small to matter: it's still
          the correct value instead of a constant wrong one on 32/32
          stages. Also captured a live racing screenshot (stage 1,
          `NFM_SCREENSHOT_FRAME=40`) confirming the fix renders without
          artifacts -- cars' shadows show the expected blue-grey tint
          matching stage 1's blue `texture(...)` directive.
  - [x] Part 23: two bot-AI navigation bugs. User-reported after playing:
        "eu joguei e eles [bots] parecem meio perdidos" (the bots seem
        kind of lost). Found via a full line-by-line audit of
        `control_preform()` against `Control.java:295-2350`:
        1. **`forget`-flag inversion** (`Control.java:1794`). `forget` is
           a *persistent* field that gates whether a bot keeps steering
           toward its nearest anti-stuck fix point across ticks, once it
           enters that recovery block. Java: `if (special) forget=true;
           else forget=false;` -- a special fix point should LATCH
           recovery steering, a normal one should CLEAR it. The port had
           `c->forget = !checkPoints->special[n26];`, which flips both
           outcomes: bots abandoned recovery early at special fix points
           and kept re-triggering it at normal ones -- directly matching
           a "wanders/seems lost after getting stuck" symptom.
        2. **Garbled stage 18/19/21 attack-range block**
           (`Control.java:786-807`). Real source is three separate `if`s
           (stage 18, then 19, then 21); the port had merged the stage-21
           `else`-branch onto stage 18's condition (so it never ran for
           stage 18 and ran on *every other stage instead of only 21*),
           dropped stage 19's block entirely, and set `c->afta = true`
           unconditionally for stage 18 instead of only inside stage 21's
           `bulistc` branch. Affects only the attack-target detection
           range/aggression trigger on those three stages.
        Also corrected a stale `control.h` doc comment that claimed
        `preform`/`control_preform` was "NOT ported" -- it is, and has
        been since Part 14.2.
        - **Verification**: rebuilt clean, full 25-test suite passes.
          Added a new regression test (`control_test.c`'s
          `run_forget_flag_scenario()`) that drives `control_preform()`
          into the exact fix-point-retarget block under a scenario
          engineered to isolate it (SECTION 1 skipped via `stcnt <=
          statusque` so it can't clobber the `trfix` this scenario sets
          directly; car placed far enough from the fix point that the
          block's own later "clear forget if within 2000" check can't
          mask the assignment being tested) and asserts `forget` latches
          `true` for a special fix point and clears to `false` for a
          normal one. Confirmed this test actually catches the bug by
          temporarily reintroducing the `!` inversion and re-running --
          both assertions failed as expected, then reverted. The stage
          18/19/21 fix was verified by direct line-by-line comparison
          against `Control.java:786-807` (exact match) rather than a
          dedicated scenario -- it only changes attack-range tuning on 3
          scripted stages, not general navigation, so it's lower-risk and
          harder to isolate behaviourally than the `forget` fix.
  - [x] Part 24: music playing too loud (actually clipping). User-reported:
        "ajustar volume da música (está muito alto)". Root cause found in
        `mod_play.c`'s `render_channel_span()`: the 8-bit-to-16-bit sample
        upscale was a flat `sample * 256`, applied per channel BEFORE
        summing all 4 channels into the output buffer (only clamped to
        int16 range at the very end). At full note volume that alone puts
        a SINGLE channel at +-32512 -- already pegged at the int16
        ceiling -- so any two or more of this game's 4 real MOD channels
        playing loudly at the same time (routine in real music) hard-
        clipped constantly. That reads as harsh, distorted, "too loud"
        audio, not just elevated volume.
        Compared against the real mixing formula, `ModSlayer.java:405`'s
        `vol_adj[chVol] * gain >> (vol_shift + 8)` (`vol_adj` is an
        identity table outside "loud" mode; `vol_shift` is 0/1/2 for
        <=4/<=8/>8-channel files -- `ModSlayer.java:626-635` -- always 0
        here since `MOD_NUM_CHANNELS` caps this port at 4, confirmed by
        checking several real `music/*.zip` files' MOD headers, all
        "M.K." 4-channel). `gain` is `RadicalMod`'s 4th constructor
        argument, hardcoded per stage in `xtGraphics.java:2984-3095`'s
        `loadstrack()` -- 125 for every stage except explicit overrides
        (1:135, 2/3/10/27:145) -- and was never ported at all; this port
        always mixed at raw full amplitude regardless of the real
        engine's own per-stage authored headroom.
        Added `gain` to `ModPlayState` (`mod_play.h`/`mod_play.c`,
        `mod_play_init()` gained a `gain` parameter), replaced the
        `sample * 256` upscale with `sample * vol_scale * gain / 4`
        (the exact derivation of Java's formula in terms of this port's
        own `vol_scale = chVol/64` convention, for this port's
        always-0 `vol_shift` -- see `render_channel_span()`'s own
        comment), threaded a `gain` parameter through both platforms'
        `audio_start_music()` (`platform/linux/audio.*`,
        `platform/vita/audio.*`), and added `game.c`'s
        `stage_music_gain()` transcribing the real per-stage table.
        - **Verification**: rebuilt clean, full 25-test suite passes.
          Added a regression test (`mod_play_test.c`) that builds a
          synthetic worst-case MOD (all 4 channels playing the same
          max-amplitude, max-volume sample in unison -- the loudest a
          real MOD can get) and asserts the mixed output stays well
          under the int16 ceiling at every real per-stage gain
          (125/130/.../145); confirmed it fails against the old `* 256`
          formula (peaks pegged at the ceiling) and passes against the
          fix. Also captured real stage-1 audio via SDL's `disk` driver
          for a genuine before/after: the OLD code hit exact int16
          clipping (524 samples pegged at +-32767/32768) within ~5s of
          real gameplay audio; the FIXED code peaks at ~31% of full
          scale (10085/32767) over the same span with zero clipped
          samples.
  - [x] Part 25: racing-VFX audit (task #61) found two real bugs in
        `cont_o.c`, one a crash:
        1. **`cont_o_fixit()` crash**. It was still a stubbed `abort()`
           call (see `cont_o.h`'s own notes on why `lowshadow`/
           `electrify`/`pdust`/`dsprk` were originally stubbed the same
           way, on the "unreachable for a fresh, undamaged car" theory).
           `mad.c` sets `co->fix = true` (matching Mad.java) whenever a
           damaged car nears a `fix(`-placed repair point -- an ordinary
           stage feature, not an edge case -- and `cont_o_d()` calls
           `cont_o_fixit()` every frame while it's true, so any damaged
           car that ever reached a repair pad crashed the whole process.
           Ported `ContO.java:1582-1766` in full: the Plane-retint flash
           (reuses `plane.c`'s already-ported `flx` flash state machine,
           just arms it) and the two overlapping 8-point "star" polygons
           drawn around the car. Preserved a real source quirk exactly
           (`this.z - this.m.y`, not `this.m.z`, at `ContO.java:1634`) --
           same fidelity-over-"fixing" stance as this file's other
           deliberately-preserved oddities.
        2. **`cont_o_pdust()` structurally wrong**. Comparing the
           previously-ported version against `ContO.java:1950-2105`
           directly (not just the `web/ContO.js` intermediate it was
           translated from) turned up a from-scratch rewrite's worth of
           divergence: a completely different puff geometry (uniform
           45-degree/0.7071 octagon vs Java's uneven 22.5-degree shape
           with 1.5x/1.7x-stretched vertices), no alpha blending at all
           (Java fades the puff via `AlphaComposite` as `stg[n]` climbs;
           the port drew fully opaque, never-fading blobs), wrong/
           incomplete `sbln[n]` base values (missing the 0.6 default and
           the `skd==0` case; `skd==1` was 0.25 instead of 0.4), an
           always-`{0,0,0}` tint-add array that should be a real
           snap-derived value, a camera-relative-instead-of-absolute
           coordinate bug that made the track-colour sector lookup
           almost never match, and a wrong `smag` growth rate/update
           order. Rewritten from `ContO.java` directly, including
           preserving two more real quirks: `scx[n] *= (int)n3` (zeroes
           a puff's drift velocity on ~90% of frames -- almost certainly
           an authoring slip in the original, kept anyway) and the
           visibility-cull threshold being 4 (half the 8 points), not 8
           like this file's other similar checks.
        - **Verification**: rebuilt clean, full 25-test suite passes.
          Added `test_fixit()` (drives `cont_o_fixit()` through its full
          `fcnt` 0-8 cycle against a real car -- the crash reproduction
          test: this call used to `abort()` unconditionally) and
          `test_pdust()` (a scenario with a deliberately large, nonzero
          camera position specifically designed to fail against EITHER
          the old camera-relative coordinate bug or the old wrong `sbln`
          constant) to `cont_o_test.c`. Confirmed both tests actually
          catch their bugs by temporarily reintroducing the old code and
          re-running -- `test_fixit` would abort, `test_pdust` fails 4
          assertions -- then reverted. Also ran a 1500-frame real-race
          smoke test on a stage with a repair pad (no crash, matching the
          unit test) and captured a live screenshot showing the dust puff
          now rendering as a translucent (not solid) cloud blended into
          the road, consistent with the alpha-blending fix.
  - [x] Part 26: menu-VFX audit (task #60) found the post-race stage/car
        unlock celebration overlay was entirely missing. Java's `finish()`
        (`xtGraphics.java:6692-6852`) draws this as a SEPARATE system on
        top of the ordinary win/lose banner (already ported) whenever the
        race just crossed the current campaign's unlock threshold: a
        golden aflk-flickered "Stage N is now unlocked!" line, and (on
        stages that also grant a bonus car) a bordered card with a
        50%-random translucent wash, a live spinning 3D render of the
        newly-unlocked car, a 50%-random reflection-line overlay (both
        re-rolled every single draw, which is what makes the card
        shimmer), the car's name, and a "GAME SAVED" line. Verified the
        underlying progression logic (`GameProgress::unlocked[]`/`scm[]`/
        `justwon1`/`justwon2`, `game_progress_finish_stage()`) was already
        correctly wired from an earlier part -- only this celebration
        VFX layered on top of it was missing.
        Implemented in `game.c`'s `STATE_POST_RACE` block, gated on
        `justwon1`/`justwon2` (progress.c's own faithful "did THIS race
        cross the threshold" flag, reused rather than re-deriving the
        threshold arithmetic here) rather than the stage==27 branch's own
        `n4`/`y` derivation being copied verbatim. Exported
        `game_progress_bonus_car_for()` (previously `static` inside
        progress.c) so this new code can re-derive the unlocked car index
        fresh every draw -- `GameProgress::scm[]` alone is NOT a safe
        substitute, since it only updates on stages that actually grant a
        bonus car and so holds a stale value from an earlier milestone on
        every other stage's finish screen. Added `post_race_unlock_car_y()`
        (the card's fixed 3D-render Y position, keyed by car index --
        verified both gamemodes' Java tables agree on Y for every car
        index that appears in both). Reused the shared-base-model-mutate-
        then-`cont_o_d()` pattern `draw_car_preview()` already established
        for car-select's own live 3D preview.
        **NOT ported**: the separate `stage==27` campaign-completion
        variant (`xtGraphics.java:6853-6913`) -- it slides in a
        "radicalplay" logo image, and this port's asset set has no such
        file at all (confirmed via a repo-wide search), so faithfully
        rendering it isn't possible without fabricating placeholder art.
        Flagging this as a genuine asset gap rather than a port bug.
        - **Verification**: rebuilt clean, full 25-test suite passes
          (added `test_bonus_car_for()` to `progress_test.c`, checking
          `game_progress_bonus_car_for()` against every real (gmode,
          stage) pair from `xtGraphics.java:6697-6776`). Visually
          confirmed via a temporary env-var-gated force-path (added,
          screenshotted, then fully reverted -- not kept in the
          committed code) forcing NFM1/stage-2's win-with-bonus-car
          scenario: captured a screenshot showing "STAGE 3 IS NOW
          UNLOCKED!", "AND:", a live spinning 3D render of the correct
          unlocked car (Max Revenge, car index 5), "MAX REVENGE HAS BEEN
          UNLOCKED!", and "GAME SAVED", all in the expected colour. The
          card's black border/wash aren't visible against this screen's
          existing solid-black background (a pre-existing, documented
          simplification -- Java overlays this atop the live race scene,
          which this port doesn't keep around after STATE_RACING ends),
          not a new bug.
  - [x] Part 27: car-select's locked-car gate overlay (task #60, the other
        finding). The port's LEFT/RIGHT nav SKIPPED locked cars entirely
        (a search loop that kept stepping until `game_progress_can_pick_car`
        passed) -- but the real Java (`xtGraphics.java:5306-5365`) lets you
        land on ANY car, and instead draws an animated 9-segment fence-gate
        arch sliding down over the car plus "[ Car Locked ]" text when the
        browsed slot is locked. The port's skip-locked-cars behaviour made
        this whole feature permanently unreachable, and `progress.h`'s own
        doc comment on `game_progress_can_pick_car` incorrectly asserted
        that skip was the intended translation (an earlier session's
        misreading, not a documented deferral).
        Fixed the nav loop to land on any of cars 0-15 (keeping the
        SEPARATE, correct skip for the custom "Simple_Car.rad" slot
        outside Free Play -- Java's own login-gated "Private Car"
        restriction, `xtGraphics.java:6349`, unrelated to this gate and
        out of scope for this single-player port). Added
        `game_progress_car_unlock_stage()` (exported, mirrors
        `game_progress_can_pick_car`'s thresholds but returns the literal
        "unlocks when stage K" value Java's message names -- preserved
        exactly that NFM2's K is the campaign-RELATIVE index (2,4,..16),
        not the real absolute stage number, matching a genuine quirk in
        the Java text itself). Ported the gate's exact wave-bounce math
        (`pgatx`/`pgaty`/`pgady[]`/`pgas[9]`, `gatey` 300->0 slide) and
        wired CONFIRM to block proceeding to stage-select while on a
        locked car -- the source doesn't spell out whether confirm itself
        is blocked (the gate is drawn as a passive overlay in `carselect()`
        rather than through an explicit confirm-time branch the way
        `cantgo()` guards stage-select), so this is a deliberate,
        conservative reading rather than a directly-cited line, called
        out here rather than presented as certain.
        Added a permanent `NFM_GAMEMODE` diagnostic env var (alongside the
        existing `NFM_CAR_INDEX`/`NFM_STAGE_NUM`) so this and future
        car/stage-gating scenarios can be screenshotted headlessly.
        - **Verification**: rebuilt clean, full 25-test suite passes
          (added `test_car_unlock_stage()` to `progress_test.c` against
          every real Java (gmode, car) pair, including the NFM2 relative-
          vs-absolute quirk). Screenshotted NFM1/car-5 (locked until
          stage 2) at frame 1 (gate visibly off-screen high, mid-slide-in)
          and frame 30 (settled, mid wave-bounce, "CAR LOCKED"/"THIS CAR
          UNLOCKS WHEN STAGE 2 IS COMPLETED..." both showing in the
          correct colours) -- and confirmed NFM1/car-0 (always unlocked)
          shows the normal preview with no gate at all, so the fix doesn't
          regress ordinary car browsing.
  - [x] Part 28: car-select's "smoke warp" entrance transition -- the one
        finding from task #60's menu-VFX audit flagged as high-cost/maybe-
        deliberately-deferred, implemented per the user's explicit call
        ("vamos ter que implementar, não tem jeito. Tem que ser 1:1 com o
        original"). Every time car-select is (re-)entered, Java
        (`xtGraphics.java:10146-10232` `drawSmokeCarsbg()`/
        `carsbginflex()`, dispatched from `carselect()` itself at
        `:5086-5102`) warps the `cars.gif` backdrop through a smoke-shaped
        mask (`smokey.gif`, one-time hue/saturation-tinted at load via
        `smokeypix()`, `:10018-10040`) before settling to the plain
        static image -- a per-pixel radial-displacement effect, not a
        simple fade/scale.
        Confirmed the required assets (`cars.gif` 670x400, `smokey.gif`
        466x202) both exist in `data/images.zip` at their real Java
        dimensions -- this is NOT a missing-asset case like the
        campaign-completion screen's logo. Confirmed via
        `GameSparker.java:299-325`'s `fase==-9` transition that
        `inishcarselect()` (which arms this) is the ONLY path that ever
        sets `fase=7`, including "back" from stage-select -- there's no
        separate `this.fase = 7` anywhere else in `xtGraphics.java` -- so
        the intro genuinely replays on every entry, not just the first.
        Implemented as a new `CarSmokeWarp` struct in `game.c`: raw
        `cars.gif`/`smokey.gif` pixels (kept as persistent RGBA8 buffers,
        unlike every other menu image which uploads-then-frees via
        `load_menu_gif`) plus the per-frame working buffer (`flexpix`
        equivalent) and its own dynamic GL texture, added
        `gfx_gl_update_texture()` (`glTexSubImage2D`, `gfx_gl.c`) so that
        texture updates in place every frame instead of leaking a fresh
        GL texture name per frame the way re-calling
        `gfx_gl_upload_texture()` would. Ported `drawSmokeCarsbg()`'s
        exact per-pixel blend formula (466x202 mask iterated against the
        670x400 backdrop, `pys()` radial distance, `flang`/`flatr`-driven
        alpha blend) and the three-way `flatrstart` dispatch (0-1 =
        animating, 2-5 = one white-flash transition frame, 6 = settled).
        Dropped only the `badmac` legacy-browser-compatibility fallback
        branch (irrelevant to a native port, consistent with every other
        browser-era compat hack already dropped from this project).
        - **Verification**: rebuilt clean, full 25-test suite passes (no
          new unit-testable logic here beyond what compiles -- this is a
          pure-rendering effect, verified visually instead). Screenshotted
          7 frames across a full car-select entry (1/5/15/25/33/34/40):
          frames 1-25 show a growing radial warp radiating from the
          smoke-mask's ring shape (the floor/background lineup visibly
          smearing outward, matching Java's description); frame 33 is
          the single white-flash transition frame (matches
          `xtGraphics.java:5097-5099`'s `fillRect` exactly); frame 40 is
          back to the plain, undistorted static backdrop. No crash, and
          a 40-frame full run completed in ~1 second wall-clock
          (including process startup/asset decode), so the added
          94132-pixel-per-frame blend loop (only active for ~33 frames
          per car-select entry, not continuously) has no meaningful
          performance cost even before considering the Vita's weaker CPU.
  - [x] Part 29: stage-select's live 3D track preview. User-reported after
        a first Linux build round: "vamos lá, não tem o render 3d
        mostrando um preview das pistas". An earlier session had already
        looked at this and wrongly concluded the Java has no such
        preview -- that investigation only read `xtGraphics.java`'s
        `stageselect()`/`cantgo()` in isolation (pure 2D/AWT UI methods,
        genuinely no 3D calls in either), and left behind a comment in
        `game.c` saying so ("No visual preview here since the Java
        original doesn't have one either"). The user's persistence was
        correct: the preview is real, it just lives in a DIFFERENT
        file/method than the one that was checked.
        Root cause of the miss: `GameSparker.java`'s `run()` dispatches
        fases via a chain of independent sequential `if`s, not a
        switch -- a fase can be set inside one `if` block and read by a
        LATER `if` in the SAME tick. `loadstage()` (called from the
        `fase==2` block, :2735-2749) arms `medium.trx`/`trz` from the
        stage's bounding box and sets `xtGraphics.fase = 1` at the end;
        the very next `if (xtGraphics.fase == 1)` block in the same
        `run()` tick (:453-497) then calls `medium.aroundtrack()` and
        depth-sort-draws the stage's `ContO`s every frame, BEFORE calling
        `xtGraphics.stageselect()` to draw the 2D frame/caption/name/
        arrows on top. A single-method read of `stageselect()` alone can
        never see this, since the 3D draw isn't inside it.
        Ported `Medium.aroundtrack()` (`Medium.java:304-380`) as
        `medium_aroundtrack()` in `core/medium.c`/`.h` -- a two-phase
        camera: `hit > 5000` scripts a fixed dive from `hit=45000` down
        to cruise altitude while orbiting the track's bounding-box center
        at a constant 17000 radius (`vxz` advancing 3deg/frame); once
        settled (`hit <= 5000`) it pans the look-at point checkpoint-to-
        checkpoint every 7 frames (`ptr`/`ptcnt`, `nrnd` counting full
        sweeps) with a slowly breathing depth-of-fog focus (`fo`/`gofo`).
        Cross-checked the transcription against `web/Medium.js:286-342`
        (a full line-by-line JS transpile of the same Java method, not
        the lean subset `web/GameSparker.js` ports) rather than trusting
        my own reading alone -- caught that Java has two literal extra
        lines (`:373-374`, an unused local distance computation and an
        empty `if` body) that `web/Medium.js` already omits as dead
        decompiler artifacts with no observable effect; followed that
        precedent rather than re-including them.
        Exposed the stage's bounding-box center (Java's
        `medium.trx = (getint2+getint)/2` / `trz = (getint3+getint4)/2`,
        `GameSparker.java:2736-2737`) via two new nullable out-params on
        `game_sparker_loadstage()` -- the one value needed to arm the
        camera that isn't otherwise recoverable from `Trackers` after the
        fact (`devidetrackers`'s integer division loses precision).
        Wired into `game.c`'s existing `STATE_STAGE_SELECT` draw block,
        which already had `load_stage_objects()` silently pre-parsing
        every browsed stage's geometry (originally for the instant-
        transition-to-racing case) but no render attached to it: on a
        fresh stage load, arms the camera exactly like the Java's
        `fase==2` block (`trx`/`trz` from the new out-params, `hit=45000`,
        `ptcnt=-10`, viewport `iw/ih/w/h` = 65/25/735/425); every frame,
        ticks `medium_aroundtrack()` and depth-sort-renders
        `stage_objects[]` with the SAME painter's-algorithm helper the
        racing loop uses (`render_sorted_objects()`, factored out so both
        call sites share it). Reordered the screen's own draw sequence so
        the 3D scene renders BETWEEN the beige/bggo backdrop and the
        `br.png` torn-paper frame -- matching the Java's own compositing
        order (scene first, frame+caption+name+arrows on top) -- rather
        than after the frame, which would have let the preview's full-
        viewport sky/ground bands paint over the frame's cropping edges
        and the SELECT caption.
        - **Verification**: added `test_loadstage_center()`
          (`game_sparker_test.c`, asymmetric bounds so a mixed-up
          `ge1..ge4` or swapped x/z would fail it) and
          `test_aroundtrack_dive_and_settle()` (`medium_test.c`,
          hand-traced first-frame values against the Java's exact
          `hit==45000` arming, then runs the dive to settling and checks
          `ptr`/`nrnd`/`cpflik` cycle correctly once orbiting) -- full
          25-test suite passes. Visually verified with Xvfb: screenshotted
          `STATE_STAGE_SELECT` at frames 1/30/90/150/300 of a fresh entry
          -- frame 1 shows a near-vertical bird's-eye view of the full
          oval track from high altitude, frames 30-90 show the dive
          passing the boundary walls and swooping toward track level,
          frames 150-300 show a low, panning tracking shot along the
          circuit (a checkpoint gate and decoration geometry visible) --
          with the "SELECT STAGE" caption, stage name, back/next arrows
          and continue button legibly composited on top throughout, no
          frame where the 3D scene obscures the UI. Corrected the
          disproven "no visual preview" comment and the screen's own
          numbered layout list to document the real mechanism with exact
          `GameSparker.java`/`Medium.java` line citations.
  - [x] Part 30: pre-race starting-grid camera preview. User-reported:
        "também falta um preview dos carros quando o jogo inicia, antes de
        começar a contagem" -- a second real missing 3D-camera feature
        found the same week as Part 29's stage preview, and by the same
        pattern: a cinematic camera pass that lives in `GameSparker.java`'s
        `run()` fase dispatch (this time inside the `fase==0` race tick
        itself, `:958-1017`) rather than anywhere `xtGraphics.stat()`/HUD
        code would suggest looking.
        The real Java: at the exact tick `xtGraphics.starcnt` is (re)set
        to 130 (race start), it arms `medium.adv=1900`/`zy=40`/`vxz=70`
        and flashes the screen white for one tick (painted OVER that
        tick's already-fully-rendered 3D scene, same "flash transition"
        idiom Part 28's smoke-warp uses). Every tick while
        `starcnt >= 38` (from 130 down through the whole pre-countdown
        window -- about 4.9s at the game's 18.9 ticks/sec, unless
        skipped), the camera runs `medium.around(car, true)` -- a fast
        dolly-in-and-orbit -- instead of the normal chase cam, giving a
        clear view of the full starting grid (all cars, not just the
        player's) before the 3-2-1-GO countdown text even appears.
        ENTER/handbrake skips straight to `starcnt=38`, the one-time
        transition point that resets `vert`/`adv`/`vxz`, calls
        `checkPoints.checkstat()` once, and calls `medium.follow()`
        (which overrides `around()`'s camera position set moments earlier
        in the very same tick) -- from the next tick on, `starcnt<38`
        permanently and the ordinary chase cam takes over.
        Ported `Medium.around()` (`Medium.java:382-431`) as
        `medium_around()` in `core/medium.c`/`.h` -- confirmed identical
        to `web/Medium.js:344-373`'s already-vetted transpile, so no
        dead-code surprises this time (unlike Part 29's `aroundtrack()`).
        Needed a `struct ContO;` forward-declaration in `medium.h` (same
        pattern as its existing `struct Graphics2D;` one) since `cont_o.h`
        itself includes `medium.h` and so can't be included back; the
        function itself lives in `medium.c`, which CAN include `cont_o.h`
        directly (only headers have the cycle, not translation units).
        Wired into `game.c`'s `STATE_RACING` setup (arms the camera the
        moment `starcnt` is reset to 130) and its existing once-per-frame
        `medium_follow()` call site, which now dispatches between
        `medium_around()` (while `starcnt>=38`, including the skip-input
        check and the `starcnt==38` transition reset) and the original
        `medium_follow()` path (`starcnt<38`) -- plus the one-tick white
        flash, drawn right after the normal 3D object-draw loop and before
        the HUD, matching the Java's own draw order exactly.
        - **Verification**: added `test_around_fast_intro()`
          (`medium_test.c`) -- hand-traces the `b==true` intro mode's
          first-frame bookkeeping (`adv`/`n`/`y`/`vxz`/`xz`) exactly, and
          cross-checks the trig-dependent `x`/`z`/`zy` fields by calling
          the SAME `medium_cos`/`medium_sin`/`sqrt`/`atan` formula
          independently (authored fresh from the Java, not copied from
          `medium.c`, so it still catches integration bugs); separately
          runs the ordinary `b==false` mode for 2000 frames and checks
          `adv` actually oscillates across its full `[-500,900]` band
          (initially failed at `[-500,900]` exactly -- the real bound is
          `[-502,902]`, since the flip check runs AFTER the `+=2`/`-=2`
          step and can overshoot by one step first; fixed the test, not
          the code, after confirming that's exactly what the Java does
          too). Full 25-test suite passes. Visually verified with Xvfb:
          screenshotted `STATE_RACING` from a fresh race start at frames
          1/60/200/260/300/400 -- frame 1 is solid white (the transition
          flash) with the HUD still visible on top; frame 60 shows a
          wide, angled dolly shot of all four cars sitting on the
          starting grid (the actual "preview of the cars" the user
          described); frames 200/260 show the ordinary chase cam already
          active with the "3"/arrow countdown glyphs overlaid, confirming
          the starcnt==38 handoff completed smoothly before this point;
          frames 300/400 show normal racing well underway (GO, then live
          position/speed/damage HUD), confirming the feature hands off
          cleanly into ordinary gameplay afterward.
  - [x] Part 31: menu motion-blur/ghosting missing on car-select and
        stage-select. User-reported: "os menus de seleção também possuem
        um certo blur nos shaders como no modo corrida, a renderização
        dos menus está muito nítida". Correct, and traceable to a real
        gap: `mvect` (the render-target blit's alpha, already ported and
        working for STATE_RACING -- see Part 30 and the composite/blit
        site's own doc comment) was blanket-reset to 100 (fully opaque,
        zero ghosting) for EVERY non-racing state. That blanket reset was
        itself built on a wrong assumption -- it cited "the many OTHER
        `this.mvect = 100` resets scattered across every non-gameplay
        Java fase" as if ALL of them used 100, but `GameSparker.java`'s
        actual fase dispatch (:80-734, the same run() loop Parts 29/30
        already mined for the 3D previews) sets `mvect = 50` on entering
        car-select (:323, armed once at the "-9 -> 7" transition,
        fase==7's own block never resets it) and holds stage-select at
        `mvect = 20` while the dive-and-orbit camera is still diving
        (:452, re-armed every `loadstage()` call, i.e. every stage the
        player browses to) then lets it creep up to a `mvect = 40` cap
        once the camera settles (:466-467, `if (medium.hit==5000 &&
        mvect<40) ++mvect`) -- never back to 100. Only main menu (:341),
        the gamemode submenu (:352), and post-race (:400) actually reset
        to "no effect."
        Wired `mvect = 50` into `car_select_needs_intro`'s existing
        arming block (the same "-9"-equivalent trigger moment the smoke-
        warp intro already uses) and `mvect = 20` into stage-select's
        existing `stage_preview_loaded_num != stage_num` reload gate
        (Part 29's own per-highlight-change trigger, which fires exactly
        once per `loadstage()` call, matching the Java 1:1), plus the
        `hit==5000 && mvect<40` creep in the same per-frame spot
        `medium_aroundtrack()` is already called. Narrowed the blit-time
        blanket reset to exclude `STATE_CAR_SELECT`/`STATE_STAGE_SELECT`
        (which now self-manage `mvect`) and `STATE_STAGE_LOCKED`
        (cantgo -- Java's fase==4 has no mvect assignment either, so it
        should inherit stage-select's value, the only state that reaches
        it, rather than snap to 100).
        - **Verification**: pure-rendering change, no new unit-testable
          logic (same category as Part 28's smoke-warp) -- verified
          visually instead, and had to dig further than a screenshot diff
          to actually confirm it. A naive two-consecutive-frame pixel
          diff at car-select's real `mvect=50` looked deceptively like a
          plain hard edge (a single-step 50% blend between two similar
          5-degree-apart rotation frames is subtle), which briefly looked
          like the blit's alpha blending might not be taking effect at
          all. Ruled that out properly: temporarily forced the blit alpha
          to extreme test values via an env-var hook (never committed) --
          alpha=0.0 produced a frozen solid-black capture (the blit
          contributing literally nothing, exactly as expected, since
          nothing else ever draws to that framebuffer), alpha=0.1
          produced an obviously heavy multi-exposure ghosting smear
          across the spinning car -- both confirm the existing blit
          mechanism genuinely accumulates across frames and responds
          correctly to alpha; `mvect=50`/`20-40` are just proportionally
          subtler, matching the Java's own restrained values rather than
          an exaggerated effect. Re-verified with the real values
          afterward: a frame-300 zoomed crop of the spinning car shows
          soft double-edges along the roofline/hood/rear consistent with
          a genuine 50%-blend trail, and a frame-280 stage-select capture
          shows a visibly hazier, softer scene than Part 29's original
          (unblurred) screenshots. Full 25-test suite still passes
          (game.c has no dedicated test binary; this touches nothing
          core/ tests exercise).
  - [x] Part 32: black borders around the 3D scene during racing.
        User-reported with a screenshot right after the Part 29-31 build:
        "parece que retrocedemos no render na hora da corrida, a pista ta
        sendo renderizada com algumas bordas pretas na tela". A real
        regression from Part 29, and a self-inflicted one: `medium.iw`/
        `ih`/`h`/`w` bound the world backdrop drawn in `medium_d()`
        (sky/ground gradient bands, `medium.c`'s own `arr_x[0]=m->iw`
        etc.) to a sub-rectangle of the canvas -- Part 29 arms them to
        `iw=65`/`ih=25`/`h=425`/`w=735` (the stage-select preview's
        letterboxed interior window, matching GameSparker.java:2741-2744)
        every time the player browses to a stage, but nothing ever set
        them back to the full-canvas `iw=0`/`ih=0`/`h=450`/`w=800` before
        racing starts -- `m` is the SAME shared `Medium` instance across
        every screen, so whatever stage-select last armed stayed in place
        straight into the race, and `medium_d()` kept painting the
        backdrop only inside that smaller rectangle, leaving the outer
        65px (sides) / 25px (top/bottom) band showing raw GL clear color
        (near-black) instead -- exactly the reported border. Java itself
        never hits this because `xtGraphics.java`'s `musicomp()` (fase 6,
        the tick immediately before racing's fase 0 begins) explicitly
        resets `m.ih=0`/`m.iw=0`/`m.h=450`/`m.w=800` -- a reset this port
        had never ported at all, since every racing entry point tested
        so far (this session's own screenshot-hook shortcuts included)
        happened to jump straight into `STATE_RACING` without ever
        visiting stage-select first, so `m` was still at its
        `medium_init()` defaults (already 0/0/800/450) and the missing
        reset had no observable effect until a real player took the real
        path through the menus.
        Fix: added the exact same reset at the exact same point Java
        does it -- the top of `STATE_RACING`'s one-time setup block, right
        before `load_stage_objects()`.
        - **Verification**: the normal `NFM_SCREENSHOT_PPM`-without-
          `NFM_SCREENSHOT_MENU` hook forces `STATE_RACING` directly
          (see its own doc comment), which -- ironically -- made it
          impossible to reproduce this exact bug through that hook alone,
          since it never visits stage-select either. Tried real end-to-
          end input simulation first (Xvfb + `xdotool`, no window
          manager): unreliable, 0-2 of 4 scripted key presses registered
          across repeated attempts depending on focus/timing, not a
          trustworthy regression check. Switched to a targeted,
          deterministic repro instead: a temporary (never committed)
          env-var hook that set `m.ih`/`iw`/`h`/`w` to stage-select's
          exact armed values right before racing's setup block, i.e.
          "simulate having just left stage-select" without needing the
          flaky UI replay. With the bug present this reproduced the
          reported borders exactly (screenshot: black bars on all four
          sides, track/cars confined to the inner rectangle); with the
          fix applied and the SAME injected corruption, the borders
          vanished completely and the scene filled the full canvas --
          same frame, same camera, same cars, only the reset differing.
          Confirmed via a real (non-shortcut) menu walkthrough too (Xvfb
          + `xdotool`, main menu -> NFM1 -> car select -> stage select ->
          confirm -> racing) once input timing was made reliable: clean,
          border-free full-screen racing render. Full 25-test suite still
          passes (no core/ logic touched).
  - [ ] (investigated, not a bug -- but the FIRST investigation's reasoning
        was wrong; see the correction below) Car "flying" on ramps.
        User-reported twice, the second time as "o carro sai do chao quando
        esta em cima de uma rampa" -- i.e. framed as the CAR leaving the
        surface, not just its shadow.
        **First pass (WRONG reasoning, right outcome).** It concluded the
        shadow's "follow real geometry" branch was dead code in the
        original, on the grounds that `set(` never registers `Trackers`
        entries and that only the boundary-wall macros do, all at
        `zy`/`xy` = +-90. Both halves of that are false, and the second
        pass measured it:
          - `cont_o_init_copy()` (cont_o.c:79, used by EVERY `set(`
            placement) pushes the placed model's own trackers into the
            global array with rotated `xy`/`zy` (cont_o.c:126-161). Real
            stages therefore carry plenty of sloped trackers -- counted by
            instrumenting a real load: 20 on stage 1, 59 on stage 5, 141
            on stage 9, 67 on stage 14.
          - Instrumenting which shadow mode actually runs during a real
            race on stage 9: 1546 of 1546 shadow draws took mode 0, the
            geometry-following one. Mode 1 (the flat `medium.ground`
            projection) never fired at all -- the exact opposite of what
            the first pass claimed.
        **What is actually going on.** `plane_s()` mode 0 walks the
        sector's tracker list BACKWARD and snaps every shadow vertex to
        the FIRST match it finds, then breaks (plane.c:889-924, matching
        Plane.java:1239-1262 line for line, `rady != 801` exclusion
        included). Over a ramp the sector holds both the ramp and the flat
        ground under it, so which one wins is decided purely by their
        order in `sect[]` -- and when the flat ground wins, the shadow
        snaps to the flat ground while the car sits high on the ramp.
        That IS what the original does: the outer mode gate (cont_o.c:
        1165-1174) and the snap loop are both byte-faithful to
        ContO.java:1357-1362 and Plane.java's own, so the detached look is
        authentic, just not for the reason first given.
        **The car itself was measured, not assumed.** A throwaway probe
        drove the mad_test slope scenario's car up a 30-degree ramp and
        logged body-y against the analytic ramp surface every tick: the
        offset held constant (~-81 to -103 units, i.e. ride height) for
        the whole climb, then the car left the ramp at its edge and
        followed a clean ballistic arc. No float, no early lift-off. The
        first probe run looked alarming until the ramp's own sign was
        checked -- that ramp DESCENDS below the world's default y=250
        floor, so the car correctly transferred onto the floor and its y
        stopped changing.
        Left unchecked (closed, not done): the user already chose to keep
        the original's shadow behaviour rather than add real ramp-following
        shadows as an acknowledged departure. Reopen only to implement
        that departure -- not to re-derive the analysis, which is now
        measured rather than argued.
  - [x] Part 33: instant replay -- "the highlight reel of the race's best
        moment" (user-reported: "ainda falta o replay do melhor momento da
        corrida como no original"). The single largest feature ported this
        session, on par with the physics engine/AI driver in scope.
        `Record.java` (923 lines, no `web/Record.js` port exists to
        cross-check against, so this went straight to the decompiled Java)
        is a per-car, per-tick history ring: 300-tick position/orientation
        buffer, spark/skid-spark rings, and a damage-dent ring (already
        ported in an earlier session), plus a "photo finish" freeze
        (`cotchinow()`) that deep-copies every live ring into a frozen h*-
        prefixed twin the instant either (a) a car's own `dest` countdown
        crosses 230 (just got wasted) or (b) `CheckPoints.checkstat()`
        detects a close photo-finish. `core/record.h`/`.c` now port the
        whole thing: `record_init`/`reset`/`rec`/`cotchinow`/`play`/
        `playh`/`regy`/`regx`/`regz`/`chipx`/`chipz`, including a confirmed
        GENUINE divergence between Record.java's own damage-recolor clamp
        ladder (used only when REPLAYING a dent) and Mad.java's live one
        (already in `mad_recolor_plane`) -- two different HSB curves in
        the real original, preserved as two separate functions rather than
        sharing one.
        Wiring into `game.c` turned out to be mostly already anticipated
        by an earlier session (`Record rpd;`, `record_init(&rpd)`, both
        `check_points_checkstat()` call sites already passing `&rpd`, and
        `Mad`'s own `record_recy/recx/recz`/powerup-catch `cotchinow`
        trigger already wired in `mad.c`) -- the only missing pieces were
        `record_reset(&rpd, ...)` at each race's setup (GameSparker.java
        :2768, right after every car's `ContO` is (re)built for the race)
        and a `record_rec()` loop per car per tick, inserted between the
        `mad_drive()` loop and `check_points_checkstat()` (GameSparker.java
        :950-953) -- matching the original's exact ordering.
        Added a new `STATE_REPLAY` (Java fase==-3, GameSparker.java
        :1388-1630) reached from the win/hold-card advance instead of
        going straight to `STATE_POST_RACE`, whenever `record.hcaught` is
        true (something dramatic enough got frozen this race) -- ported
        `xtGraphics.java`'s own fase==-2 gate first (the `stage==1||2 &&
        looped!=0` special-case that suppresses a replay of the player's
        own harmless loop-crash on the first two stages, `control_falseo`
        reset, and the random camera-twist seed). Reconstructs every car's
        frozen pose each tick via `record_playh()` and swings the camera
        with `medium_around()`/the newly-ported `medium_transaround()`
        (`Medium.java:582-620` -- linear position lerp between two cars
        plus the same orbiting-radius math `around()` uses) across one of
        four choreographies picked by `record.wasted`/`record.closefinish`:
        a single-car "crash reel" (local player was the one caught, 3
        random strobe-flash variants) or one of three two/three-swing
        reels between the local player and whichever car was caught/almost
        caught. ENTER/handbrake or 2 full 300-tick loops through the reel
        advance past it. Ported `xtGraphics.levelhigh()`'s caption panel
        (`gameh.gif`, present in `data/images.zip` and now loaded via the
        existing `load_menu_gif` menu-asset table) + flashing "You Wasted
        'em!"/"Close Finish!"/"Close Finish! Almost got it!"/"Wasted!"/
        "Stunts!"/"Best Stunt!" text, reusing the existing `hud_say_draw()`
        drawcs-equivalent helper.
        Deliberately skipped, matching this port's established pattern of
        skipping purely-cosmetic transitions: Java's fase==-4 (a 7-tick
        Madness-logo pixel-melt) between the reel and `finish()`. Its ONE
        gameplay-relevant side effect -- `xtGraphics.sendwin()`'s
        `unlocked[]++` stage-unlock -- already happens via this port's
        `game_progress_finish_stage()`, called at hold-card-advance time
        regardless of whether a replay plays; going straight to
        `STATE_POST_RACE` after the reel loses no functionality. Also out
        of scope, and explicitly NOT ported: the PAUSE-MENU "Watch Replay"
        option (`xtGraphics.java` fase==-1, opselect==1 during a paused
        race) -- a manually-triggered on-demand replay of the raw
        (non-frozen) 300-tick ring, a genuinely separate feature from the
        automatic post-race highlight reel the user actually asked for;
        left as a known, documented gap for a future session.
        - **Verification**: `record_test.c` (new) covers `reset()`,
          `rec()`'s position-ring shift, the checkpoint ring's `n==im`
          gate, `caught`/`cotchinow`'s deep-copy freeze, and `playh()`
          reading back the frozen snapshot -- all against a real car
          loaded via `RecordTestRig` (same `Simple_Car.rad` pattern
          `mad_test.c` uses). `medium_test.c` adds
          `test_transaround_lerp_and_orbit()` (hand-traced bookkeeping +
          an independently-authored trig cross-check). All 25 tests in
          the suite pass, all new tests passed on the first run (unusual
          for this session -- attributed to reading every one of `rec()`/
          `cotchinow()`/`play()`/`playh()`/`regy()`/`regx()`/`regz()`/
          `chipx()`/`chipz()` word-for-word before transcribing any of
          it). Visually verified with Xvfb: since a real crash/close-
          finish can't be scripted through this sandbox's Xvfb (no
          reliable `xdotool` input replay, same limitation Part 32 hit),
          added a temporary `NFM_SCREENSHOT_MENU=replay{0,1,2,crash}`
          headless-preview hook (same pattern as the pre-existing
          `holdcard` hook) that forces a synthetic frozen snapshot and
          jumps straight to `STATE_REPLAY`. All four captured cleanly:
          `replay0`/`replay1`/`replay2` show the "GAME HIGHLIGHT" panel
          with "...ASTED 'EM!"/"...E FINISH!"/"...ALMOST GOT IT!" (the
          `closefinish` 0/1/else captions) over the starting-grid pose;
          `replaycrash` shows the camera already mid-orbit around the
          local player's own car via `medium_around()`. A separate 600-
          frame real-gameplay smoke run (no screenshot-menu shortcut, so
          `record_rec()` runs live every tick alongside real bots
          crashing/racing) completed without a crash and rendered a
          normal HUD frame afterward, confirming the new per-tick
          `record_rec()` calls don't destabilize ordinary racing. Full
          25-test suite passes.
  - [x] Part 34: "Car Fixed" message never disappears + car turns
        permanently discolored after a repair-zone fix. User-reported:
        "quando passa pelo fix do carro, aparece 'car fixed' e o carro
        fica todo bugado em azul, e a escrita car fixed nunca desaparece".
        Also incidentally corrects a stale, wrong assumption repeated in
        two other comments (the STATE_RACING draw block and Part 33's own
        STATE_REPLAY fix-zone comment): this port's `CheckPoints` DOES
        have real fix zones -- `game_sparker.c`'s stage parser already
        populates `cp->fx/fy/fz/roted` from a real `fix(` stage command,
        and `mad.c:1793` already sets `mad->newcar = true` whenever a
        damaged car reaches one (the fcnt==7||8 transition). The real bug:
        `GameSparker.java:891-903` -- the single-player "newcar rebuild"
        that runs once per drawn frame, rebuilds a fixed car's `ContO`
        fresh from its pristine base model (wiping every damage dent/
        recolor), and clears `newcar` back to `false` -- was never ported
        at all. Without it, `newcar` stays `true` forever once set, so (a)
        the car keeps whatever damaged/recoloured planes it had at the
        exact moment it drove over the repair pad instead of returning to
        its pristine paint (the "car turns all blue" -- accumulated damage
        recolor hue never resets), and (b) `hud_messages_tick()`'s own
        `if (mad->newcar) { say="Car Fixed"; tcnt=0; }` trigger
        (xtGraphics.java:8330-8341, already correctly ported) re-forces
        `tcnt=0` EVERY single frame forever, so the message's own
        `tcnt<30` display window never counts up past 0 and the banner
        never times out.
        Fixed by porting the missing rebuild into `game.c`'s STATE_RACING
        draw block, for every car, gated on `mad[i].newcar`: saves
        `xz`/`xy`/`zy`, reconstructs `co[i]` via `cont_o_init_copy()` from
        the car's pristine base model (`base_models[mad[i].cn]`, or
        `car_base` for the custom-car slot -- same pristine-source pattern
        already used by Part 33's STATE_REPLAY fix-zone code and the
        starting-grid setup), restores the saved orientation, then clears
        `newcar`. Placement matters and isn't arbitrary: Java's own
        rebuild sits at the TOP of its per-frame block, but that same
        block's tick-equivalent (`drive()`, which is what actually SETS
        `newcar` true) and the "Car Fixed" trigger (inside `stat()`) both
        run LATER in that same per-frame pass -- so a given frame's
        rebuild always consumes the PREVIOUS frame's flag, while that
        SAME frame's fresh flag (set moments later by `drive()`) is still
        visible to the message trigger before anything clears it. This
        port's tick loop and draw pass are already separate (unlike
        Java's unified per-frame loop), so matching that exact ordering
        meant placing the rebuild AFTER `hud_messages_tick()` in the same
        draw call, not before -- placing it before (matching Java's
        source-line ORDER literally rather than its actual DATA-FLOW
        ordering) was tried first and silently broke the message trigger
        the other way: newcar got cleared before hud_messages_tick() ever
        saw it true, so "Car Fixed" would never show at all. Also updated
        `mad.h`'s own `just_fixed` doc comment (previously claimed newcar
        "stays true for a 10-tick window," which is actually the
        multiplayer-only `newedcar`/`colorCar` variant, fase==7001,
        genuinely out of scope here) and corrected the two stale "fix
        zones can't happen here" comments mentioned above.
        - **Verification**: full 25-test suite passes (this is pure
          game.c wiring, no new core/ logic, same category as Parts 31/32
          -- no dedicated test binary). Visually verified with Xvfb using
          a temporary (never committed) `NFM_DEBUG_FORCE_NEWCAR_FRAME=N`
          env-var hook that force-sets `mad[0].newcar=true` on a chosen
          frame, matching the exact same "inject the precise condition
          directly, since real gameplay can't be scripted through this
          sandbox's flaky xdotool" technique Parts 31/32/33 already
          established. Confirmed the full lifecycle: the frame the flag
          is forced shows nothing yet (matches the source's own "a
          freshly-set message displays starting next frame" ordering,
          already documented above the say/tcnt loop); the VERY NEXT
          frame shows "CAR FIXED" appearing; 34 frames later (well past
          the `tcnt<30` window) it's gone again -- confirming the message
          now correctly times out instead of persisting forever. Also
          confirmed no regression: a 600-frame real-gameplay smoke run (no
          forced flag) renders a completely normal HUD frame.
  - [x] Part 35: three Vita-specific bugs -- low FPS, not fullscreen,
        controls stuck on the digital d-pad. User-reported: "o FPS no
        vita está muito baixo do esperado... também a resolução não fica
        full screen no ps vita e os controles estão nos direcionais
        digitais e não nos analógicos". None of these are testable in
        this sandbox (no VitaSDK, no hardware -- same standing limitation
        every Vita-side change in this file carries), so each fix below
        is reasoned from source + documented API behavior and verified
        only insofar as the SHARED code (game.c, tests) still builds/
        passes/renders correctly on Linux; the Vita-specific files
        (platform/vita/*.c) remain unverified on a real toolchain per
        this section's standing caveat.
        **FPS**: the user's own guess ("deve ser alguma diferença na
        contagem de ticks") pointed at the tick/timing code, but that
        code is 100% shared with Linux (game.c's fixed-53ms accumulator,
        `platform_ticks_ms()`) and has nothing platform-specific to
        diverge in -- a red herring, most likely. The actual, verifiable
        bug: `native/CMakeLists.txt` never set `CMAKE_BUILD_TYPE` (nor
        added an explicit `-O` flag), so EVERY build in this repo --
        including every Linux build this whole port has ever been
        verified against -- was silently compiling with NO optimization
        at all (the compiler's own default absent an explicit flag).
        Invisible on a fast desktop CPU, which is exactly why nobody
        noticed; the Vita's much weaker embedded ARM core pays for it
        directly, running unoptimized physics (`mad_drive()`/
        `mad_colide()` for up to 7 cars/tick) and this port's own
        immediate-mode `glBegin`/`glVertex`-per-triangle rendering
        (gfx_gl.c's own top comment already flags that style as "slower
        than a real VBO... optimize later if profiling says so" -- -O0
        makes that gap far worse). Fixed by defaulting
        `CMAKE_BUILD_TYPE` to `Release` when the caller doesn't specify
        one (confirmed via `VERBOSE=1`: now compiles at `-O3 -DNDEBUG`).
        Secondary, lower-confidence contributor also fixed while in
        there: `platform/vita/platform.c`'s `VGL_HEAP_SIZE` was still
        8MB, sized for the old spinning-hexagon tech demo's one rotating
        shape -- this port now holds ~139 decoded-to-RGBA menu/HUD
        textures resident all session (several full 800x450 photo
        backdrops alone exceed a megabyte each uncompressed), plausibly
        forcing vitaGL into texture eviction/reload thrashing once that
        heap fills. Raised to 64MB.
        **Not fullscreen**: real, concrete, and unrelated to the FPS fix.
        `game.c`'s `width`/`height` locals are a fixed LOGICAL 800x450
        game-space (every HUD coordinate and the one `glOrtho(0,800,450,
        0,-1,1)` call are sized to it) -- correct as the render-target's
        own viewport (the offscreen texture genuinely IS 800x450) but
        also reused, unchanged, for the FINAL composite blit's viewport
        onto the real screen. On Linux that's harmless (the SDL window
        IS 800x450). On the Vita, vitaGL always owns the full physical
        960x544 framebuffer -- viewporting the final blit to 800x450
        only ever drew into a sub-rectangle of it, leaving the outer
        ~14% of the screen on every side undrawn. Since
        `gfx_gl_render_target_blit()`'s quad is drawn in that SAME fixed
        logical clip space regardless of viewport (glOrtho never
        changes), the fix needs nothing more than widening the viewport
        for that one blit: added `platform_display_size()` to the
        shared `platform.h` contract (Linux: `SDL_GetWindowSize()`,
        identical to the logical size; Vita: hardcoded 960x544, no
        window to query) and switched the composite blit's own
        `glViewport` call to use it instead of the logical width/height,
        which now stretches the finished frame to fill the real screen
        with no other code needing to know the physical resolution at
        all. Also applied the same fix to the (normally-dead, only
        reachable if `gfx_gl_render_target_init()` itself ever fails)
        direct-render fallback path, for robustness, since that
        genuinely untested-on-hardware failure mode would otherwise hit
        the identical bug a different way.
        **Analog stick**: `platform/vita/input.c` (drives the actual
        race -- `Control.up/down/left/right/handb`, read by
        `mad_drive()`) and `platform/vita/platform.c`'s `platform_poll()`
        (drives menu navigation) both ONLY read `SceCtrlData.buttons`'
        d-pad bits, matching this port's own doc comments at the time
        ("Analog stick input... NOT wired -- out of scope"). That was a
        real, reported gap, not a documented non-goal holding up under
        a real complaint: added `pad.lx`/`pad.ly` (left stick, 0..255,
        128=centered) reads in BOTH files, OR'd onto the existing d-pad
        checks (so d-pad keeps working too) with a deliberately generous
        +-40 deadzone -- this game's whole input model is boolean
        held/not-held with no analog throttle or steering GRADIENT to
        preserve, so there's no reason to use a tight deadzone tuned for
        analog precision.
        - **Verification**: full 25-test suite passes; confirmed the
          Linux build now actually compiles at `-O3 -DNDEBUG` (was
          previously unflagged/`-O0`, checked via a `VERBOSE=1` rebuild
          before/after). Re-verified the STATE_RACING/menu Xvfb
          screenshot regression checks render pixel-identical to before
          this change (expected: `platform_display_size()` returns the
          same 800x450 the logical constants already used on Linux, so
          this whole Part is a structural no-op there by design -- its
          actual effect is Vita-only and can't be screenshotted from
          this sandbox). The three fixes themselves (build type/heap
          size, fullscreen viewport, analog input) could not be verified
          against real Vita hardware or even a VitaSDK compile in this
          environment -- flagged as the next thing to confirm once
          someone with a real device/toolchain can (see item #59 below).

### Vita-specific (does not block the Linux track)
- [x] Asset shipping decision (previously open): bundled inside the
      `.vpk` via `vita_create_vpk`'s `FILE` list (Part 18 above) --
      total real asset size (~5MB) settled this in favor of the simpler
      option; `ux0:`-separate-push was the fallback if it had been much
      larger.
- [x] `native/platform/vita/input.c`: written (Part 18 above) -- sceCtrl
      D-pad/face-buttons -> the same shared `Control`/`Button` contract
      the Linux backend produces. Still unverified against real hardware.
- [~] The VitaSDK cross-build itself has never been run in THIS sandbox (no
      VitaSDK here, and this session's own attempt to bootstrap one found
      that this sandbox's network proxy allows `git clone` of public
      GitHub repos but not the prebuilt-toolchain binary downloads
      `vdpm`'s installer normally uses, and building the full ARM
      gcc/binutils/newlib toolchain from source here was judged too
      slow/risky to attempt blind) — user was pointed at installing
      VitaSDK on their own machine instead, where the normal prebuilt-
      binary path just works, and IS now actually cross-compiling there.
      Real errors found and fixed so far, in order hit:
      - `find_package(ZLIB REQUIRED)` (`native/CMakeLists.txt:43`) doesn't
        resolve on its own -- the VitaSDK's zlib portlib isn't installed
        by `install-all.sh` by default, needs `vdpm zlib` explicitly.
        Not a code bug, just an undocumented setup step -- no source
        change.
      - vitaGL and vitashark are separate portlibs too (`vdpm vitaGL`,
        `vdpm vitashark`) -- `platform/vita/CMakeLists.txt:8` already
        documented the vitaGL requirement, vitashark (vitaGL's own
        shader-compiler dependency) wasn't previously known to this repo.
        No source change.
      - **Real bug, fixed**: `platform/vita/platform.c`'s
        `platform_shutdown()` called `vglEnd()`, which does not exist in
        the real vitaGL public API (confirmed via `grep` against the
        user's actually-installed `vitaGL.h` -- no `vglEnd`/`vglStop`/
        `vglTerminate` of any kind, only setup calls and
        `vglSwapBuffers`). vitaGL apparently has no explicit teardown
        function at all; `platform_shutdown()` is now a no-op with a
        comment explaining why (`main()`'s own return unwinds through
        the standard VitaSDK exit path, which reclaims the GPU heap).
      - **Real bug, fixed**: the Threadmgr stub lib was named wrong --
        `platform/vita/CMakeLists.txt` linked `SceThreadmgr_stub`, which
        doesn't exist under that name; the real usermode stub is
        `SceKernelThreadMgr_stub` (confirmed via `find $VITASDK -iname
        '*Threadmgr*'` against the user's install -- the ForKernel/
        ForDriver variants sitting next to it are kernel-side, not what
        a usermode app links against).
      - **Real bug, fixed**: linking `nfm_vita` with the plain C driver
        left every `std::`/`operator new`/`delete`/`__cxa_*`/
        `__gxx_personality_v0` symbol vitaGL's own C++ objects
        (`preprocessor.o`, `expression.o`, `gxm.o`) need unresolved --
        hundreds of undefined-reference lines from one root cause. Fixed
        by promoting the top-level `project()` to `C CXX` and setting
        `LINKER_LANGUAGE CXX` on the `nfm_vita` target specifically (the
        Linux target and every source file in this repo stay plain C;
        only Vita's final link step now goes through `g++`).
      - Also needed, once the C++ runtime resolved: `vitashark` (vitaGL's
        shader-compile pipeline calls `shark_*` directly) plus
        `SceShaccCgExt`/`SceShaccCg_stub`/`taihen_stub` alongside it (the
        two `shark_compile_shader_extended`/`shark_clear_output` symbols
        specifically stayed undefined with only `vitashark` linked, until
        these three joined it), `mathneon` for vitaGL's NEON math helpers
        (`matmul4_neon`/`normalize3_neon`/`sincosf_c`/etc.), and two more
        SCE stubs: `SceAppMgr_stub` (`sceAppMgrGetBudgetInfo`, the
        `sceSharedFb*` family) and `SceKernelDmacMgr_stub`
        (`sceDmacMemcpy`). None of these are source bugs, just the real,
        undocumented-anywhere-in-this-repo vitaGL dependency closure --
        `platform/vita/CMakeLists.txt` now documents each one inline
        with which vitaGL object file needed it.
      - **vdpm gotcha, not a code issue**: the mathneon portlib's real
        package name is `libmathneon`, not `math-neon`/`mathneon` --
        `vdpm` doesn't check the download's HTTP status, so a wrong name
        pipes a GitHub 404 page into `tar` and fails as "xz: unrecognised
        file format" (and, worse, `vdpm` had already recorded the
        package as "installed" from the failed attempt, so a second
        `vdpm math-neon` silently no-op'd until `-f` was added) --
        confirmed the real name via `vitasdk/packages`'
        `libmathneon/VITABUILD` (`pkgname=libmathneon`). The produced
        archive is `libmathneon.a` either way, so `mathneon` in
        `target_link_libraries` was already correct.
      **Result: `nfm_vita.self`/`nfm_vita.vpk` build clean end to end** on
      the user's real VitaSDK install -- the first real Vita build this
      port has ever produced. Still needs: confirm the `.vpk` installs
      and actually runs on real hardware -- everything past the build
      itself (input.c, audio.c, the rest of platform.c's actual runtime
      behavior) is unverified.

    - **First real-hardware run: stack overflow, in our own code.** User
      ran the `.vpk` on real Vita hardware; it crashed immediately with
      the generic homebrew-crash dialog ("C2-12828-1", which carries no
      diagnostic value beyond "something crashed"). Root-caused from the
      `.psp2dmp` coredump against the user's own unstripped
      `nfm_vita.velf`; the method matters, because the first attempt at
      this got it **completely wrong** and the wrong answer was reported
      to the user before being caught:

      **The relocation step is what everything hinges on.** The `.velf`
      links at `0x81000000`, but Vita main modules are `ET_SCE_RELEXEC`
      (`e_type=0xfe04`) and are *relocated at load time*. Reading
      addresses straight off the `.velf` is therefore meaningless on its
      own. The real load address is recorded in the coredump's own
      `MODULE_INFO` section (find the `nfm_vita` name string, the segment
      descriptors follow it): seg0 `addr=0x81045000 size=0x0e8860`, seg1
      `addr=0x81200000 size=0x383664`. Both sizes match the `.velf`'s
      program headers exactly, which also confirms the `.velf` and the
      crashing `eboot.bin` are the same build. So **text slide =
      0x81045000 - 0x81000000 = 0x45000**, and any runtime address must
      have that subtracted before symbol lookup. Skipping this made the
      faulting PC appear to land inside `vgl_splash_data` (vitaGL's boot
      splashscreen blob) and produced a confident, entirely fictitious
      "the crash is inside vitaGL, not our code" conclusion.

      **Registers come from the dump, not from a screen reading.** The
      dump's `THREAD_REG_INFO` holds the real set (r0-r12, sp, lr, pc,
      cpsr in that order): `pc=0x810c72d6 lr=0x810c8273 sp=0x815fef18`,
      `cpsr=0x60000030` (T bit set -- Thumb, user mode). Note these sit
      exactly `0x8000` below the `pc 0x810cf2d6, LR 0x810d0273` the user
      read out of VitaShell's coredump viewer, i.e. that reading was from
      a *different build's* dump. The dump's own registers are the ones
      that match this `.velf`, and they are internally consistent in a
      way the other pair is not: they resolve to a genuine caller/callee
      pair, where the VitaShell numbers put both pc and lr inside a
      single function.

      With the slide applied: **`pc` = `lzw_decode+0x16`, `lr` =
      `gif_decode+0x64f`** -- GIF decoding, during startup asset loading.
      Disassembling the prologue (capstone, `CS_ARCH_ARM`/`CS_MODE_THUMB`,
      file offset `0x1000 + (linked_addr - 0x81000000)`) names the exact
      instruction:
      ```
      sub.w sp, sp, #0x6000     <- grow frame by 24576 bytes
      sub   sp, #0x34
      str   r3, [sp, #8]        <- FIRST write into it: Data Abort
      ```
      A fault on the first touch of a freshly-grown frame is a stack
      overflow, full stop. `lzw_decode`'s `prefix[4096]`/`suffix[4096]`/
      `rev[4096]` are exactly that `0x6000`.

      **Why 24KB was enough to overflow:** `game_run()` is one 3,800-line
      function whose own frame measures **~1,043,000 bytes**
      (`gcc -fstack-usage`), against a main thread stack of exactly 1MB
      (`sceUserMainThreadStackSize`, platform/vita/main.c). It had ~20KB
      of headroom; the decoder wanted 24KB.

      Fixed on three levels, all verified on the Linux build (clean
      `-Wall -Wextra`, `java_compat_test` green, all six menu screens
      screenshot-compared and unchanged -- GIF decoding is what draws
      them, so a regression there would be immediately visible):
      1. `core/gif_decode.c` -- LZW tables moved to one heap block
         (`lzw_decode` splits into an `_impl` taking `LzwTables *`, plus a
         malloc/free wrapper, so none of the many `return false` paths
         needed touching). Frame: 24,628 -> ~1.8KB.
      2. `platform/common/game.c` -- `Trackers t` (~353KB alone) made
         `static`; safe because `game_run()` is called exactly once from
         `main()`, never recursively or from a second thread. Frame:
         1,043,008 -> 681,152.
      3. `platform/vita/main.c` -- `sceUserMainThreadStackSize` 1MB ->
         4MB, for real headroom rather than a thin margin.

      **Still open, and worth a task of its own:** ~646KB of
      `game_run()`'s remaining 681KB frame **cannot be attributed to any
      named local** -- every declaration in the function body sums to
      ~34KB. It is not inlining (`-fno-inline` changes nothing), not the
      optimizer (identical at `-O0` through `-O3`), and not a VLA/alloca
      (`-Wvla -Walloca` are silent). Something about how a function this
      size is compiled is holding far more stack than its variables need.
      The right fix is to break `game_run()` up rather than to keep
      raising the stack size; until then the 4MB covers it.

### Leftover HUD/menu assets: audited, nothing left to wire

A standing task read "wire remaining relevant HUD assets (arrows.gif,
start1/2, pgate)". Audited all three against the Java; none needs work,
recorded here so it does not get re-investigated:

- **`arrows.gif`** — already wired. The original draws it from exactly
  four places, all inside `inst()` (xtGraphics.java:4141, :4178, :4249,
  :4265, the Instructions screen's page-navigation arrows), and this port
  draws all four at those same coordinates: (505,323), (491,323),
  (505,83), (491,213). Done as part of the Instructions flipbook work.
- **`pgate.gif`** — already wired. Two draw sites in the original, the
  cantgo() padlock row (:2001-2003) and the car-select locked-car fence
  (:5335); the port has both.
- ~~**`start1.gif` / `start2.gif`** — OUT OF SCOPE~~ — wrong, struck.
  `hipnoload()` is not only a download screen: it is the stage
  presentation card (fase 5 loadmusic + fase 6 musicomp) that waits for
  Start before EVERY race, with Coach Insano's per-stage hint. Ported as
  `STATE_STAGE_INTRO` (see "Fidelity pass" below).

## Fidelity pass: stage card, music, menu track

- [x] **Stage presentation card** (`STATE_STAGE_INTRO`): hipnoload() with
      loadopsnap()'d loadingmusic/start1/start2/float.gif, the hints, the
      one-frame "N KB / Please Wait..." step, the blinking Start; music
      starts on the card, confirm runs musicomp()'s resets. Compared
      against the real jar driven by java.awt.Robot under Xvfb.
- [x] **Music is the original's renderer, byte for byte**
      (`core/radical_mod.c`, replacing `mod_play.c`/`mod_decode.c`): Mod
      parser + ModSlayer + SuperClip loop. All 34 tracks byte-identical to
      the jar's stream, loop points included (`tests/radical_mod_test.c`).
      The old player ignored the BPM and rate arguments and used the BPM
      as gain.
- [x] **Menu track** (`music/interface.zip`): plays from car select through
      the stage list, stops on stage confirm, as intertrack does.
- [x] **Stage 27 in the NFM2 campaign plays `music/party.zip`.**
- [x] **M-key unmute resumes the track** instead of restarting it.
- [x] HUD textures refilled per race instead of re-uploaded (leaked ~25
      textures per race).
- [ ] **Text is still the 5x7 vector font, uppercase only.** The original
      draws every menu string in Arial bold 11/13 (mixed case, `&`, `?`,
      `>`...). The biggest remaining visible difference on every screen.

## Performance / memory

- [x] **`cont_o_init_copy()` over a live ContO leaked the whole old car.**
      It starts with `memset(dst, 0, ...)`, and the replay ring
      (`record_rec`, 6 copies per car every ~50 calls), the newcar rebuild
      on every repair, the replays and `record_init`'s own memset all
      called it on ContOs that already owned their Planes. ~50KB per car
      copy, ~55MB per minute of a 7-car race measured on the host; the
      Linux build's peak RSS went 159MB -> 193MB between frame 1500 and
      3000 of a headless race before the fix, flat 137MB after. On the
      Vita that is heap growth and fragmentation for the whole session,
      plus a burst of ~800 mallocs on every replay keyframe tick. Fixed
      with `cont_o_recopy()` (free, then copy) at those sites,
      `record_free()` before each race's `record_init()`, and `co[]`/`rpd`
      zeroed at declaration so the first free is safe. `init_copy` itself
      is unchanged, since the stage loader and the tests hand it
      uninitialised memory.
- [x] `gfx.c`'s `fill_trapezoid` did two malloc/free pairs per concave
      polygon (hundreds a frame); stack buffers now, heap only past 28
      vertices.
- [x] **Car select's smoke-warp entrance dropped frames on the Vita.**
      `car_smoke_warp_step` (drawSmokeCarsbg) runs every rendered frame for
      ~33 frames: a sqrt and five float divisions per smoke pixel plus a
      full 670x400 RGBA re-upload. The smoke pixels (23,131 of the mask's
      94,132) and their per-channel factors are now tabulated once at load
      in the Java loop's own order -- bit-identical output over a whole
      animation, checked on the host -- and only the rows a step wrote
      (~58% on average) are re-uploaded. Host: 0.65 -> 0.47 ms per step.
      NOT measured on hardware. If it still drops there, the next lever is
      pacing the step at the 53ms tick like the original (it currently runs
      ~3x as often, and so ~3x as fast, as the Java did).
- [x] **Menus were smaller than the race and not fullscreen.** They draw
      in the original's 670x400 letterbox at (65,25) inside the 800x450
      frame; the final blit now copies just that rectangle, stretched to the
      screen (`gfx_gl_render_target_blit_region`), for every menu state.
      Racing, the replays, pause and cantreply keep the whole frame.
- [x] **Pause menu brought to 1:1 with pausedgame().** It redrew the
      frozen 3D scene in colour, sharp and without the HUD, under square
      highlights. Now: fase -6's `pauseimage()` is transcribed
      (`pause_image()` -- per-row running-average greyscale smear, the
      237x188 panel under paused.gif tinted blue), applied to the frame
      read back from scene_rt at the top of the frame after the pause
      (HUD included, as Java's offImage), and again after the pause replay
      (GameSparker.java:1340 returns through fase -6). Highlights and the
      cantreply plate use new `gfx_fill_round_rect`/`gfx_draw_round_rect`
      (arc 7x20); cantreply's text gets drawcs mode 1's drop shadow, now
      implemented in hud_say_draw. `NFM_SCREENSHOT_MENU=paused` dumps it
      headless. Unverified on the Vita: glReadPixels from an FBO under
      vitaGL (falls back to the old frozen-scene redraw only when render
      targets are unavailable altogether, not if the read misbehaves).
- [x] **Rendering performance pass (Vita frame rate in race, car select,
      stage select).** Measured on the host with a core-only model of one
      race frame (medium_d + 7 cars + stage, no GL) under callgrind, every
      change checked bit-identical by hashing the whole vertex buffer over
      300 frames on six stages with dents/chips/sparks/dust/repair:
      - plane_d/plane_s malloc'd 7 + 5 scratch arrays per face (~20% of
        the frame) -> stack arrays (PLANE_MAX_N = the parser's 100).
      - cont_o_d's O(npl^2) face ranking -> stable merge sort (same
        permutation, same replacement as web/ContO.js).
      - per-vertex helpers (jtrunc, medium_xs/ys/sin/cos/rot, wrappers)
        static inline; gfx reserves per polygon, not per vertex; plane_d's
        pairwise max |dx|/|dy| is max - min with the pair loop as fallback.
      Race draw: 13.25M -> 8.68M instructions/frame.
      - record_rec moved the keyframe ring instead of 5 deep copies (a
        ~2.8M-instruction spike every ~7 ticks); cars with a gr == -15 face
        keep the copies, since those re-randomise from the sim stream.
      - Motion blur via a ping-pong pair: 2 passes instead of 3; screens
        with no read-back draw straight to the display (1 pass).
      - Racing and both replays draw once per tick (53ms), car/stage select
        once per ~40ms (the Java loop's menu rate), presenting the last
        picture in between. Besides ~3x/2.4x less work this fixes fidelity:
        the trail blended per display frame (a fraction of the original's),
        the HUD's per-draw timers ran 3x fast ('Checkpoint!' 0.5s instead
        of 1.6s), the replays played 3x fast, the menus animated 2.4x fast.
      - Vita clocks raised to 444/222/222/166 MHz (were the system defaults).
      - NFM_SHOW_FPS adds a logic:build:gl:swap ms breakdown line.
      NOT measured on hardware yet -- read the breakdown line on a Vita
      next: a high `swap` with low `build` means the GPU is the limit.
- [ ] NOT verified on hardware: whether the reported drop near the repair
      ring is fully explained. On the host the ring itself costs ~0.03ms of
      CPU and ~0.08 screens of extra fill, i.e. nothing. Remaining
      suspects on the Vita: the always-on motion-blur trail in a race
      (two extra full-screen passes, `mvect` is always < 100 in view 0),
      and the audio thread holding `al->lock` for a whole grain's mix +
      MOD render, which blocks every `audio_play` from the main thread.

## Open questions for the user, not yet decided
- ~~Control scheme~~ — resolved: `platform/linux/input.c` uses
  `web/main.js`'s own keyboard binding (see above). `platform/vita/
  input.c` (Part 18) mirrors it onto sceCtrl's D-pad, same scheme, no
  new decision needed.
- ~~Vita asset packaging~~ — resolved: bundled in the `.vpk` (Part 18,
  Vita-specific section above).
- Whether netplay is in scope at all for v1, or a native-only single-player
  build ships first

## How to verify what exists so far
```sh
# core/ only, fastest, no window:
cmake -S native/tests -B native/tests/build && cmake --build native/tests/build
native/tests/build/java_compat_test

# Linux target -- opens a window, actually watch it run:
cmake -S native -B native/build-linux -DNFM_PLATFORM=linux
cmake --build native/build-linux
native/build-linux/platform/linux/nfm_linux
```
The VitaSDK cross-build (`-DNFM_PLATFORM=vita`) has never been run. The next
session with access to a VitaSDK install should run it before doing anything
else in this file, and correct `platform/vita/main.c`'s vitaGL calls against
whatever that build actually reports.
