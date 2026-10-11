#include "replay_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// "NFMR", a version, the meta, then the ring car by car (nplayers of them),
// all in the machine's own byte order (the Switch and a PC agree).
#define REPLAY_MAGIC "NFMR"
#define REPLAY_VERSION 2   // 2: the stage script's looks after the cars

// A file, or with no file a buffer in memory (`len` its size to read, or
// its capacity while written).
typedef struct {
  FILE *f;
  bool ok;
  uint8_t *mem;
  size_t pos, len;
} Io;

static void io_w(Io *io, const void *p, size_t n) {
  if (!io->ok || !n) return;
  if (io->f) {
    if (fwrite(p, 1, n, io->f) != n) io->ok = false;
    return;
  }
  if (io->pos + n > io->len) {
    const size_t cap = io->pos + n > io->len * 2 ? io->pos + n : io->len * 2;
    uint8_t *m = realloc(io->mem, cap);
    if (!m) {
      io->ok = false;
      return;
    }
    io->mem = m, io->len = cap;
  }
  memcpy(io->mem + io->pos, p, n);
  io->pos += n;
}

static void io_r(Io *io, void *p, size_t n) {
  if (!io->ok || !n) return;
  if (io->f) {
    if (fread(p, 1, n, io->f) != n) io->ok = false;
    return;
  }
  if (io->pos + n > io->len) {
    io->ok = false;
    return;
  }
  memcpy(p, io->mem + io->pos, n);
  io->pos += n;
}

// ---- The stage script's looks ----------------------------------------------

ObjLook look_of(const ContO *co, int32_t fade) {
  ObjLook lk;
  memset(&lk, 0, sizeof(lk));
  lk.x = co->x, lk.y = co->y, lk.z = co->z;
  lk.xz = (int16_t)co->xz, lk.xy = (int16_t)co->xy, lk.zy = (int16_t)co->zy;
  lk.fade = (uint8_t)(fade < 0 ? 0 : fade > 255 ? 255 : fade);
  if (co->shadow) lk.flags |= LOOK_SHADOW;
  if (co->glowlines) {
    lk.flags |= LOOK_GLOW;
    for (int32_t q = 0; q < 3; q++) lk.glow[q] = (uint8_t)co->glowc[q];
  }
  if (co->flame_on) {
    lk.flags |= LOOK_FLAME;
    for (int32_t q = 0; q < 3; q++) lk.flame[q] = (uint8_t)co->flame_rgb[q];
  }
  // setfire's faces burn (embos 16..70) until unsetfire; the first one tells.
  for (int32_t i = 0; i < co->npl; i++) {
    const Plane *p = &co->p[i];
    if (p->wz == 0 || p->gr == -17 || p->gr == -16) {
      if (p->embos != 0) lk.flags |= LOOK_FIRE;
      break;
    }
  }
  return lk;
}

static bool look_eq(const ObjLook *a, const ObjLook *b) {
  return a->x == b->x && a->y == b->y && a->z == b->z && a->xz == b->xz && a->xy == b->xy && a->zy == b->zy &&
         a->fade == b->fade && a->flags == b->flags && memcmp(a->glow, b->glow, 3) == 0 &&
         memcmp(a->flame, b->flame, 3) == 0;
}

void looks_free(LookRing *l) {
  free(l->start);
  free(l->kept);
  for (int32_t k = 0; k < l->n; k++) free(l->ring[k]);
  memset(l, 0, sizeof(*l));
}

void looks_reset(LookRing *l, const ObjLook *now, int32_t nobj) {
  looks_free(l);
  l->start = nobj > 0 ? malloc(sizeof(ObjLook) * (size_t)nobj) : NULL;
  l->kept = nobj > 0 ? calloc((size_t)nobj, 1) : NULL;
  if (l->start && l->kept) memcpy(l->start, now, sizeof(ObjLook) * (size_t)nobj);
  l->nobj = l->start && l->kept ? nobj : 0;
}

