#include <dirent.h>
#include <stdarg.h>

// The Gallery (main menu > Gallery): every car to turn around, read about,
// dent, wreck and fix, in its Shadow, Beast and Undead looks; and every
// stage's music to play. This port's own screens, #included by game.c before
// game_run(), whose locals reach them through GalCtx.

enum { GAL_TABS = 5, GAL_ACTIONS = 5, GAL_LOOKS = 5, GAL_LIST_ROWS = 6, GAL_MUSIC_ROWS = 14 };
enum { GAL_HUB_ROWS = 5, GAL_HUB_Y = 66, GAL_HUB_STEP = 66 };
static const char *const kGalTabs[GAL_TABS] = {"Info", "Stats", "Special", "Perks", "Facts"};
static const char *const kGalActions[GAL_ACTIONS] = {"Hit", "Wreck", "Fix", "Look", "Zoom"};
static const char *const kGalLooks[GAL_LOOKS] = {"Normal", "Shadow", "Beast", "Undead", "Special"};
static const int32_t kGalFilters[] = {CF_ALL, CF_CLASSIC, CF_EXTENDED, CF_RR, CF_CUSTOM};
#define GAL_FILTERS ((int32_t)(sizeof(kGalFilters) / sizeof(kGalFilters[0])))
static const int32_t kGalZoom[3] = {95, 130, 70};   // the car's radius on screen, px
#define GAL_TRACK_INTERFACE 400   // music/interface.zip, already rendered at boot

typedef struct {
  Graphics2D *g;
  Medium *m;
  CarDefine *cd;
  Record *rpd;
  Audio *audio;
  RadicalTrack *interface_music;
  int32_t *stage_music_loaded_for;
  ContO *base_models, *ext_models, *new_models;
  const bool *held, *prev;
} GalCtx;

typedef struct {
  int32_t id;   // load_music_track's, 0 for a group's heading
  char label[80];
} GalTrack;

static struct {
  int32_t hub;   // Cars, Music, Back
  int32_t filter, sel, top;
  int32_t list[NEW_CAR_FIRST + NEW_CARS_MAX], nlist;
  // The car on the stand: a copy of its model, so dents never reach the
  // shared one, and a Mad of its own to take the hits.
  int32_t cn, tab, action, look, zoom, yaw, pitch;
  ContO co;
  Mad mad;
  XtGraphicsStub xt;
  CarDefine cd;
  bool wrecked;
  int32_t npl[CAR_FACTS_BASE];
  bool npl_ok;
  bool drag;
  int32_t drag_x, drag_y;
  GalTrack tracks[128];
  int32_t ntracks, msel, mtop, playing, pending;
  RadicalTrack track;
} gal = {.playing = -1, .pending = -1};

#define GAL_EDGE(c, b) ((c)->held[(b)] && !(c)->prev[(b)])

static ContO *gal_model(const GalCtx *c, int32_t cn) {
  if (cn >= NEW_CAR_FIRST) return &c->new_models[cn - NEW_CAR_FIRST];
  return cn < EXT_FIRST_CAR ? &c->base_models[cn] : &c->ext_models[cn - EXT_FIRST_CAR];
}

static bool gal_new_car(int32_t cn) { return cn >= NEW_CAR_FIRST && cn < NEW_CAR_FIRST + g_new_car_count; }

/** Words of `s` (translated) over lines at most `w` wide from (x, y), `lh`
 * apart; returns the y after the last. */
static int32_t gal_text(Graphics2D *g, const char *s, int32_t x, int32_t y, int32_t w, int32_t lh) {
  const char *p = tr(s);
  char line[200];
  while (*p) {
    const char *end = NULL, *q = p;
    for (;;) {
      const char *we = q;
      while (*we && *we != ' ') we++;
      const size_t len = (size_t)(we - p);
      if (len >= sizeof(line)) break;
      memcpy(line, p, len);
      line[len] = '\0';
      if (end && font_width(line) > w) break;
      end = we;
      if (!*we) break;
      q = we + 1;
    }
    if (!end) end = p + strlen(p);
    const size_t len = (size_t)(end - p) < sizeof(line) ? (size_t)(end - p) : sizeof(line) - 1;
    memcpy(line, p, len);
    line[len] = '\0';
    font_draw(g, line, x, y);
    y += lh;
    p = end;
    while (*p == ' ') p++;
  }
  return y;
}

/** The Settings screen's beige stripes and title bar. */
static void gal_backdrop(Graphics2D *g, const char *title) {
  gfx_set_color(g, SET_BG);
  gfx_fill_rect(g, 0, 0, 800, 450);
  gfx_set_color(g, SET_BG_DARK);
  for (int32_t x = 60; x < 800; x += 120) gfx_fill_rect(g, x, 0, 60, 450);
  gfx_set_color(g, 17, 17, 17);
  gfx_fill_rect(g, 120, 22, 560, 32);
  gfx_set_color(g, SET_YELLOW);
  font_set(FONT_BOLD, 22);
  draw_centered(g, title, 400, 46);
}

static void gal_hint(Graphics2D *g, const char *hint) {
  gfx_set_color(g, 90, 88, 80);
  font_set(FONT_BOLD, 12);
  draw_centered(g, hint, 400, 438);
}

/** A Settings-style row: black border, beige face, hazard stripes when chosen. */
static void gal_row(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h, bool sel) {
  gfx_set_color(g, 17, 17, 17);
  gfx_fill_rect(g, x, y, w, h);
  if (sel) {
    draw_hazard(g, x + 3, y + 3, 18, h - 6);
  } else {
    gfx_set_color(g, SET_BG);
    gfx_fill_rect(g, x + 3, y + 3, w - 6, h - 6);
  }
}

// ---- The car list ---------------------------------------------------------

static void gal_build_list(void) {
  gal.nlist = 0;
  const int32_t last = car_last_index(false);
  for (int32_t cn = 0; cn <= last; cn++) {
    if (cn == CUSTOM_CAR_INDEX) continue;   // Simple_Car.rad: the Car Maker's sample, not a car
    if (car_in_filter(cn, kGalFilters[gal.filter])) gal.list[gal.nlist++] = cn;
  }
  if (gal.sel >= gal.nlist) gal.sel = gal.nlist > 0 ? gal.nlist - 1 : 0;
  if (gal.top > gal.sel) gal.top = gal.sel;
}

static const char *gal_origin(int32_t cn) {
  if (cn < 16) return "Need for Madness 2";
  if (cn < CAR_COUNT) return "Need for Madness 2 Extended";
  if (gal_new_car(cn)) return g_new_cars[cn - NEW_CAR_FIRST].from_sd ? "Your SD card" : "Revised and Recharged";
  return "";
}

