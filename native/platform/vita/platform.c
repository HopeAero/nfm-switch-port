// vitaGL + sceCtrl implementation of common/platform.h. See that header
// for the shared contract; PORT_SPEC.md §5 for why this isn't a web/*.js
// port (same category as main.c). Written against the documented vitaGL/
// sceCtrl/sceKernel APIs but not compiled anywhere in this repo -- no
// VitaSDK in this environment, see ../../TASKS_NATIVE.md's Part 18 entry;
// verify the exact call shapes on a real toolchain before trusting them
// (matches this file's own predecessor, the old platform/vita/main.c demo,
// whose own header carried the identical caveat).
#include "platform.h"

#include <vitaGL.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <stdio.h>

// vglInit()'s argument is the LEGACY POOL size -- vitaGL's own header is
// explicit about this: "Legacy pool size is the amount of memory to
// reserve to handle immediate mode usage." It is scratch space for
// glBegin/glVertex/glEnd vertex data, and nothing else. It is NOT a
// texture heap.
//
// This was previously set to 64MB on exactly that misunderstanding, with
// a long comment reasoning about keeping ~139 decoded RGBA menu textures
// resident. Textures do not live here; they come out of vitaGL's own
// separate RAM/CDRAM pools -- the same budget this reservation takes 64MB
// away from. So the old value was both far too large for what the
// parameter actually controls and actively starving the allocations it
// was meant to help.
//
// 8MB -- restoring the value this port actually ran on real hardware
// with. It was 8MB on 13/08, the last day the .vpk is known to have
// worked on a Vita; it was later raised to 64MB on the mistaken belief
// that this parameter sized a texture heap, and the build stopped
// rendering. Two independent lines of evidence agree on 8MB, so it is
// preferred over any figure derived from calculation alone.
//
// Measured evidence: peak immediate-mode vertices submitted in a single
// frame, instrumented on the Linux build (which issues every one of
// these draw calls through the same core/gfx_gl.c):
//
//     main menu            102 verts
//     instructions       7,002
//     car select         7,623
//     stage select      12,657
//     racing            53,529   <- worst case, 7 cars plus track
//
// At 53,529 verts that is ~3.4MB even allowing a fat 64-byte interleaved
// vertex, and the pool is reset per frame, so 8MB clears the real worst
// case with room to spare -- and hands the other 56MB of the old
// reservation back to the pools textures actually come from.
//
// The 102-vertex main menu also narrows down the failure this replaces.
// Hardware reported a fault in glVertex2f (called from gfx_submit_gl)
// with BadVaddr = 0x00000000, i.e. vitaGL's immediate-mode write pointer
// was null. A 102-vertex frame cannot exhaust a pool of any size, so
// that pointer was not null from exhaustion -- the pool was not there at
// all, which points at the 64MB reservation having failed internally.
// Note that vitaGL does NOT report such a failure to the caller (see
// platform_init below on why its return value cannot be tested), so this
// is inference from the fault, not something the code could have caught.
#define VGL_LEGACY_POOL_SIZE 0x800000

