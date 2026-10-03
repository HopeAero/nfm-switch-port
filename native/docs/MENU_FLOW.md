# NFM Menu Flow — 1:1 Spec from Original Java

**Source of truth:** `decompilation/java-src/xtGraphics.java` + `decompilation/java-src/GameSparker.java` + `data/images.zip` (139 assets).

**Purpose:** This document is the read-only reference every menu screen in `native/platform/common/game.c` (shared by every platform target since Part 18, see `../TASKS_NATIVE.md`) is built from. Coordinates, colors, animations, transitions — all traced back to specific line numbers in the Java source so nothing is invented.

**Coordinate space:** everything below is in 800×450 game-space pixels, matching the Java's fixed canvas size. `(0,0)` is top-left. When a screen scales to a different physical resolution (Vita is 960×544), the whole 800×450 frame scales uniformly with letterbox bars.

---

## 1. Asset name → filename map

Extracted from `xtGraphics.java` line 776+, the `loadimages()` zip reader block. Every menu asset is loaded once at boot.

| Java field | File in `data/images.zip` | Size | Notes |
|---|---|---|---|
| `bgmain` | `bgmain.jpg` | 670×400 | main menu scrolling backdrop |
| `bggo` | `bggo.jpg` | 800×450 | game-over / pause backdrop |
| `carsbg` | `cars.gif` | 670×400 | car-select photo backdrop |
| `br` | `br.png` | 670×400 | stage-select backdrop |
| `logomadbg` | `logomadbg.jpg` | 670×400 | main-menu semi-transparent watermark |
| `logomadnes` | `logomad.png` | 367×41 | gold "MADNESS" wordmark |
| `logocars` | `logocars.png` | 638×243 | top-left "CARS" logo strip |
| `nfm` | `nfm.gif` | 184×17 | "NEED FOR" small wordmark |
| `mdness` | `madness.gif` | 231×46 | large "MADNESS!" (green box) |
| `racing` | `racing.gif` | 234×117 | "RACING" wordmark |
| `dude[0..2]` | `d1.png`, `d2.png`, `d3.png` | ~40×40 each | face sprites (main-menu blinker + countdown) |
| `opback` | `opback.png` | 300×177 | brown "pill" behind option list |
| `opti` | `options.png` | 211×105 | main-menu 4-option label block |
| `opti2` | `options2.png` | 107×100 | gamemode sub-menu 4-option label block |
| `byrd` | `byrd.png` | ~ | byline / signature (bottom-left) |
| `nfmcoms` | `nfmcoms.png` | 161×8 | www.NFM.com small footer |
| `nfmcom` | `nfmcom.gif` | 419×21 | www.NFM.com big footer |
| `radicalplay` | `radicalplay.gif` | 375×59 | boot-splash logo |
| `rpro` | `rpro.gif` | ~ | radicalplay pro (unused-ish) |
| `selectcar` | `selectcar.gif` | 158×16 | "SELECT YOUR CAR" caption |
| `select` | `select.gif` | 125×18 | "SELECT" caption (stage picker) |
| `stunts` | `stunts.png` | 464×110 | 4 cars-with-arrows (instructions screen) |
| `chil` | `chil.gif` | 475×118 | double-outline instructions frame |
| `back[0]`/`back[1]` | `back.gif`,`pbc*.gif` | 60×21 | "< BACK" button (idle/pressed) |
| `next[0]`/`next[1]` | `next.gif` | 60×21 | "NEXT >" button |
| `contin[0]` | `continue.gif` | 90×23 | "CONTINUE" button |
| `play` | `play.gif` | 81×18 | "PLAY >" button |
| `cancel` | `cancel.gif` | 78×18 | "CANCEL" button |
| `redy` | `ready.gif` | 270×18 | "READY FOR MULTIPLAYER?!" |
| `exit` | `exit.gif` | 52×15 | "EXIT" small button |
| `exitgame` | `exitgame.gif` | 144×22 | "EXIT GAME" button |
| `star[0..1]` | `start1.gif`,`start2.gif` | ~ | GO! star burst (countdown) |
| `cntdn[0..3]` | `0c.gif`..`3c.gif` | ~ | 3-2-1 countdown digits |
| `sarrow` | `arrow.gif` | ~ | HUD checkpoint arrow (already ported) |
| `arrows` | `arrows.gif` | ~ | instructions arrow-key illustration |
| `pgate` | `pgate.gif` | ~ | checkpoint gate marker |
| `paused` | `paused.gif` | ~ | "PAUSED" banner |
| `gameov` | `gameov.gif` | ~ | "GAME OVER" banner |
| `oyoulost`→`youlost` | `youlost.gif` | ~ | "YOU LOST" post-race |
| `oyouwon`→`youwon` | `youwon.gif` | ~ | "YOU WON" post-race |
| `oyourwasted`→`yourwasted` | `yourwasted.gif` | ~ | "YOU'RE WASTED" post-race |
| `oyouwastedem`→`youwastedem` | `youwastedem.gif` | ~ | "YOU WASTED THEM" post-race |
| `owgame`→`wgame` | `wgame.gif` | ~ | won-game panel |
| `ogamefinished`→`gamefinished` | `gamefinished.gif` | ~ | "GAME FINISHED" banner |
| `ogameh`→`gameh` | `gameh.gif` | ~ | game header |
| `congrd` | `congrad.gif` | ~ | "CONGRATULATIONS" |
| `mload` | `mload.gif` | ~ | music-loading spinner |
| `oloadingmusic`→`loadingmusic` | `loadingmusic.gif` | ~ | "LOADING MUSIC" banner |
| `sdets` | `sdets.gif` | 128×18 | "SEND DETAILS" (multiplayer only) |
| `ory` | `ory.gif` | 32×18 | "OR" separator |
| `sts` | `sts.gif` | 48×12 | "STATUS" tab |
| `gac` | `gac.gif` | 98×12 | "< GAME CARS" tab |
| `yac` | `yac.gif` | 153×12 | "YOUR ACCOUNT CARS" caption |
| `ccar` | `ccar.gif` | 107×15 | "CHANGE CAR" |
| `ycmc` | `ycmc.gif` | 126×13 | "YOUR CAR MAKER CARS" caption |
| `cmc` | `cmc.gif` | 126×13 | "CAR MAKER CARS" |
| `cnmc` | `cnmc.gif` | 422×16 | "MAIN / PROFILE / INTERACTION / CHAT" tabs |
| `gmc` | `gmc.gif` | 42×12 | "GAME" mini-caption |
| `stg` | `stg.gif` | 47×13 | "STAGE" caption |
| `crd` | `crd.gif` | 89×12 | "CAR DISPLAY" |
| `players` | `players.gif` | 135×15 | "PLAYERS ONLINE" |
| `pon` | `pon.gif` | 275×18 | "PLAYERS ONLINE:" (long) |
| `chat` | `chat.gif` | 50×15 | "CHAT" |
| `cgame` | `cgame.gif` | 122×15 | "CREATE GAME" |
| `lanm` | `lanm.gif` | 215×23 | "LAN MULTIPLAYER" |
| `dome` | `dome.gif` | ~ | "DOME" mode caption |
| `login` | `login.gif` | 65×18 | "LOGIN" |
| `logout` | `logout.gif` | 64×15 | "LOGOUT" |
| `register` | `register.gif` | 96×18 | "REGISTER" |
| `notreg`→`ntrg` | `notreg.gif` | 219×15 | "NOT REGISTERED YET?" |
| `upgrade` | `upgrade.gif` | 83×18 | "UPGRADE" |
| `top20s` | `top20s.gif` | 73×15 | "TOP 20" |
| `change` | `change.gif` | 85×18 | "CHANGE" |
| `plus` | `plus.gif` | 25×25 | "+" icon |
| `space` | `space.gif` | ~ | "SPACE" key icon |
| `kv/kn/ks/kx/kz/km` | `k?.gif` | 29×33 each | keyboard keys V/N/S/X/Z/M |
| `kenter` | `kenter.gif` | 97×33 | Enter key icon |
| `bcl[0..1]`/`bcr[0..1]`/`bc[0..1]` | `bcl.gif`+`pbcl.gif` etc | ~ | chat bubble corners (idle/pressed) |
| `bob`,`bot`,`bol`,`bolp`,`bor`,`borp`,`bols`,`bolps`,`bors`,`borps` | corresponding `.gif`s | ~ | more chat bubble parts |
| `myfr`,`mycl`,`myc`,`roomp` | `.gif`s | ~ | player list / clan / room |
| `pln` | `pln.gif` | ~ | player name label |
| `pls` | `pls.gif` | ~ | players list |
| `brt`→`brit` | `brit.gif` | 9×9 | flag/star icon |
| `arn` | `arn.gif` | ~ | arrow-narrow icon |
| `asu` | `asu.gif` | ~ | arrow-scroll-up |
| `asd` | `asd.gif` | ~ | arrow-scroll-down |
| `statb`,`statbo` | `.gif` | ~ | stats box (in-race) |
| `fixhoop` | `fixhoop.png` | ~ | 3D fix ring (in-race, floor decoration) |
| `dmg`,`pwr`,`pos`,`sped`,`was`,`lap` | `damage.gif` etc | already ported | HUD panels |
| `rank[0..7]` | `1.gif`..`8.gif` | already ported | position badges |

