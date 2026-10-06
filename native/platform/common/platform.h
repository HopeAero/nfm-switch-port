// Minimal windowing/lifecycle interface game.c needs from whichever real
// platform it's running on -- NOT a port of anything in web/*.js (see
// PORT_SPEC.md §5). This is the ENTIRE platform-specific surface the old
// single-file platform/linux/main.c had (SDL window/GL-context creation,
// the event pump, vsync'd buffer swap, wall-clock ticks): isolating it
// here is what lets game.c itself stay one shared file for every target
// instead of a per-platform copy that silently drifts out of sync (see
// native/TASKS_NATIVE.md's Part 18 entry for the concrete bug class this
// avoids -- the stages 28-32 crash was exactly a "only fixed in one
// branch" risk).
//
// Genuinely platform-neutral (unlike audio.h/gl_include.h, whose CONTENT
// differs per platform) -- every implementation exposes the exact same
// functions below, so this header lives once in common/ rather than
// being duplicated per platform/<name>/ directory. Implemented by
// platform/linux/platform.c (SDL2 + desktop GL) and platform/vita/
// platform.c (vitaGL + sceCtrl's START button for quit).
#ifndef NFM_PLATFORM_H
#define NFM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buttons.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opens a window (if the platform has one) and an OpenGL 1.x-compatible
 * context at exactly `width`x`height`, current on return. Returns false on
 * failure -- callers should treat that as fatal (nothing else in game.c
 * can run without a GL context). */
bool platform_init(int32_t width, int32_t height);

/** Tears down whatever platform_init created. */
void platform_shutdown(void);

/** Milliseconds on some monotonic wall clock -- only ever used for deltas
 * (game.c's fixed-timestep accumulator), so the epoch doesn't matter. */
uint32_t platform_ticks_ms(void);

/** Monotonic microseconds, for the NFM_SHOW_FPS frame-phase breakdown
 * (platform_ticks_ms is too coarse to split a 16ms frame). */
uint64_t platform_ticks_us(void);

/** Best-effort sleep, for the desktop build's own CPU-usage throttle.
 * Implementations that already block on vsync inside
 * platform_swap_buffers() (the Vita target) may make this a no-op. */
void platform_delay_ms(uint32_t ms);

/** Presents the frame rendered since the last call (vitaGL/SDL's own
 * double-buffer swap). */
void platform_swap_buffers(void);

/** The ACTUAL output resolution this platform presents to, which need
 * not match the `width`/`height` passed to platform_init(): those pick
 * game.c's fixed 800x450 logical game-space (every HUD coordinate, the
 * glOrtho projection, and the offscreen motion-blur render target are
 * all sized to it, see game.c's own doc comment on that glOrtho call),
 * while this is whatever the real display device physically is. On
 * desktop that's the same 800x450 window platform_init() created
 * (SDL_GetWindowSize), so callers get back exactly what they passed in.
 * On the Vita there is no window to resize -- the screen is a fixed
 * 960x544 -- so this returns that instead. game.c's own final composite
 * blit (the only place besides the offscreen render target that calls
 * glViewport) uses this, NOT the logical width/height, specifically so
 * that blit's full-quad draw stretches to fill the entire physical
 * screen instead of only an 800x450 sub-rectangle of it. */
void platform_display_size(int32_t *out_width, int32_t *out_height);

/** Pumps whatever OS/hardware event source this platform has and fills
 * `held[BTN_COUNT]` with this frame's logical button state (see
 * buttons.h). Returns false if the app should exit (window-close/Escape
 * on desktop, START on Vita) -- game.c's own main loop condition. Call
 * once per frame, before reading `held[]` or calling input_poll(). */
bool platform_poll(bool held[BTN_COUNT]);

/** Prefix game.c passes straight to vfs_set_fpath() (core/vfs.h) before
 * reading any asset, so every "data/images.zip"/"stages/N.txt"/
 * "music/stageN.zip" literal in game.c stays platform-neutral: "" on
 * desktop (paths are relative to the CWD this binary is run from), the
 * Vita build's assets are bundled into the .vpk and read back via the
 * "app0:" device prefix instead. */
const char *platform_asset_prefix(void);

/** Writes this platform's save-file path for core/progress.c's
 * game_progress_save_to_disk()/load_from_disk() into `buf` (must fit
 * within `buf_len`, including the NUL). Returns false if no usable
 * location exists (e.g. desktop with neither $XDG_DATA_HOME nor $HOME
 * set) -- callers should treat that as "progress won't persist this
 * session" rather than fatal, same fallback philosophy as a failed
 * platform_asset_prefix()-rooted asset load. Deliberately separate from
 * platform_asset_prefix(): that one picks a READ-ONLY prefix vfs.c
 * prepends to many small relative asset paths, this picks one WRITABLE
 * full path for the one save file, and the two differ on Vita (app0: is
 * read-only; save data goes to ux0:). */
bool platform_progress_path(char *buf, size_t buf_len);

/** Whether this platform can vibrate its controller (the Settings screen
 * only offers Vibration where it can). */
bool platform_has_rumble(void);

/** Vibrates the controller at `strength` (0..1) for about `ms` milliseconds,
 * replacing any vibration still running; a no-op where there is none.
 * platform_poll() ends it when the time is up. */
void platform_rumble(float strength, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