static const char *gal_creator(int32_t cn) {
  const char *by = car_author(cn);
  if (by) return by;
  return cn < 16 ? "Radicalplay (Omar Waly)" : "Unknown";
}

// ---- The inspector ----------------------------------------------------------

/** Puts car `cn` on the stand in look `gal.look`: a fresh copy of its model
 * (its beast's for the Beast look), undented, and a Mad for it. */
static void gal_load_car(GalCtx *c) {
  ContO *base = gal_model(c, gal.cn);
  if (gal.look == 2 && gal.cn < CAR_COUNT) base = &c->ext_models[EXT_MODEL_BEAST + ext_car_of(gal.cn)];
  if (!base->p) base = gal_model(c, gal.cn);
  cont_o_recopy(&gal.co, base, 0, 0, 0, 0);
  gal.co.shadow = false;
  gal.co.fade = gal.look == 1 ? 255 - 80 : 0;   // RACE_LOOK's Shadow
  if (gal.look == 3) {
    // The career's undead (career_stage.c 1433-1444): burning, blue flames.
    cont_o_setfire(&gal.co);
    gal.co.flame_on = true;
    gal.co.flame_rgb[0] = 50;
    gal.co.flame_rgb[1] = 180;
    gal.co.flame_rgb[2] = 255;
  }
  if (gal.look == 4) {
    // A running special's red outline (specials.c 480).
    gal.co.spec_on = true;
    gal.co.spec[0] = 185;
    gal.co.spec[1] = 0;
    gal.co.spec[2] = 0;
  }
  gal.cd = *c->cd;
  xt_graphics_stub_init(&gal.xt);
  gal.xt.extended = true;
  gal.xt.im = 0;
  mad_init(&gal.mad, &gal.cd, c->m, c->rpd, &gal.xt, 0);
  gal.mad.cn = gal.cn;
  gal.mad.mtouch = true;   // regy dents a car standing on its wheels
  gal.wrecked = false;
}

static void gal_open_car(GalCtx *c, int32_t cn) {
  gal.cn = cn;
  gal.look = 0;
  gal.yaw = -130;
  gal.pitch = 0;
  gal.action = 0;
  gal_load_car(c);
}

/** The car turned to (yaw, pitch), its radius `px` pixels, centred on (sx, sy):
 * draw_car_thumb's camera, moved. */
static void gal_draw_car(GalCtx *c, int32_t sx, int32_t sy, int32_t px) {
  Medium *m = c->m;
  m->trk = 0;
  m->crs = true;
  m->x = -sx;
  m->y = -sy;
  m->z = -50;
  m->xz = 0;
  m->zy = 0;
  m->ground = 495;
  m->ih = 0;
  m->iw = 0;
  m->w = 800;
  m->h = 450;
  m->focus_point = 400;
  m->cx = sx;
  m->cy = sy;
  m->cz = 50;
  ContO *car = &gal.co;
  // Scaled by the car's own model, so its beast stands bigger.
  const ContO *own = gal_model(c, gal.cn);
  const int32_t r = own->maxR > 0 ? own->maxR : car->maxR > 0 ? car->maxR : 300;
  car->x = 0;
  car->y = -(int32_t)car->grat / 2;
  car->z = r * 400 / px;
  car->xz = gal.yaw;
  car->zy = gal.pitch;
  car->xy = 0;
  car->wzy = 0;
  nfm_set_draw_phase(true);
  cont_o_d(car, c->g);
  nfm_set_draw_phase(false);
}

static void gal_wreck(void) {
  if (gal.wrecked) return;
  mad_distruct(&gal.mad, &gal.co);   // plane_d plays the rest: the blast, the smoke, the fire
  gal.wrecked = true;
}

/** A hit at a random corner from a random side, as hard as a crash at speed. */
static void gal_hit(void) {
  if (gal.wrecked || gal.co.fix) return;
  const int32_t corner = (int32_t)(nfm_random() * 4.0) & 3;
  float force = 600.0f + (float)(nfm_random() * 1900.0);
  if (nfm_random() < 0.5) force = -force;
  gal.co.xz = gal.yaw;
  gal.co.zy = 0;
  const double kind = nfm_random();
  if (kind < 0.4) mad_regx(&gal.mad, corner, force, &gal.co);
  else if (kind < 0.8) mad_regz(&gal.mad, corner, force, &gal.co);
  else mad_regy(&gal.mad, corner, fabsf(force), &gal.co);
  // As in a race: past its health the car is wasted.
  if (gal.mad.hitmag >= gal.cd.maxmag[gal.cn]) gal_wreck();
}

static void gal_act(GalCtx *c) {
  switch (gal.action) {
    case 0: gal_hit(); break;
    case 1: gal_wreck(); break;
    case 2:
      // The "Car Fixed" flash (cont_o_fixit), then a clean model.
      if (!gal.co.fix) {
        gal.co.fix = true;
        gal.co.fcnt = 0;
      }
      break;
    case 3:
      gal.look = (gal.look + 1) % GAL_LOOKS;
      if (gal.look == 2 && gal.cn >= CAR_COUNT) gal.look = 3;   // only the 39 have beasts
      gal_load_car(c);
      break;
    case 4: gal.zoom = (gal.zoom + 1) % 3; break;
  }
}

static void gal_stat_line(Graphics2D *g, const char *label, const char *value, int32_t x, int32_t y) {
  gfx_set_color(g, 90, 88, 80);
  font_draw(g, label, x, y);
  gfx_set_color(g, SET_INK);
  font_draw(g, value, x + 150, y);
}