---

## 2. `fase` (screen state) transition graph

Every ticked frame `GameSparker.run()` dispatches on `xtGraphics.fase`. Below is every fase the code checks, in-order flow left→right:

```
BOOT
 │
 ▼
fase 111  ── click / mouses==1 ──▶  fase 9     ── ~76 frames ──▶  fase 10 (MAIN MENU)
"clicknow"                          "rad"                          maini()
                                    Radicalplay
                                    splash
                                    (rad(n) in xt.java:1575)

fase 10 (MAIN MENU) — maini(), xt.java:4309
 │
 ├── opselect 0 (Play Game)        ──▶  fase 102 (GAMEMODE)
 ├── opselect 1 (Play Multiplayer) ──▶  fase -9 (net login lobby) — OUT OF SCOPE
 ├── opselect 2 (Instructions)     ──▶  fase 11 (inst()) 
 └── opselect 3 (Credits)          ──▶  fase 8 (credits())

fase 11 (INSTRUCTIONS) — inst()
 └── ENTER/BACK ──▶ back to oldfase (10 or 102)

fase 8 (CREDITS) — credits()
 └── auto-cycles rad() splash then loops back to fase 10

fase 102 (GAMEMODE SUBMENU) — maini2(), xt.java:4504
 │
 ├── opselect 0 (NFM 1)          ──▶  gmode=1, fase -9
 ├── opselect 1 (NFM 2)          ──▶  gmode=2, fase -9
 ├── opselect 2 (Multiplayer)    ──▶  gmode=3, fase -9 (net) — OUT OF SCOPE
 └── opselect 3 (Free Play)      ──▶  gmode=0, fase -9
     (Free Play is our default; others select stage progression)

fase -9  ── memory setup, then ──▶  fase 7 (CAR SELECT)
        inishcarselect(array) 
        called (xt.java:4844)

fase 7 (CAR SELECT) — carselect(), xt.java:5080
 │  (cfase sub-states: 0=own car, 3=car-maker cars, 5=action, 
 │                     7=deleting, 8=confirm delete, 9=action done, 
 │                     10=customize, 11=Top-20, 100/101=account cars)
 │
 ├── PLAY button       ──▶  fase 3 (STAGE PICKER)
 └── BACK              ──▶  fase 102 (GAMEMODE) or fase 10 (MAIN MENU)

fase 3 (STAGE-SELECT INIT) — inishstageselect(checkPoints)
 └── ──▶ fase 5 (LOAD MUSIC → shows stageselect())

fase 5 (LOAD MUSIC)  — loadmusic(stage, trackname, trackvol)
 └── ──▶ fase 6 (MUSIC COMPILATION) — musicomp(stage, control)
                                       stageselect() runs inside musicomp

fase 6 (STAGE SELECT + MUSIC COMPILE) — musicomp()
 │  stageselect() draws the actual picker over
 │  the music-compile progress
 │  Uses AWT widgets (app.sgame, app.snfm1, app.snfm2, app.mstgs) 
 │  we need to replace with our own list widget
 │
 ├── PLAY / ENTER              ──▶  fase 4 (cantgo check) or 2 (loading)
 └── BACK                      ──▶  fase 7 (CAR SELECT)

fase 4 (CANTGO)   — cantgo(control) 
 └── shows "CAN'T PLAY THIS STAGE" and returns

fase 2 (LOADING)  — loadingstage(stage, true) + loadstage()
 └── ──▶ fase 1 (STAGE PREVIEW)

fase 1 (STAGE PREVIEW / TRACKBG)  — trackbg(false) + aroundtrack camera
 └── ──▶ fase 0 (RACING) after starcnt=130 countdown

fase 0 (RACING) — the tick we already have
 │
 └── ENTER/ESC to menu ──▶ fase -5 (finish/post-race)

fase -5 (POST-RACE) — finish() ── ENTER ──▶ fase 3 (STAGE PICKER)
                                         or fase 10 (MAIN MENU)
```

