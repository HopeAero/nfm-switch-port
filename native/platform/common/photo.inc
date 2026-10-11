// The photo mode (pause menu > Photo Mode, and a saved replay's viewer): the
// race frozen, a free camera over the whole stage, and photos saved as PNG
// into the save folder's gallery/photos. This port's own; #included by game.c
// before game_run(), like gallery.inc.

typedef struct {
  float x, y, z;      // where the camera is, in the world
  float yaw, pitch;   // Medium.xz and Medium.zy, degrees
  float zoom;         // Medium.focus_point, 400 the race's own
  int32_t car;        // the car the D-pad last jumped to
  bool hide_ui;
  bool capture;       // a photo is wanted: the next frame drawn has no UI...
  bool shoot;         // ...and this one is it, read back after it is drawn
  int32_t toast;      // frames left of `toast_text`
  char toast_text[160];
  // The race camera as it was, for the way out.
  int32_t saved[6];
  uint32_t prev_buttons;
} PhotoCam;
static PhotoCam photo;

/** The save folder's gallery/<what>/ into `out` (with the slash); false
 * when there is no save folder. The folder is made if it is missing. */
static bool gallery_dir(const char *what, char *out, size_t outsz) {
  char save[512];
  if (!platform_progress_path(save, sizeof(save))) return false;
  char *slash = strrchr(save, '/');
  if (!slash) return false;
  slash[1] = '\0';
  const int n = snprintf(out, outsz, "%sgallery/%s/", save, what);
  if (n <= 0 || (size_t)n >= outsz) return false;
  char probe[600];
  snprintf(probe, sizeof(probe), "%sx", out);
  progress_ensure_dir(probe);
  return true;
}

/** A new file name in gallery/<what>/: <prefix>_YYYYMMDD_HHMMSS.<ext>. */
static bool gallery_new_file(const char *what, const char *prefix, const char *ext, char *out, size_t outsz) {
  char dir[560];
  if (!gallery_dir(what, dir, sizeof(dir))) return false;
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);
  char stamp[32] = "0";
  if (t) strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", t);
  for (int32_t k = 1; k < 100; k++) {
    if (k == 1) snprintf(out, outsz, "%s%s_%s.%s", dir, prefix, stamp, ext);
    else snprintf(out, outsz, "%s%s_%s_%d.%s", dir, prefix, stamp, (int)k, ext);
    FILE *f = fopen(out, "rb");
    if (!f) return true;
    fclose(f);
  }
  return false;
}

/** A message over the scene for a few seconds: `msg` (translated), then `arg`. */
static void photo_say(const char *msg, const char *arg) {
  snprintf(photo.toast_text, sizeof(photo.toast_text), "%.80s%.70s", tr(msg), arg ? arg : "");
  photo.toast = 150;
}

/** The free camera where the Medium's is now. */
static void photo_from(const Medium *m) {
  photo.x = (float)(m->x + m->cx);
  photo.y = (float)(m->y + m->cy);
  photo.z = (float)(m->z + m->cz);
  photo.yaw = (float)m->xz;
  photo.pitch = (float)m->zy;
  photo.zoom = (float)m->focus_point;
}

/** Into the photo mode from the race camera as it is. */
static void photo_enter(Medium *m) {
  photo.saved[0] = m->x;
  photo.saved[1] = m->y;
  photo.saved[2] = m->z;
  photo.saved[3] = m->xz;
  photo.saved[4] = m->zy;
  photo.saved[5] = m->focus_point;
  photo_from(m);
  photo.capture = false;
  photo.prev_buttons = ~0u;   // a button held on the way in does nothing
}

static void photo_leave(Medium *m) {
  m->x = photo.saved[0];
  m->y = photo.saved[1];
  m->z = photo.saved[2];
  m->xz = photo.saved[3];
  m->zy = photo.saved[4];
  m->focus_point = photo.saved[5];
}

/** Behind car `co`, as the race's chase camera sits (medium_follow). */
static void photo_to_car(const ContO *co) {
  const double a = (double)co->xz * 3.141592653589793 / 180.0;
  photo.yaw = (float)-co->xz;
  photo.pitch = 10.0f;
  photo.x = (float)co->x + 800.0f * (float)sin(a);
  photo.z = (float)co->z - 800.0f * (float)cos(a);
  photo.y = (float)co->y - 250.0f;
}

/** The camera onto the Medium the scene is drawn with. */
static void photo_apply(Medium *m) {
  m->x = (int32_t)photo.x - m->cx;
  m->y = (int32_t)photo.y - m->cy;
  m->z = (int32_t)photo.z - m->cz;
  m->xz = (int32_t)photo.yaw;
  m->zy = (int32_t)photo.pitch;
  m->focus_point = (int32_t)photo.zoom;
}

