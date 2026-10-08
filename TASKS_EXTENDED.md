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
- **All content reachable.** No `betalimit` / "END OF BETA": every stage
  and car can be won. Unlocking stays as it is -- played for, not open from
  the start (the user, 2026-10-07).
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
      Special (L) fires a full one, the AI fires as soon as it is full;
      nitroandspecials outside career: per-tick stat rebuild from the car's
      base table, self boosts, freeze / strswap / leech / redstr with
      sortpower and randomise, the 1v1 no-attacks rule. NFM_HOOK_SPECIALS=1
      starts every bar full (headless).
- [x] Specials HUD: the Special bar under Power (data/port/special.png from
      Extended's special.GIF + "SPECIAL" in Adventure, tools/bake_special.py),
      the status lines on the left, the player's +/- stat tabs.
- [x] No knockback for Extended's 15/25/38 while active; the car-select
      SPECIAL ATTACK panel; the outline glow (ContO.spec_on/spec, drawn by
      plane_set_outline) cycling through a car's conditions.
- [x] Extended's car list, always up under the Special bar (draw_ext_board):
      ordinals, names in their condition's glow, a bar per car -- damage
      (arrow on cars), power (arrow on track), the special's charge after
      Settings > Controls' Car List Bars (D-Pad Right, its D).
- [x] Extended's far camera (its view 1, medium_watch_far): a fourth view in
      the camera cycle. NFM_HOOK_VIEW=n / NFM_HOOK_LISTBARS=1 headless.
- [x] Classic AI changes (control.c, `xt.extended`): decide_ext() ports
      Extended's whole decision cycle (no rubber-banding, its skiplev, rampp,
      turntyp, mustland, stuntf, trickprf, attack odds, fix and bulistc
      rules); per tick: wall "stuck" reversing, stage 13/22 routing dropped,
      stage 19 routed like 24, fewer air-stunt rules. control_reset_ext()
      for its hold/revstart. Fix targets stay NFM 2's nearest-hoop (stage
      files carry no `setpoint` yet).
- [x] Progression stays: stages and cars unlock by winning, as in NFM 2
      (no betalimit to port: nothing past it is cut off).
- [x] Stages: NFM 2's own files stay (the user wants the 2015 look).
      classictracks.radq is the same tracks from the older NFM 2: no
      decoration or piles, and its AI repair targets marked with
      `setpoint` (13 of 17 stages; stage 16 also moves a fix hoop).
- [x] The setpoint repair targets for the AI: each classictracks setpoint
      is matched by place to the NFM 2 file's route point and becomes the
      AI's fpnt[0]; the 4 stages without one keep NFM 2's nearest hoop.
- [ ] 7 cars and opponents as sortcars already picks them (Extended keeps
      NFM 2's rules); verify against Extended's sortcars for Classic.

## Phase 3 -- Extended's content

- [x] Extended's 23 cars as this port's 16-38: models from ext_models[0-22]
      (CAR_MODEL), stats in car_define_extended (turn/push float; Extended's
      double wheel angle in mad.c), names, specials, car-select bars
      (Extended's Defence table, Control from grip); Free Play offers all 39;
      the custom car moved to 39.
- [x] Extended's model table (ext_stage.c ext_loadbase: 129 models, 6
      wheels, firedam, sfactor 6 for 78-119) and stage loader (ext_loadstage:
      every set/chk/fix/wall directive, old-model renumbering, faded pieces,
      glow lines, fire), limits (2000 checkpoints, 50 fixes, 20000 trackers,
      1600 objects), coverage sector grid for collision. All 27 tracks, 17
      classic, 5 match and 35 career stages load (ext_stage_test).
      Headless: NFM_EXT_STAGE=tracks:1.
- [ ] specialchk (checkpoint that repairs) and the floating checkpoint height
      window in mad.c; Extended's fadefrom 12000 left out (C fog kept).
- [x] Extended's normal mode (docs/extended-normal-mode.md): the game-mode
      menu's fourth row "Extended" (data/port/extended_label.png); Free Play's
      flow with ext_normal: all 39 cars, stages 1-28 of tracks.radq with
      their names and previews, unlocked by winning (ext_progress.txt), 11
      cars from Extended's sortcars (ext_mode.c), its music (stageN.radq with
      its loadMod numbers), "Stage N is now unlocked!" / "Stage N
      Completed!". The AI knows the mode (control.c: classic-only branches
      off, Extended's own stage numbers, the tournament's per-match tuning).
      Headless: NFM_EXT_NORMAL=1 NFM_STAGE_NUM=n (NFM_PTMATCH=m).
