// See new_cars.h.
#include "new_cars.h"

#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "java_compat.h"
#include "vfs.h"


/** Revised and Recharged's cars: file, name (web carstore.RR_MODELS), and
 * for seven of them NFM World's numbers for the same cars as raw stats
 * (carstore.RR_STATS, verbatim; its models carry none). */
static const struct {
  const char *file;
  const char *name;
  const char *stats;
} kRR[] = {
    {"basicracer", "Basic Racer",
     NULL},
    {"dragster", "Turbo Dragster",
     "swits(50,200,400)\nacelf(22.0,14.0,10.0)\nhandb(20)\nairs(1.2)\nairc(30)\nturn(14)\ngrip(50.0)\nbounce(0.8)\nsimag(0.85)\nmoment(0.75)\ncomprad(0.4)\npush(2)\nrevpush(3)\nlift(30)\nrevlift(0)\npowerloss(4000000)\nflipy(-26)\nmsquash(3)\nclrad(1500)\ndammult(0.96)\nmaxmag(2000)\ndishandle(1.0)\noutdam(0.35)\nenginsignature(1)"},
    {"humvee", "Desert Humvee",
     NULL},
    {"lamborghini", "Lamborghini Gallardo",
     NULL},
    {"corvette", "Armored Corvette",
     NULL},
    {"radicalracer", "Radical Racer",
     NULL},
    {"saleens7twinturbo", "Saleen S7 Twin Turbo",
     NULL},
    {"stingrod", "Sting Rod",
     NULL},
    {"zonichtank", "Zonich Tank",
     NULL},
    {"matlostank", "Matlos Tank",
     NULL},
    {"rebound", "Air Rebound",
     "swits(70,210,285)\nacelf(5.0,5.0,5.0)\nhandb(17)\nairs(1.0)\nairc(64)\nturn(12)\ngrip(34.0)\nbounce(1.4)\nsimag(1.15)\nmoment(2.0)\ncomprad(0.5)\npush(3)\nrevpush(1)\nlift(0)\nrevlift(100)\npowerloss(4500000)\nflipy(-50)\nmsquash(0)\nclrad(12000)\ndammult(0.501)\nmaxmag(5000)\ndishandle(0.8)\noutdam(0.6)\nenginsignature(3)"},
    {"bugatti", "Bugatti Veyron",
     NULL},
    {"rocketking", "EL ROCKET KING",
     "swits(50,160,1000)\nacelf(9.0,9.0,9.0)\nhandb(10)\nairs(0.8)\nairc(10)\nturn(4)\ngrip(25.0)\nbounce(0.8)\nsimag(1.1)\nmoment(2.0)\ncomprad(1.0)\npush(4)\nrevpush(1)\nlift(0)\nrevlift(0)\npowerloss(4500000)\nflipy(-85)\nmsquash(10)\nclrad(7000)\ndammult(0.5)\nmaxmag(11700)\ndishandle(0.4)\noutdam(0.95)"},
    {"rocketmasheen", "ROCKET M A S H E E N",
     "swits(50,130,875)\nacelf(7.5,7.5,7.5)\nhandb(12)\nairs(0.3)\nairc(0)\nturn(5)\ngrip(27.0)\nbounce(0.8)\nsimag(1.3)\nmoment(3.0)\ncomprad(0.6)\npush(2)\nrevpush(2)\nlift(0)\nrevlift(0)\npowerloss(16700000)\nflipy(-100)\nmsquash(20)\nclrad(30000)\ndammult(0.176)\nmaxmag(31000)\ndishandle(0.42)\noutdam(1.0)"},
    {"rocketmonster", "DR Rocket Monstaa",
     "swits(80,200,1000)\nacelf(12.0,12.0,12.0)\nhandb(7)\nairs(1.0)\nairc(60)\nturn(6)\ngrip(27.0)\nbounce(1.15)\nsimag(1.15)\nmoment(2.0)\ncomprad(0.8)\npush(2)\nrevpush(1)\nlift(0)\nrevlift(32)\npowerloss(5500000)\nflipy(-127)\nmsquash(8)\nclrad(5000)\ndammult(0.46)\nmaxmag(19000)\ndishandle(0.95)\noutdam(1.0)"},
    {"awesomeradicalone", "The Awesome Radical One",
     "swits(80,200,1000)\nacelf(22.0,14.0,14.0)\nhandb(60)\nairs(2.0)\nairc(100)\nturn(100)\ngrip(100.0)\nbounce(1.1)\nsimag(0.9)\nmoment(1.5)\ncomprad(0.5)\npush(2)\nrevpush(2)\nlift(30)\nrevlift(0)\npowerloss(4100000)\nflipy(-30)\nmsquash(3)\nclrad(4000)\ndammult(0.8266)\nmaxmag(4400)\ndishandle(1.0)\noutdam(0.75)"},
    {"overkill", "Over=Kill",
     "swits(200,300,500)\nacelf(30.0,30.0,30.0)\nhandb(60)\nairs(1.3)\nairc(200)\nturn(72)\ngrip(75.0)\nbounce(1.0)\nsimag(1.5)\nmoment(2.0)\ncomprad(0.25)\npush(3)\nrevpush(1)\nlift(0)\nrevlift(0)\npowerloss(2147483647)\nflipy(-60)\nmsquash(1)\nclrad(4000)\ndammult(1.0)\nmaxmag(30000)\ndishandle(1.0)\noutdam(1.0)\nenginsignature(1)"},
    {"tacticalnuke", "Tactical Nuke",
     NULL},
    {"phantom", "The Phantom",
     NULL},
    {"lightning", "Lightning Rod",
     NULL},
    {"epictank", "EPIC TANK",
     NULL},
    {"killomatic", "KILL-O-MATIC",
     NULL},
    {"destroyer", "The Destroyer",
     NULL},
    {"train", "TRAIN of TERROR",
     NULL},
    {"A-1", "A-1",
     NULL},
};