void looks_rec(LookRing *l, const ObjLook *now, uint8_t outline) {
  // Newly changed objects join, their past as they started.
  for (int32_t i = 0; i < l->nobj && l->n < LOOK_MAX; i++) {
    if (l->kept[i] || look_eq(&now[i], &l->start[i])) continue;
    ObjLook *r = malloc(sizeof(ObjLook) * 300);
    if (!r) break;
    for (int32_t t = 0; t < 300; t++) r[t] = l->start[i];
    l->kept[i] = 1;
    l->idx[l->n] = i;
    l->ring[l->n++] = r;
  }
  l->head = (l->head + 1) % 300;
  for (int32_t k = 0; k < l->n; k++) l->ring[k][l->head] = now[l->idx[k]];
  l->outline[l->head] = outline;
}

void looks_copy(LookRing *to, const LookRing *from) {
  for (int32_t k = 0; k < to->n; k++) free(to->ring[k]);
  to->n = 0;
  to->head = from->head;
  memcpy(to->outline, from->outline, sizeof(to->outline));
  for (int32_t k = 0; k < from->n; k++) {
    ObjLook *r = malloc(sizeof(ObjLook) * 300);
    if (!r) break;
    memcpy(r, from->ring[k], sizeof(ObjLook) * 300);
    to->idx[to->n] = from->idx[k];
    to->ring[to->n++] = r;
  }
}

const ObjLook *looks_at(const LookRing *l, int32_t k, int32_t t) { return &l->ring[k][(l->head + 1 + t) % 300]; }
uint8_t looks_outline(const LookRing *l, int32_t t) { return l->outline[(l->head + 1 + t) % 300]; }

void look_apply(ContO *co, const ObjLook *lk, bool pose) {
  if (pose) {
    co->x = lk->x, co->y = lk->y, co->z = lk->z;
    co->xz = lk->xz, co->xy = lk->xy, co->zy = lk->zy;
  }
  co->fade = lk->fade;
  co->shadow = (lk->flags & LOOK_SHADOW) && co->stg;
  co->glowlines = (lk->flags & LOOK_GLOW) != 0;
  for (int32_t q = 0; q < 3; q++) co->glowc[q] = lk->glow[q], co->flame_rgb[q] = lk->flame[q];
  co->flame_on = (lk->flags & LOOK_FLAME) != 0;
  const bool fire = (lk->flags & LOOK_FIRE) != 0;
  for (int32_t i = 0; pose && i < co->npl; i++) {   // a car's wreck flames are the ring's
    Plane *p = &co->p[i];
    if (p->wz != 0 && p->gr != -17 && p->gr != -16) continue;
    if (fire && p->embos == 0) p->embos = 16;   // cont_o_setfire
    if (!fire) p->embos = 0;                    // career_stage.c's unsetfire
  }
}

// In order, oldest tick first, so a read needs no head.
static void looks_io(Io *io, LookRing *l, bool w, int32_t nobj) {
  int32_t n = l->n;
  if (w) io_w(io, &n, sizeof(n));
  else io_r(io, &n, sizeof(n));
  if (!io->ok || n < 0 || n > LOOK_MAX) {
    io->ok = false;
    return;
  }
  if (w) {
    uint8_t outline[300];
    for (int32_t t = 0; t < 300; t++) outline[t] = looks_outline(l, t);
    io_w(io, outline, sizeof(outline));
  } else {
    io_r(io, l->outline, sizeof(l->outline));
  }
  for (int32_t k = 0; k < n && io->ok; k++) {
    if (w) {
      io_w(io, &l->idx[k], sizeof(int32_t));
      for (int32_t t = 0; t < 300; t++) io_w(io, looks_at(l, k, t), sizeof(ObjLook));
      continue;
    }
    int32_t idx = -1;
    io_r(io, &idx, sizeof(idx));
    ObjLook *r = malloc(sizeof(ObjLook) * 300);
    if (!r) io->ok = false;
    if (r) io_r(io, r, sizeof(ObjLook) * 300);
    if (!io->ok || idx < 0 || idx >= nobj) {   // not one of this race's objects: dropped
      free(r);
      continue;
    }
    l->idx[l->n] = idx;
    l->ring[l->n++] = r;
  }
  if (!w) l->head = 299;
}