**Fases OUT OF SCOPE for this port** (all internet/LAN/lobby, `multion != 0`):
- `-1, -2, -3, -4, -6, -7, -8` — network states (login/logout/lobby/room)
- `21, 22, 23, 24, 1177, 7001` — multiplayer variants
- The `nettitle()`, `openm`, `logged`, `app.tnick/tpass` machinery — bureaucratic login UI

**Fases IN SCOPE** (single-player, no netplay):
- `111` (boot click)
- `9` (radicalplay splash — 76 frames)
- `10` (main menu — 4 options)
- `11` (instructions)
- `8` (credits)
- `102` (gamemode submenu — 4 options)
- `7` (car select — with 3D preview)
- `6` (stage select — with 3D overhead preview)
- `2` (loading)
- `1` (stage preview / trackbg)
- `0` (racing)
- `-5` (post-race)

That's **12 screens** minimum for a 1:1 experience.

---

## 3. Screen-by-screen 1:1 spec

### 3.1 Boot splash (`fase == 111` and `9`)

`fase == 111` — one frame that runs `clicknow()` until player clicks, then advances to fase 9.

**In our port**: skip this (no click-to-start needed on Vita — the user already picked the game from LiveArea). Jump straight to fase 9 or 10.

`fase == 9` — `rad(n)` for n=0..75 (~4 seconds at 53ms/tick).

