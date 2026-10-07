// SDL2 + desktop OpenGL implementation of common/platform.h. See that
// header for the shared contract; PORT_SPEC.md §5 for why this isn't a
// web/*.js port (same category as main.c).
#include "platform.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "gl_include.h"

static SDL_Window *g_window = NULL;
static SDL_GLContext g_gl = NULL;

bool platform_init(int32_t width, int32_t height) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  g_window = SDL_CreateWindow(
      "nfm native port -- linux dev target",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (!g_window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  g_gl = SDL_GL_CreateContext(g_window);
  if (!g_gl) {
    fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_GL_SetSwapInterval(1);
  return true;
}

void platform_shutdown(void) {
  if (g_gl) { SDL_GL_DeleteContext(g_gl); g_gl = NULL; }
  if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
  SDL_Quit();
}

uint32_t platform_ticks_ms(void) {
  return SDL_GetTicks();
}

uint64_t platform_ticks_us(void) {
  return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e6 / (double)SDL_GetPerformanceFrequency());
}

void platform_delay_ms(uint32_t ms) {
  SDL_Delay(ms);
}

void platform_swap_buffers(void) {
  SDL_GL_SwapWindow(g_window);
}

void platform_display_size(int32_t *out_width, int32_t *out_height) {
  int w = 0, h = 0;
  SDL_GetWindowSize(g_window, &w, &h);
  *out_width = w;
  *out_height = h;
}

const char *platform_asset_prefix(void) {
  // Repo-root-relative paths (see AGENTS.md's convention for the JS dev
  // server) -- run this binary from the repo root.
  return "";
}

// Build "<base>/nfm-psivta/progress.bin" in the caller-provided buffer;
// returns true if it fits and was written. `base` may be NULL/empty -- in
// which case this returns false so the caller tries the next fallback.
static bool build_progress_path(char *buf, size_t buf_len, const char *base) {
  if (!base || !base[0]) return false;
  int n = snprintf(buf, buf_len, "%s/nfm-psivta/progress.bin", base);
  return n > 0 && (size_t)n < buf_len;
}

bool platform_progress_path(char *buf, size_t buf_len) {
  // XDG-first then $HOME/.local/share fallback -- matches the
  // freedesktop.org basedir spec every desktop Linux distro implements.
  const char *xdg = getenv("XDG_DATA_HOME");
  if (build_progress_path(buf, buf_len, xdg)) return true;
  const char *home = getenv("HOME");
  if (!home || !home[0]) return false;
  char share[1024];
  int n = snprintf(share, sizeof(share), "%s/.local/share", home);
  if (n <= 0 || (size_t)n >= sizeof(share)) return false;
  return build_progress_path(buf, buf_len, share);
}

bool platform_poll(bool held[BTN_COUNT]) {
  bool running = true;
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_QUIT) running = false;
    if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
  }

  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  held[BTN_UP] = keys[SDL_SCANCODE_UP];
  held[BTN_DOWN] = keys[SDL_SCANCODE_DOWN];
  held[BTN_LEFT] = keys[SDL_SCANCODE_LEFT];
  held[BTN_RIGHT] = keys[SDL_SCANCODE_RIGHT];
  held[BTN_CONFIRM] = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
  held[BTN_CANCEL] = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
  held[BTN_ABANDON] = keys[SDL_SCANCODE_F10];
  // Same physical keys the original binds (GameSparker.java:3626-3639).
  held[BTN_VIEW] = keys[SDL_SCANCODE_V];
  held[BTN_MUTE_MUSIC] = keys[SDL_SCANCODE_M];
  held[BTN_MUTE_SFX] = keys[SDL_SCANCODE_N];
  // A and S are free to mean what the original means by them now that
  // input.c no longer aliases them onto steering -- see its own comment.
  held[BTN_ARRACE] = keys[SDL_SCANCODE_A];
  held[BTN_RADAR] = keys[SDL_SCANCODE_S];
  // Return only -- see buttons.h on why this is not BTN_CONFIRM. The
  // original also pauses on `control.exit` (Escape), but Escape is this
  // build's own quit-the-application key (see the event loop above) and
  // taking that away would leave no way out of the window, so pause is
  // Return-only here.
  held[BTN_PAUSE] = keys[SDL_SCANCODE_RETURN];
  held[BTN_SPECIAL] = keys[SDL_SCANCODE_Q];
  return running;
}

// No controller vibration on this target (see common/platform.h).
bool platform_pointer(int32_t *x, int32_t *y) {
  int mx, my, w, h;
  if (!(SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT))) return false;
  SDL_GetWindowSize(g_window, &w, &h);
  if (w <= 0 || h <= 0) return false;
  *x = mx * 800 / w;
  *y = my * 450 / h;
  return true;
}

bool platform_take_focus_lost(void) {
  return false;
}

bool platform_has_rumble(void) {
  return false;
}

void platform_rumble(float strength, uint32_t ms) {
  (void)strength;
  (void)ms;
}
