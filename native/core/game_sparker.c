// ports web/GameSparker.js's loadbase() and a lean loadstage() -- see
// game_sparker.h for the exact scope.
#include "game_sparker.h"
#include "vfs.h"
#include <stdlib.h>
#include <string.h>

static bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

/** Matches JS String.trim(): strips ASCII whitespace from both ends. */
static char *trim_line(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
  size_t len = strlen(s);
  while (len > 0) {
    char c = s[len - 1];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') len--;
    else break;
  }
  s[len] = '\0';
  return s;
}

/**
 * Ports GameSparker.js's getint(s, s2, n): extracts the Nth comma/paren-
 * separated field after "s(" in s2, parses it as an int. Same double-
 * increment-on-separator quirk as ContO's own getvalue (cont_o.c) --
 * these are two separately-hand-written near-duplicates in the JS too,
 * not a shared helper there either.
 */
static int32_t gs_getint(const char *cmd, const char *line, int32_t n) {
  size_t cmd_len = strlen(cmd);
  size_t line_len = strlen(line);
  int32_t n2 = 0;
  char buf[64];
  size_t buf_len = 0;
  for (size_t i = cmd_len + 1; i < line_len; i++) {
    char c1 = line[i];
    if (c1 == ',' || c1 == ')') {
      n2++;
      i++;
    }
    if (n2 == n) {
      if (i < line_len && buf_len < sizeof(buf) - 1) buf[buf_len++] = line[i];
    }
  }
  buf[buf_len] = '\0';
  return (int32_t)strtol(buf, NULL, 10);
}

// GameSparker.js's own CAR_NAMES/TRACK_NAMES, slot order preserved exactly
// (car slots 0-15, track slots 56-123 -- the +56 comes from loadbase()'s
// `array[n2] = j + 56`, and stage files add their own +46 on top when
// referencing a track slot from `set(`/`chk(`/`fix(`/etc -- see the JS's
// own comment on why: legacy numbering this port has no reason to change).
static const char *CAR_NAMES[16] = {
  "2000tornados", "formula7", "canyenaro", "lescrab", "nimi", "maxrevenge",
  "leadoxide", "koolkat", "drifter", "policecops", "mustang", "king",
  "audir8", "masheen", "radicalone", "drmonster",
};
static const char *TRACK_NAMES[68] = {
  "road", "froad", "twister2", "twister1", "turn", "offroad", "bumproad",
  "offturn", "nroad", "nturn", "roblend", "noblend", "rnblend", "roadend",
  "offroadend", "hpground", "ramp30", "cramp35", "dramp15", "dhilo15",
  "slide10", "takeoff", "sramp22", "offbump", "offramp", "sofframp",
  "halfpipe", "spikes", "rail", "thewall", "checkpoint", "fixpoint",
  "offcheckpoint", "sideoff", "bsideoff", "uprise", "riseroad", "sroad",
  "soffroad", "tside", "launchpad", "thenet", "speedramp", "offhill",
  "slider", "uphill", "roll1", "roll2", "roll3", "roll4", "roll5", "roll6",
  "opile1", "opile2", "aircheckpoint", "tree1", "tree2", "tree3", "tree4",
  "tree5", "tree6", "tree7", "tree8", "cac1", "cac2", "cac3", "8sroad",
  "8soffroad",
};

bool game_sparker_loadbase(ContO *base_models, Medium *m, Trackers *t, const char *zip_path) {
  VfsZip zip;
  if (!vfs_read_zip(zip_path, &zip)) return false;

  int64_t total = 0;
  for (int32_t i = 0; i < zip.count; i++) {
    int32_t slot = 0;
    for (int32_t c = 0; c < 16; c++) {
      if (starts_with(zip.entries[i].name, CAR_NAMES[c])) slot = c;
    }
    for (int32_t tk = 0; tk < 68; tk++) {
      if (starts_with(zip.entries[i].name, TRACK_NAMES[tk])) slot = tk + 56;
    }
    total += zip.entries[i].len;

    char *text = vfs_entry_text(&zip.entries[i]);
    // NOT m->loadnew = true here -- the JS's own loadbase() never touches
    // medium.loadnew around this call (unlike CarDefine.loadcar()), so
    // base models from models.zip do NOT get the loadnew-gated fs
    // orientation post-processing pass. Preserved, not an oversight.
    cont_o_init_buf(&base_models[slot], text, m, t);
    free(text);
    base_models[slot].baseIndex = slot;
  }
  vfs_free_zip(&zip);

  // Mirrors the JS's own tamper/truncation check against the real
  // data/models.zip's exact known-good total.
  return total == 621172;
}