**Layout** (xt.java:1575):
- `mainbg(-101)`-ish (or just black `fillRect(65,25,670,400)`)
- **top/bottom black bars**: `fillRect(65,135,670,59)` (horizontal band)
- `radicalplay` image (375×59) sliding in from right: `drawImage(radicalplay, x, 135)` where x scrolls from 735 to 212, then holds
- When settled: `powerup.wav` sound + `drawcs(185+jitter, "Radicalplay.com", 112,120,143, mode=3)` in `Arial Bold 11`
- Under that: `drawcs(215/217, "And we are never going to find the new unless we get a little crazy...", 112,120,143 alt 150,150,150, mode=3)` alternating aflk
- `rpro` (radicalplay pro) at (275, 265)
- Black borders: `(0,0,65,450)`, `(735,0,65,450)`, `(65,0,670,25)`, `(65,425,670,25)` — 65px letterbox

After 76 frames → `fase = 10`.

### 3.2 Main menu (`fase == 10`) — `maini()` xt.java:4309

**Draw order** (top to bottom in code):
```
1. mainbg(1)                          // background - bgmain.jpg tiling vertically with orange tint (255,176,67)
2. drawImage(logomadbg, 65, 25) at alpha=0.6   // 670×400 backdrop
3. drawImage(logomadnes, 233, 186)             // gold "MADNESS" wordmark 367×41
4. drawImage(dude[0], 351+jitter, 28+jitter) at alpha=flkat animation
   // face blinks: flkat 0→800 (flkat/800 alpha up to 0.2),
   //              flkat 200→400: (400-flkat)/1000 alpha down
   // gxdu/gydu jitter: (5.0 - 11.0*random()) reset every 2 frames
5. drawImage(logocars, 66, 33)         // 638×243 "CARS" logo strip
6. drawImage(opback, 247, 237)         // 300×177 brown pill
7. Focus/highlight rectangles for opselect (roundRect strokes, colors below)
8. drawImage(opti, 294, 265)           // 211×105 option labels
9. drawImage(byrd, 72, 410)            // byline bottom-left
10. drawImage(nfmcoms, 567, 410)       // www.NFM.com bottom-right
```