static void gal_draw_panel(GalCtx *c, int32_t x, int32_t y, int32_t w) {
  Graphics2D *g = c->g;
  const int32_t cn = gal.cn, ident = car_identity(cn);
  char v[160];
  font_set(FONT_BOLD, 12);
  if (gal.tab == 0) {
    gal_stat_line(g, "Origin:", gal_origin(cn), x, y);
    gal_stat_line(g, "Created by:", gal_creator(cn), x, y += 20);
    if (gal_new_car(cn)) gal_stat_line(g, "Built on:", car_name(ident), x, y += 20);
    int32_t body = 0;
    for (int32_t i = 0; i < gal.co.npl; i++) body += gal.co.p[i].wz == 0;
    snprintf(v, sizeof(v), "%d", (int)gal.co.npl);
    gal_stat_line(g, "Polygons:", v, x, y += 20);
    snprintf(v, sizeof(v), "%d body, %d wheels", (int)body, (int)(gal.co.npl - body));
    gal_stat_line(g, "", v, x, y += 18);
    snprintf(v, sizeof(v), "%d", (int)c->cd->maxmag[cn]);
    gal_stat_line(g, "Health:", v, x, y += 20);
    snprintf(v, sizeof(v), "%d", (int)mad_dent_limit(ident, NULL));
    gal_stat_line(g, "Dents up to:", v, x, y += 20);
    snprintf(v, sizeof(v), "%d", (int)sqrt((double)c->cd->clrad[cn]));
    gal_stat_line(g, "Hit reach:", v, x, y += 20);
    snprintf(v, sizeof(v), "%.2fx", (double)c->cd->dammult[cn]);
    gal_stat_line(g, "Damage taken:", v, x, y += 20);
  } else if (gal.tab == 1) {
    static const int32_t kNoPoints[CS_N];
    float e[7];
    const float sw2 = ext_stat_values(c->cd, cn, kNoPoints, e);
    static const char *const kLabels[7] = {"Overall", "Top speed", "Acceleration", "Control",
                                           "Stunting", "Strength", "Defence"};
    for (int32_t i = 0; i < 7; i++) {
      const int32_t by = y + i * 19 - 10;
      gfx_set_color(g, SET_INK);
      font_draw(g, kLabels[i], x, by + 10);
      int32_t fill = (int32_t)(e[i] * 150.0f);
      if (fill < 0) fill = 0;
      if (fill > 150) fill = 150;
      gfx_set_color(g, 17, 17, 17);
      gfx_fill_rect(g, x + 110, by, 152, 12);
      gfx_set_color(g, i == 0 ? 0 : 200, i == 0 ? 150 : 120, 0);
      gfx_fill_rect(g, x + 111, by + 1, fill, 10);
      if (i == 1) snprintf(v, sizeof(v), "%d MPH", (int)(sw2 / 2.0f));
      else snprintf(v, sizeof(v), "%d", (int)(e[i] * 100.0f));
      gfx_set_color(g, SET_INK);
      font_draw(g, v, x + 270, by + 10);
    }
    y += 7 * 19 + 8;
    font_set(FONT_BOLD, 11);
    snprintf(v, sizeof(v), "%.1f / %.1f / %.1f", (double)c->cd->acelf[cn][0], (double)c->cd->acelf[cn][1],
             (double)c->cd->acelf[cn][2]);
    gal_stat_line(g, "Gears:", v, x, y);
    snprintf(v, sizeof(v), "%.1f", (double)c->cd->grip[cn]);
    gal_stat_line(g, "Grip:", v, x, y += 16);
    snprintf(v, sizeof(v), "%.2f", (double)c->cd->moment[cn]);
    gal_stat_line(g, "Weight:", v, x, y += 16);
    snprintf(v, sizeof(v), "%.2f / %d", (double)c->cd->airs[cn], (int)c->cd->airc[cn]);
    gal_stat_line(g, "Flip power / air control:", v, x, y += 16);
    snprintf(v, sizeof(v), "%.1f", (double)c->cd->push[cn]);
    gal_stat_line(g, "Push:", v, x, y += 16);
  } else if (gal.tab == 2) {
    const char *const *lines = specials_describe(cn);
    gfx_set_color(g, SET_INK);
    if (!lines || !lines[0]) {
      font_draw(g, "This car has no special.", x, y);
    } else {
      for (int32_t i = 0; lines[i] && i < 5; i++) y = gal_text(g, lines[i], x, y, w, 16) + 4;
    }
    y += 10;
    font_set(FONT_BOLD, 11);
    gfx_set_color(g, 90, 88, 80);
    gal_text(g, "Fired with the special button once its bar is full; it fills from stunts.", x, y, w, 14);
  } else if (gal.tab == 3) {
    const int32_t ec = ext_car_of(ident);
    int32_t slots[CAREER_PERK_SLOTS];
    const int32_t n = ec >= 0 ? career_perk_rows(ec, slots) : 0;
    if (n == 0) {
      gfx_set_color(g, SET_INK);
      font_draw(g, "No perks: RPG Mode does not race this car.", x, y);
    }
    for (int32_t r = 0; r < n; r++) {
      const int32_t p = career_statsalc[ec][slots[r]];
      const CareerPerkText *t = &career_perk_text[p];
      font_set(FONT_BOLD, 12);
      gfx_set_color(g, 160, 60, 0);
      font_draw(g, career_perk_name[p], x, y);
      font_set(FONT_BOLD, 11);
      gfx_set_color(g, SET_INK);
      snprintf(v, sizeof(v), "%s %s", tr(t->what[0]), t->what[1] ? tr(t->what[1]) : "");
      y = gal_text(g, v, x + 8, y + 14, w - 8, 13);
      gfx_set_color(g, 0, 110, 0);
      snprintf(v, sizeof(v), "%s %s", tr(t->max[0]), t->max[1] ? tr(t->max[1]) : "");
      y = gal_text(g, v, x + 8, y, w - 8, 13) + 4;
    }
  } else {
    if (!gal.npl_ok) {
      for (int32_t i = 0; i < CAR_FACTS_BASE; i++) gal.npl[i] = gal_model(c, i)->npl;
      gal.npl_ok = true;
    }
    char facts[12][CAR_FACT_LEN];
    int32_t n = 0;
    if (gal_new_car(cn)) {
      snprintf(facts[n++], CAR_FACT_LEN, "Built on %s: it has that car's special and perks.", car_name(ident));
    }
    n += car_facts(c->cd, cn, ident, gal.npl, gal_model(c, cn)->npl, facts + n, 12 - n);
    gfx_set_color(g, SET_INK);
    if (n == 0) font_draw(g, "Nothing out of the ordinary: a car for every road.", x, y);
    for (int32_t i = 0; i < n; i++) {
      gfx_fill_rect(g, x, y - 7, 4, 4);
      y = gal_text(g, facts[i], x + 10, y, w - 10, 15) + 5;
    }
  }
}

// ---- The music --------------------------------------------------------------

static void gal_add_track(int32_t id, const char *fmt, ...) {
  if (gal.ntracks >= (int32_t)(sizeof(gal.tracks) / sizeof(gal.tracks[0]))) return;
  GalTrack *t = &gal.tracks[gal.ntracks++];
  t->id = id;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(t->label, sizeof(t->label), fmt, ap);
  va_end(ap);
}