/** `name(a)` read as one int in [lo, hi]; -1 when missing or out of range. */
static int32_t ext_line(const char *text, const char *name, int32_t lo, int32_t hi) {
  char key[24];
  snprintf(key, sizeof(key), "\n%s(", name);
  const char *p = strstr(text, key);
  if (!p && strncmp(text, key + 1, strlen(key + 1)) == 0) p = text - 1;
  if (!p) return -1;
  int v;
  if (sscanf(p + strlen(key), "%d", &v) != 1 || v < lo || v > hi) return -1;
  return v;
}

// ---- the Car Maker's Extended stats (web/ext/extlines.js forLoadstat) ----
// extstat(5) and extphysics(11) are the car's own stats and handling in
// Extended: when both are there and valid, Extended loads the car with them
// in place of stat() and physics()'s first 11 values (the crash look, engine
// and actmag stay the shared ones). NFM 2 never reads a line starting with e.

/** The first line whose trimmed form starts with `name(` (rad.js findLine):
 * where it starts and how long it is, trimmed. */
static bool find_line(const char *text, const char *name, const char **start, size_t *len) {
  const size_t nl = strlen(name);
  for (const char *l = text; l && *l;) {
    const char *eol = strchr(l, '\n');
    const char *e = eol ? eol : l + strlen(l);
    const char *a = l;
    while (a < e && isspace((unsigned char)*a)) a++;
    const char *b = e;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    if ((size_t)(b - a) > nl && strncmp(a, name, nl) == 0 && a[nl] == '(') {
      *start = a;
      *len = (size_t)(b - a);
      return true;
    }
    l = eol ? eol + 1 : NULL;
  }
  return false;
}

/** rad.js argsOf: the comma-separated pieces between the first ( and the
 * last ), trimmed, into out (each cut to 31 chars); how many, -1 when no ( ). */