bool platform_init(int32_t width, int32_t height) {
  (void)width;
  (void)height;
  // vitaGL owns the display/framebuffer setup itself (unlike SDL, there's
  // no separate "create a window" step -- the Vita has exactly one
  // screen). It always renders at the device's native 960x544; game.c's
  // own 800x450 game-space projection (glOrtho) still maps correctly
  // onto that through the same glViewport(0,0,width,height) call the
  // draw loop already makes every frame -- see gfx.c's own coordinate
  // convention.
  // DO NOT test this return value for success. It looks like a success
  // flag -- it is declared `GLboolean` -- but it is not one. Following it
  // through vitaGL's own source (vglInit -> vglInitExtended ->
  // vglInitWithCustomThreshold -> vglInitWithCustomSizes), the value
  // returned is a local `res_fallback`, initialised to GL_FALSE and set
  // to GL_TRUE only when the requested framebuffer dimensions exceeded
  // what the device supports and had to be corrected. A completely
  // normal, fully successful init therefore returns GL_FALSE.
  //
  // This is written as a warning because getting it wrong is not subtle:
  // an earlier version of this file did `if (!vglInit(...)) return
  // false;`, which turned every healthy startup into an aborted one and
  // the app stopped launching at all. The genuine failure modes here
  // (a pool that could not be allocated, for instance) are not reported
  // through this value at all -- vitaGL logs them internally and carries
  // on -- so there is nothing to usefully branch on.
  // Clocks. A homebrew app starts at the system's power-saving defaults
  // (ARM 333MHz, GPU 111MHz); every frame of this port -- the per-face
  // projection in plane_d, the vitaGL submission, the motion-blur
  // passes -- was running on those. 444/222/222/166 is the standard
  // maximum games and ports select, and is safe to request: the system
  // clamps anything it will not grant. Set before vglInit so the GPU is
  // already at speed when vitaGL sets itself up.
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
  vglInit(VGL_LEGACY_POOL_SIZE);
  // MUST come before any sceCtrlPeekBufferPositive() that reads lx/ly or
  // rx/ry. The pad defaults to SCE_CTRL_MODE_DIGITAL, in which the analog
  // fields are not populated and read back as 0 -- so without this the
  // stick support is not merely inert, it is actively harmful: poll()'s
  // own `pad.lx < (128 - deadzone)` sees 0 < 88 and reports a hard LEFT
  // (and UP) held forever, with the player unable to override it. The car
  // would drive itself into the nearest wall from the first frame.
  // ANALOG (not ANALOG_WIDE) because the deadzone maths in platform_poll()
  // and input.c both assume the documented 0..255 range centred on 128.
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  return true;
}

void platform_shutdown(void) {
  // vitaGL has no explicit teardown call (verified against a real
  // VitaSDK/vitaGL install -- no vglEnd/vglStop/vglTerminate in its
  // public API, only setup + vglSwapBuffers). main()'s own return
  // (main.c) unwinds through the standard VitaSDK exit path, which
  // reclaims the pools vglInit() reserved, matching how vitaGL's own
  // sample homebrew never calls any shutdown function either.
}

uint32_t platform_ticks_ms(void) {
  return (uint32_t)(sceKernelGetProcessTimeWide() / 1000);
}

uint64_t platform_ticks_us(void) {
  return (uint64_t)sceKernelGetProcessTimeWide();
}

void platform_delay_ms(uint32_t ms) {
  // No-op: vglSwapBuffers() below already blocks until the next vblank,
  // same reason SDL_GL_SetSwapInterval(1) makes the desktop backend's own
  // post-swap SDL_Delay(16) mostly redundant there too -- an unconditional
  // extra sleep here would just halve the achievable framerate.
  (void)ms;
}

void platform_swap_buffers(void) {
  vglSwapBuffers(GL_FALSE);
}

// The Vita's screen is a fixed 960x544 -- unlike platform_init()'s
// width/height (game.c's logical 800x450 game-space, see platform.h's own
// doc comment on this function), there is no window to query here, so
// this is just the device's known-fixed native resolution (already
// referenced by platform_init()'s own comment above).
void platform_display_size(int32_t *out_width, int32_t *out_height) {
  *out_width = 960;
  *out_height = 544;
}

const char *platform_asset_prefix(void) {
  // Assets are bundled into the .vpk itself (see platform/vita/
  // CMakeLists.txt's vita_create_vpk FILE list) and read back through
  // the app's own read-only romfs-style mount, not a real filesystem
  // path -- "app0:" is the standard VitaSDK convention for that.
  return "app0:";
}

bool platform_progress_path(char *buf, size_t buf_len) {
  // app0: (see platform_asset_prefix() above) is the read-only app
  // package itself -- ux0: is the writable memory-card-backed device
  // every homebrew save uses instead. VITA_TITLEID must match
  // platform/vita/CMakeLists.txt's own VITA_TITLEID exactly, or this
  // save lands in a directory nothing else on the system associates
  // with this app.
  int n = snprintf(buf, buf_len, "ux0:data/NFMD00001/progress.bin");
  return n > 0 && (size_t)n < buf_len;
}