static void gal_build_tracks(void) {
  if (gal.ntracks > 0) return;
  char name[64];
  gal_add_track(0, "NEED FOR MADNESS 2");
  for (int32_t s = 1; s <= NUM_STAGES; s++) {
    stage_read_name(s, name, sizeof(name));
    gal_add_track(s, "Stage %d: %s", (int)s, name);
  }
  gal_add_track(33, "The party (NFM 2's stage 27)");
  gal_add_track(0, "EXTENDED");
  for (int32_t s = 1; s <= EXT_NORMAL_STAGES; s++) {
    ext_read_name(s, name, sizeof(name));
    gal_add_track(100 + s, "Stage %d: %s", (int)s, name);
  }
  gal_add_track(0, "RPG MODE");
  const bool was_career = g_ext_career;
  const int32_t was_bonus = g_career_bonus;
  g_ext_career = true;
  g_career_bonus = 0;
  for (int32_t s = 1; s <= 28; s++) {
    ext_read_name(s, name, sizeof(name));
    gal_add_track(200 + s, "Stage %d: %s", (int)s, name);
  }
  gal_add_track(GAL_TRACK_BOSS, "The Titan fight");
  for (int32_t b = 1; b <= 4; b++) {
    g_career_bonus = b;
    ext_read_name(1, name, sizeof(name));
    gal_add_track(250 + b, "Bonus stage %d: %s", (int)b, name);
  }
  g_ext_career = was_career;
  g_career_bonus = was_bonus;
  gal_add_track(0, "CLASSIC MODE");
  for (int32_t s = 1; s <= 17; s++) gal_add_track(GAL_TRACK_CLASSIC + s - 1, "Classic stage %d", (int)s);
  gal_add_track(0, "MENUS");
  gal_add_track(GAL_TRACK_INTERFACE, "Car and stage select");
  gal_add_track(GAL_TRACK_MENU, "Extended's main menu");
  gal_add_track(GAL_TRACK_MENU + 1, "Extended's car select");
  gal_add_track(GAL_TRACK_MENU + 2, "Extended's stage select");
}

static void gal_stop_music(GalCtx *c) {
  audio_stop_music(c->audio);
  radical_track_free(&gal.track);
  gal.playing = -1;
}

static void gal_play(GalCtx *c, int32_t idx) {
  gal_stop_music(c);
  const int32_t id = gal.tracks[idx].id;
  if (id == GAL_TRACK_INTERFACE) {
    if (c->interface_music->bytes) audio_start_music(c->audio, c->interface_music);
  } else {
    // The stage's own track and the career's Ogg buffers are the Gallery's
    // now: the next race loads its music afresh.
    *c->stage_music_loaded_for = -1;
    if (!load_music_track(id, &gal.track)) {
      gal.playing = -2;   // shown as "could not be loaded"
      return;
    }
    if (!gal.track.bytes && g_career_ogg[1])
      audio_start_ogg(c->audio, g_career_ogg[0], g_career_ogg_len[0], g_career_ogg[1], g_career_ogg_len[1]);
    else
      audio_start_music(c->audio, &gal.track);
  }
  audio_set_music_muted(c->audio, false);
  gal.playing = idx;
}

static void gal_music_step(int32_t dir) {
  int32_t s = gal.msel;
  do s += dir; while (s >= 0 && s < gal.ntracks && gal.tracks[s].id == 0);
  if (s >= 0 && s < gal.ntracks) gal.msel = s;
}

// ---- Photos and replays: the files in gallery/<what>/ ----------------------

enum { GALF_MAX = 256, GALF_NAME = 64, GAL_PHOTO_W = 640, GAL_PHOTO_H = 360, GAL_FILE_ROWS = 8 };
static struct {
  char dir[560];
  char names[GALF_MAX][GALF_NAME];
  int32_t n, sel, top;
  bool armed;   // the delete button pressed once
  int32_t shown, tex;   // the photo on screen, and its texture
  bool bad;
  ReplayMeta meta[GALF_MAX];
  bool meta_ok[GALF_MAX];
} galf = {.shown = -1, .tex = -1};

// The replay the Replays screen picked, for game.c to set up (STATE_REPLAY_VIEW).
static char g_replay_open[700];

static int galf_newest_first(const void *a, const void *b) { return strcmp((const char *)b, (const char *)a); }

/** Lists gallery/<what>/'s files ending in `ext`, newest first (their names
 * carry the date). */
static void galf_open(const char *what, const char *ext) {
  galf.n = 0;
  galf.armed = false;
  galf.shown = -1;
  if (!gallery_dir(what, galf.dir, sizeof(galf.dir))) return;
  char d[560];
  snprintf(d, sizeof(d), "%s", galf.dir);
  d[strlen(d) - 1] = '\0';   // opendir without the trailing slash
  DIR *dir = opendir(d);
  if (!dir) return;
  const size_t el = strlen(ext);
  struct dirent *e;
  while ((e = readdir(dir)) && galf.n < GALF_MAX) {
    const size_t len = strlen(e->d_name);
    if (len > el && len < GALF_NAME && strcmp(e->d_name + len - el, ext) == 0)
      snprintf(galf.names[galf.n++], GALF_NAME, "%s", e->d_name);
  }
  closedir(dir);
  qsort(galf.names, (size_t)galf.n, GALF_NAME, galf_newest_first);
  if (strcmp(ext, ".nfmr") == 0) {
    for (int32_t i = 0; i < galf.n; i++) {
      char path[700];
      snprintf(path, sizeof(path), "%s%s", galf.dir, galf.names[i]);
      galf.meta_ok[i] = replay_read_meta(path, &galf.meta[i]);
    }
  }
  if (galf.sel >= galf.n) galf.sel = galf.n > 0 ? galf.n - 1 : 0;
}

/** The chosen photo, decoded and brought to 640x360 for the screen. */
static void galf_show_photo(void) {
  galf.shown = galf.sel;
  galf.bad = true;
  char path[700];
  snprintf(path, sizeof(path), "%s%s", galf.dir, galf.names[galf.sel]);
  FILE *f = fopen(path, "rb");
  if (!f) return;
  fseek(f, 0, SEEK_END);
  const long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *buf = len > 0 ? malloc((size_t)len) : NULL;
  const bool read_ok = buf && fread(buf, 1, (size_t)len, f) == (size_t)len;
  fclose(f);
  PngImage img = {0};
  const bool ok = read_ok && png_decode(buf, (size_t)len, &img);
  free(buf);
  if (!ok) return;
  static uint8_t px[GAL_PHOTO_W * GAL_PHOTO_H * 4];
  for (int32_t y = 0; y < GAL_PHOTO_H; y++) {
    const uint8_t *row = img.rgba + (size_t)(y * img.height / GAL_PHOTO_H) * img.width * 4;
    for (int32_t x = 0; x < GAL_PHOTO_W; x++)
      memcpy(px + ((size_t)y * GAL_PHOTO_W + x) * 4, row + (size_t)(x * img.width / GAL_PHOTO_W) * 4, 4);
  }
  png_free(&img);
  if (galf.tex < 0) galf.tex = gfx_gl_upload_texture(px, GAL_PHOTO_W, GAL_PHOTO_H);
  else gfx_gl_update_texture(galf.tex, px, GAL_PHOTO_W, GAL_PHOTO_H);
  galf.bad = false;
}