static int32_t line_args(const char *line, size_t len, char out[][32], int32_t max) {
  const char *a = memchr(line, '(', len);
  const char *b = line + len;
  while (b > line && b[-1] != ')') b--;
  if (!a || b == line || b - 1 < a) return -1;
  b--;   // on the )
  int32_t n = 0;
  for (const char *p = a + 1;;) {
    const char *c = memchr(p, ',', (size_t)(b - p));
    const char *e = c ? c : b;
    const char *x = p, *y = e;
    while (x < y && isspace((unsigned char)*x)) x++;
    while (y > x && isspace((unsigned char)y[-1])) y--;
    if (n < max) {
      const size_t k = (size_t)(y - x) < 31 ? (size_t)(y - x) : 31;
      memcpy(out[n], x, k);
      out[n][k] = '\0';
    }
    n++;
    if (!c) break;
    p = c + 1;
  }
  return n;
}

/** extlines.js ints(): exactly n integers in [lo, hi]. 1 valid, 0 missing, -1 invalid. */
static int32_t ext_ints(const char *text, const char *name, int32_t n, int32_t lo, int32_t hi, int32_t *v) {
  const char *l;
  size_t len;
  if (!find_line(text, name, &l, &len)) return 0;
  char a[24][32];
  if (line_args(l, len, a, 24) != n) return -1;
  for (int32_t i = 0; i < n; i++) {
    const char *q = a[i] + (a[i][0] == '-');
    if (!*q) return -1;
    for (; *q; q++)
      if (!isdigit((unsigned char)*q)) return -1;
    const long x = strtol(a[i], NULL, 10);
    if (x < lo || x > hi) return -1;
    v[i] = (int32_t)x;
  }
  return 1;
}

/** rad.js setLine: the first `name(` line becomes `repl` (later copies
 * dropped), or `repl` is appended. A new string. */
static char *set_line(const char *text, const char *name, const char *repl) {
  const size_t tl = strlen(text), rl = strlen(repl), nl = strlen(name);
  char *out = malloc(tl + rl + 4);
  if (!out) return NULL;
  const char *found;
  size_t flen;
  if (!find_line(text, name, &found, &flen)) {
    // Appended after the text, its trailing blanks gone, past an empty line.
    size_t n = tl;
    while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
    memcpy(out, text, n);
    out[n] = '\n';
    out[n + 1] = '\n';
    memcpy(out + n + 2, repl, rl);
    out[n + 2 + rl] = '\n';
    out[n + 3 + rl] = '\0';
    return out;
  }
  size_t o = 0;
  bool done = false, first = true;
  for (const char *l = text;;) {
    const char *eol = strchr(l, '\n');
    const char *e = eol ? eol : l + strlen(l);
    const char *a = l;
    while (a < e && isspace((unsigned char)*a)) a++;
    const bool hit = (size_t)(e - a) > nl && strncmp(a, name, nl) == 0 && a[nl] == '(';
    if (!hit || !done) {
      if (!first) out[o++] = '\n';
      first = false;
      if (hit) {
        memcpy(out + o, repl, rl);
        o += rl;
        done = true;
      } else {
        memcpy(out + o, l, (size_t)(e - l));
        o += (size_t)(e - l);
      }
    }
    if (!eol) break;
    l = eol + 1;
  }
  out[o] = '\0';
  return out;
}