/** One boundary wall tracker, x-aligned (maxr/maxl). */
static void gs_wall(Trackers *t, int32_t y, int32_t rady, int32_t x, int32_t radx,
                     int32_t z, int32_t radz, int32_t xy, int32_t zy) {
  t->y[t->nt] = y;
  t->rady[t->nt] = rady;
  t->x[t->nt] = x;
  t->radx[t->nt] = radx;
  t->z[t->nt] = z;
  t->radz[t->nt] = radz;
  t->xy[t->nt] = xy;
  t->zy[t->nt] = zy;
  t->dam[t->nt] = 167;
  t->decor[t->nt] = false;
  t->skd[t->nt] = 0;
  t->nt++;
}

/** One boundary wall tracker, z-aligned (maxt/maxb). */
static void gs_wallz(Trackers *t, int32_t y, int32_t rady, int32_t z, int32_t radz,
                      int32_t x, int32_t radx, int32_t zy, int32_t xy) {
  t->y[t->nt] = y;
  t->rady[t->nt] = rady;
  t->z[t->nt] = z;
  t->radz[t->nt] = radz;
  t->x[t->nt] = x;
  t->radx[t->nt] = radx;
  t->zy[t->nt] = zy;
  t->xy[t->nt] = xy;
  t->dam[t->nt] = 167;
  t->decor[t->nt] = false;
  t->skd[t->nt] = 0;
  t->nt++;
}