// Deflated: a tower's hundreds of pieces mostly sit still from tick to tick.
static void looks_save(Io *io, const LookRing *l) {
  Io m = {NULL, true, NULL, 0, 0};
  looks_io(&m, (LookRing *)l, true, 0);
  uLongf clen = compressBound((uLong)m.pos);
  uint8_t *c = m.ok ? malloc(clen) : NULL;
  if (!c || compress2(c, &clen, m.mem, (uLong)m.pos, 6) != Z_OK) {
    io->ok = false;
  } else {
    const uint32_t lens[2] = {(uint32_t)m.pos, (uint32_t)clen};
    io_w(io, lens, sizeof(lens));
    io_w(io, c, clen);
  }
  free(c);
  free(m.mem);
}

static void looks_load(Io *io, LookRing *l, int32_t nobj) {
  uint32_t lens[2] = {0, 0};
  io_r(io, lens, sizeof(lens));
  const size_t most = 4 + 300 + (size_t)LOOK_MAX * (4 + 300 * sizeof(ObjLook));
  if (!io->ok || lens[0] > most || lens[1] > compressBound((uLong)most)) {
    io->ok = false;
    return;
  }
  uint8_t *c = malloc(lens[1] ? lens[1] : 1), *raw = malloc(lens[0] ? lens[0] : 1);
  uLongf rl = lens[0];
  if (c && raw) io_r(io, c, lens[1]);
  else io->ok = false;
  if (io->ok && (uncompress(raw, &rl, c, lens[1]) != Z_OK || rl != lens[0])) io->ok = false;
  if (io->ok) {
    Io m = {NULL, true, raw, 0, rl};
    looks_io(&m, l, false, nobj);
    io->ok = m.ok;
  }
  free(c);
  free(raw);
}

// The ring's arrays, wherever they live (the live set or the h* twins).
typedef struct {
  int32_t (*pose[8])[NFM_MAX_CARS];   // x y z xy zy xz wxz wzy, [300][cars]
  int32_t *checkpoint;
  bool *lastcheck;
  int32_t *fix, *dest;
  int32_t (*sspark)[20][30], (*sx)[20][30], (*sy)[20][30], (*sz)[20][30], (*scx)[20][30], (*scz)[20][30];
  float (*smag)[20][30];
  int32_t (*rspark)[200], (*sprk)[200], (*srx)[200], (*sry)[200], (*srz)[200];
  float (*rcx)[200], (*rcy)[200], (*rcz)[200];
  int32_t (*ry)[4][7], (*magy)[4][7], (*rx)[4][7], (*magx)[4][7], (*rz)[4][7], (*magz)[4][7];
  bool (*mtouch)[7];
  ContO *start;
} Ring;

