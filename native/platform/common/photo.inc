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
  bool capture;       // this frame is the photo: no UI on it, then saved
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

/** Into the photo mode from the race camera as it is. */
static void photo_enter(Medium *m) {
  photo.saved[0] = m->x;
  photo.saved[1] = m->y;
  photo.saved[2] = m->z;
  photo.saved[3] = m->xz;
  photo.saved[4] = m->zy;
  photo.saved[5] = m->focus_point;
  photo.x = (float)(m->x + m->cx);
  photo.y = (float)(m->y + m->cy);
  photo.z = (float)(m->z + m->cz);
  photo.yaw = (float)m->xz;
  photo.pitch = (float)m->zy;
  photo.zoom = (float)m->focus_point;
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

/** One frame of the free camera; returns true to leave (B). Left stick
 * moves, right stick looks, ZL/ZR down and up, L/R zoom, D-pad left and
 * right the cars, Y hides the help, A takes the photo. */
static bool photo_input(const ContO *co, int32_t ncars, const bool *held, const bool *prev) {
  PadAxes p;
  if (!platform_pad_axes(&p)) p = (PadAxes){0};
  const uint32_t press = p.buttons & ~photo.prev_buttons;
  photo.prev_buttons = p.buttons;
  // X held moves four times as fast.
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
  if (ncars > 0 && (press & (PAD_LEFT | PAD_RIGHT))) {
    photo.car = (photo.car + ((press & PAD_RIGHT) ? 1 : ncars - 1)) % ncars;
    photo_to_car(&co[photo.car]);
  }
  if (press & PAD_Y) photo.hide_ui = !photo.hide_ui;
  if ((press & PAD_A) || (held[BTN_CONFIRM] && !prev[BTN_CONFIRM] && !(p.buttons & PAD_A))) photo.capture = true;
  if (photo.toast > 0) photo.toast--;
  return (press & PAD_B) || (held[BTN_CANCEL] && !prev[BTN_CANCEL] && !(p.buttons & PAD_B));
}

/** The help and the last message, over the scene (not on the photo). */
static void photo_draw_ui(Graphics2D *g, const char *car_name_now, bool replay) {
  if (photo.capture) return;
  if (!photo.hide_ui) {
    gfx_set_color(g, 0, 0, 0);
    gfx_fill_rect(g, 0, 418, 800, 32);
    gfx_set_color(g, 255, 255, 255);
    font_set(FONT_BOLD, 11);
    draw_centered(g, replay ? "REPLAY - PHOTO MODE" : "PHOTO MODE", 400, 431);
    gfx_set_color(g, 200, 200, 200);
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
  if (photo.toast > 0) {
    font_set(FONT_BOLD, 13);
    const int32_t w = font_width(photo.toast_text) + 24;
    gfx_set_color(g, 0, 0, 0);
    gfx_fill_rect(g, 400 - w / 2, 14, w, 24);
    gfx_set_color(g, 120, 255, 120);
    draw_centered(g, photo.toast_text, 400, 31);
  }
}

/** After the photo frame is drawn: the whole display, as shown, to a PNG. */
static void photo_save_frame(void) {
  photo.capture = false;
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