/** The view button twice: the chosen file goes. */
static void galf_delete(const char *what, const char *ext) {
  if (!galf.armed) {
    galf.armed = true;
    return;
  }
  char path[700];
  snprintf(path, sizeof(path), "%s%s", galf.dir, galf.names[galf.sel]);
  remove(path);
  galf_open(what, ext);
}

static const char *galf_mode(const ReplayMeta *r) {
  if (r->ext_career) return "RPG Mode";
  if (r->ext_normal) return r->ptmatch > 0 ? "Tournament" : "Extended";
  return r->gmode == 1 ? "NFM 1" : r->gmode == 2 ? "NFM 2" : "Free Play";
}

// ---- Screens ----------------------------------------------------------------

/** NFM_SCREENSHOT_MENU=gallery|galcars|galcar|galmusic, headless: on frame 2
 * open that screen, with NFM_GALLERY_FILTER, _CAR (a port car number), _TAB,
 * _LOOK, _HITS (that many hits), _WRECK=1 and _MUSIC (a list row). */
static GameState gallery_hook(GalCtx *c, const char *menu, int32_t frame, GameState state) {
  if (!menu || frame != 2 || strncmp(menu, "gal", 3) != 0) return state;
  const char *e;
  if ((e = getenv("NFM_GALLERY_FILTER"))) gal.filter = atoi(e) % GAL_FILTERS;
  gal_build_list();
  if (strcmp(menu, "galcars") == 0) return STATE_GALLERY_CARS;
  if (strcmp(menu, "galphotos") == 0) {
    galf_open("photos", ".png");
    return STATE_GALLERY_PHOTOS;
  }
  if (strcmp(menu, "galreplays") == 0) {
    galf_open("replays", ".nfmr");
    return STATE_GALLERY_REPLAYS;
  }
  if (strcmp(menu, "galreplay") == 0 && (e = getenv("NFM_GALLERY_REPLAY"))) {
    // NFM_GALLERY_REPLAY=<file>: straight into the viewer, NFM_REPLAY_CAM its camera.
    snprintf(g_replay_open, sizeof(g_replay_open), "%s", e);
    return STATE_REPLAY_VIEW;
  }
  if (strcmp(menu, "galmusic") == 0) {
    gal_build_tracks();
    if (getenv("NFM_GALLERY_MUSICCHECK")) {
      // Every listed track loads (headless has no audio device to hear them).
      int32_t ok = 0, n = 0;
      for (int32_t i = 0; i < gal.ntracks; i++) {
        const int32_t id = gal.tracks[i].id;
        if (id == 0 || id == GAL_TRACK_INTERFACE) continue;
        n++;
        RadicalTrack t;
        if (load_music_track(id, &t)) ok++;
        else fprintf(stderr, "gallery: track %d (%s) did not load\n", (int)id, gal.tracks[i].label);
        radical_track_free(&t);
      }
      fprintf(stderr, "gallery: %d of %d tracks load\n", (int)ok, (int)n);
      *c->stage_music_loaded_for = -1;
    }
    gal.msel = (e = getenv("NFM_GALLERY_MUSIC")) ? atoi(e) : 1;
    if (gal.msel >= GAL_MUSIC_ROWS) gal.mtop = gal.msel - GAL_MUSIC_ROWS / 2;
    return STATE_GALLERY_MUSIC;
  }
  if (strcmp(menu, "galcar") == 0) {
    gal_open_car(c, (e = getenv("NFM_GALLERY_CAR")) ? atoi(e) : 0);
    if ((e = getenv("NFM_GALLERY_TAB"))) gal.tab = atoi(e) % GAL_TABS;
    if ((e = getenv("NFM_GALLERY_LOOK"))) {
      gal.look = atoi(e) % GAL_LOOKS;
      gal_load_car(c);
    }
    for (int32_t k = (e = getenv("NFM_GALLERY_HITS")) ? atoi(e) : 0; k > 0; k--) gal_hit();
    if (getenv("NFM_GALLERY_WRECK")) gal_wreck();
    return STATE_GALLERY_CAR;
  }
  return STATE_GALLERY;
}

/** Entering from the main menu. */
static void gallery_open(void) {
  gal.hub = 0;
  gal.pending = -1;
}