static Ring ring_of(Record *r, bool h) {
  Ring g;
  if (!h) {
    int32_t(*p[8])[NFM_MAX_CARS] = {r->x, r->y, r->z, r->xy, r->zy, r->xz, r->wxz, r->wzy};
    memcpy(g.pose, p, sizeof(p));
    g.checkpoint = r->checkpoint;
    g.lastcheck = r->lastcheck;
    g.fix = r->fix;
    g.dest = r->dest;
    g.sspark = r->sspark, g.sx = r->sx, g.sy = r->sy, g.sz = r->sz, g.scx = r->scx, g.scz = r->scz, g.smag = r->smag;
    g.rspark = r->rspark, g.sprk = r->sprk, g.srx = r->srx, g.sry = r->sry, g.srz = r->srz;
    g.rcx = r->rcx, g.rcy = r->rcy, g.rcz = r->rcz;
    g.ry = r->ry, g.magy = r->magy, g.rx = r->rx, g.magx = r->magx, g.rz = r->rz, g.magz = r->magz;
    g.mtouch = r->mtouch;
    g.start = r->car[0];
  } else {
    int32_t(*p[8])[NFM_MAX_CARS] = {r->hx, r->hy, r->hz, r->hxy, r->hzy, r->hxz, r->hwxz, r->hwzy};
    memcpy(g.pose, p, sizeof(p));
    g.checkpoint = r->hcheckpoint;
    g.lastcheck = r->hlastcheck;
    g.fix = r->hfix;
    g.dest = r->hdest;
    g.sspark = r->hsspark, g.sx = r->hsx, g.sy = r->hsy, g.sz = r->hsz, g.scx = r->hscx, g.scz = r->hscz;
    g.smag = r->hsmag;
    g.rspark = r->hrspark, g.sprk = r->hsprk, g.srx = r->hsrx, g.sry = r->hsry, g.srz = r->hsrz;
    g.rcx = r->hrcx, g.rcy = r->hrcy, g.rcz = r->hrcz;
    g.ry = r->hry, g.magy = r->hmagy, g.rx = r->hrx, g.magx = r->hmagx, g.rz = r->hrz, g.magz = r->hmagz;
    g.mtouch = r->hmtouch;
    g.start = r->starcar;
  }
  return g;
}

// One direction for both: `w` writes the ring out, else reads it in.
static void ring_io(Io *io, Ring *g, int32_t n, bool w) {
#define RW(p, len) (w ? io_w(io, (p), (len)) : io_r(io, (p), (len)))
  for (int32_t k = 0; k < 8; k++)
    for (int32_t t = 0; t < 300; t++) RW(g->pose[k][t], sizeof(int32_t) * (size_t)n);
  RW(g->checkpoint, sizeof(int32_t) * 300);
  RW(g->lastcheck, sizeof(bool) * 300);
  RW(g->fix, sizeof(int32_t) * (size_t)n);
  RW(g->dest, sizeof(int32_t) * (size_t)n);
#define CARS(a) RW((a), sizeof((a)[0]) * (size_t)n)
  CARS(g->sspark), CARS(g->sx), CARS(g->sy), CARS(g->sz), CARS(g->scx), CARS(g->scz), CARS(g->smag);
  CARS(g->rspark), CARS(g->sprk), CARS(g->srx), CARS(g->sry), CARS(g->srz), CARS(g->rcx), CARS(g->rcy), CARS(g->rcz);
  CARS(g->ry), CARS(g->magy), CARS(g->rx), CARS(g->magx), CARS(g->rz), CARS(g->magz), CARS(g->mtouch);
#undef CARS
#undef RW
}

// A car's faces as the ring found them: per face its points and colours.
static void shape_write(Io *io, const ContO *co) {
  io_w(io, &co->npl, sizeof(co->npl));
  for (int32_t i = 0; i < co->npl; i++) {
    const Plane *p = &co->p[i];
    io_w(io, &p->n, sizeof(p->n));
    io_w(io, p->ox, sizeof(int32_t) * (size_t)p->n);
    io_w(io, p->oy, sizeof(int32_t) * (size_t)p->n);
    io_w(io, p->oz, sizeof(int32_t) * (size_t)p->n);
    const int32_t v[7] = {p->c[0], p->c[1], p->c[2], p->bfase, p->embos, p->gr, p->flx};
    io_w(io, v, sizeof(v));
    io_w(io, p->hsb, sizeof(p->hsb));
  }
}

