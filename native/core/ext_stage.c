// See ext_stage.h. Extended's loadstage, line for line where it decides what a
// stage is (which model, where, which checkpoint, which wall tracker), on
// this engine's own types. Left out, being Extended's career or its own
// renderer: the stage-number effects (water, snow, lightning...), floors
// and teleport behaviour (their data is kept), fadefrom's 12000 cap (this
// engine's fog keeps NFM 2's 8000) and its own wall/disline/wheel drawing.
#include "ext_stage.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vfs.h"

// Extended's model list, in its loadbase order (GameSparker.java:336).
static const char *const kExtModels[EXT_NUM_MODELS] = {
    "2000tornados", "formula7", "canyenaro", "lescrab", "nimi", "maxrevenge", "leadoxide", "koolkat", "drifter",
    "policecops", "mustang", "king", "audir8", "masheen", "radicalone", "drmonster", "newcar1", "newcar2",
    "newcar3", "newcar4", "secretcar1", "secretcar2", "secretcar3", "tornadoshark", "formula72", "wowcaninaro",
    "lavitacrab", "nimi2", "maxrevenge2", "leadoxide2", "koolkat2", "drifterx", "swordofjustice", "highrider",
    "elking", "mightyeight", "masheen2", "radicalone2", "drmonstaa", "road", "froad", "twister2", "twister1",
    "turn", "offroad", "bumproad", "offturn", "nroad", "nturn", "roblend", "noblend", "rnblend", "roadend",
    "offroadend", "hpground", "ramp30", "cramp35", "dramp15", "dhilo15", "slide10", "takeoff", "sramp22",
    "offbump", "offramp", "thewall", "halfpipe", "spikes", "rail", "sofframp", "checkpoint", "fixpoint",
    "offcheckpoint", "sideoff", "bsideoff", "uprise", "riseroad", "sroad", "soffroad", "2000tornadosB",
    "formula7B", "canyenaroB", "lescrabB", "nimiB", "maxrevengeB", "leadoxideB", "koolkatB", "drifterB",
    "policecopsB", "mustangB", "kingB", "audir8B", "masheenB", "radicaloneB", "drmonsterB", "newcar1B",
    "newcar2B", "newcar3B", "newcar4B", "secretcar1B", "secretcar2B", "secretcar3B", "tornadosharkB",
    "formula72B", "wowcaninaroB", "lavitacrabB", "nimi2B", "maxrevenge2B", "leadoxide2B", "koolkat2B",
    "drifterxB", "swordofjusticeB", "highriderB", "elkingB", "mightyeightB", "masheen2B", "radicalone2B",
    "drmonstaaB", "tree4", "tree6", "offhill", "spikefire", "railfire", "cactus", "roll1", "roll2", "roll3",
    "roll4", "roll5", "roll6",
};