/** The Gallery's input for this frame; returns the next state. */
static GameState gallery_input(GameState state, GalCtx *c) {
  // The music waits for the "Loading" frame to be on screen first: a
  // module renders whole before it plays.
  if (gal.pending >= 0) {
    gal_play(c, gal.pending);
    gal.pending = -1;
  }
  int32_t px, py;
  const bool down = platform_pointer(&px, &py);
  static bool was_down;
  static int32_t down_x, down_y;
  const bool tapped = !down && was_down && abs(px - down_x) < 12 && abs(py - down_y) < 12;
  if (down && !was_down) {
    down_x = px;
    down_y = py;
  }
  if (!down) {
    px = down_x;
    py = down_y;
  }
  was_down = down;

  if (state == STATE_GALLERY) {
    bool go = GAL_EDGE(c, BTN_CONFIRM);
    if (tapped && px > 120 && px < 680 && py > GAL_HUB_Y && py < GAL_HUB_Y + GAL_HUB_ROWS * GAL_HUB_STEP) {
      const int32_t r = (py - GAL_HUB_Y) / GAL_HUB_STEP;
      if (r == gal.hub) go = true;
      gal.hub = r;
    }
    if (GAL_EDGE(c, BTN_DOWN)) gal.hub = (gal.hub + 1) % GAL_HUB_ROWS;
    if (GAL_EDGE(c, BTN_UP)) gal.hub = (gal.hub + GAL_HUB_ROWS - 1) % GAL_HUB_ROWS;
    if (GAL_EDGE(c, BTN_CANCEL)) {
      gal_stop_music(c);
      return STATE_MAIN_MENU;
    }
    if (go) {
      if (gal.hub == 0) {
        gal_build_list();
        return STATE_GALLERY_CARS;
      }
      if (gal.hub == 1) {
        gal_build_tracks();
        if (gal.tracks[gal.msel].id == 0) gal_music_step(1);
        return STATE_GALLERY_MUSIC;
      }
      if (gal.hub == 2) {
        galf_open("photos", ".png");
        return STATE_GALLERY_PHOTOS;
      }
      if (gal.hub == 3) {
        galf_open("replays", ".nfmr");
        return STATE_GALLERY_REPLAYS;
      }
      gal_stop_music(c);
      return STATE_MAIN_MENU;
    }
  } else if (state == STATE_GALLERY_CARS) {
    bool go = GAL_EDGE(c, BTN_CONFIRM);
    if (tapped && px > 120 && px < 680 && py > 68 && py < 68 + GAL_LIST_ROWS * 58) {
      const int32_t r = gal.top + (py - 68) / 58;
      if (r < gal.nlist) {
        if (r == gal.sel) go = true;
        gal.sel = r;
      }
    }
    if (tapped && py < 60 && px < 160) {
      gal.filter = (gal.filter + GAL_FILTERS - 1) % GAL_FILTERS;
      gal_build_list();
    }
    if (tapped && py < 60 && px > 640) {
      gal.filter = (gal.filter + 1) % GAL_FILTERS;
      gal_build_list();
    }
    if (GAL_EDGE(c, BTN_DOWN) && gal.sel + 1 < gal.nlist) gal.sel++;
    if (GAL_EDGE(c, BTN_UP) && gal.sel > 0) gal.sel--;
    if (GAL_EDGE(c, BTN_RIGHT) || GAL_EDGE(c, BTN_LEFT)) {
      gal.filter = (gal.filter + (GAL_EDGE(c, BTN_RIGHT) ? 1 : GAL_FILTERS - 1)) % GAL_FILTERS;
      gal.sel = gal.top = 0;
      gal_build_list();
    }
    if (gal.sel < gal.top) gal.top = gal.sel;
    if (gal.sel >= gal.top + GAL_LIST_ROWS) gal.top = gal.sel - GAL_LIST_ROWS + 1;
    if (GAL_EDGE(c, BTN_CANCEL)) return STATE_GALLERY;
    if (go && gal.nlist > 0) {
      gal_open_car(c, gal.list[gal.sel]);
      return STATE_GALLERY_CAR;
    }
  } else if (state == STATE_GALLERY_CAR) {
    // Turning: the D-pad held, or a drag on the car.
    if (c->held[BTN_LEFT]) gal.yaw -= 3;
    if (c->held[BTN_RIGHT]) gal.yaw += 3;
    if (down && down_x < 440 && down_y > 60 && down_y < 400) {
      if (gal.drag) {
        gal.yaw -= (px - gal.drag_x) / 2;
        gal.pitch += (py - gal.drag_y) / 2;
        if (gal.pitch > 80) gal.pitch = 80;
        if (gal.pitch < -80) gal.pitch = -80;
      }
      gal.drag = true;
      gal.drag_x = px;
      gal.drag_y = py;
    } else {
      gal.drag = false;
    }
    gal.yaw %= 360;
    if (GAL_EDGE(c, BTN_DOWN)) gal.action = (gal.action + 1) % GAL_ACTIONS;
    if (GAL_EDGE(c, BTN_UP)) gal.action = (gal.action + GAL_ACTIONS - 1) % GAL_ACTIONS;
    if (GAL_EDGE(c, BTN_VIEW)) gal.tab = (gal.tab + 1) % GAL_TABS;
    if (tapped && py > 62 && py < 84 && px > 440 && px < 780) gal.tab = (px - 440) / 68;
    if (tapped && px > 24 && px < 124 && py > 74 && py < 74 + GAL_ACTIONS * 30) {
      gal.action = (py - 74) / 30;
      gal_act(c);
    }
    if (GAL_EDGE(c, BTN_CONFIRM)) gal_act(c);
    if (GAL_EDGE(c, BTN_CANCEL)) return STATE_GALLERY_CARS;
  } else if (state == STATE_GALLERY_PHOTOS) {
    const int32_t was = galf.sel;
    if ((GAL_EDGE(c, BTN_RIGHT) || (tapped && px > 560)) && galf.sel + 1 < galf.n) galf.sel++;
    if ((GAL_EDGE(c, BTN_LEFT) || (tapped && px < 240)) && galf.sel > 0) galf.sel--;
    if (galf.sel != was) galf.armed = false;
    if (GAL_EDGE(c, BTN_VIEW) && galf.n > 0) galf_delete("photos", ".png");
    if (GAL_EDGE(c, BTN_CANCEL)) return STATE_GALLERY;
  } else if (state == STATE_GALLERY_REPLAYS) {
    bool go = GAL_EDGE(c, BTN_CONFIRM);
    const int32_t was = galf.sel;
    if (tapped && px > 120 && px < 680 && py > 66 && py < 66 + GAL_FILE_ROWS * 44) {
      const int32_t r = galf.top + (py - 66) / 44;
      if (r < galf.n) {
        if (r == galf.sel) go = true;
        galf.sel = r;
      }
    }
    if (GAL_EDGE(c, BTN_DOWN) && galf.sel + 1 < galf.n) galf.sel++;
    if (GAL_EDGE(c, BTN_UP) && galf.sel > 0) galf.sel--;
    if (galf.sel != was) galf.armed = false;
    if (galf.sel < galf.top) galf.top = galf.sel;
    if (galf.sel >= galf.top + GAL_FILE_ROWS) galf.top = galf.sel - GAL_FILE_ROWS + 1;
    if (GAL_EDGE(c, BTN_VIEW) && galf.n > 0) galf_delete("replays", ".nfmr");
    if (GAL_EDGE(c, BTN_CANCEL)) return STATE_GALLERY;
    if (go && galf.n > 0 && galf.meta_ok[galf.sel]) {
      gal_stop_music(c);
      snprintf(g_replay_open, sizeof(g_replay_open), "%s%s", galf.dir, galf.names[galf.sel]);
      return STATE_REPLAY_VIEW;
    }
  } else if (state == STATE_GALLERY_MUSIC) {
    if (tapped && px > 120 && px < 680 && py > 66 && py < 66 + GAL_MUSIC_ROWS * 24) {
      const int32_t r = gal.mtop + (py - 66) / 24;
      if (r < gal.ntracks && gal.tracks[r].id != 0) {
        if (r == gal.msel) gal.pending = r;
        gal.msel = r;
      }
    }
    if (GAL_EDGE(c, BTN_DOWN)) gal_music_step(1);
    if (GAL_EDGE(c, BTN_UP)) gal_music_step(-1);
    if (GAL_EDGE(c, BTN_RIGHT) || GAL_EDGE(c, BTN_LEFT)) {
      // The next (or this, then the previous) group's first track.
      const int32_t dir = GAL_EDGE(c, BTN_RIGHT) ? 1 : -1;
      int32_t s = gal.msel;
      if (dir < 0) while (s > 0 && gal.tracks[s - 1].id != 0) s--;
      if (dir < 0 && s == gal.msel) {
        s -= 2;
        while (s > 0 && gal.tracks[s - 1].id != 0) s--;
      }
      if (dir > 0) {
        while (s < gal.ntracks && gal.tracks[s].id != 0) s++;
        s++;
      }
      if (s >= 0 && s < gal.ntracks && gal.tracks[s].id != 0) gal.msel = s;
    }
    if (gal.msel < gal.mtop + 1) gal.mtop = gal.msel > 0 ? gal.msel - 1 : 0;
    if (gal.msel >= gal.mtop + GAL_MUSIC_ROWS) gal.mtop = gal.msel - GAL_MUSIC_ROWS + 1;
    if (GAL_EDGE(c, BTN_CONFIRM)) gal.pending = gal.msel;
    if (GAL_EDGE(c, BTN_VIEW)) gal_stop_music(c);
    if (GAL_EDGE(c, BTN_CANCEL)) return STATE_GALLERY;
  }
  return state;
}

