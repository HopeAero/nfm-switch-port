# Extended v2.8 normal mode — spec for the port

From the decompiled source (nfm-master/decompilation/extended/java-src/;
XT = xtGraphics.java, GS = GameSparker.java). Normal mode = neither
classicmode nor careermode; its menu entry ("UNAVAILABLE!") does nothing in
v2.8 but the code works. Port decisions are marked **Port:**.

## Key facts

- Entering: as Classic's menu option (XT 15211-15224) with both flags false:
  justcs -1, sc[0] = lastcar, stage = laststage, fase -9 (car select).
- **11 cars** per race (randomno XT 18964-18965); Classic is 7.
- Premier Tournament (stage 26) never reaches finish(): unlocked[0] can't
  pass 26, matches 1 and 4 never end (players never gain points:
  ptscore1fase never set), nothing ends the tournament after match 5
  (soft-lock). **Port:** fix these (see §4).
- **No car locks** in normal mode: all 39 pickable; navigation needs its own
  limits 0..38.

## 1. Progression

- unlocked = {1,1} (XT 14505); normal uses unlocked[0]. Save line
  `unlocked(u0,u1,checksum)` (GS 3773, read 3500-3504/3684-3688).
- finish() on Enter (XT 12648-12672): if winner && stage == unlocked[0] &&
  != 31 → unlocked[0]++, stage = +1. Clamp ≤31. Enter → main menu.
- Win: clear == nlaps*nsp && pos 0 (XT 3824-3844), or wasted == nplayers-1
  (3693-3717). Loss: another car finishes first, or player wasted
  (cntwis == 8). nlaps(-1) stages (27, PT 1 and 5) only by wasting.
- Screens: re-win "Stage N Completed!"; new win "Stage N+1 is now
  unlocked!" + car showcase (cosmetic) on stage 2→Comet(8), 4→Das Cop(9),
  6→Hellfire(10), 8→Old Van(11), 10→Redspeed(12), 12→Stampede(13),
  14→Skyrider(14), 16→DR Chaos(15), 18→Bounty Hunter(16), 20→Radical
  Racer(17), 23→Titan(18), 26→Deity(19); skipped if sc[0] is that car.
  Loss "Failed to complete Stage N!".
- 28 is treated as last (sortcars 12711, NEXT hidden at 28) but a win on 28
  would go to 29 (no file). **Port:** cap at 28, win on 28 = completion.
- Stage select: stages 1..unlocked[0]; Right past it → cantgo "This stage
  will be unlocked when stage {mx} is complete!" / "[ Stage {mx+1} Locked ]"
  (XT 1276-1293). Title "Stage N:   name"; 26 → "THE PREMIER TOURNAMENT"
  (dontdisplay). Drop betalimit (14) everywhere.
- Car select: all 39 (0-38), no locks (notunlocked false, XT 15964).

## 2. Opponents: sortcars(i) (XT 12707-13342)

A. Newest stage (unlocked[0] == i && != 28): boss bestcar = 7 + (i+1)/2
   (20/21 → 22); sc[10] = bestcar. Slots 1..9: roll floor(rand*bestcar),
   reject duplicates of any slot 0..10 unless repeats allowed or stage in
   {6,7,8,11,12,13,16,17,19,23,24}; repeats disallowed when nplayers <=
   (i+17)/2 && <= 18 (stage ≥ 5 with 11 players); reject 13 (Stampede)
   unless i is 11/12 or slot is 1/2.
B. Other stages: bestcar2 = 7 + (i+1)/2 (no cap); if sc[0] != bestcar2:
   sc[10] = bestcar2, fill 1..9, else fill 1..10. Roll floor(rand *
   (bestunlocked+1)), bestunlocked = 7 + (unlocked[0]+1)/2; reject
   duplicates; then reject with probability f = proba[sc] (+ (i-sc-4)/10 if
   i-sc > 4 && i != 28, cap 0.9; i == 16: f = max(f, 0.9)).
   proba = {.5,.5,.4,.3,.3,.4,.3,.3,.3,.1,.1,.5,.1,0,0,0,0,.1,.1,.5,.85,.85,
   0,.5,.5,.4,.3,.5,.4,.3,.3,.3,.1,.1,.5,.1,0,0,0} (XT 14428).
C. PT (XT 13254-13280): all 11 slots incl. the player: match 1 Bounty
   Hunter(16), 2 Mighty Eight(35), 3 Nimi(27), 4 DR Chaos(15), 5 Old Van(11)
   (Extended numbers).

## 3. Music

