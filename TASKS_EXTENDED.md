# Need for Madness 2 Extended on the Switch port (branch `extended`)

Extended Mode v2.8's content and gameplay on this C engine. Builds
`nfm_extended.nro` ("Need for Madness 2 Extended", saves in
`sdmc:/switch/nfm-extended/`), so it sits beside the original.

## Rules (the user, 2026-10-07)

- **The C engine stays.** Its rendering, optimisations and visual details
  (sparks, dust, the 2015 wheels, draw order, Smooth Frames, HD) are kept.
  Extended's own rendering is not ported.
- **Extended's gameplay comes in:** its car stats, the physics and AI changes
  its author made on purpose, the specials, its content.
- **Everything unlocked.** No `betalimit` / "END OF BETA".
- **1000-piece cars**, for Revised and Recharged's.
- Users add content without code (cars and stages from the SD card).

What Extended changed and why (intentional vs inherited from the older NFM2):
`nfm-master/research/extended-mode/engine-analysis.md`. The web port of
Extended (`nfm-master/web/ext/`, transpiled from the same source) is the
executable reference: differential tests run against it under Node, and its
fixes for many cars in view (trackgrid, archive loading) are reused.

## Phase 1 -- foundations

- [x] Branch, `nfm_extended.nro`, own title and save folder.
- [x] 1000 pieces per model (`CONT_O_MAX_PLANES`, as web/ContO.js).
- [x] `.radq` archives: zip, 14 of 78 byte-pair swapped (vfs_read_zip, as web/ext/radq.js).
      Extended's data is in `ext/` (career music left out until phase 4).
- [x] Per-car live stats: each racing Mad points at its own CarDefine copy
      (game.c `live_cd`), as each Extended Madness owns its tables. The race
      is pixel-identical before/after (scratchpad ab.sh, smooth frames off).
- [ ] Limits: 6 wheels, objects per stage 610 -> 1106+, checkpoints 140 ->
      440+, fix points 5 -> 50, cars per race 8 -> 20 (+ per-car arrays).
      Moved to phase 3: Classic Mode needs none of them (7 cars, NFM2 stages).

## Phase 2 -- Classic Mode as Extended plays it

- [x] Extended's stat tables for the 16 NFM2 cars: `car_define_extended()`
      over NFM 2's table (64 values; revpush is float now).
- [x] Physics, behind `xt.extended` so the NFM 2 oracle tests still hold
      (mad_test runs a second pass with it on): forca /8000, airborne
      righting toward upright/inverted, wall-surf +15, bumpy road only while
      gripping with one draw and bounce <= 1.35, float handb/2 and gear
      halves, recoil from the other car's tables capped at 3, M A S H E E N
      in classic (x1.27 hits, dammult .225, clrad 30000), the player's 0.76
      power below 98.
- [ ] Still to port when their content arrives: road scan dy<1000 /
      groundlevel (floating floors), double `turn` / ContO.wxz (Extended
      cars' 7.5, 4.5...).
- NFM 2 numbering stays: its cars are 0-15 here (Extended's 23-38); when
  porting Extended code, car `cn` 23+k is k, and Extended's own 0-22 will be
  16+k.
- [x] Specials (core/specials.c, the bar in mad.c): stunts charge the bar
      (AI /3, player /5 at landing, a trickle each tick), Settings > Controls'
      Special (R) fires a full one, the AI fires as soon as it is full;
      nitroandspecials outside career: per-tick stat rebuild from the car's
      base table, self boosts, freeze / strswap / leech / redstr with
      sortpower and randomise, the 1v1 no-attacks rule. NFM_HOOK_SPECIALS=1
      starts every bar full (headless).
- [x] Specials HUD: the Special bar under Power (data/port/special.png from
      Extended's special.GIF + "SPECIAL" in Adventure, tools/bake_special.py),
      the status lines on the left, the player's +/- stat tabs.
- [ ] Still: no-knockback for 15/25/38 while active, car-list colours and
      the outline glow per condition, the car-select description
      (specials_describe() has the texts).
- [x] Classic AI changes (control.c, `xt.extended`): decide_ext() ports
      Extended's whole decision cycle (no rubber-banding, its skiplev, rampp,
      turntyp, mustland, stuntf, trickprf, attack odds, fix and bulistc
      rules); per tick: wall "stuck" reversing, stage 13/22 routing dropped,
      stage 19 routed like 24, fewer air-stunt rules. control_reset_ext()
      for its hold/revstart. Fix targets stay NFM 2's nearest-hoop (stage
      files carry no `setpoint` yet).
- [ ] classictracks.radq stages, 7 cars, unique opponents, all open.

## Phase 3 -- Extended's content

- [ ] Extended's 23 cars (models.radq + stats), car select for 39.
- [ ] Extended stage parser and directives (float, fade, fire, colour
      outlines, special checkpoints, fake walls, igmax, setpoint...),
      old-model renumbering (stagecompat), collision without the sector grid
      (trackgrid).
- [ ] Normal-mode stages (tracks.radq), Premier Tournament.
- [ ] Revised and Recharged cars; cars and stages from the SD card.

## Phase 4 -- RPG / career

- [ ] careermode, levels, perks, bonus stages, beasts, shadows, undead, boss,
      recorded bots, teleports, stage effects, Ogg music, saves.