/** The pad this frame, and the buttons pressed since the last. */
static uint32_t photo_pad(PadAxes *p) {
  if (!platform_pad_axes(p)) *p = (PadAxes){0};
  const uint32_t press = p->buttons & ~photo.prev_buttons;
  photo.prev_buttons = p->buttons;
  return press;
}

/** The free camera's flight: left stick moves, right stick looks, ZL/ZR
 * down and up, L/R zoom, X held four times as fast. */
static void photo_move(const PadAxes *pp) {
  const PadAxes p = *pp;
  const float speed = (p.buttons & PAD_X) ? 160.0f : 40.0f;
  const double a = (double)photo.yaw * 3.141592653589793 / 180.0;
  const float fx = (float)sin(a), fz = (float)cos(a);   // where the camera looks, on the ground
  photo.x += (fx * p.ly + fz * p.lx) * speed;
  photo.z += (fz * p.ly - fx * p.lx) * speed;
  if (p.buttons & PAD_ZL) photo.y += speed * 0.75f;
  if (p.buttons & PAD_ZR) photo.y -= speed * 0.75f;
  photo.yaw += p.rx * 3.0f;
  photo.pitch -= p.ry * 2.0f;
  if (photo.pitch > 89.0f) photo.pitch = 89.0f;
  if (photo.pitch < -89.0f) photo.pitch = -89.0f;
  if (photo.yaw > 360.0f) photo.yaw -= 360.0f;
  if (photo.yaw < -360.0f) photo.yaw += 360.0f;
  if (p.buttons & PAD_R) photo.zoom *= 1.02f;
  if (p.buttons & PAD_L) photo.zoom /= 1.02f;
  if (photo.zoom < 120.0f) photo.zoom = 120.0f;
  if (photo.zoom > 2400.0f) photo.zoom = 2400.0f;
}

/** True when either stick is pushed: the replay viewer goes free with it. */
static bool photo_sticks(const PadAxes *p) { return p->lx != 0.0f || p->ly != 0.0f || p->rx != 0.0f || p->ry != 0.0f; }

/** One frame of the photo mode; returns true to leave (B). The flight
 * (photo_move), D-pad left and right the cars, Y hides the help, A takes
 * the photo. */
static bool photo_input(const ContO *co, int32_t ncars, const bool *held, const bool *prev) {
  PadAxes p;
  const uint32_t press = photo_pad(&p);
  photo_move(&p);
  if (ncars > 0 && (press & (PAD_LEFT | PAD_RIGHT))) {
    photo.car = (photo.car + ((press & PAD_RIGHT) ? 1 : ncars - 1)) % ncars;
    photo_to_car(&co[photo.car]);
  }
  if (press & PAD_Y) photo.hide_ui = !photo.hide_ui;
  if ((press & PAD_A) || (held[BTN_CONFIRM] && !prev[BTN_CONFIRM] && !(p.buttons & PAD_A))) photo.capture = true;
  if (photo.toast > 0) photo.toast--;
  return (press & PAD_B) || (held[BTN_CANCEL] && !prev[BTN_CANCEL] && !(p.buttons & PAD_B));
}

/** The last message (a photo or a replay saved), at the top. */
static void photo_draw_toast(Graphics2D *g) {
  if (photo.toast <= 0 || photo.shoot) return;
  font_set(FONT_BOLD, 13);
  const int32_t w = font_width(photo.toast_text) + 24;
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 400 - w / 2, 14, w, 24);
  gfx_set_color(g, 120, 255, 120);
  draw_centered(g, photo.toast_text, 400, 31);
}

/** Where the frame being drawn stands: a photo wanted makes it the photo
 * (call once per drawn frame, before the UI). */
static void photo_frame(void) {
  photo.shoot = photo.capture;
  photo.capture = false;
}

/** The help and the last message, over the scene (not on the photo). */
static void photo_draw_ui(Graphics2D *g, const char *car_name_now, bool replay) {
  if (photo.shoot) return;
  if (!photo.hide_ui) {
    gfx_set_color(g, 0, 0, 0);
    gfx_fill_rect(g, 0, 418, 800, 32);
    gfx_set_color(g, 255, 255, 255);
    font_set(FONT_BOLD, 11);
    draw_centered(g, replay ? "REPLAY" : "PHOTO MODE", 400, 431);
    gfx_set_color(g, 200, 200, 200);
    if (replay)
      draw_centered(g,
                    "Y: Camera   D-pad: Cars   Sticks: Free camera   L/R: Zoom   Plus: Pause   "
                    "A: Photo   Minus: Hide   B: Back",
                    400, 445);
    else
      draw_centered(g,
                    "L stick: Move   R stick: Look   ZL/ZR: Down/Up   L/R: Zoom   X: Faster   "
                    "D-pad: Cars   A: Photo   Y: Hide   B: Back",
                    400, 445);
    if (car_name_now) {
      font_set(FONT_BOLD, 12);
      gfx_set_color(g, 255, 210, 0);
      draw_centered(g, car_name_now, 400, 412);
    }
  }
  photo_draw_toast(g);
}