**Option rectangles** (roundRect stroke, radius=(7,20)):
| # | Label | Rect (x,y,w,h) | aflk color | !aflk color | shaded fill |
|---|---|---|---|---|---|
| 0 | Play Game | (343, 261, 110, 22) | (200,200,0) | (255,128,0) | (140,70,0) |
| 1 | Play Multiplayer Game | (288, 291, 221, 22) | (200,191,0) | (255,95,0) | (140,70,0) |
| 2 | Game Instructions | (301, 321, 196, 22) | (200,128,0) | (255,128,0) | (140,70,0) |
| 3 | Credits | (357, 351, 85, 22) | (200,0,0) | (255,128,0) | (140,70,0) |

Non-selected rectangles: `(0,0,0)` black stroke.

**Extra `muhi` blink** on option 1 (Multiplayer) area: `fillRoundRect(335,293,114,19,7,20)` in (140,70,0) when `muhi < 0`. `muhi--` per frame, resets to 50 when < -5. This makes the Multiplayer row occasionally flash brown as attention-getter.

**Input** (`ctachm()` + `if (this.fase == 10)` mouse in xt.java:7433):
- Mouse hover sets `opselect` to hovered option
- Click sets `shaded = true` + `control.enter`
- Keyboard: `control.up`/`down` cycles opselect 0↔3 (in `maini()` body xt.java:4354-4367)

**On ENTER/handbrake** (xt.java:4449):
- 0 (Play Game) → check `unlocked[0]==11` (skips to opselect 1 or 2 in some cases), then `fase = 102` (with `oldfase = 102`, `firstime` handling)
- 1 (Multiplayer) → `mtop = true`, `multion = 1`, `gmode = 0`, `fase = -9` (or 11 first time)
- 2 (Instructions) → `oldfase = 10, fase = 11`
- 3 (Credits) → `fase = 8`

### 3.3 Gamemode submenu (`fase == 102`) — `maini2()` xt.java:4504

**Same background/layout as maini** (bg, logomadbg, logomadnes, dude, logocars, opback), but `dropf` variable adds vertical offset to positions 0-2 (some kind of dropdown animation).

**Option rectangles** (dropf=0 default):
| # | Label | Rect (x, 262+dropf/etc, w, h) | aflk | !aflk |
|---|---|---|---|---|
| 0 | NFM 1 | (358, 262+dropf, 82, 22) | (200,64,0) | (255,128,0) |
| 1 | NFM 2 | (358, 290+dropf, 82, 22) | (200,64,0) | (255,95,0) |
| 2 | Multiplayer | (333, 318+dropf, 132, 22) | (200,255,0) | (255,128,0) |
| 3 | Free Play | (348, 346, 102, 22) | (200,64,0) | (255,128,0) |

`drawImage(opti2, 346, 265+dropf)` for the option labels (107×100).

When `dropf != 0`: extra `fillRect(357,365,87,15)` in dark brown `(58,30,8)` to hide the 4th option (Free Play). This is used when the "NFM 1" progression state gates access to Free Play.

**On ENTER** (xt.java:4638):
- 0 → `multion=0, clangame=0, gmode=1, fase=-9` (NFM 1)
- 1 → `multion=0, clangame=0, gmode=2, fase=-9` (NFM 2)
- 2 → multiplayer flow (OUT OF SCOPE)
- 3 → `multion=0, clangame=0, gmode=0, fase=-9` (Free Play — our current default)

### 3.4 Instructions (`fase == 11`) — `inst()`

Uses:
- `chil` (475×118) at (167, 295) as bottom outline
- `stunts` (464×110) at (105, 175) — 4 car stunt icons with arrows
- Keyboard-key icons: `arrows` (WASD?), `kv/kn/ks/kx/kz/km/kenter` (view keys)
- Text captions via `drawcs()` describing controls
- `back` and `contin` buttons