static bool starts(const char *s, const char *prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

bool ext_loadbase(ContO *models, Medium *m, Trackers *t) {
  VfsZip zip;
  if (!vfs_read_zip("ext/data/models.radq", &zip)) return false;
  for (int32_t e = 0; e < zip.count; e++) {
    // Java: j = 0, then every name the entry starts with -- the last wins
    // ("roadend" over "road", "nimi2" over "nimi", the B models over theirs).
    int32_t slot = 0;
    for (int32_t k = 0; k < EXT_NUM_MODELS; k++) {
      if (starts(zip.entries[e].name, kExtModels[k])) slot = k;
    }
    char *text = vfs_entry_text(&zip.entries[e]);
    if (!text) continue;
    if (models[slot].p) cont_o_free(&models[slot]);
    // ContO.java:357-362: the beast models and the scenery after them read
    // div() and idiv() on a 6 scale, everything else on NFM 2's 10.
    cont_o_sfactor = (slot >= 78 && slot < 120) ? 6.0f : 10.0f;
    cont_o_init_buf(&models[slot], text, m, t);
    cont_o_sfactor = 10.0f;
    models[slot].baseIndex = slot;
    free(text);
  }
  vfs_free_zip(&zip);
  return true;
}

char *ext_stage_text(const char *pack, const char *entry) {
  char path[96];
  snprintf(path, sizeof(path), "ext/data/Files/%s.radq", pack);
  VfsZip zip;
  if (!vfs_read_zip(path, &zip)) return NULL;
  char *text = NULL;
  for (int32_t e = 0; e < zip.count; e++) {
    if (strcmp(zip.entries[e].name, entry) == 0) {
      text = vfs_entry_text(&zip.entries[e]);
      break;
    }
  }
  vfs_free_zip(&zip);
  if (text && strcmp(pack, "tracks") == 0) ext_renumber_old_stage(text);
  return text;
}

/** The line starting at `s` (after leading blanks) begins with `head` then
 * word characters then "(": returns the character after the "(", or NULL. */
static const char *directive_args(const char *s, const char *head, bool word_suffix) {
  while (*s == ' ' || *s == '\t') s++;
  if (!starts(s, head)) return NULL;
  s += strlen(head);
  if (word_suffix) while (isalnum((unsigned char)*s) || *s == '_') s++;
  return *s == '(' ? s + 1 : NULL;
}

bool ext_renumber_old_stage(char *text) {
  // Only a stage with a checkpoint on 44 (web/ext/stagecompat.js).
  bool old = false;
  for (const char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
    const char *a = directive_args(line, "chk", true);
    if (a && strncmp(a, "44,", 3) == 0) { old = true; break; }
  }
  if (!old) return false;
  char *out = text;
  const char *in = text;
  while (*in) {
    const char *eol = strchr(in, '\n');
    const size_t len = eol ? (size_t)(eol - in) + 1 : strlen(in);
    const char *a = directive_args(in, "set", true);
    if (!a) a = directive_args(in, "chk", true);
    if (!a) a = directive_args(in, "fix", false);
    if (!a) a = directive_args(in, "teleset", false);
    char *num_end = NULL;
    long id = a && isdigit((unsigned char)*a) ? strtol(a, &num_end, 10) : -1;
    if (a && id >= 0 && num_end && *num_end == ',') {
      const size_t head = (size_t)(a - in);
      // The new id is never longer than the old, so the text only moves
      // left; format it aside first (sprintf's NUL would land on the ',').
      char num[16];
      const int written = snprintf(num, sizeof(num), "%ld", id - 4);
      const size_t rest = len - (size_t)(num_end - in);
      memmove(out, in, head);
      memcpy(out + head, num, (size_t)written);
      memmove(out + head + (size_t)written, num_end, rest);
      out += head + (size_t)written + rest;
    } else {
      memmove(out, in, len);
      out += len;
    }
    in += len;
  }
  *out = '\0';
  return true;
}

/** GameSparker.getint (Extended :296): the n-th field after `type(`, read
 * the way Java does (a delimiter steps one character on). A field Java
 * would fail Integer.valueOf on sets *bad. */
static int32_t gi(const char *type, const char *s, int32_t idx, bool *bad) {
  const size_t len = strlen(s);
  char buf[24];
  int32_t bl = 0, k = 0;
  for (size_t j = strlen(type) + 1; j < len; j++) {
    if (s[j] == ',' || s[j] == ')') {
      k++;
      j++;
    }
    if (k == idx) {
      if (j >= len) { *bad = true; return 0; }   // charAt past the end
      if (bl < (int32_t)sizeof(buf) - 1) buf[bl++] = s[j];
    }
  }
  buf[bl] = '\0';
  char *end = NULL;
  const long v = strtol(buf, &end, 10);
  if (bl == 0 || *end != '\0' || isspace((unsigned char)buf[0])) { *bad = true; return 0; }
  return (int32_t)v;
}

static void gs(const char *type, const char *s, char *out, size_t out_len) {
  const size_t len = strlen(s);
  size_t o = 0;
  int32_t k = 0;
  for (size_t j = strlen(type) + 1; j < len; j++) {
    if (s[j] == ',' || s[j] == ')') {
      k++;
      j++;
    }
    if (k == 0 && j < len && o + 1 < out_len) out[o++] = s[j] == '|' ? ',' : s[j];
  }
  out[o] = '\0';
}

/** Java's String.trim(): every character <= ' ' off both ends, in place. */
static char *jtrim(char *s) {
  while (*s && (unsigned char)*s <= ' ') s++;
  char *e = s + strlen(s);
  while (e > s && (unsigned char)e[-1] <= ' ') e--;
  *e = '\0';
  return s;
}

/** The longest of `names` the line starts with (Java's chain of startsWith
 * checks, each later one overriding, picks exactly that), or NULL. */
static const char *pick(const char *s, const char *const *names, int32_t n) {
  const char *best = NULL;
  for (int32_t i = 0; i < n; i++) {
    if (starts(s, names[i]) && (!best || strlen(names[i]) > strlen(best))) best = names[i];
  }
  return best;
}

static void wall_tracker(Trackers *t, int32_t y, int32_t x, int32_t radx, int32_t z, int32_t radz, int32_t xy,
                         int32_t zy) {
  if (t->nt >= TRACKERS_MAX) return;   // the stage is refused below
  t->y[t->nt] = y;
  t->rady[t->nt] = 7100;
  t->x[t->nt] = x;
  t->radx[t->nt] = radx;
  t->z[t->nt] = z;
  t->radz[t->nt] = radz;
  t->xy[t->nt] = xy;
  t->zy[t->nt] = zy;
  t->dam[t->nt] = 1;   // Extended's walls: no 167 marker, no sector rule of their own
  t->decor[t->nt] = false;
  t->notwall[t->nt] = false;
  t->skd[t->nt] = 0;
  t->nt++;
}

bool ext_loadstage(ContO *out, int32_t cap, int32_t *out_count, ContO *models, Medium *m, Trackers *t,
                   CheckPoints *cp, const char *text, int32_t player_ext_car, ExtStageInfo *info) {
  memset(info, 0, sizeof(*info));
  t->nt = 0;
  m->resdown = 0;
  m->rescnt = 5;
  m->lightson = false;
  m->noelec = 0;
  m->ground = 250;
  m->trk = 0;
  cp->n = 0;
  cp->nsp = 0;
  cp->fn = 0;
  cp->wasted = 0;
  cp->catchfin = 0;
  cp->haltall = false;

  int32_t nob = 0, notb = 0;
  int32_t i = 0, j = 100, k = 0, l = 100;               // maxr, maxl, maxt, maxb
  int32_t wallr = 0, walll = 100, wallt = 0, wallb = 100;  // the same, igmax* not counted
  bool bad = false, ok = true;
  const ContO *wall = &models[EXT_MODEL_WALL];

  char *copy = malloc(strlen(text) + 1);
  strcpy(copy, text);
  char **lines;
  int32_t line_count;
  vfs_read_lines(copy, &lines, &line_count);
  free(copy);

#define ROOM() do { if (nob >= cap) { ok = false; break; } } while (0)
#define GI(type, idx) gi((type), s1, (idx), &bad)

  for (int32_t li = 0; li < line_count && ok && !bad; li++) {
    char *s1 = jtrim(lines[li]);

    if (starts(s1, "snap")) medium_setsnap(m, GI("snap", 0), GI("snap", 1), GI("snap", 2));
    if (starts(s1, "sky")) medium_setsky(m, GI("sky", 0), GI("sky", 1), GI("sky", 2));
    if (starts(s1, "ground")) medium_setgrnd(m, GI("ground", 0), GI("ground", 1), GI("ground", 2));
    if (starts(s1, "polys")) medium_setpolys(m, GI("polys", 0), GI("polys", 1), GI("polys", 2));
    if (starts(s1, "fog")) medium_setfade(m, GI("fog", 0), GI("fog", 1), GI("fog", 2));
    // Extended reads density as it is (NFM 2: 2n + 1, 1..30); a 0 would
    // divide nothing here but keeps the fog maths sane at 1.
    if (starts(s1, "density")) {
      m->fogd = GI("density", 0);
      if (m->fogd < 1) m->fogd = 1;
    }
    if (starts(s1, "fadefrom")) medium_fadfrom(m, GI("fadefrom", 0));
    if (starts(s1, "lightson")) m->lightson = true;
    if (starts(s1, "pile")) {
      ROOM();
      if (!ok) break;
      cont_o_init_pile(&out[nob], GI("pile", 0), GI("pile", 1), GI("pile", 2), m, t, GI("pile", 3), GI("pile", 4),
                       m->ground);
      nob++;
    }

    // fakewall*: the wall's look, no tracker (:617-666).
    static const char *const kFake[] = {"fakewallr", "fakewalll", "fakewallt", "fakewallb"};
    const char *fake = pick(s1, kFake, 4);
    if (fake) {
      const int32_t cnt = GI(fake, 0), at = GI(fake, 1), off = GI(fake, 2);
      const bool along_z = fake[8] == 'r' || fake[8] == 'l';
      for (int32_t q = 0; q < cnt && ok; q++) {
        ROOM();
        if (!ok) break;
        if (along_z) cont_o_init_copy(&out[nob], (ContO *)wall, at, m->ground - wall->grat, q * 4800 + off, 0);
        else cont_o_init_copy(&out[nob], (ContO *)wall, q * 4800 + off, m->ground - wall->grat, at, 90);
        out[nob].grounded = 1.0f;
        nob++;
      }
      notb = nob + 1;
    }

    static const char *const kSet[] = {"set", "setpoint", "setfloat", "setfloatpoint", "setfire", "setfade",
                                       "setfadepoint", "setfadefloat", "setcol", "setcolcode", "setfloatcolcode",
                                       "teleset"};
    const char *type = pick(s1, kSet, 12);
    if (type) {
      int32_t k6 = GI(type, 0);
      if (k6 == 666) k6 = -29 + player_ext_car;   // the player's own car
      if (k6 == 616) k6 = 49 + player_ext_car;    // its beast
      k6 += (k6 == 35) ? 33 : 29;
      if (k6 < 0 || k6 >= EXT_NUM_MODELS || !models[k6].p) { ok = false; break; }
      ROOM();
      if (!ok) break;
      const bool floating = !strcmp(type, "setfloat") || !strcmp(type, "setfadefloat") ||
                            !strcmp(type, "setfloatcolcode") || !strcmp(type, "teleset") ||
                            !strcmp(type, "setfloatpoint");
      const int32_t y = floating ? GI(type, 4) - models[k6].grat : m->ground - models[k6].grat;
      cont_o_init_copy(&out[nob], &models[k6], GI(type, 1), y, GI(type, 2), GI(type, 3));
      ContO *o = &out[nob];
      o->telechk = -1;
      if (!strcmp(type, "setfadefloat")) o->fade = 255 - GI(type, 5);
      if (!strcmp(type, "setfire")) {
        cont_o_setfire(o);
        o->flameheight = GI(type, 5);
        o->fade = 255 - GI(type, 4);
      }
      if (!strcmp(type, "setcol")) o->glowlines = true;
      if (!strcmp(type, "setcolcode") || !strcmp(type, "setfloatcolcode")) {
        const int32_t c = !strcmp(type, "setfloatcolcode") ? 1 : 0;
        o->glowlines = true;
        o->glowc[0] = GI(type, 4 + c);
        o->glowc[1] = GI(type, 5 + c);
        o->glowc[2] = GI(type, 6 + c);
      }
      if (!strcmp(type, "setfade") || !strcmp(type, "setfadepoint")) o->fade = 255 - GI(type, 4);
      if (cp->n < CHECK_POINTS_MAX) {
        cp->telefloor[cp->n] = -1;
        if (!strcmp(type, "teleset")) {
          cp->telefloor[cp->n] = GI(type, 5);
          o->telechk = GI(type, 5);
        }
      }
      if (strstr(s1, ")p") && cp->n < CHECK_POINTS_MAX) {
        cp->floor[cp->n] = 0;
        cp->x[cp->n] = GI(type, 1);
        cp->z[cp->n] = GI(type, 2);
        cp->rotation[cp->n] = GI(type, 3);
        cp->y[cp->n] = 0;
        if (!strcmp(type, "setfloat") || !strcmp(type, "setfadefloat") || !strcmp(type, "setfloatcolcode") ||
            !strcmp(type, "teleset")) {
          cp->y[cp->n] = GI(type, 4);
        }
        cp->typ[cp->n] = 0;
        if (strstr(s1, ")pt")) cp->typ[cp->n] = -1;
        if (strstr(s1, ")pr")) cp->typ[cp->n] = -2;
        if (strstr(s1, ")po")) cp->typ[cp->n] = -3;
        if (strstr(s1, ")ph")) cp->typ[cp->n] = -4;
        if ((!strcmp(type, "setpoint") || !strcmp(type, "setfadepoint") || !strcmp(type, "setfloatpoint")) &&
            info->numfixes < CHECK_POINTS_MAX_FIX) {
          info->fixpoint[info->numfixes++] = cp->n;
        }
        cp->n++;
        notb = nob + 1;
      }
      nob++;
    }

    static const char *const kChk[] = {"chk", "chkfloat", "specialchk", "chkcol", "chkcolcode", "chkfade", "telechk"};
    const char *type2 = pick(s1, kChk, 7);
    if (type2) {
      const int32_t l6 = GI(type2, 0) + 29;
      if (l6 < 0 || l6 >= EXT_NUM_MODELS || !models[l6].p) { ok = false; break; }
      ROOM();
      if (!ok) break;
      if (cp->n >= CHECK_POINTS_MAX) { ok = false; break; }
      const bool floating = !strcmp(type2, "chkfloat") || !strcmp(type2, "telechk");
      const int32_t y = floating ? GI(type2, 4) - models[l6].grat : m->ground - models[l6].grat;
      cont_o_init_copy(&out[nob], &models[l6], GI(type2, 1), y, GI(type2, 2), GI(type2, 3));
      ContO *o = &out[nob];
      cp->y[cp->n] = y;
      cp->x[cp->n] = GI(type2, 1);
      cp->z[cp->n] = GI(type2, 2);
      if (!strcmp(type2, "chkcol")) o->glowlines = true;
      if (!strcmp(type2, "chkfade")) o->fade = 255 - GI(type2, 4);
      cp->telefloor[cp->n] = -1;
      o->telechk = -1;
      if (!strcmp(type2, "telechk")) {
        cp->telefloor[cp->n] = GI(type2, 5);
        o->telechk = GI(type2, 5);
      }
      cp->floor[cp->n] = 0;
      if (!strcmp(type2, "chkcolcode")) {
        o->glowlines = true;
        o->glowc[0] = GI(type2, 4);
        o->glowc[1] = GI(type2, 5);
        o->glowc[2] = GI(type2, 6);
      }
      const int32_t rot = GI(type2, 3);
      cp->rotation[cp->n] = rot;
      if (cp->nsp < CHECK_POINTS_MAX) cp->chkcode[cp->nsp] = cp->n;
      if (strcmp(type2, "specialchk") != 0) {
        cp->typ[cp->n] = (rot == 0 || rot == 180) ? 1 : 2;
      } else {
        // A checkpoint that also repairs (typ 3/4), and an AI repair target.
        cp->typ[cp->n] = rot == 0 ? 3 : 4;
        if (info->numfixes < CHECK_POINTS_MAX_FIX) info->fixpoint[info->numfixes++] = cp->n;
      }
      cp->pcs = cp->n;
      cp->n++;
      o->checkpoint = cp->nsp + 1;
      cp->nsp++;
      nob++;
      notb = nob;
    }

    if (starts(s1, "fix")) {
      const int32_t i8 = GI("fix", 0) + 29;
      if (i8 < 0 || i8 >= EXT_NUM_MODELS || !models[i8].p) { ok = false; break; }
      ROOM();
      if (!ok) break;
      cont_o_init_copy(&out[nob], &models[i8], GI("fix", 1), GI("fix", 3), GI("fix", 2), GI("fix", 4));
      out[nob].elec = true;
      if (cp->fn < CHECK_POINTS_MAX_FIX) {
        cp->fx[cp->fn] = GI("fix", 1);
        cp->fz[cp->fn] = GI("fix", 2);
        cp->fy[cp->fn] = GI("fix", 3);
        cp->roted[cp->fn] = GI("fix", 4) != 0;
        cp->special[cp->fn] = strstr(s1, ")s") != NULL;
        cp->fn++;
      }
      if (GI("fix", 4) != 0) out[nob].roted = true;
      nob++;
      notb = nob;
    }
    if (starts(s1, "nlaps")) {
      // Extended takes it as it is: The Warzone's -1 is laps that never end
      // (a wasting arena). Only 0 would finish a race at the start.
      cp->nlaps = GI("nlaps", 0);
      if (cp->nlaps == 0) cp->nlaps = 1;
    }
    if (starts(s1, "name")) gs("name", s1, info->name, sizeof(info->name));
    if (starts(s1, "switch")) info->musicswitch = (int64_t)GI("switch", 0) * 1000000;
    if (starts(s1, "mountains")) m->mgen = GI("mountains", 0);
    if (starts(s1, "clouds")) {
      medium_setcloads(m, GI("clouds", 0), GI("clouds", 1), GI("clouds", 2), GI("clouds", 3), GI("clouds", 4));
    }

    // The boundary walls and their float / flame / see-through / ignored
    // variants (:917-1205). `side`: 0 r, 1 l, 2 t, 3 b.
    static const char *const kWall[4][5] = {{"maxr", "maxrfloat", "flamerw", "noseerw", "igmaxr"},
                                            {"maxl", "maxlfloat", "flamelw", "noseelw", "igmaxl"},
                                            {"maxt", "maxtfloat", "flametw", "noseetw", "igmaxt"},
                                            {"maxb", "maxbfloat", "flamebw", "noseebw", "igmaxb"}};
    for (int32_t side = 0; side < 4 && ok; side++) {
      const char *wt = pick(s1, kWall[side], 5);
      if (!wt) continue;
      const bool is_float = strstr(wt, "float") != NULL, flame = starts(wt, "flame"), nosee = starts(wt, "nosee"),
                 ig = starts(wt, "igmax");
      const int32_t cnt = GI(wt, 0), at = GI(wt, 1), off = GI(wt, 2);
      if (side == 0) { i = at; if (!ig) wallr = at; }
      if (side == 1) { j = at; if (!ig) walll = at; }
      if (side == 2) { k = at; if (!ig) wallt = at; }
      if (side == 3) { l = at; if (!ig) wallb = at; }
      const bool along_z = side < 2;   // r/l walls run along z at x = at; t/b along x at z = at
      for (int32_t q = 0; q < cnt && ok; q++) {
        ROOM();
        if (!ok) break;
        const int32_t y = is_float ? GI(wt, 3) - wall->grat : m->ground - wall->grat;
        if (along_z) cont_o_init_copy(&out[nob], (ContO *)wall, at, y, q * 4800 + off, 0);
        else cont_o_init_copy(&out[nob], (ContO *)wall, q * 4800 + off, y, at, 90);
        if (flame) {
          cont_o_setfire(&out[nob]);
          out[nob].fade = 255 - 50;
          out[nob].flameheight = GI(wt, 3);
        }
        if (nosee) {
          int32_t fadeness = GI(wt, 3);
          if (fadeness < 0) fadeness = 0;
          out[nob].fade = 255 - fadeness;
        }
        out[nob].wallpiece = true;
        nob++;
      }
      if (!ok) break;
      // A see-through wall with a negative value lets cars through.
      if (!nosee || GI(wt, 3) >= 0) {
        const int32_t ty = is_float ? -5000 + GI(wt, 3) : -5000;
        const int32_t half = cnt * 4800 / 2;
        if (side == 0) wall_tracker(t, ty, at + 500, 600, half + off - 2400, half, 90, 0);
        if (side == 1) wall_tracker(t, ty, at - 500, 600, half + off - 2400, half, -90, 0);
        if (side == 2) wall_tracker(t, ty, half + off - 2400, half, at + 500, 600, 0, 90);
        if (side == 3) wall_tracker(t, ty, half + off - 2400, half, at - 500, 600, 0, -90);
      }
      notb = nob;
    }
  }
#undef ROOM
#undef GI
  vfs_free_lines(lines, line_count);
  if (bad) ok = false;

  info->wallr = wallr;
  info->walll = walll;
  info->wallt = wallt;
  info->wallb = wallb;
  info->center_x = (j + i) / 2;
  info->center_z = (k + l) / 2;

  // :1287-1290: ground patches inside the walls; mountains, clouds and stars
  // around the stage. Then this engine's sector grid, by coverage.
  medium_newpolys(m, walll, wallr - walll, wallb, wallt - wallb, t, notb);
  medium_newclouds(m, j, i, l, k);
  medium_newmountains(m, j, i, l, k);
  medium_newstars(m);
  trackers_devidetrackers_cover(t, j, i - j, l, k - l);

  // Extended keeps going on a broken stage; this engine cannot race one
  // with no room left or under two checkpoints (mad_drive's checkpoint
  // loops never end on nsp 0).
  if (t->nt >= TRACKERS_MAX || cp->nsp < 2) ok = false;
  *out_count = nob;
  return ok;
}