bool game_sparker_loadstage(ContO *out_objects, int32_t out_capacity, int32_t *out_count,
                             ContO *base_models, Medium *m, Trackers *t, CheckPoints *cp,
                             const char *text, int32_t *out_center_x, int32_t *out_center_z) {
  t->nt = 0;
  m->resdown = 0;
  m->rescnt = 5;
  m->lightson = false;
  m->noelec = 0;
  m->ground = 250;
  m->trk = 0;
  // Matches the JS's own loadstage() resetting these three before the
  // parse loop (lines 102-104) -- needed since a stage can be (re)loaded
  // more than once per process (e.g. picking a different stage from the
  // menu). check_points_init() already zeroes everything the FIRST time;
  // this covers every subsequent load.
  cp->n = 0;
  cp->nsp = 0;
  cp->fn = 0;

  // Stage bounds, accumulated from maxr/maxl/maxt/maxb (0/100/0/100 if a
  // stage has none, matching the JS's own defaults) and fed to
  // trackers_devidetrackers below -- same variables as the JS's
  // getint/getint2/getint3/getint4 (renamed ge1..ge4 here, `getint` is a
  // function name in this file).
  int32_t ge1 = 0, ge2 = 100, ge3 = 0, ge4 = 100;
  int32_t nob = 0;
  int32_t nfix = 0; // caps `fix(` placement at 5, matching CheckPoints.nfix's role
  int32_t notb = 0; // objects up to the last checkpoint/fix -- seeds newpolys
  bool ok = true;

  char **lines;
  int32_t line_count;
  vfs_read_lines(text, &lines, &line_count);

  for (int32_t li = 0; li < line_count && ok; li++) {
    char *trimmed = trim_line(lines[li]);

    if (starts_with(trimmed, "snap")) {
      medium_setsnap(m, gs_getint("snap", trimmed, 0), gs_getint("snap", trimmed, 1),
                      gs_getint("snap", trimmed, 2));
    }
    if (starts_with(trimmed, "sky")) {
      medium_setsky(m, gs_getint("sky", trimmed, 0), gs_getint("sky", trimmed, 1),
                    gs_getint("sky", trimmed, 2));
    }
    if (starts_with(trimmed, "ground")) {
      medium_setgrnd(m, gs_getint("ground", trimmed, 0), gs_getint("ground", trimmed, 1),
                      gs_getint("ground", trimmed, 2));
    }
    if (starts_with(trimmed, "polys")) {
      medium_setpolys(m, gs_getint("polys", trimmed, 0), gs_getint("polys", trimmed, 1),
                        gs_getint("polys", trimmed, 2));
    }
    if (starts_with(trimmed, "texture")) {
      medium_setexture(m, gs_getint("texture", trimmed, 0), gs_getint("texture", trimmed, 1),
                        gs_getint("texture", trimmed, 2), gs_getint("texture", trimmed, 3));
    }
    if (starts_with(trimmed, "fog")) {
      medium_setfade(m, gs_getint("fog", trimmed, 0), gs_getint("fog", trimmed, 1),
                      gs_getint("fog", trimmed, 2));
    }
    if (starts_with(trimmed, "clouds")) {
      medium_setcloads(m, gs_getint("clouds", trimmed, 0), gs_getint("clouds", trimmed, 1),
                       gs_getint("clouds", trimmed, 2), gs_getint("clouds", trimmed, 3),
                       gs_getint("clouds", trimmed, 4));
    }
    if (starts_with(trimmed, "density")) {
      m->fogd = (gs_getint("density", trimmed, 0) + 1) * 2 - 1;
      if (m->fogd < 1) m->fogd = 1;
      if (m->fogd > 30) m->fogd = 30;
    }
    if (starts_with(trimmed, "fadefrom")) medium_fadfrom(m, gs_getint("fadefrom", trimmed, 0));
    if (starts_with(trimmed, "lightson")) m->lightson = true;
    if (starts_with(trimmed, "mountains")) m->mgen = gs_getint("mountains", trimmed, 0);

    if (starts_with(trimmed, "set")) {
      int32_t slot = gs_getint("set", trimmed, 0) + 46;
      if (nob >= out_capacity) { ok = false; break; }
      cont_o_init_copy(&out_objects[nob], &base_models[slot],
                        gs_getint("set", trimmed, 1),
                        m->ground - base_models[slot].grat,
                        gs_getint("set", trimmed, 2),
                        gs_getint("set", trimmed, 3));
      // `set(...)p` -- a road piece that is also a route point for the AI
      // (GameSparker.java's set branch): typ 0 plain, -1 `)pt`, -2 `)pr`
      // (ramp), -3 `)po`, -4 `)ph`. control_preform steers bots along
      // these between the checkpoints; without them they aimed straight
      // from one gate to the next.
      if (strstr(trimmed, ")p")) {
        if (cp->n < CHECK_POINTS_MAX) {
          cp->x[cp->n] = gs_getint("set", trimmed, 1);
          cp->z[cp->n] = gs_getint("set", trimmed, 2);
          cp->y[cp->n] = 0;
          cp->typ[cp->n] = 0;
          if (strstr(trimmed, ")pt")) cp->typ[cp->n] = -1;
          if (strstr(trimmed, ")pr")) cp->typ[cp->n] = -2;
          if (strstr(trimmed, ")po")) cp->typ[cp->n] = -3;
          if (strstr(trimmed, ")ph")) cp->typ[cp->n] = -4;
          cp->n++;
        }
        notb = nob + 1;
      }
      nob++;
    }
    if (starts_with(trimmed, "chk")) {
      int32_t slot = gs_getint("chk", trimmed, 0) + 46;
      int32_t y = m->ground - base_models[slot].grat;
      if (slot == 110) y = gs_getint("chk", trimmed, 4);
      if (nob >= out_capacity) { ok = false; break; }
      cont_o_init_copy(&out_objects[nob], &base_models[slot],
                        gs_getint("chk", trimmed, 1), y,
                        gs_getint("chk", trimmed, 2),
                        gs_getint("chk", trimmed, 3));
      // Real checkpoint/lap bookkeeping (JS lines 202-210) -- this is
      // what mad_drive()'s own focus/clear logic and check_points_
      // checkstat() actually race against; the ContO placed above is
      // just the checkpoint GATE's visible geometry.
      if (cp->n < CHECK_POINTS_MAX) {
        cp->x[cp->n] = gs_getint("chk", trimmed, 1);
        cp->z[cp->n] = gs_getint("chk", trimmed, 2);
        cp->y[cp->n] = y;
        cp->typ[cp->n] = (gs_getint("chk", trimmed, 3) == 0) ? 1 : 2;
        cp->pcs = cp->n;
        cp->n++;
      }
      // co->checkpoint - 1 == m->checkpoint is cont_o_d()'s own "is this
      // the gate the player is currently racing toward" highlight check
      // (see cont_o.c) -- already ported, just needed this field set.
      out_objects[nob].checkpoint = cp->nsp + 1;
      cp->nsp++;
      nob++;
      notb = nob;
    }
    if (nfix != 5 && starts_with(trimmed, "fix")) {
      int32_t slot = gs_getint("fix", trimmed, 0) + 46;
      if (nob >= out_capacity) { ok = false; break; }
      // Args are (base, x, y, z, a) with y/z SWAPPED relative to the
      // stage file's own field order (field 3 -> y, field 2 -> z) --
      // matches the JS's own `new ContO(array2[n], get(1), get(3),
      // get(2), get(4))` exactly, not a transcription slip.
      cont_o_init_copy(&out_objects[nob], &base_models[slot],
                        gs_getint("fix", trimmed, 1),
                        gs_getint("fix", trimmed, 3),
                        gs_getint("fix", trimmed, 2),
                        gs_getint("fix", trimmed, 4));
      out_objects[nob].elec = true;
      if (gs_getint("fix", trimmed, 4) != 0) out_objects[nob].roted = true;
      // Real "fix point" bookkeeping (JS lines 219-230) -- capped at 5,
      // matching checkPoints.nfix's own role (mad_drive() reads
      // cp->fx/fy/fz/roted/special through checkPoints->fn, not through
      // this function's own `nfix` loop-cap local).
      // The cap the comment above describes, actually enforced. It was
      // only ever a comment: cp->fn was incremented without a bound while
      // cp->fx/fz/fy hold 5 entries each (check_points.h) and Control's
      // own fpnt[] holds 5 as well, so a stage carrying a sixth `fix`
      // line wrote straight past all of them and left cp->fn too large
      // for control_reset()'s and mad_drive()'s `< cp->fn` loops. No
      // shipped stage has more than 4, so nothing was corrupting yet --
      // but mystages/ is user-editable, which is exactly where a sixth
      // would come from. Java is bounded here by its own array length
      // throwing; C needs the check written out.
      if (cp->fn < (int32_t)(sizeof(cp->fx) / sizeof(cp->fx[0]))) {
        cp->fx[cp->fn] = gs_getint("fix", trimmed, 1);
        cp->fz[cp->fn] = gs_getint("fix", trimmed, 2);
        cp->fy[cp->fn] = gs_getint("fix", trimmed, 3);
        cp->roted[cp->fn] = gs_getint("fix", trimmed, 4) != 0;
        cp->special[cp->fn] = strstr(trimmed, ")s") != NULL;
        cp->fn++;
      }
      nob++;
      notb = nob;
      nfix++;
    }
    if (starts_with(trimmed, "nlaps")) {
      // Always applied (not gated on multion like the JS -- that gate is
      // `xtGraphics.multion === 0`, always true in this single-player-
      // only port).
      cp->nlaps = gs_getint("nlaps", trimmed, 0);
      if (cp->nlaps < 1) cp->nlaps = 1;
      if (cp->nlaps > 15) cp->nlaps = 15;
    }

    if (starts_with(trimmed, "maxr")) {
      int32_t cnt = gs_getint("maxr", trimmed, 0);
      int32_t n2 = gs_getint("maxr", trimmed, 1);
      ge1 = n2;
      int32_t off = gs_getint("maxr", trimmed, 2);
      for (int32_t k = 0; k < cnt; k++) {
        if (nob >= out_capacity) { ok = false; break; }
        cont_o_init_copy(&out_objects[nob], &base_models[85], n2,
                          m->ground - base_models[85].grat, k * 4800 + off, 0);
        nob++;
      }
      gs_wall(t, -5000, 7100, n2 + 500, 600, (cnt * 4800) / 2 + off - 2400, (cnt * 4800) / 2, 90, 0);
    }
    if (starts_with(trimmed, "maxl")) {
      int32_t cnt = gs_getint("maxl", trimmed, 0);
      int32_t n3 = gs_getint("maxl", trimmed, 1);
      ge2 = n3;
      int32_t off = gs_getint("maxl", trimmed, 2);
      for (int32_t k = 0; k < cnt; k++) {
        if (nob >= out_capacity) { ok = false; break; }
        cont_o_init_copy(&out_objects[nob], &base_models[85], n3,
                          m->ground - base_models[85].grat, k * 4800 + off, 180);
        nob++;
      }
      gs_wall(t, -5000, 7100, n3 - 500, 600, (cnt * 4800) / 2 + off - 2400, (cnt * 4800) / 2, -90, 0);
    }
    if (starts_with(trimmed, "maxt")) {
      int32_t cnt = gs_getint("maxt", trimmed, 0);
      int32_t n4 = gs_getint("maxt", trimmed, 1);
      ge3 = n4;
      int32_t off = gs_getint("maxt", trimmed, 2);
      for (int32_t k = 0; k < cnt; k++) {
        if (nob >= out_capacity) { ok = false; break; }
        cont_o_init_copy(&out_objects[nob], &base_models[85], k * 4800 + off,
                          m->ground - base_models[85].grat, n4, 90);
        nob++;
      }
      gs_wallz(t, -5000, 7100, n4 + 500, 600, (cnt * 4800) / 2 + off - 2400, (cnt * 4800) / 2, 90, 0);
    }
    if (starts_with(trimmed, "maxb")) {
      int32_t cnt = gs_getint("maxb", trimmed, 0);
      int32_t n6 = gs_getint("maxb", trimmed, 1);
      ge4 = n6;
      int32_t off = gs_getint("maxb", trimmed, 2);
      for (int32_t k = 0; k < cnt; k++) {
        if (nob >= out_capacity) { ok = false; break; }
        cont_o_init_copy(&out_objects[nob], &base_models[85], k * 4800 + off,
                          m->ground - base_models[85].grat, n6, -90);
        nob++;
      }
      gs_wallz(t, -5000, 7100, n6 - 500, 600, (cnt * 4800) / 2 + off - 2400, (cnt * 4800) / 2, -90, 0);
    }
  }
  vfs_free_lines(lines, line_count);

  medium_newpolys(m, ge2, ge1 - ge2, ge4, ge3 - ge4, t, notb);
  medium_newclouds(m, ge2, ge1, ge4, ge3);
  medium_newmountains(m, ge2, ge1, ge4, ge3);
  medium_newstars(m);
  trackers_devidetrackers(t, ge2, ge1 - ge2, ge4, ge3 - ge4);

  // GameSparker.java:2736-2737 -- the stage-select 3D preview's camera
  // center (medium.trx/trz's arming values). Computed here rather than
  // reconstructed from Trackers because devidetrackers's integer
  // division (trackers.c) loses the precision this needs.
  if (out_center_x) *out_center_x = (ge2 + ge1) / 2;
  if (out_center_z) *out_center_z = (ge3 + ge4) / 2;

  *out_count = nob;
  return ok;
}
