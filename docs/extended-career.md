# Extended v2.8 career (RPG mode): port map

Refs: `web/ext/<file>:line` in nfm-master (the JS transpile of the jar);
Java `decompilation/extended/java-src/` (E). Static reading, 2026-10-07.
Corrections to engine-analysis.md: 40 perks (6 per car, 0-20 points each);
Control's intercept/waitforuser camping is dead code; Medium.effect[10] is
never set; perks 8 and 25-39 are described but never applied.

## State
- Per Madness instance, indexed by car id: `level[39] exp[39] aitssp aiaccsp
  aigripsp aistusp aistrsp aiendsp` (Madness.js:249-254). Player =
  `madness[0].*[sc[0]]`. Also `beast[] multiplier[] shadowcar revive
  fakedest isabot nostunts sendtofloor`.
- xtGraphics: `unlocked[2]` ([1] career, cap 31), `statpoints[39]`,
  `extpoints[39]`, `kills wins killscn[39] winscn[39]`, `statchangers[2]`,
  `carpoints`, `specialstats[39][40][6]`, `statsalc[39][6]` (776),
  `rebsp xbsp xbspratio`, `boncomp[6]`, `maxlevel[31] = {4,7,10,13,16,20,23,
  26,30,33,36,39,42,45,49,52,55,58,62,65,70,74,77,84,90,97,103,111,120,125,125}`,
  `bonstage bonusstage[] hardstage scalelevels nolevels disablexp`,
  `beastopponent[] beastcar[] shadow[] undead[] noarrow[] bossbattle
  stunthealth floor[] lives[]`, `laststage lastcar betalimit(14)`.

## Save (GameSparker.js:3269-3682; port: web/ext/career-save.js)
savedata.radq = zip with ud.txt, byte pairs swapped. Lines: unlocked,
kills/wins/code, changers, usercar, 39 x car(a,level,exp,statpoints,ts,acc,
grip,stu,str,end,killscn,winscn,extpoints+46,chk), bonus, laststage,
newbonus, cpoints, 39 x {special(a,p0..p5,chk), bsp(a,rebsp,xbsp)}.
Checksums not enforced. This port saves its own text file instead.

## Flow (fase)
10 menu -> RPG MODE (xt 14573: careermode, sc[0]=lastcar, stage=laststage)
-> -9 -> 7 car select (carselect 15229; unlock gates 15274-15309; stat spend
15705-15760; sell/shuffle) -> 6476 (resetbeasts, resetshadows, randomno
18144) -> 2 load (bots GS 1750-1790, loadstage, sortcars 12546, beasts GS
1246, grid) -> -69 getstats/airpgstats (10049) -> 205 scouting -> 1 stage
select (13298; extras: bonus stage on 5/11/15/18, hard/scale/no levels,
xp toggle, scouting, change car) -> 5 loadmusic -> 176 -> 6 -> 0 race
(careermode$m every frame, GS 2485) -> stat$m (3803) -> -5 finish (12098)
-> unlocks (12499-12531).

XP: win/race/waste 4030-4062, 4152; checkpoint 5121-5175; waste 5241-5297;
hits Madness.js:1006-1013; stunts 2848-2860; full power 9086-9108.
Level-up 9112-9143: exp -= reqneed(level,car); level++; statpoints +=
4 + level/15; every ai*sp +1. Bonus stat points 5040-5420.
Unlocks: win highest stage -> unlocked[1]++. Cars by stage 12116-12167:
car-select rule (car-7)*2 < unlocked for cars <= 17; (car-11)*3+2 <
unlocked for 18/19; 22 needs unlocked > 30.

## Race setup
nplayers (randomno): 11; 3/17 -> 19; 8 -> 15; 13 -> 16; 11 -> 17 (beaten,
not hard); 20 -> 2; 5 and 18 -> 7; 23 -> 12 hard / 11; 14 -> 6; bonus 3 -> 9,
bonus 4 -> 5. Opponents sortcars 12546 (lineups 12656-12903, forced
13103-13120, bonus 13121-13160). Levels airpgstats 10049-10161:
maxlevel[st-1] - variance + rand(variance) (3; 4 on 5/14/26; 2 on 4/6/24;
1 on 8/10/12/16/18/22/25/29), last car = maxlevel. Stat pools spcalc 10024,
splits 10224-12042, then +(level-1) on all six. Grid GS 1263-1481 (3 wide,
z pitch 760). Beasts 9391, shadows 9616, undead on 11/17/23.

