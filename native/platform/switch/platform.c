// Nintendo Switch (libnx homebrew, .nro) implementation of common/platform.h.
// SDL2 for the window, the GL context and the event pump, as on Linux
// (platform/linux/platform.c); libnx's pad API for the Joy-Cons, as the Vita
// reads sceCtrl (platform/vita/platform.c) -- the button scheme is the Vita's,
// moved to the same physical positions (see input.c).
#include "platform.h"

#include <SDL.h>
#include <stdio.h>
#include <switch.h>
#include "gl_include.h"

// The GL entry points gl_include.h declares, and their loader.
#define NFM_GL_DEFINE(f) nfm_pfn_##f nfm_##f = NULL;
NFM_GL_FUNCS(NFM_GL_DEFINE)
#undef NFM_GL_DEFINE

const char *nfm_gl_load(void *(*get_proc)(const char *name)) {
#define NFM_GL_LOAD(f) if (!(nfm_##f = (nfm_pfn_##f)get_proc(#f))) return #f;
  NFM_GL_FUNCS(NFM_GL_LOAD)
#undef NFM_GL_LOAD
  return NULL;
}

static SDL_Window *g_window = NULL;
static SDL_GLContext g_gl = NULL;
PadState g_pad;   // shared with input.c

bool platform_init(int32_t width, int32_t height) {
  (void)width;
  (void)height;
  // The assets (data/, stages/, music/, mycars/, mystages/) are packed into
  // the .nro's RomFS; platform_asset_prefix() reads them back as romfs:/.
  Result rc = romfsInit();
  if (R_FAILED(rc)) {
    fprintf(stderr, "romfsInit failed: 0x%x\n", rc);
    return false;
  }
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&g_pad);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  // A legacy (pre-3.1) context: Mesa serves the GL 1.1 fixed-function
  // pipeline in it, which is what the renderer is written in; the FBO calls
  // (ARB_framebuffer_object) are there too on nouveau.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // The screen is 1280x720 (docked output is scaled by the console); game.c
  // draws its 800x450 game space to an offscreen target and stretches it to
  // platform_display_size() at the end of each frame.
  g_window = SDL_CreateWindow("Need for Madness", 0, 0, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
  if (!g_window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }
  g_gl = SDL_GL_CreateContext(g_window);
  if (!g_gl) {
    fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    return false;
  }
  const char *missing = nfm_gl_load(SDL_GL_GetProcAddress);
  if (missing) {
    fprintf(stderr, "GL function not available: %s\n", missing);
    return false;
  }
  SDL_GL_SetSwapInterval(1);
  return true;
}

void platform_shutdown(void) {
  if (g_gl) { SDL_GL_DeleteContext(g_gl); g_gl = NULL; }
  if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
  SDL_Quit();
  romfsExit();
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
  int w = 1280, h = 720;
  if (g_window) SDL_GetWindowSize(g_window, &w, &h);
  *out_width = w;
  *out_height = h;
}

const char *platform_asset_prefix(void) {
  return "romfs:/";
}

bool platform_progress_path(char *buf, size_t buf_len) {
  // The SD card, where homebrew keeps its data; progress.c creates the folder.
  int n = snprintf(buf, buf_len, "sdmc:/switch/nfm/progress.bin");
  return n > 0 && (size_t)n < buf_len;
}

bool platform_poll(bool held[BTN_COUNT]) {
  bool running = appletMainLoop();   // false when HOME > close asks the app to quit
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_QUIT) running = false;
  }
  padUpdate(&g_pad);
  const u64 b = padGetButtons(&g_pad);
  const HidAnalogStickState ls = padGetStickPos(&g_pad, 0);
  // Stick range is +-32767, up is positive; the Vita's 40/128 deadzone.
  const s32 kDead = 10240;
  // Menus move on the D-pad or the left stick, as on the Vita.
  held[BTN_UP] = (b & HidNpadButton_Up) || ls.y > kDead;
  held[BTN_DOWN] = (b & HidNpadButton_Down) || ls.y < -kDead;
  held[BTN_LEFT] = (b & HidNpadButton_Left) || ls.x < -kDead;
  held[BTN_RIGHT] = (b & HidNpadButton_Right) || ls.x > kDead;
  // Nintendo's own convention: A confirms, B goes back.
  held[BTN_CONFIRM] = (b & HidNpadButton_A) != 0;
  held[BTN_CANCEL] = (b & HidNpadButton_B) != 0;
  held[BTN_ABANDON] = false;
  // The Vita's face buttons, by position: Triangle (top) -> X,
  // Square (left) -> Y, Select -> Minus, Start -> Plus.
  held[BTN_VIEW] = (b & HidNpadButton_X) != 0;
  held[BTN_MUTE_MUSIC] = (b & HidNpadButton_Y) != 0;
  held[BTN_MUTE_SFX] = (b & HidNpadButton_Minus) != 0;
  held[BTN_ARRACE] = (b & HidNpadButton_Up) != 0;
  held[BTN_RADAR] = (b & HidNpadButton_Down) != 0;
  held[BTN_PAUSE] = (b & HidNpadButton_Plus) != 0;
  return running;
}