**Full port TBD** — need to read `inst()` method in detail (not yet extracted).

### 3.5 Credits (`fase == 8`) — `credits()` xt.java:1626

Runs `rad(n)` splash for 100 frames, then shows a static credits screen with `nfmcom` (419×21 URL) prominently displayed and various contributor text. Falls back to main menu on ENTER.

**Full port TBD** — content is simple text list.

### 3.6 Car select (`fase == 7`) — `carselect()` xt.java:5080

**Setup** (`inishcarselect(array)` xt.java:4844): initializes the spinning-car preview state and populates `sc[]` (each player's car choice; `sc[0]` is us).

**Background**:
- Black letterbox: 65px borders top/bottom/left/right
- `carsbg` (`cars.gif` 670×400) at (65, 25) — this is the tilted stormy sky background
- Alternative when multion/testdrive: `carsbgc` (also cars.gif variant with different tint)
- `selectcar` (158×16) at (321, 37) — "SELECT YOUR CAR" caption
- Optional captions above: `ycmc` at (337, 58) or `yac` at (323, 58) depending on `cd.lastload`

**3D car** (already ported as `draw_car_preview`):
- Camera at `xtGraphics.java:5058` constants (already documented in our code)
- `array[sc[0]].d(rd)` — the actual car ContO, spinning

**Cars-select sub-states** (`cfase`):
- `0`: default (own account car), Free Play / NFM
- `3`: "car-maker cars" mode (custom `.rad` files)
- `5`: action in progress (loading/saving)
- `7`: deleting car
- `8`: confirm-delete dialog
- `9`: action done
- `10`: customize (color picker via `color.gif` at some position)
- `11`: "Top 20 cars" browser
- `100`, `101`: other player accounts' cars

For our port, `cfase == 0` covers 90% of use.

**Extra widgets in `cfase == 0` / Free Play**:
- Car-name text via `Arial Bold 13` centered
- Class label ("A", "B", "C") via `class.gif` swap
- Stat bars (6 bars from `stunts.png` + drawn rectangles) — see `carStats()` in web/preview.js for the formulas (already documented there)
- `back` at (95, 275) and `next` at (645, 275) — arrows
- `contin` ("continue" = PLAY) at (355, 385)

**Input** (`fase == 7` mouse in xt.java:7357):
- Hover on `next[0]` → `pnext = 1`, click → `control.right = true`
- Hover on `back[0]` → `pback = 1`, click → `control.left = true`
- Hover on `contin[0]` (if !openm) → `pcontin = 1`, click → `control.enter = true`

**Right/Left cycle** `sc[0]` through available cars (0-15 base + 16+ custom).

**On ENTER** → transition to `fase = 3` (stage picker init).

### 3.7 Stage select (`fase == 6`, via `musicomp` → `stageselect`) — xt.java:2024

Runs INSIDE `musicomp()` (fase 6) which also compiles music while the picker is shown.

**Background**:
- Black letterbox 65px borders (same as everywhere)
- `br` (`br.png` 670×400) at (65, 25) — this is a special stage-select backdrop
- `select` (`select.gif` 125×18) at (338, 35) — "SELECT" caption
- `back[pback]` at (115, 135) if `stage != 1` (or `stage != 11 && gmode != 2`)
- `next[pnext]` at (625, 135) if `stage != 27`

**Text** (Arial Bold 13):
- Stage name centered at y=132 via `drawcs()`
  - Colors alternate `(240,240,240)` / `(176,176,176)` (aflk gray blink)
  - If `top20 >= 3`: prefix "N#{nto}  " on name
- If `stage == -2` (custom stage): "Created by: {maker}" or "Created by You" at (70, 115) in `(255,176,85)` Arial Bold 11

**Widgets** (AWT native — needs custom replacement):
- `sgame` (game-type dropdown): NFM1 / NFM2 / My Stages / Top20 / Class Top20 / SM Stages
- `snfm1` (NFM1 stage list)
- `snfm2` (NFM2 stage list)  
- `mstgs` (custom stage list)
- Positions computed dynamically to center the sgame+snfm* pair at x=400, y=62
- Widget size 131×22 and 338×22

For our port, replace the AWT dropdowns with a keyboard-navigable list: `↑/↓` moves stage, `LEFT/RIGHT` moves game-type tab. Show current tab name as text at (400, 62) instead of a dropdown widget.

**In our port** we already have overhead 3D stage preview via `draw_stage_preview` — that's a bonus we add on top of the stock layout (the JS's own launcher `preview.js` does the same thing, so it's period-correct even if the applet itself didn't show it).