/** forLoadstat: the text loadstat reads in Extended (NULL: the file's own). */
static char *ext_for_loadstat(const char *text) {
  int32_t stat[5], phys[11];
  if (ext_ints(text, "extstat", 5, 16, 200, stat) != 1 || ext_ints(text, "extphysics", 11, 0, 100, phys) != 1)
    return NULL;
  // readPhysics: the shared crash look, engine and actmag (defaults 50,50,50 / 0 / 0)
  int32_t crash[3] = {50, 50, 50}, engsel = 0, actmag = 0;
  const char *l;
  size_t len;
  if (find_line(text, "physics", &l, &len)) {
    char a[24][32];
    const int32_t n = line_args(l, len, a, 24);
    if (n >= 15) {
      int32_t v[16] = {0};
      for (int32_t i = 0; i < 16 && i < n; i++) {
        char *end;
        const double d = strtod(a[i], &end);
        v[i] = (end != a[i] && *end == '\0' && isfinite(d)) ? jtrunc_d(d) : 0;   // Number(), trunc
      }
      for (int32_t i = 0; i < 3; i++) crash[i] = v[11 + i];
      engsel = v[14] < 0 ? 0 : (v[14] > 4 ? 4 : v[14]);
      actmag = n > 15 ? v[15] : 0;
    }
  }
  char sl[96], pl[256];
  snprintf(sl, sizeof(sl), "stat(%d,%d,%d,%d,%d)", (int)stat[0], (int)stat[1], (int)stat[2], (int)stat[3],
           (int)stat[4]);
  int o = snprintf(pl, sizeof(pl), "physics(");
  for (int32_t i = 0; i < 11; i++) o += snprintf(pl + o, sizeof(pl) - (size_t)o, "%d,", (int)phys[i]);
  snprintf(pl + o, sizeof(pl) - (size_t)o, "%d,%d,%d,%d,%d)", (int)crash[0], (int)crash[1], (int)crash[2],
           (int)engsel, (int)actmag);
  char *t1 = set_line(text, "stat", sl);
  if (!t1) return NULL;
  char *t2 = set_line(t1, "physics", pl);
  free(t1);
  return t2;
}

/** "saleens7twinturbo.rad" -> "Saleens7twinturbo". */
static void display_name(const char *file, char *out, size_t outsz) {
  const char *base = strrchr(file, '/');
  base = base ? base + 1 : file;
  snprintf(out, outsz, "%s", base);
  char *dot = strrchr(out, '.');
  if (dot) *dot = '\0';
  if (out[0]) out[0] = (char)toupper((unsigned char)out[0]);
}

bool new_car_from_rad(const char *name, const char *text, ContO *model, CarDefine *cd, int32_t i, Medium *m,
                      Trackers *t, bool stock, NewCarInfo *out) {
  if (!strstr(text, "stat(") && !strstr(text, "maxmag(")) return false;   // no stats: not raceable
  const bool loadnew = m->loadnew;
  m->loadnew = true;
  cont_o_init_buf(model, text, m, t);
  if (model->errd && stock) {
    // The Car Maker's checks (wheels on the ground, in reach) are its own:
    // a game's model loads without them, as R&R's own ROCKET M A S H E E N
    // (idiv(295) puts its wheels' ground at 144, past the 140 allowed).
    cont_o_free(model);
    m->loadnew = false;
    cont_o_init_buf(model, text, m, t);
  }
  m->loadnew = loadnew;
  const int32_t slot = NEW_CAR_FIRST + i;
  // Extended's own stats and handling, when the Car Maker's Extended tab set them.
  char *ext_text = ext_for_loadstat(text);
  const bool loaded = !model->errd && model->npl > 60 &&
                      car_define_loadcar(cd, ext_text ? ext_text : text, model, model->maxR, model->roofat, model->wh, slot);
  free(ext_text);
  if (!loaded) {
    cont_o_free(model);
    return false;
  }
  // The Car Maker's Extended tab (web/ext/extlines.js): health and damage as
  // percentages of NFM 2's, and the donor.
  const int32_t health = ext_line(text, "exthealth", 50, 300);
  const int32_t damage = ext_line(text, "extdamage", 50, 200);
  if (health >= 0) cd->maxmag[slot] = jtrunc((float)cd->maxmag[slot] * (float)health / 100.0f);
  if (damage >= 0) cd->dammult[slot] = cd->dammult[slot] * (float)damage / 100.0f;
  const int32_t special = ext_line(text, "extspecial", 0, 38);
  int32_t donor;
  if (special >= 0) {
    donor = special >= 23 ? special - 23 : special + 16;   // Extended's number -> this port's
  } else {
    // By class, as newcars-stats.js's defaultDonor: the first NFM 2 car of it.
    static const int32_t kByClass[5] = {0, 5, 6, 10, 11};
    const int32_t c = cd->cclass[slot];
    donor = kByClass[c >= 0 && c < 5 ? c : 0];
  }
  memset(out, 0, sizeof(*out));
  display_name(name, out->name, sizeof(out->name));
  out->donor = donor;
  // rad.js readAuthor: carmaker(name), between the first ( and the last ), trimmed.
  const char *cl;
  size_t clen;
  if (find_line(text, "carmaker", &cl, &clen)) {
    const char *a = memchr(cl, '(', clen);
    const char *b = cl + clen;
    while (b > cl && b[-1] != ')') b--;
    if (a && b > a + 1) {
      a++;
      b--;
      while (a < b && isspace((unsigned char)*a)) a++;
      while (b > a && isspace((unsigned char)b[-1])) b--;
      const size_t k = (size_t)(b - a) < sizeof(out->author) - 1 ? (size_t)(b - a) : sizeof(out->author) - 1;
      memcpy(out->author, a, k);
      out->author[k] = '\0';
    }
  }
  // Their car-select bars: Extended's Control from grip, NFM 2's Endurance
  // as loadstat set it.
  cd->dishandle[slot] = (cd->grip[slot] - 10.0f) / 20.0f;
  return true;
}

