// Oracle for native/tests/control_career_test.c: runs the real Extended
// Control.preform/reset and Contva.sortvariables (web/ext/Control.js,
// Contva.js of the nfm web port) on the same seeded race states the C test
// builds, and prints the digests as native/tests/control_career_cases.h.
//
//   node tools/control_career_oracle.mjs <web/ext dir> > native/tests/control_career_cases.h
//   node tools/control_career_oracle.mjs <web/ext dir> --dump <stage> <bonus> <seed> [reset]
//
// gen_case/gen_tick/digest below mirror control_career_test.c's draw for
// draw; change one, change the other. --dump prints every digested word
// (index value), as CC_DUMP=<stage>,<bonus>,<seed>[,reset] does in the C test.
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const dir = process.argv[2];
if (!dir) {
  console.error('usage: control_career_oracle.mjs <web/ext dir> [--dump stage bonus seed [reset]]');
  process.exit(2);
}
const imp = (f) => import(pathToFileURL(path.join(dir, f)).href);
const { Control } = await imp('Control.js');
const { Contva } = await imp('Contva.js');
const java = await imp('../java.js');

// Per stage, the values preform's own code compares pcleared with and the cars
// it names, read off Control.js (an `if (checkpoints.stage === N)` encloses
// them by indentation). Only a sampling bias: both sides draw from the same
// lists, which go into the header.
const fs = await import('node:fs');
const SPOOL = Array.from({ length: 32 }, () => new Set());
const SCARS = Array.from({ length: 32 }, () => new Set());
{
  const L = fs.readFileSync(path.join(dir, 'Control.js'), 'utf8').split('\n');
  const from = L.findIndex((l) => /^\s+preform\(/.test(l));
  const to = L.findIndex((l, i) => i > from && /^\s+reset\(/.test(l));
  const stack = [];
  for (let i = from; i < to; i++) {
    const l = L[i];
    const ind = l.length - l.trimStart().length;
    while (stack.length && stack[stack.length - 1][0] >= ind) stack.pop();
    const m = /^\s*if \(checkpoints\.stage === (\d+)\)/.exec(l);
    if (m) {
      stack.push([ind, Number(m[1])]);
      continue;
    }
    if (!stack.length) continue;
    const st = stack[stack.length - 1][1];
    if (st > 31) continue;
    for (const x of l.matchAll(/madness\.pcleared (?:===|!==|>=|<=|<|>) (\d+)/g)) SPOOL[st].add(Number(x[1]));
    for (const x of l.matchAll(/id\(madness\.cn\) (?:===|!==) (\d+)/g)) SCARS[st].add(Number(x[1]));
  }
}
for (let i = 0; i < 32; i++) {
  SPOOL[i] = [...SPOOL[i]].sort((x, y) => x - y);
  SCARS[i] = [...SCARS[i]].sort((x, y) => x - y);
}

const MAXC = 20; // NFM_MAX_CARS
const NPTS = 340;
const NTRK = 64;
const NRLOG = 61;
const TICKS = 6;

// ---- the shared generator ----------------------------------------------------
let gs = 1;
function gnext() {
  let x = gs;
  x ^= x << 13; x >>>= 0;
  x ^= x >>> 17;
  x ^= x << 5; x >>>= 0;
  gs = x;
  return x;
}
const gi = (n) => gnext() % n;
const gb = (pct) => gi(100) < pct;
const POOL = [
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,
  83, 84, 85, 86, 87, 88, 89, 90, 91, 93, 95, 96, 99, 100, 101, 103, 105, 106, 107, 110, 111,
  113, 114, 115, 117, 120, 122, 124, 125, 126, 127, 128, 130, 131, 132, 134, 140, 142, 144, 148, 149, 151,
  156, 158, 159, 162, 165, 166, 172, 173, 175, 176, 177, 182, 183, 184, 188, 189, 190, 193, 195, 197, 198,
  199, 203, 206, 207, 208, 210, 211, 221, 226, 230, 234, 240, 243, 248, 259, 272, 278, 285, 289, 292, 297,
  298, 301, 305, 311, 316, 320, 321, 331, 332];
const gpool = () => POOL[gi(POOL.length)];
const groute = () => { const v = gpool(); return v < 0 ? 0 : v; };
const gtenth = () => Math.fround(gi(10) / 10);
const POW = [98, 98, 90, 80, 75, 70, 65, 60, 50, 30];
const gpower = () => POW[gi(10)];

const i32a = (n) => new Int32Array(n);
const boola = (n) => new Array(n).fill(false);
const DEG = 0.017453292519943295;
const TSIN = Array.from({ length: 360 }, (_, i) => Math.fround(Math.sin(i * DEG)));
const TCOS = Array.from({ length: 360 }, (_, i) => Math.fround(Math.cos(i * DEG)));

let W;

function newMedium() {
  return {
    rlog: new Array(NRLOG).fill(0), rp: 0,
    random() { const v = this.rlog[this.rp % NRLOG]; this.rp++; return v; },
    sin(i) { while (i >= 360) i -= 360; while (i < 0) i += 360; return TSIN[i | 0]; },
    cos(i) { while (i >= 360) i -= 360; while (i < 0) i += 360; return TCOS[i | 0]; },
  };
}

function genTables() {
  for (let x = 0; x < 39; x++) {
    W.maxmag[x] = 200 + gi(2000);
    for (let j = 0; j < 3; j++) W.swits[x][j] = 50 + gi(450);
    W.moment[x] = Math.fround((5 + gi(60)) / 10);
  }
}

function genMad(a, full) {
  const np = SPOOL[W.stage].length;
  if (np > 0 && gb(60)) {
    a.pcleared = SPOOL[W.stage][gi(np)];
  } else {
    a.pcleared = gpool();
  }
  a.clear = gi(40);
  a.point = groute();
  a.power = gpower();
  a.speed = gi(600);
  a.hitmag = gi(2200);
  a.specialact = gb(20);
  a.mtouch = gb(85);
  a.wtouch = gb(70);
  if (!full) return;
  a.frozen = gb(10);
  a.redstr = gb(10);
  a.strswap = gb(10);
  a.leech = gb(10);
  a.dest = gb(3);
  a.gtouch = gb(60);
  a.capsized = gb(15);
  for (let j = 0; j < 4; j++) a.scy[j] = gi(400) - 200;
  a.pxy = gi(720) - 360;
  a.pzy = gi(720) - 360;
  a.lcomp = gi(30);
  a.rcomp = gi(30);
  a.ucomp = gi(30);
  a.dcomp = gi(30);
  a.missedcp = gi(3) - 1;
  a.nlaps = gi(3);
  a.nofocus = gb(30);
  a.spatk = gi(121);
}

function genOthers() {
  const cp = W.cp;
  for (let k = 0; k < W.nplayers; k++) {
    cp.pos[k] = gi(W.nplayers);
    cp.clear[k] = gi(40);
    cp.dested[k] = gb(15) ? 1 : 0;
    cp.opx[k] = gi(120000) - 60000;
    cp.opz[k] = gi(120000) - 60000;
    cp.omxz[k] = gi(720) - 360;
  }
  cp.wasted = gi(W.nplayers);
  cp.pcleared = gpool();
  W.conto.x = gi(120000) - 60000;
  W.conto.z = gi(120000) - 60000;
  W.conto.y = gi(2000) - 1500;
  W.conto.xz = gi(720) - 360;
  W.conto.fix = gb(5);
  if (gb(40)) {
    const j = groute();
    W.conto.x = cp.x[j] + gi(3000) - 1500;
    W.conto.z = cp.z[j] + gi(3000) - 1500;
  }
  for (let k = 0; k < W.nplayers; k++) {
    if (gb(30)) {
      cp.opx[k] = W.conto.x + gi(4000) - 2000;
      cp.opz[k] = W.conto.z + gi(4000) - 2000;
    }
  }
}

function newMad(k) {
  return {
    cn: 0, im: k, pcleared: 0, clear: 0, point: 0, power: 0, speed: 0, hitmag: 0, specialact: false,
    mtouch: false, wtouch: false, frozen: false, redstr: false, strswap: false, leech: false, dest: false,
    gtouch: false, capsized: false, scy: [0, 0, 0, 0], pxy: 0, pzy: 0, lcomp: 0, rcomp: 0, ucomp: 0, dcomp: 0,
    missedcp: 0, nlaps: 0, nofocus: false, spatk: 0,
    maxmag: W.maxmag, swits: W.swits, moment: W.moment,
    aistrsp: i32a(39), level: i32a(39), beast: boola(101),
    shadowcar: false, nostunts: 0, nofix: false, groundlevel: 0,
  };
}

function genCase(seed, stage, bonus) {
  W = {
    maxmag: i32a(39), swits: Array.from({ length: 39 }, () => i32a(3)), moment: new Array(39).fill(0),
    xt: {
      careermode: true, classicmode: false, ptmatch: 0, dontdisplay: false, nplayers: 0,
      bonusstage: boola(6), bonstage: false, hardstage: false, unlocked: [1, 1], extpoints: i32a(39),
      sc: i32a(101), beastopponent: boola(101), undead: boola(101), entered: boola(101), fixspecials: boola(101),
      floor: i32a(101), randomcar: i32a(101), undeadlock: i32a(101), undeadtarget: 0, targetcar: 100,
      verydark: false, bossbattle: false, invulnerable: false, endsp: i32a(101), findi: i32a(101), justcs: -1,
    },
    cx: {}, // per-slot Madness/ContO extras, as the C context holds them
    cp: {
      stage: 0, n: 0, nsp: 0, nlaps: 0, pos: i32a(101), clear: i32a(101), dested: i32a(101), opx: i32a(101),
      opz: i32a(101), omxz: i32a(101), wasted: 0, pcleared: 0, x: i32a(2000), z: i32a(2000), typ: i32a(2000),
      floor: i32a(2000), telefloor: i32a(2000), chkcode: i32a(2000), fx: i32a(50), fz: i32a(50), fn: 0,
    },
    conto: { x: 0, z: 0, y: 0, xz: 0, fix: false, floorguardian: false, guardswitch: false },
    tr: { dam: i32a(NTRK), skd: i32a(NTRK) },
    bots: { botbreak: boola(101) },
    m: newMedium(),
  };
  W.contva = new Contva();
  gs = seed;
  W.stage = stage;
  W.nplayers = 7 + gi(13);
  W.im = 1 + gi(W.nplayers - 1);
  const xt = W.xt;
  xt.nplayers = W.nplayers;
  for (let k = 0; k < 4; k++) xt.bonusstage[k] = bonus === k + 1;
  xt.bonstage = bonus !== 0;
  xt.hardstage = gb(30);
  xt.unlocked[1] = 1 + gi(31);
  for (let k = 0; k < 39; k++) xt.extpoints[k] = gi(40);
  const ex = { beast: [], shadowcar: [], level: [], aistrsp: [], nostunts: [], nofix: [], groundlevel: [],
    floorguardian: [], guardswitch: [] };
  for (let k = 0; k < W.nplayers; k++) {
    const nc = SCARS[stage].length;
    if (k === W.im && nc > 0 && gb(50)) {
      xt.sc[k] = SCARS[stage][gi(nc)];
    } else {
      xt.sc[k] = gi(39);
    }
    xt.beastopponent[k] = gb(25);
    xt.undead[k] = gb(20);
    xt.entered[k] = gb(50);
    xt.fixspecials[k] = gb(20);
    xt.floor[k] = gi(4);
    xt.randomcar[k] = gi(W.nplayers);
    xt.undeadlock[k] = gi(W.nplayers);
    ex.beast[k] = gb(25);
    ex.shadowcar[k] = gb(15);
    ex.level[k] = 1 + gi(30);
    ex.aistrsp[k] = gi(40);
    ex.nostunts[k] = gi(3);
    ex.nofix[k] = gb(20);
    ex.groundlevel[k] = gb(50) ? 0 : -10000 * gi(4);
    ex.floorguardian[k] = gb(15);
    ex.guardswitch[k] = gb(50);
    W.bots.botbreak[k] = gb(20);
    xt.endsp[k] = gi(60);
  }
  xt.undeadtarget = gi(W.nplayers);
  xt.verydark = gb(30);
  xt.targetcar = xt.verydark ? gi(W.nplayers) : (gb(50) ? 100 : gi(W.nplayers));
  xt.bossbattle = gb(30);
  xt.invulnerable = gb(30);

  const cv = W.contva;
  for (let k = 0; k < MAXC; k++) {
    cv.vulnerable[k] = gb(15);
    cv.freeze[k] = gb(10);
    cv.weaken[k] = gb(10);
    cv.biglead[k] = gb(20);
    cv.completed[k] = gi(101);
    cv.needhelp[k] = gb(20);
    cv.nearchk[k] = gb(30);
    cv.chkcircle[k] = gi(200);
    cv.moreslow[k] = gi(81);
    cv.slowdown[k] = 150 + gi(100);
    cv.slowrange[k] = gi(9000);
    cv.opbackloops[k] = gb(10);
    cv.dontstunt[k] = gb(20);
    cv.spdexception[k] = gb(20);
    cv.dontmiss[k] = gb(30);
    cv.layoff[k] = gb(20);
    cv.sharpturn[k] = gi(3);
  }
  cv.lotswasted = gb(30);
  cv.numfixes = 8;
  for (let k = 0; k < 8; k++) cv.fixpoint[k] = gi(NPTS);
  cv.whichfix = gi(8);

  genTables();
  W.mad = [];
  for (let k = 0; k < W.nplayers; k++) {
    const a = newMad(k);
    a.cn = xt.sc[k];
    a.aistrsp[a.cn] = ex.aistrsp[k];
    a.level[a.cn] = ex.level[k];
    a.beast[k] = ex.beast[k];
    a.shadowcar = ex.shadowcar[k];
    a.nostunts = ex.nostunts[k];
    a.nofix = ex.nofix[k];
    a.groundlevel = ex.groundlevel[k];
    W.mad[k] = a;
    genMad(a, k === W.im);
  }
  W.conto.floorguardian = ex.floorguardian[W.im];
  W.conto.guardswitch = ex.guardswitch[W.im];

  const cp = W.cp;
  cp.stage = stage;
  cp.n = NPTS;
  cp.nsp = 1 + gi(20);
  cp.nlaps = 1 + gi(3);
  for (let i = 0; i < NPTS; i++) {
    cp.x[i] = gi(120000) - 60000;
    cp.z[i] = gi(120000) - 60000;
    cp.typ[i] = gi(9) - 4;
    cp.floor[i] = gi(4);
    cp.telefloor[i] = gi(4);
    cp.chkcode[i] = gi(4);
  }
  cp.fn = 1 + gi(8);
  for (let i = 0; i < 8; i++) {
    cp.fx[i] = gi(120000) - 60000;
    cp.fz[i] = gi(120000) - 60000;
  }
  genOthers();
  for (let i = 0; i < NTRK; i++) {
    W.tr.dam[i] = gb(50) ? 0 : 1;
    W.tr.skd[i] = gi(3);
  }

  const c = new Control(W.m, W.contva);
  W.c = c;
  c.pan = gi(360) - 180;
  c.attack = gb(50) ? 0 : gi(300);
  c.acr = gi(W.nplayers);
  c.afta = gb(50);
  for (let k = 0; k < 50; k++) c.fpnt[k] = gi(NPTS);
  c.trfix = gi(5);
  c.forget = gb(30);
  c.bulistc = gb(30);
  c.runbul = gi(100);
  c.acuracy = gi(40);
  c.upwait = gi(40);
  c.agressed = gb(50);
  c.skiplev = gtenth();
  c.clrnce = gi(10);
  c.rampp = gi(5) - 2;
  c.turntyp = gi(3);
  c.aim = gtenth();
  c.saftey = gi(30);
  c.perfection = gb(50);
  c.mustland = gtenth();
  c.usebounce = gb(20);
  c.trickprf = gtenth();
  c.stuntf = gi(15);
  c.zyinv = gb(20);
  c.lastl = gb(50);
  c.wlastl = gb(50);
  c.hold = gb(70) ? 0 : gi(40);
  c.wall = gb(60) ? -1 : gi(NTRK);
  c.lwall = gb(50) ? -1 : gi(NTRK);
  c.stcnt = gi(30);
  c.statusque = gi(20);
  c.turncnt = gi(10);
  c.randtcnt = gi(10);
  c.upcnt = gi(40);
  c.trickfase = gi(4) - 1;
  c.swat = gi(4);
  c.udcomp = gb(50);
  c.lrcomp = gb(50);
  c.udbare = gb(50);
  c.lrbare = gb(50);
  c.onceu = gb(50);
  c.onced = gb(50);
  c.oncel = gb(50);
  c.oncer = gb(50);
  c.lrdirect = gi(3) - 1;
  c.uddirect = gi(3) - 1;
  c.lrstart = gi(30);
  c.udstart = gi(30);
  c.oxy = gi(720) - 360;
  c.ozy = gi(720) - 360;
  c.flycnt = gi(40);
  c.lrswt = gb(30);
  c.udswt = gb(30);
  c.gowait = gb(20);
  c.actwait = gi(40);
  c.cntrn = gi(6);
  c.revstart = gb(70) ? 0 : gi(30);
  c.oupnt = gb(10) ? -gi(200) : gpool();
  c.wtz = gi(120000) - 60000;
  c.wtx = gi(120000) - 60000;
  c.frx = gi(120000) - 60000;
  c.frz = gi(120000) - 60000;
  c.frad = gi(90000);
  c.apunch = gi(21);
  c.exitattack = gb(30);
  c.stuck = gi(90);
  c.downuse = gi(10);
  c.fewsecs = gi(50);
  c.fewsecson = gb(20);
  c.fixby = 40 + gi(61);
  c.abboost = gb(70) ? 0 : gi(4000);
  c.abdelay = gi(4);
  c.campchk = gi(NPTS + 1) - 1;
  c.campcool = gi(100);
  c.chkahead = gi(5);
  c.waitforuser = gb(20);
  c.intercept = gb(20);
  c.l1 = gi(2000);
  c.l3 = gi(120000) - 60000;
  c.k5 = gi(120000) - 60000;
  c.backfix = gb(30);
  c.switchspot = gb(50);
  c.waited = gb(30);
  c.delayturn = gb(30);
  c.dontback = gb(30);
  c.needtofix = gb(30);
  c.staythere = gi(60);
  c.waitman = gi(60);
  c.wrongfloor = gb(30);
  c.setfixfloor = gb(30);
  c.gotofloor = gi(4);
  for (let k = 0; k < MAXC; k++) {
    c.neverhit[k] = gb(30);
    c.avoidnlev[k] = gb(50) ? 0 : gi(20000);
  }
  W.u = [];
  for (let k = 0; k < MAXC; k++) W.u[k] = { trfix: gi(5) };

  for (let i = 0; i < NRLOG; i++) W.m.rlog[i] = gtenth();
  W.m.rp = 0;
  java.setSeed(gnext());
}

function genTick() {
  genMad(W.mad[W.im], false);
  W.mad[0].speed = gi(600);
  W.mad[0].specialact = gb(20);
  genOthers();
  for (let k = 0; k < W.nplayers; k++) W.xt.floor[k] = gi(4);
  if (gb(30)) W.c.stcnt = W.c.statusque + 1;
}

// ---- the digest ----------------------------------------------------------------
let h = 0;
let dump = null;
const fbuf = new DataView(new ArrayBuffer(4));
function hw(w) {
  w >>>= 0;
  if (dump) dump.push(w);
  h = (Math.imul((h ^ w) >>> 0, 16777619)) >>> 0;
}
function hi(v) {
  if (typeof v === 'boolean') v = v ? 1 : 0;
  if (!Number.isInteger(v)) throw new Error('non-integer ' + v);
  hw(v | 0);
}
function hf(v) {
  fbuf.setFloat32(0, v);
  hw(fbuf.getUint32(0));
}

function digestTick() {
  const c = W.c;
  hi(c.pan); hi(c.attack); hi(c.acr); hi(c.afta); hi(c.trfix); hi(c.forget); hi(c.bulistc);
  hi(c.runbul); hi(c.acuracy); hi(c.upwait); hi(c.agressed); hf(c.skiplev); hi(c.clrnce); hi(c.rampp);
  hi(c.turntyp); hf(c.aim); hi(c.saftey); hi(c.perfection); hf(c.mustland); hi(c.usebounce);
  hf(c.trickprf); hi(c.stuntf); hi(c.zyinv); hi(c.lastl); hi(c.wlastl); hi(c.hold); hi(c.wall);
  hi(c.lwall); hi(c.stcnt); hi(c.statusque); hi(c.turncnt); hi(c.randtcnt); hi(c.upcnt);
  hi(c.trickfase); hi(c.swat); hi(c.udcomp); hi(c.lrcomp); hi(c.udbare); hi(c.lrbare); hi(c.onceu);
  hi(c.onced); hi(c.oncel); hi(c.oncer); hi(c.lrdirect); hi(c.uddirect); hi(c.lrstart); hi(c.udstart);
  hi(c.oxy); hi(c.ozy); hi(c.flycnt); hi(c.lrswt); hi(c.udswt); hi(c.gowait); hi(c.actwait);
  hi(c.cntrn); hi(c.revstart); hi(c.oupnt); hi(c.wtz); hi(c.wtx); hi(c.frx); hi(c.frz); hi(c.frad);
  hi(c.apunch); hi(c.exitattack); hi(c.stuck); hi(c.downuse); hi(c.fewsecs); hi(c.fewsecson);
  hi(c.fixby); hi(c.abboost); hi(c.abdelay); hi(c.campchk); hi(c.campcool); hi(c.chkahead);
  hi(c.waitforuser); hi(c.intercept); hi(c.l1); hi(c.l3); hi(c.k5); hi(c.backfix); hi(c.switchspot);
  hi(c.waited); hi(c.delayturn); hi(c.dontback); hi(c.needtofix); hi(c.staythere); hi(c.waitman);
  hi(c.wrongfloor); hi(c.setfixfloor); hi(c.gotofloor); hi(c.left); hi(c.right); hi(c.up); hi(c.down);
  hi(c.handb); hi(c.spatk);
  for (let k = 0; k < 50; k++) hi(c.fpnt[k]);
  for (let k = 0; k < MAXC; k++) {
    hi(c.neverhit[k]);
    hi(c.avoidnlev[k]);
  }
  const a = W.mad[W.im];
  hi(a.pcleared); hi(a.clear); hi(a.nofocus);
  const cv = W.contva;
  for (let k = 0; k < MAXC; k++) {
    hi(cv.nearchk[k]); hi(cv.dontmiss[k]); hi(cv.dontstunt[k]); hi(cv.sharpturn[k]);
    hi(cv.slowrange[k]); hi(W.xt.findi[k]);
  }
  hi(cv.whichfix);
  for (let k = 0; k < 101; k++) hi(W.xt.endsp[k]);
  hi(W.m.rp);
  hw(java.random() * 4294967296);
}

function digestContva() {
  const cv = W.contva;
  for (let k = 0; k < MAXC; k++) {
    hi(cv.vulnerable[k]); hi(cv.freeze[k]); hi(cv.swapped[k]); hi(cv.leeching[k]); hi(cv.weaken[k]);
    hi(cv.specon[k]); hi(cv.biglead[k]); hi(cv.hugelead[k]); hi(cv.completed[k]); hi(cv.needhelp[k]);
    hi(cv.chkcircle[k]); hi(cv.moreslow[k]); hi(cv.slowdown[k]); hi(cv.slowrange[k]);
    hi(cv.opbackloops[k]); hi(cv.dontdistract[k]); hi(cv.spdexception[k]); hi(cv.layoff[k]);
  }
  hi(cv.lotswasted);
}

function runCase(seed, stage, bonus) {
  genCase(seed, stage, bonus);
  h = 2166136261;
  for (let t = 0; t < TICKS; t++) {
    if (t !== 0) genTick();
    W.c.preform(W.mad[W.im], W.conto, W.cp, W.tr, W.xt, W.mad[0], W.bots);
    digestTick();
  }
  for (let k = 0; k < W.nplayers; k++) W.contva.sortvariables(W.mad[k], W.cp, W.u, true, W.nplayers);
  digestContva();
  return h;
}

function runReset(seed, stage, bonus) {
  genCase(seed, stage, bonus);
  h = 2166136261;
  W.c.reset(W.cp, W.mad[W.im].cn, W.xt, W.mad[W.im], W.mad[0]);
  digestTick();
  return h;
}

const a = process.argv.slice(3);
if (a[0] === '--dump') {
  dump = [];
  const [stage, bonus, seed] = [Number(a[1]), Number(a[2]), Number(a[3]) >>> 0];
  const r = a[4] === 'reset' ? runReset(seed, stage, bonus) : runCase(seed, stage, bonus);
  dump.forEach((w, i) => console.log(i, w | 0));
  console.log('digest', '0x' + r.toString(16).padStart(8, '0'));
} else {
  // Groups of PER preform cases and NRESET resets; each case's seed steps an LCG
  // from the group's seed0 and the digests chain (as the C test does).
  const PER = 300;
  const NRESET = 30;
  const list = a[0] === '--list';
  const rows = [];
  let s0 = 0x9e3779b9;
  const groups = [];
  for (let st = 1; st <= 31; st++) groups.push([st, 0]);
  groups.push([5, 1], [11, 2], [15, 3], [18, 4]);
  for (const [st, bon] of groups) {
    s0 = (Math.imul(s0, 1664525) + 1013904223) >>> 0;
    let s = s0, hh = 2166136261;
    for (let k = 0; k < PER + NRESET; k++) {
      s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
      const reset = k >= PER;
      const d = reset ? runReset(s, st, bon) : runCase(s, st, bon);
      if (list) console.log(`${st} ${bon} ${reset ? 'reset' : 'preform'} 0x${s.toString(16).padStart(8, '0')} 0x${d.toString(16).padStart(8, '0')}`);
      hh = Math.imul((hh ^ d) >>> 0, 16777619) >>> 0;
    }
    rows.push([st, bon, PER, NRESET, s0, hh]);
  }
  if (list) process.exit(0);
  const hex = (v) => '0x' + v.toString(16).padStart(8, '0') + 'u';
  const out = [];
  out.push('// GENERATED by tools/control_career_oracle.mjs from web/ext/Control.js and Contva.js');
  out.push('// (nfm web port) -- see control_career_test.c. Do not edit by hand.');
  out.push('#ifndef CONTROL_CAREER_CASES_H');
  out.push('#define CONTROL_CAREER_CASES_H');
  out.push('#include <stdint.h>');
  out.push(`#define CC_TICKS ${TICKS}`);
  const w = (arr) => Math.max(1, ...arr.map((x) => x.length));
  out.push('// Per stage: the pcleared values and cars its code tests (a sampling bias).');
  out.push(`static const int32_t CC_SPOOLN[32] = {${SPOOL.map((x) => x.length).join(', ')}};`);
  out.push(`static const int32_t CC_SPOOL[32][${w(SPOOL)}] = {`);
  for (const x of SPOOL) out.push(`  {${x.length ? x.join(', ') : '0'}},`);
  out.push('};');
  out.push(`static const int32_t CC_SCARSN[32] = {${SCARS.map((x) => x.length).join(', ')}};`);
  out.push(`static const int32_t CC_SCARS[32][${w(SCARS)}] = {`);
  for (const x of SCARS) out.push(`  {${x.length ? x.join(', ') : '0'}},`);
  out.push('};');
  out.push('typedef struct { int32_t stage, bonus, ncases, nresets; uint32_t seed0, digest; } CareerGroup;');
  out.push('static const CareerGroup CC_GROUPS[] = {');
  for (const r of rows) out.push(`  {${r[0]}, ${r[1]}, ${r[2]}, ${r[3]}, ${hex(r[4])}, ${hex(r[5])}},`);
  out.push('};');
  out.push('#define CC_NGROUPS ((int32_t)(sizeof(CC_GROUPS) / sizeof(CC_GROUPS[0])))');
  out.push('#endif');
  console.log(out.join('\n'));
}