Stage table (careertracks.radq <n>.txt, bonus/1-4.txt):
1-4 basic; 5 Crystal Cavern 7 cars, beast, bot, sunk ground; 6 Ghost Planet
teleports; 7 Matrix green outlines; 8 Carnival 15 cars ground cycle; 9
Burning Abyss bot, flames, grip drain; 10 Galaxy bot, stars; 11 Junkyard
undead vans; 12 Factory outline pulses; 13 Sky Fortress tower floors and
portals; 14 Forest 6 cars repairing checkpoints, bot; 15 Spaceship gravity;
16 Sheer Cold snow, ice grip; 17 Undead 19 cars; 18 Desert 7 cars bot; 19
Glitch World; 20 1v1 vs Radical Racer bot with lives; 21 Hellzone crumbling;
22 Desert Night day/night, shadow; 23 Grassland Fires Titan boss; 24
Underwater water drag; 25-28 plain; 29-31 placeholders.
Bots: Files/Bots/stage{5,9,10,11,13,14,18,20,21}.radq, entries <slot>.txt,
lines up/down/left/right/handb(t) (+ offset(t,set) on 13). Bots.js:91.
Music: stage<=25 stage{n}a.ogg intro then b loop (switch(n) ms in the stage);
26-28 stage26-28.radq (MOD); boss bossbattlea/b.ogg; bonus bonusmusic.

## Madness career hooks
handb/turn + 0.1*(aigripsp-(level-1)) (cap turn 15); player accel boost
min(75, aiaccsp-(level-1)): power x (0.76+0.24*boost/75); stunt duration
from aistusp. nitroandspecials 7001-7051 every frame: topspeed =
swits[2]+aitssp; acelf0 + aiaccsp/10; grip + 0.2*aigripsp; airs +
0.025*aistusp; airc + aistusp; moment + 0.025*aistrsp; maxmag =
healthcalc(health, aiendsp) (6323: health + sp*min(500?, health/20 cap 1250)).
Beast: powerloss x3, radius x2, B model, XP x2.5. Shadow: -50 grip/accel,
dark outline, XP x3. Undead: immortal, power 98, no ranking, flames. Titan:
undead boss damaged by player stunts only.

## Control career code (9,944 lines, all in preform)
Per-stage blocks: clrnce 287-385, stuntf/saftey 687-986, avoid 1025-1185,
attack prob 1802-2344, targets 2510-4216, fix 4257-4342, bully 4679-4861,
checkpoint slowdown 5910-5950 (Contva), scripted routes 6179-8562 (2383),
fix routing 8933-9509, reset 9841-9877.

## Perks (applied ones)
0 ENERGY powerloss, 1 GAMBLER, 2 GREED xp, 3 GETAWAY, 4 CHEAPSHOT, 5 ESCAPE,
6 FEARLESS, 7 PUSHING, 9 RECKLESS, 10 SURVIVAL, 11 RAMPAGE, 12 BRAVERY, 13
AWARENESS, 14 BACKHIT, 15 ARMOUR, 16 WEIGHT, 17 LIFTING, 18 DRAINER, 19
LEAKAGE, 20 RUTHLESS, 21 FRESHNESS, 22 STEROIDS, 23 BERSERK, 24 SAFETY.
Port: core/career_perks.h (each as the original computes it), mad.c and
specials.c through Mad.perks / Specials.perks (NULL outside the career).
Every loop takes the last of the six slots holding the perk with points.
Dead in v2.8 by its own table: LEAKAGE (reads DRAINER's slot; cancels it),
SAFETY (needs the hitter's car to hold SAFETY and the player's FEARLESS in
the same slot -- no two cars do). Car points: only selling earns them
(level / 3; the original pays before bonus stage 4 too, its text says not);
bonus stage 4 makes them usable.

## Bonus stat points
stat$m: a checkpoint rolls winchance[0] = rand*1000+1 (not with noexp)
against (18 + min(clear, 25) + bspbuff) * doublechance * plshelp * GAMBLER,
a waste rolls killchance[0] against 140 * doublechance * chancemod (beast
2.55 / 1.3 bonus, shadow 2.8) * plshelp * GAMBLER * levelboost; under the
soft level cap a hit rolls [1]; its popup pays 4 (<= 50 + 200 * morechance),
2 (<= 300 + 1000 * morechance) or 1 as it first shows (rcestatgain /
wststatgain, the waste's held while its popup is up), into statpoints,
extpoints and statgain. Port: career.c, career_popups_tick.

## Bonus stages
1 (stage 5) Snake Dance 18 laps -> cars 31-33; 2 (11) AI Revenge 15 laps ->
34, then 35 by racing / 36 by wasting; 3 (15) 20 laps -> 37-38; 4 (18) ->
car points. Secret cars 20/21 unreachable (save only).

## HUD
XP bar + "level N" bottom-left (9164-9196), XP popups, level-up animation,
opponent "Level N" (red if > 5 above), boss bar.