**On ENTER** → `fase = 2` (loading) → `fase = 1` (trackbg) → `fase = 0` (race).

### 3.8 Loading (`fase == 2`) — `loadingstage()` xt.java:1971

Full-screen `bggo` at (0,-25) + "LOADING STAGE {N} {name}" text via drawcs. `loadingmusic` (`loadingmusic.gif`) spinner if music is loading. This is a transient screen — 1-2 frames typically.

### 3.9 Race intro (`fase == 1`) — `trackbg(false)` + around-track camera

Camera orbits the stage from above/around while `starcnt` counts down 130→0. `cntdn[3..1]` sequential draw (3, 2, 1 digits). `star[0/1]` GO! burst at starcnt=0.

**Position of countdown**: probably centered — need to read stat() for exact coords.

### 3.10 Racing (`fase == 0`) — already fully ported

### 3.11 Post-race (`fase == -5`) — `finish()`

Shows one of `youwon.gif` / `youlost.gif` / `yourwasted.gif` / `youwastedem.gif` depending on outcome, plus `contin[0]` at (355, 380) to return to stage picker.

---

## 4. `mainbg(n)` — background modes reference

xt.java:1734. `n` picks:

| n | Where used | Behavior |
|---|---|---|
| -101 | between screens | just paints black `(0,0,0)` to letterbox, doesn't draw bgmain |
| -1 | (net?) | color green (144,222,9), n2=8 scroll speed |
| 0 | (login?) | color pulses (191,184,124)↔(255,176,67) via `bgf` sine 0.02-0.9, n2=4 |
| 1 | main menu (fase 10) | constant orange (255,176,67), n2=8 scroll speed |
| 2 | fase 102 gamemode | bege (188,170,122), flipo==16 → blue transition, n2=2 |
| 3 | (?) | same pulse as n=0 but n2=2 |
| 4 | (?) | orange (216,177,100) + 4 animated ovals moving right→left |

The `bgmain.jpg` (670×400) image tiles vertically:  scrolls at `bgmy[k] += n2` per frame, wraps at 400 (`bgmy[0]` starts 0, `bgmy[1]` starts -400 so they cover seamlessly). Drawn at `(65, 25 + bgmy[k])` for k=0,1.

Then black borders always: `(0,0,65,450)`, `(735,0,65,450)`, `(65,0,670,25)`, `(65,425,670,25)` — the 65px letterbox.

---

## 5. Face animation ("dude" blink)

The face at top-right of main menu (`dude[0]` = d1.png at (351+jitter, 28+jitter)):

- `flkat` counter: 0→400, then wraps back to 0
- Alpha computed as:
  - `flkat 0..200`: alpha = flkat/800 (max at 0.2 == very transparent)
  - `flkat 200..400`: alpha = (400-flkat)/1000 (fade back to 0)
- Position jitter (`gxdu`, `gydu`): reset every 2 frames to random `-5..+6` px offset

`d1.png` = the NFM face with green background (green-keyed to alpha at load via `loadude()` at xt.java:9891). Already handled correctly by our `png_decode.c` since we don't zero-out RGB under transparent — same fix we made for `loadsnap`.

---

## 6. Controls mapping (Java `Control` object → Vita/Linux input)

The Java `Control` object has these menu-relevant flags:
- `control.up` / `control.down` — cycle option
- `control.left` / `control.right` — decrement/increment stage/car
- `control.enter` — confirm
- `control.handb` — same as enter in most contexts (was handbrake)
- `control.chatup` — chat overlay (out of scope)