/** After the photo frame is drawn: the whole display, as shown, to a PNG. */
static void photo_save_frame(void) {
  photo.shoot = false;
  int32_t w, h;
  platform_display_size(&w, &h);
  uint8_t *px = malloc((size_t)w * h * 4), *flip = malloc((size_t)w * h * 3);
  char path[700];
  bool ok = false;
  if (px && flip && gallery_new_file("photos", "photo", "png", path, sizeof(path))) {
    glFinish();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    // glReadPixels' rows run bottom up, a PNG's top down; and no alpha.
    for (int32_t row = 0; row < h; row++) {
      const uint8_t *src = px + (size_t)(h - 1 - row) * w * 4;
      uint8_t *dst = flip + (size_t)row * w * 3;
      for (int32_t x = 0; x < w; x++) {
        dst[x * 3] = src[x * 4];
        dst[x * 3 + 1] = src[x * 4 + 1];
        dst[x * 3 + 2] = src[x * 4 + 2];
      }
    }
    ok = png_write_rgb(path, flip, w, h);
  }
  free(px);
  free(flip);
  if (ok) {
    const char *name = strrchr(path, '/');
    photo_say("Photo saved: ", name ? name + 1 : path);
  } else {
    photo_say("The photo could not be saved.", NULL);
  }
}

// ---- The saved-replay viewer (gallery > Replays) ----------------------------

typedef struct {
  int32_t tick;   // the ring's next tick, 0-299, round and round
  int32_t shown;  // the tick on screen (-1: none yet)
  int32_t cam;    // 0 chase, 1 orbit, 2 free
  int32_t car;
  bool paused;
} ReplayView;
static ReplayView rview;
static const char *const kReplayCams[3] = {"Chase camera", "Orbit camera", "Free camera"};

static void replay_view_start(void) {
  rview = (ReplayView){0};
  rview.shown = -1;
  const char *e = getenv("NFM_REPLAY_CAM");   // headless: the camera to start on
  if (e) rview.cam = atoi(e) % 3;
  photo.hide_ui = false;
  photo.capture = photo.shoot = false;
  photo.zoom = 400.0f;
  photo.prev_buttons = ~0u;
}

/** One frame of the viewer's input; true to leave (B). Y the camera, the
 * D-pad the car it follows, a stick goes free (photo_move), Plus pauses,
 * Minus hides the help, A takes a photo. */
static bool replay_view_input(const Medium *m, const ContO *co, int32_t ncars, const bool *held, const bool *prev) {
  PadAxes p;
  const uint32_t press = photo_pad(&p);
  if (rview.cam != 2 && photo_sticks(&p)) {
    photo_from(m);
    rview.cam = 2;
  }
  if (press & PAD_Y) {
    rview.cam = (rview.cam + 1) % 3;
    if (rview.cam == 2) photo_from(m);
  }
  if (rview.cam == 2) photo_move(&p);
  if (ncars > 0 && (press & (PAD_LEFT | PAD_RIGHT))) {
    rview.car = (rview.car + ((press & PAD_RIGHT) ? 1 : ncars - 1)) % ncars;
    if (rview.cam == 2) photo_to_car(&co[rview.car]);
  }
  if (press & PAD_PLUS) rview.paused = !rview.paused;
  if (press & PAD_MINUS) photo.hide_ui = !photo.hide_ui;
  if ((press & PAD_A) || (held[BTN_CONFIRM] && !prev[BTN_CONFIRM] && !(p.buttons & PAD_A))) photo.capture = true;
  if (photo.toast > 0) photo.toast--;
  return (press & PAD_B) || (held[BTN_CANCEL] && !prev[BTN_CANCEL] && !(p.buttons & PAD_B));
}

/** Over the Instant Replay and the finish's replay: how to keep it, and the
 * message once it is kept. */
static void replay_save_hint(Graphics2D *g) {
  font_set(FONT_BOLD, 12);
  const char *hint = tr("X: Save replay");
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 792 - font_width(hint) - 10, 422, font_width(hint) + 12, 20);
  gfx_set_color(g, 255, 255, 255);
  font_draw(g, hint, 792 - font_width(hint) - 4, 437);
  photo_draw_toast(g);
}