- [x] Premier Tournament (ext_pt.c): stage 26 opens a rules screen per match,
      races matchtracks 26m1-5 with every car the match's, tourney()'s rules
      (revives, no damage, eliminations, no specials), the drivers' names in
      the car list, the scoreboard, points after each match, the champion
      after five; taking it opens stage 27. v2.8's dead ends are mended
      (kills score, match 5 eliminates the last car, it can end).
- [x] New cars (new_cars.c), Free Play only, numbered 40 on: Revised and
      Recharged's ten with numbers (Bugatti Veyron, Lightning Rod, The
      Phantom carry stat(); seven take NFM World's numbers as raw
      "Recharged stats", web carstore.RR_STATS), then every .rad in the save
      folder's cars/ (sdmc:/switch/nfm-extended/cars). car_define_loadstat
      reads raw stat lines (web readRawStats): a car with them needs no
      stat(), with raw maxmag no physics(). extspecial(n) borrows Extended
      car n's special and AI quirks (car_identity), by class when missing;
      exthealth/extdamage scale health and damage. R&R's own models skip the
      Car Maker's load checks (ROCKET M A S H E E N's wheels sit at 144 >
      140); a player's .rad must pass them. R&R's other 15 have no numbers
      anywhere and stay out, as in the web port.
- [ ] Stages from the SD card.

## Phase 4 -- RPG / career

Map of the original: docs/extended-career.md.

- [x] Milestone 1, a career you can play through (2026-10-07): the game-mode
      menu's fifth row "RPG Mode" (data/port/career_label.png); normal mode's
      flow on careertracks.radq 1-31; career.txt beside the progress file
      (career.c, its own text format, written aside and renamed); the car
      locks (stage wins, bonus prizes); per car level, experience and stat
      points, spent in the car select's panel (the Special button); the
      field (career_lineup.c: randomno, sortcars, beasts, sortshadows, the
      original's re-roll over loads) and every opponent's level and points
      (career_stats.c: airpgstats whole), both tested against the JS
      transpile; each car's points into its stats (career_apply_stats, per
      racing slot, specials rebuild from them); the player's power factor;
      experience from checkpoints, wastes, stunts, full power and the win,
      level-ups, the loss on the newest stage given back; the HUD's level
      bar; winning the newest stage opens the next; Ogg music (stages 1-25)
      and modules (26-28). Headless: NFM_CAREER=1 NFM_STAGE_NUM=n
      NFM_CAR_INDEX=c (a career.txt with unlocked >= n), NFM_CAREER_PANEL=1.
- [x] Beasts (B model, x3 powerloss, x2 collision radius), shadows
      (see-through at shadowtrans 80, no outlines), undead (wrecked once,
      then immortal at full power, no special, out of the ranking).
- [ ] The Titan boss on 23; the undead's per-stage scripts (17's late
      entry and targeting, 11's wrecks turning undead); the name prefixes
      ("Beast ", "Shadow ", "Undead ") in the car list; the start ghosting.
- [ ] Stage effects and gimmicks per stage (Medium.effect, water, snow,
      floors and teleports on 13, lives on 20, crumbling 21, gravity 15).
- [ ] Career AI (Control's per-stage branches) and recorded bots.
- [ ] Bonus stages 1-4 and their prizes, scouting, perks (car points),
      bonus stat point rolls (winchance/killchance), stat changers / sell,
      hard / scale / no levels, opponent levels on the HUD, start grids.