bool platform_poll(bool held[BTN_COUNT]) {
  SceCtrlData pad;
  sceCtrlPeekBufferPositive(0, &pad, 1);

  // Left analog stick (pad.lx/ly, 0..255, 128 = centered) maps onto the
  // SAME digital up/down/left/right this port's whole control scheme is
  // built on (matching the original keyboard-only game, which never had
  // an analog axis to begin with) -- OR'd with the d-pad rather than
  // replacing it, so both work. +-40 (~31% throw) is a deliberately
  // generous deadzone: driving needs a firm, decisive push to register,
  // not stick drift, and this game's own input is boolean (held or not),
  // so there's no analog throttle/steering GRADIENT to preserve by using
  // a smaller one.
  const int32_t kStickDeadzone = 40;
  bool stick_up = pad.ly < (128 - kStickDeadzone);
  bool stick_down = pad.ly > (128 + kStickDeadzone);
  bool stick_left = pad.lx < (128 - kStickDeadzone);
  bool stick_right = pad.lx > (128 + kStickDeadzone);

  held[BTN_UP] = ((pad.buttons & SCE_CTRL_UP) != 0) || stick_up;
  held[BTN_DOWN] = ((pad.buttons & SCE_CTRL_DOWN) != 0) || stick_down;
  held[BTN_LEFT] = ((pad.buttons & SCE_CTRL_LEFT) != 0) || stick_left;
  held[BTN_RIGHT] = ((pad.buttons & SCE_CTRL_RIGHT) != 0) || stick_right;
  held[BTN_CONFIRM] = (pad.buttons & SCE_CTRL_CROSS) != 0;
  held[BTN_CANCEL] = (pad.buttons & SCE_CTRL_CIRCLE) != 0;
  held[BTN_ABANDON] = false; // F10 is this port's own dev-only hotkey, no Vita equivalent
  // The original's V/M/N keys have no Vita equivalent, so they get face/
  // shoulder buttons instead -- the same "transplant onto whichever
  // physical control reads most naturally" approach input.c's own doc
  // comment already takes for the d-pad. Triangle cycles the camera
  // (the one a player reaches for most), Square and Select mute music
  // and sound effects.
  held[BTN_VIEW] = (pad.buttons & SCE_CTRL_TRIANGLE) != 0;
  held[BTN_MUTE_MUSIC] = (pad.buttons & SCE_CTRL_SQUARE) != 0;
  held[BTN_MUTE_SFX] = (pad.buttons & SCE_CTRL_SELECT) != 0;
  // A (arrace) and S (radar) go on the D-pad: up for the guidance arrow,
  // down for the radar. The right stick is the camera and nothing else,
  // and the D-pad no longer drives or stunts (input.c), so in a race these
  // are its only jobs; game.c edge-detects both, so a held press toggles
  // once. (The D-pad still also feeds BTN_UP/DOWN above for the menus,
  // which only read them outside a race.)
  held[BTN_ARRACE] = (pad.buttons & SCE_CTRL_UP) != 0;
  held[BTN_RADAR] = (pad.buttons & SCE_CTRL_DOWN) != 0;
  // Pause on START, where a handheld player looks for it first. That
  // costs this build its old "START returns to LiveArea" gesture (see
  // the return below), which is an acceptable trade now that a real
  // pause menu exists: leaving the app is the PS button's job on this
  // platform, and SELECT goes back to being the SFX mute it was before
  // pause needed a home.
  held[BTN_PAUSE] = (pad.buttons & SCE_CTRL_START) != 0;
  held[BTN_SPECIAL] = (pad.buttons & SCE_CTRL_RTRIGGER) != 0;
  held[BTN_LISTBARS] = (pad.buttons & SCE_CTRL_LTRIGGER) != 0;

  // No window-close/OS-quit event on this platform, and START now pauses
  // instead of quitting (see BTN_PAUSE above), so nothing here ever ends
  // the run. Closing the app is the PS button / LiveArea's job, which is
  // the standard way out of Vita homebrew; the pause menu's own "Quit
  // Game" returns to the gamemode menu, not to the OS, exactly as it does
  // on desktop.
  return true;
}

// No controller vibration on this target (see common/platform.h).
bool platform_pointer(int32_t *x, int32_t *y) {
  (void)x;
  (void)y;
  return false;
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
