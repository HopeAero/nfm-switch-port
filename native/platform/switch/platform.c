// Nintendo Switch (libnx homebrew, .nro) implementation of common/platform.h.
// SDL2 for the window, the GL context and the event pump, as on Linux
// (platform/linux/platform.c); libnx's pad API for the Joy-Cons, as the Vita
// reads sceCtrl (platform/vita/platform.c) -- the button scheme is the Vita's,
// moved to the same physical positions (see input.c).
#include "platform.h"
#include "progress.h"

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

// Vibration: the handles for each way player 1 may be holding the console --
// the Joy-Cons attached (handheld), both detached in the hands (dual), or a
// Pro Controller (full key). A value is sent to all of them; the ones not in
// use just refuse it. platform_poll() sends the stop when the time is up.
static HidVibrationDeviceHandle g_vib[3][2];
static bool g_vib_ok[3];
static uint64_t g_rumble_end_ms = 0;   // 0: nothing running

static void rumble_send(float amp) {
  HidVibrationValue v[2];
  for (int i = 0; i < 2; i++) {
    v[i].amp_low = amp;
    v[i].freq_low = 160.0f;
    v[i].amp_high = amp;
    v[i].freq_high = 320.0f;
  }
  for (int k = 0; k < 3; k++) {
    if (g_vib_ok[k]) hidSendVibrationValues(g_vib[k], v, 2);
  }
}

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
  g_vib_ok[0] = R_SUCCEEDED(hidInitializeVibrationDevices(g_vib[0], 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld));
  g_vib_ok[1] = R_SUCCEEDED(hidInitializeVibrationDevices(g_vib[1], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual));
  g_vib_ok[2] = R_SUCCEEDED(hidInitializeVibrationDevices(g_vib[2], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey));

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  // Desktop GL, compatibility profile: Mesa serves the GL 1.1 fixed-function
  // pipeline in it, which is what the renderer is written in; the FBO calls
  // (ARB_framebuffer_object) are there too on nouveau. Without the profile
  // mask SDL's Switch backend hands out an OpenGL ES 3.2 context, where
  // glBegin & co. resolve to no-ops and every frame comes out black.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // 1280x720 handheld, 1920x1080 in the dock (as the console is at launch;
  // docking or undocking mid-game keeps the size it started with, the
  // console scales the other). game.c draws its 800x450 game space into an
  // offscreen target -- at this size when Settings > Graphics is HD -- and
  // stretches it to platform_display_size() at the end of each frame.
  const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
  g_window = SDL_CreateWindow("Need for Madness", 0, 0, docked ? 1920 : 1280, docked ? 1080 : 720,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
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
  const GLubyte *(*get_string)(GLenum) = (const GLubyte *(*)(GLenum))SDL_GL_GetProcAddress("glGetString");
  if (get_string) {
    fprintf(stderr, "GL: %s | %s | %s\n", (const char *)get_string(GL_VERSION),
            (const char *)get_string(GL_RENDERER), (const char *)get_string(GL_VENDOR));
  }
  SDL_GL_SetSwapInterval(1);
  return true;
}

void platform_shutdown(void) {
  rumble_send(0.0f);
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

bool platform_has_rumble(void) {
  return true;
}

void platform_rumble(float strength, uint32_t ms) {
  if (strength <= 0.0f || ms == 0) return;
  if (strength > 1.0f) strength = 1.0f;
  rumble_send(strength);
  g_rumble_end_ms = SDL_GetTicks64() + ms;
}

extern uint64_t g_bind[BIND_COUNT];   // input.c

bool platform_poll(bool held[BTN_COUNT]) {
  bool running = appletMainLoop();   // false when HOME > close asks the app to quit
  if (g_rumble_end_ms && SDL_GetTicks64() >= g_rumble_end_ms) {
    rumble_send(0.0f);
    g_rumble_end_ms = 0;
  }
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
  // The in-race toggles, wherever Settings > Controls put them (by default
  // the Vita's face buttons by position: Triangle (top) -> X, Square
  // (left) -> Y, Select -> Minus, Start -> Plus).
  held[BTN_VIEW] = (b & g_bind[BIND_VIEW]) != 0;
  held[BTN_MUTE_MUSIC] = (b & g_bind[BIND_MUSIC]) != 0;
  held[BTN_MUTE_SFX] = (b & g_bind[BIND_SFX]) != 0;
  held[BTN_ARRACE] = (b & g_bind[BIND_ARRACE]) != 0;
  held[BTN_RADAR] = (b & g_bind[BIND_RADAR]) != 0;
  held[BTN_PAUSE] = (b & g_bind[BIND_PAUSE]) != 0;
  return running;
}