// Read back onto `co` when it is the same model (same faces, same points);
// skipped, model as it is, otherwise.
static void shape_read(Io *io, ContO *co) {
  int32_t npl = 0;
  io_r(io, &npl, sizeof(npl));
  for (int32_t i = 0; io->ok && i < npl; i++) {
    int32_t n = 0, ox[PLANE_MAX_N], oy[PLANE_MAX_N], oz[PLANE_MAX_N], v[7];
    float hsb[3];
    io_r(io, &n, sizeof(n));
    if (n < 0 || n > PLANE_MAX_N) {
      io->ok = false;
      return;
    }
    io_r(io, ox, sizeof(int32_t) * (size_t)n);
    io_r(io, oy, sizeof(int32_t) * (size_t)n);
    io_r(io, oz, sizeof(int32_t) * (size_t)n);
    io_r(io, v, sizeof(v));
    io_r(io, hsb, sizeof(hsb));
    if (!io->ok || npl != co->npl || co->p[i].n != n) continue;
    Plane *p = &co->p[i];
    memcpy(p->ox, ox, sizeof(int32_t) * (size_t)n);
    memcpy(p->oy, oy, sizeof(int32_t) * (size_t)n);
    memcpy(p->oz, oz, sizeof(int32_t) * (size_t)n);
    p->c[0] = v[0], p->c[1] = v[1], p->c[2] = v[2];
    p->bfase = v[3], p->embos = v[4], p->gr = v[5], p->flx = v[6];
    memcpy(p->hsb, hsb, sizeof(hsb));
  }
}

static bool header(Io *io, ReplayMeta *meta, bool w, int32_t *version_out) {
  char magic[4];
  int32_t version = REPLAY_VERSION;
  if (w) {
    io_w(io, REPLAY_MAGIC, 4);
    io_w(io, &version, sizeof(version));
    io_w(io, meta, sizeof(*meta));
  } else {
    io_r(io, magic, 4);
    io_r(io, &version, sizeof(version));
    io_r(io, meta, sizeof(*meta));
    if (memcmp(magic, REPLAY_MAGIC, 4) != 0 || version < 1 || version > REPLAY_VERSION) io->ok = false;
    if (version_out) *version_out = version;
    if (meta->nplayers < 1 || meta->nplayers > NFM_MAX_CARS) io->ok = false;
    meta->stage_name[sizeof(meta->stage_name) - 1] = '\0';
    meta->car_name[sizeof(meta->car_name) - 1] = '\0';
  }
  return io->ok;
}

bool replay_save(const char *path, const ReplayMeta *meta, const Record *r, bool highlight, const LookRing *looks) {
  Io io = {fopen(path, "wb"), true, NULL, 0, 0};
  if (!io.f) return false;
  ReplayMeta m = *meta;
  Ring g = ring_of((Record *)r, highlight);
  if (header(&io, &m, true, NULL)) {
    ring_io(&io, &g, m.nplayers, true);
    for (int32_t i = 0; i < m.nplayers; i++) shape_write(&io, &g.start[i]);
    LookRing none;
    memset(&none, 0, sizeof(none));
    looks_save(&io, looks ? looks : &none);
  }
  if (fclose(io.f) != 0) io.ok = false;
  if (!io.ok) remove(path);
  return io.ok;
}

bool replay_read_meta(const char *path, ReplayMeta *meta) {
  Io io = {fopen(path, "rb"), true, NULL, 0, 0};
  if (!io.f) return false;
  header(&io, meta, false, NULL);
  fclose(io.f);
  return io.ok;
}

bool replay_read(const char *path, ReplayMeta *meta, Record *r, LookRing *looks) {
  Io io = {fopen(path, "rb"), true, NULL, 0, 0};
  if (!io.f) return false;
  Ring g = ring_of(r, false);
  int32_t version = 0;
  // Kept objects are checked against this race's (looks->nobj, from looks_reset).
  for (int32_t k = 0; k < looks->n; k++) free(looks->ring[k]);
  looks->n = 0;
  if (header(&io, meta, false, &version)) {
    ring_io(&io, &g, meta->nplayers, false);
    for (int32_t i = 0; i < meta->nplayers && io.ok; i++) shape_read(&io, &r->car[0][i]);
    if (version >= 2 && io.ok) looks_load(&io, looks, looks->nobj);
  }
  fclose(io.f);
  if (io.ok) r->caught = 300;
  return io.ok;
}