Our port reads WASD + Arrows + Enter/Space via `platform/linux/input.c`'s
`input_poll` and `SDL_GetKeyboardState` for driving controls, and
`platform/linux/platform.c`'s `platform_poll` for the menu's own
logical BTN_UP/DOWN/LEFT/RIGHT/CONFIRM/CANCEL edges (see
`platform/common/buttons.h`). The Vita backend (Part 18,
`../TASKS_NATIVE.md`) maps the same two contracts onto sceCtrl in
`platform/vita/input.c`/`platform/vita/platform.c`:
- D-pad ↑↓ → up/down
- D-pad ←→ → left/right
- X (cross) → confirm / handbrake
- O (circle) → back / cancel

---

## 7. Concrete implementation checklist (next parts)

**Part 2 — Main menu (fase 10)**:
- [ ] Load `bgmain.jpg`, `logomadbg.jpg`, `logomad.png`, `d1.png`, `logocars.png`, `opback.png`, `options.png`, `byrd.png`, `nfmcoms.png` at startup
- [ ] Port `mainbg(1)` — scrolling `bgmain` on orange (255,176,67) canvas
- [ ] Port face `dude[0]` alpha animation (flkat 0..400 curve, gxdu/gydu jitter)
- [ ] Draw all 4 option roundRects with per-option aflk color
- [ ] `muhi` blink on Multiplayer row
- [ ] ↑↓ navigation, ENTER dispatches to fase 102/11/8 (or shows "not available" for MP)

**Part 3 — Gamemode submenu (fase 102)**:
- [ ] Same background pipeline (mainbg + logos) — factor out
- [ ] Draw `options2.png` + 4 roundRects with per-option colors
- [ ] `dropf`-conditional dimming of Free Play row (if unlocked state gates it)
- [ ] ENTER dispatches gmode + fase = -9 → 7

**Part 4 — Fase -9 → 7 (car select)**:
- [ ] Cleanup transition (black flash + `mainbg(-101)` letterbox)
- [ ] Load stage-independent car preview data (we already do this)
- [ ] Enter carselect with fase=7, cfase=0

**Part 5 — Car select 1:1 rework (fase 7)**:
- [ ] Use `carsbg` (`cars.gif`) as backdrop, not `bggo.jpg`
- [ ] `selectcar` caption at (321, 37)
- [ ] `back`/`next` at (95, 275) / (645, 275) — not bottom corners
- [ ] `contin` (PLAY) at (355, 385)
- [ ] Car spinning at exact preview.js camera constants (already done)
- [ ] Left/Right cycle sc[0]
- [ ] Real class label + stat bars (from `carStats()` formulas already documented)

**Part 6 — Stage select 1:1 rework (fase 6 subset)**:
- [ ] Use `br.png` as backdrop
- [ ] `select` caption at (338, 35)
- [ ] Stage name centered at y=132 with aflk blink
- [ ] `back` at (115, 135) + `next` at (625, 135)
- [ ] Replace AWT dropdowns with keyboard-navigable list (game-type tab + stage index)
- [ ] Keep our overhead 3D preview as bonus

**Part 7 — Instructions (fase 11)** — optional, do after core menu works

**Part 8 — Credits (fase 8)** — optional

**Part 9 — Post-race (fase -5)** — optional but nice to have

---

## 8. Open questions to resolve before Parts 2+

Before starting Part 2, I need to know:

1. **`bgmain.jpg` scrolls vertically** — should we implement this animation, or accept a static bg for simplicity? (1:1 says animate)
2. **Face animation (`dude[0]`) with alpha blend** — needs proper alpha compositing through `gfx_draw_image()`. Do we already support per-image alpha? (checked: yes, `alpha` field in draw command)
3. **`muhi` blink** on Multiplayer row — do we show Multiplayer option at all if netplay is out of scope? Options:
   - (a) Show it, disabled (grayed out), no click action
   - (b) Show it, click → "Coming soon" toast
   - (c) Hide it entirely (only 3 options)
4. **Scrolling bgmain**: it's 670×400 (same as content area). Scrolling `bgmy += 2` per frame means it tiles vertically. Do we implement this exactly, or draw static?

Once these are answered, we can start Part 2 (main menu) with pixel-perfect confidence.
