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
- [ ] `.radq` archives: zip, 14 of 78 byte-pair swapped (web/ext/radq.js).
- [ ] Per-car live stats: each Mad owns a copy of its car's table (specials,
      tourney and career rewrite them every frame).
- [ ] Limits: 6 wheels, objects per stage 610 -> 1106+, checkpoints 140 ->
      440+, fix points 5 -> 50, cars per race 8 -> 20 (+ per-car arrays).

## Phase 2 -- Classic Mode as Extended plays it

- [ ] Extended's stat tables for the 16 NFM2 cars (engine-analysis.md §1).
- [ ] Intentional physics changes: forca /8000, airborne righting, wall-surf
      +15, bumpy-road gating + bounce 1.35, float handb / double turn,
      recoil cap 3, road scan dy<1000 / groundlevel, MASHEEN classic rules,
      0.76 player handicap without the old stage exemptions.
- [ ] Specials: bar, charging, S button, AI auto-fire, nitroandspecials
      (buffs, freeze, strswap, leech, redstr, 1v1 rule), no-knockback.
- [ ] Specials HUD: Special bar, status lines, buff list, car-list colours,
      outline glow; car-select description.
- [ ] Classic AI changes: no rubber-banding, skiplev, rampp, turntyp,
      setpoint fix targets.
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