music/stage{i}.radq per stage, loadMod(amp, rate, tempo):
1 320/8000/125, 2 260/7200, 3 230/8000, 4 240/8000, 5 282/7800, 6 320/7600,
7 300/7500, 8 270/7900, 9 330/7900, 10 352/7300, 11 480/7900, 12 290/7900,
13 225/7600/137, 14 400/8000, 15 220/8000, 16 261/8000, 17 310/7600,
18 310/7600, 19 400/7600, 20 310/7600, 21 230/7600, 22 280/8000,
23 375/7600, 24 310/7600, 25 300/7600, 26 300/7600, 27 305/7600/136,
28 250/7600/135 (tempo 125 unless shown). menu.radq 345/7900/125 (main
menu), cars.radq 200/7900/125 (car select), stages.radq 135/7800/125
(stage select). switch(n) unused in normal mode. PT races load
stage61/74/67/73/35 which don't exist (silent). **Port:** play stage26.radq
or a fitting track during PT races.

## 4. Premier Tournament

Stage 26 = 5 matches from matchtracks.radq "26m1".."26m5". ptstart (fase
49, XT 928-1128): opponents list ptplayers[1..10] = Motion, Redline, Crash,
Swift, Omega, Olsie820, RadicalRacer, Kaffeinated, InsanElite, Grimjow;
slots 8-9 "CONTENDER", 10 "FAVOURITE"; random IN FORM (2) / OUT OF FORM
(2); player picks a name (DragShot, ToaZuka, Velocity, KRC); rules screen.
tourney() each race frame (GS 2562-2565); AI never fixes (Control
4231-4234).

| Match | Track | Car | Rules | Ends | Points |
|---|---|---|---|---|---|
| 1 | 26m1 Centrifugal Rush, Under Water? (nlaps -1) | Bounty Hunter | -1 per wasting; revive after 40 ticks (force-fix); specials off; order (score+1)*1e6+100000-hitmag+im | first to 7 (**never fires**) | 10 - pos |
| 2 | 26m2 The Fast & The Furious + The Radical (12 laps) | Mighty Eight | no damage for non-eliminated; each even leader clear ≥2 eliminates pos 10-wasted; specials off | player out or wasted 10 | 10 - pos |
| 3 | 26m3 Rolling with the Big Boys (2 laps) | Nimi | no damage, specials off | first finisher | 10 - pos |
| 4 | 26m4 The Mad Party | DR Chaos | as 1, specials on | first to 8 (**never fires**) | 20 - 2*pos |
| 5 | 26m5 The Garden of the Van (nlaps -1) | Old Van | -1 per death, revive 40; ptimer 1000 counting down; at <0 eliminate position 10-neliminated (**bug: only the player**) | neliminated 10 or player out | 20 - 2*pos |

Sources: match 1 XT 5636-5764/1107-1121; 2 5765-5817/1153-1177; 3
5818-5850/1185-1206; 4 5674-5764/1214-1236; 5 5851-5970/1244-1266. "MATCH
OVER / X WON!", Enter adds points (XT 3772-3791) → scoreshow (fase 51,
XT 695-926) → ptmatch++ → fase 49. stataffect in-form bonus is zeroed every
frame by tourney (no effect).

**Port:** award +1 per wasting so 1 and 4 can end; eliminate the right car
in 5; after match 5 show the final standings and, if the player has the
most points, count stage 26 as won (unlocked[0] → 27).

## 5. Other rules

No per-stage specials outside PT; nplayers 11; wasting win = racing win;
sortpower diffmod 50; stats × (1 + stataffect).

## 6. Stage names (tracks.radq 1-25, 27, 28; 26 = matchtracks)

1 Introductory Stage (3 laps), 2 Awesomeness Begins (3), 3 Consequences (4),
4 The Chase (4), 5 Peaceful Realm (4), 6 Drag Race of Epicness (1), 7 The
Garden of the Van (10), 8 Van's Revenge (3), 9 Snowy Raceway (2), 10 Grand
Ark's Challenge (2), 11 Peer Pressure (3), 12 Killer Grocery Store (3), 13
Lorry's Heaven (2), 14 The Gun Run (1), 15 Dances with Monsters (3), 16 Four
Dimensional Vertigo (2), 17 Race on Sunrise II (1), 18 Peaceful Sunset (2),
19 On the Moon II (1), 20 Race of the Century (1), 21 The Lair of Hell (3),
22 Serving the Boy (2), 23 Stranger Danger (2), 24 Final Showdown (2), 25
The "Finale" (7), 26 Premier Tournament, 27 The Warzone (wasting only), 28
A Competitive Ending (2).