static void gallery_draw(GameState state, GalCtx *c) {
  Graphics2D *g = c->g;
  char line[160];
  if (state == STATE_GALLERY) {
    gal_backdrop(g, "GALLERY");
    static const char *const kRows[GAL_HUB_ROWS][2] = {
        {"Cars", "Every car: turn it, read about it, dent and wreck it."},
        {"Music", "Every stage's music, Extended's and the RPG Mode's too."},
        {"Photos", "The photos taken in the photo mode (pause menu > Photo Mode)."},
        {"Replays", "The replays saved from the Instant Replay and the finish."},
        {"Back", ""}};
    for (int32_t i = 0; i < GAL_HUB_ROWS; i++) {
      const int32_t y = GAL_HUB_Y + i * GAL_HUB_STEP;
      gal_row(g, 120, y, 560, GAL_HUB_STEP - 8, i == gal.hub);
      if (i == gal.hub) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_INK);
      font_set(FONT_BOLD, 20);
      font_draw(g, kRows[i][0], 154, y + (kRows[i][1][0] ? 24 : 32));
      font_set(FONT_BOLD, 12);
      font_draw(g, kRows[i][1], 154, y + 42);
    }
    snprintf(line, sizeof(line), "%s: Select    %s: Back", KEY_CONTINUE, KEY_BACK);
    gal_hint(g, line);
  } else if (state == STATE_GALLERY_CARS) {
    snprintf(line, sizeof(line), "%s", tr(kCarFilterName[kGalFilters[gal.filter]]));
    gal_backdrop(g, line);
    draw_settings_arrow(g, 140, 38, false, true, true);
    draw_settings_arrow(g, 660, 38, true, true, true);
    if (gal.nlist == 0) {
      gfx_set_color(g, SET_INK);
      font_set(FONT_BOLD, 14);
      draw_centered(g, "No cars here yet.", 400, 200);
      font_set(FONT_BOLD, 12);
      draw_centered(g, "Your own .rad cars go in sdmc:/switch/nfm-extended/cars/", 400, 224);
    }
    for (int32_t i = 0; i < GAL_LIST_ROWS && gal.top + i < gal.nlist; i++) {
      const int32_t r = gal.top + i, cn = gal.list[r], y = 68 + i * 58;
      const bool sel = r == gal.sel;
      gal_row(g, 120, y, 560, 52, sel);
      if (sel) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_INK);
      font_set(FONT_BOLD, 17);
      font_draw(g, car_name(cn), 154, y + 23);
      font_set(FONT_BOLD, 11);
      snprintf(line, sizeof(line), "%s  -  %s", tr(gal_origin(cn)), gal_creator(cn));
      font_draw(g, line, 154, y + 41);
      ContO *model = gal_model(c, cn);
      if (model->p) draw_car_thumb(g, c->m, model, 590, y + 26);
    }
    snprintf(line, sizeof(line), "%d / %d", gal.nlist ? (int)gal.sel + 1 : 0, (int)gal.nlist);
    gfx_set_color(g, 90, 88, 80);
    font_set(FONT_BOLD, 12);
    font_draw(g, line, 680 - font_width(line), 64);
    snprintf(line, sizeof(line), "Left/Right: Group    %s: Look    %s: Back", KEY_CONTINUE, KEY_BACK);
    gal_hint(g, line);
  } else if (state == STATE_GALLERY_CAR) {
    gal_backdrop(g, car_name(gal.cn));
    // The stand: a dark bay for the car.
    gfx_set_color(g, 17, 17, 17);
    gfx_fill_rect(g, 20, 62, 412, 340);
    gfx_set_color(g, 58, 60, 66);
    gfx_fill_rect(g, 23, 65, 406, 334);
    gfx_set_color(g, 74, 76, 84);
    gfx_fill_rect(g, 23, 290, 406, 109);
    // A beast is bigger than its car: a step back so it fits the bay.
    if (gal.co.p) gal_draw_car(c, 270, 238, kGalZoom[gal.zoom] * (gal.look == 2 ? 3 : 4) / 4);
    // The fix flash runs at the menu's pace, then the model is as new.
    if (gal.co.fix) {
      cont_o_step_fix(&gal.co);
      if (gal.co.fcnt >= 7) gal_load_car(c);
    }
    // The actions, down the left of the bay.
    for (int32_t i = 0; i < GAL_ACTIONS; i++) {
      const int32_t y = 74 + i * 30;
      const bool sel = i == gal.action;
      gfx_set_color(g, sel ? 245 : 30, sel ? 208 : 30, sel ? 0 : 34);
      gfx_fill_rect(g, 30, y, 92, 24);
      gfx_set_color(g, sel ? 17 : 220, sel ? 17 : 220, sel ? 17 : 220);
      font_set(FONT_BOLD, 13);
      if (i == 3) snprintf(line, sizeof(line), "%s", tr(kGalLooks[gal.look]));
      else snprintf(line, sizeof(line), "%s", tr(kGalActions[i]));
      font_draw(g, line, 76 - font_width(line) / 2, y + 17);
    }
    // Its health, as the race HUD's damage bar.
    const int32_t maxmag = gal.cd.maxmag[gal.cn] > 0 ? gal.cd.maxmag[gal.cn] : 1;
    int32_t fill = (int32_t)(200.0 * gal.mad.hitmag / maxmag);
    if (fill > 200) fill = 200;
    gfx_set_color(g, 17, 17, 17);
    gfx_fill_rect(g, 216, 372, 204, 14);
    gfx_set_color(g, 244, fill > 66 ? 244 - (fill - 66) * 233 / 134 : 244, 11);
    gfx_fill_rect(g, 218, 374, fill, 10);
    gfx_set_color(g, 220, 220, 220);
    font_set(FONT_BOLD, 11);
    if (gal.wrecked) snprintf(line, sizeof(line), "%s", tr("WASTED"));
    else snprintf(line, sizeof(line), "%s %d / %d", tr("Damage"), (int)gal.mad.hitmag, (int)maxmag);
    font_draw(g, line, 210 - font_width(line), 384);
    // The panel: tabs, then the chosen one.
    for (int32_t i = 0; i < GAL_TABS; i++) {
      const bool sel = i == gal.tab;
      const int32_t tx = 440 + i * 68;
      gfx_set_color(g, 17, 17, 17);
      gfx_fill_rect(g, tx, 62, 66, 22);
      if (sel) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_BG_DARK);
      gfx_fill_rect(g, tx + 2, 64, 62, 18);
      gfx_set_color(g, SET_INK);
      font_set(FONT_BOLD, 12);
      font_draw(g, kGalTabs[i], tx + 33 - font_width(kGalTabs[i]) / 2, 78);
    }
    gfx_set_color(g, 17, 17, 17);
    gfx_fill_rect(g, 440, 86, 338, 316);
    gfx_set_color(g, SET_BG);
    gfx_fill_rect(g, 443, 89, 332, 310);
    gal_draw_panel(c, 454, 110, 312);
    snprintf(line, sizeof(line), "Left/Right: Turn    Up/Down + %s: Action    %s: Tab    %s: Back", KEY_CONTINUE,
             KEY_VIEW, KEY_BACK);
    gal_hint(g, line);
  } else if (state == STATE_GALLERY_PHOTOS) {
    if (galf.n > 0) snprintf(line, sizeof(line), "%s  %d / %d", tr("PHOTOS"), (int)galf.sel + 1, (int)galf.n);
    else snprintf(line, sizeof(line), "%s", tr("PHOTOS"));
    gal_backdrop(g, line);
    gfx_set_color(g, 17, 17, 17);
    gfx_fill_rect(g, 77, 59, GAL_PHOTO_W + 6, GAL_PHOTO_H + 6);
    if (galf.n == 0) {
      gfx_set_color(g, 220, 220, 220);
      font_set(FONT_BOLD, 14);
      draw_centered(g, "No photos yet.", 400, 230);
      font_set(FONT_BOLD, 12);
      draw_centered(g, "Pause a race and choose Photo Mode, then press A.", 400, 254);
    } else {
      if (galf.shown != galf.sel) galf_show_photo();
      if (!galf.bad && galf.tex >= 0) {
        gfx_draw_image(g, galf.tex, 80, 62, GAL_PHOTO_W, GAL_PHOTO_H);
      } else {
        gfx_set_color(g, 220, 220, 220);
        font_set(FONT_BOLD, 13);
        draw_centered(g, "This photo could not be read.", 400, 240);
      }
      if (galf.sel > 0) draw_settings_arrow(g, 50, 242, false, true, true);
      if (galf.sel + 1 < galf.n) draw_settings_arrow(g, 750, 242, true, true, true);
    }
    if (galf.armed) snprintf(line, sizeof(line), "%s %s", tr("Press again to delete"), galf.names[galf.sel]);
    else snprintf(line, sizeof(line), "Left/Right: Browse    %s: Delete    %s: Back", KEY_VIEW, KEY_BACK);
    gal_hint(g, line);
  } else if (state == STATE_GALLERY_REPLAYS) {
    gal_backdrop(g, "REPLAYS");
    if (galf.n == 0) {
      gfx_set_color(g, SET_INK);
      font_set(FONT_BOLD, 14);
      draw_centered(g, "No replays yet.", 400, 200);
      font_set(FONT_BOLD, 12);
      draw_centered(g, "In a race's Instant Replay or the finish's replay, press X to save it.", 400, 224);
    }
    for (int32_t i = 0; i < GAL_FILE_ROWS && galf.top + i < galf.n; i++) {
      const int32_t r = galf.top + i, y = 66 + i * 44;
      const bool sel = r == galf.sel;
      gal_row(g, 120, y, 560, 40, sel);
      if (sel) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_INK);
      const ReplayMeta *rm = &galf.meta[r];
      font_set(FONT_BOLD, 14);
      if (!galf.meta_ok[r]) {
        font_draw(g, galf.names[r], 150, y + 18);
        font_set(FONT_BOLD, 11);
        font_draw(g, "This replay could not be read.", 150, y + 33);
        continue;
      }
      font_draw(g, rm->stage_name[0] ? rm->stage_name : galf.names[r], 150, y + 18);
      char date[32] = "";
      const time_t at = (time_t)rm->saved_at;
      const struct tm *t = localtime(&at);
      if (t) strftime(date, sizeof(date), "%Y-%m-%d %H:%M", t);
      font_set(FONT_BOLD, 11);
      snprintf(line, sizeof(line), "%s  -  %s  -  %d %s  -  %s", tr(galf_mode(rm)), rm->car_name, (int)rm->nplayers,
               tr("cars"), date);
      font_draw(g, line, 150, y + 33);
    }
    if (galf.armed) snprintf(line, sizeof(line), "%s %s", tr("Press again to delete"), galf.names[galf.sel]);
    else snprintf(line, sizeof(line), "%s: Watch    %s: Delete    %s: Back", KEY_CONTINUE, KEY_VIEW, KEY_BACK);
    gal_hint(g, line);
  } else if (state == STATE_GALLERY_MUSIC) {
    gal_backdrop(g, "GALLERY - MUSIC");
    font_set(FONT_BOLD, 13);
    for (int32_t i = 0; i < GAL_MUSIC_ROWS && gal.mtop + i < gal.ntracks; i++) {
      const int32_t r = gal.mtop + i, y = 66 + i * 24;
      const GalTrack *t = &gal.tracks[r];
      if (t->id == 0) {
        gfx_set_color(g, 17, 17, 17);
        gfx_fill_rect(g, 120, y + 2, 560, 20);
        gfx_set_color(g, SET_YELLOW);
        font_draw(g, t->label, 134, y + 17);
        continue;
      }
      const bool sel = r == gal.msel;
      gal_row(g, 120, y, 560, 23, sel);
      if (sel) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_INK);
      font_draw(g, t->label, 150, y + 16);
      if (r == gal.playing) {
        gfx_set_color(g, 0, 140, 0);
        font_draw(g, "PLAYING", 668 - font_width("PLAYING"), y + 16);
      }
    }
    gfx_set_color(g, 17, 17, 17);
    font_set(FONT_BOLD, 13);
    if (gal.pending >= 0) snprintf(line, sizeof(line), "%s %s", tr("Loading"), gal.tracks[gal.pending].label);
    else if (gal.playing == -2) snprintf(line, sizeof(line), "%s", tr("This track could not be loaded."));
    else if (gal.playing >= 0) snprintf(line, sizeof(line), "%s %s", tr("Now playing:"), gal.tracks[gal.playing].label);
    else line[0] = '\0';
    draw_centered(g, line, 400, 416);
    snprintf(line, sizeof(line), "%s: Play    %s: Stop    Left/Right: Group    %s: Back", KEY_CONTINUE, KEY_VIEW,
             KEY_BACK);
    gal_hint(g, line);
  }
}