static void add(ContO *models, CarDefine *cd, Medium *m, Trackers *t, const char *name, const char *text, bool sd) {
  if (g_new_car_count >= NEW_CARS_MAX) return;
  const int32_t i = g_new_car_count;
  NewCarInfo info;
  if (!new_car_from_rad(name, text, &models[i], cd, i, m, t, !sd, &info)) {
    fprintf(stderr, "new car %s skipped: not a car the game can drive\n", name);
    return;
  }
  info.from_sd = sd;
  // Revised and Recharged's are all Ryan Albano's (the web credits them the
  // same way, carstore.readModel's appended carmaker()).
  if (!sd && !info.author[0]) snprintf(info.author, sizeof(info.author), "Ryan Albano");
  g_new_cars[i] = info;
  g_new_car_count++;
}

int32_t new_cars_load(ContO *models, CarDefine *cd, Medium *m, Trackers *t, const char *sd_dir) {
  g_new_car_count = 0;
  // Revised and Recharged: its new cars only (its 0-15 are NFM 2's sixteen);
  // the 15 with no numbers anywhere stay out, as in the web port.
  VfsZip zip;
  if (vfs_read_zip("ext/recharged/models.radq", &zip)) {
    for (int32_t e = 0; e < zip.count; e++) {
      for (size_t k = 0; k < sizeof(kRR) / sizeof(kRR[0]); k++) {
        char want[48];
        snprintf(want, sizeof(want), "%s.rad", kRR[k].file);
        if (strcmp(zip.entries[e].name, want) != 0) continue;
        char *text = vfs_entry_text(&zip.entries[e]);
        if (!text) break;
        if (kRR[k].stats) {
          const size_t len = strlen(text), add_len = strlen(kRR[k].stats);
          char *more = realloc(text, len + add_len + 3);
          if (more) {
            text = more;
            snprintf(text + len, add_len + 3, "\n%s\n", kRR[k].stats);
          }
        }
        if (strstr(text, "stat(") || strstr(text, "maxmag(")) add(models, cd, m, t, kRR[k].name, text, false);
        free(text);
        break;
      }
    }
    vfs_free_zip(&zip);
  }
  // The player's own: every .rad in the SD card's cars folder.
  if (sd_dir) {
    DIR *dir = opendir(sd_dir);
    if (dir) {
      struct dirent *de;
      while ((de = readdir(dir)) != NULL) {
        const size_t n = strlen(de->d_name);
        if (n < 5 || strcmp(de->d_name + n - 4, ".rad") != 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", sd_dir, de->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        const long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 0 && len < 4 * 1024 * 1024) {
          char *text = malloc((size_t)len + 2);
          text[0] = '\n';   // ext_line looks for "\nname("
          if (fread(text + 1, 1, (size_t)len, f) == (size_t)len) {
            text[len + 1] = '\0';
            add(models, cd, m, t, de->d_name, text + 1, true);
          }
          free(text);
        }
        fclose(f);
      }
      closedir(dir);
    }
  }
  return g_new_car_count;
}
