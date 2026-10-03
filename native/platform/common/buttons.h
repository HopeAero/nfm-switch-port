// Logical menu-navigation buttons -- NOT a port of anything in web/*.js
// (see PORT_SPEC.md §5, same category as main.c). The menu/race-loop code
// in game.c needs rising-edge ("just pressed this frame") detection for a
// small fixed set of discrete actions (move the highlighted option, confirm,
// back out, the dev-only abandon-race hotkey). Genuinely platform-neutral --
// unlike audio.h/gl_include.h (whose CONTENT differs per platform because
// the underlying device APIs differ), every platform maps its own hardware
// onto this SAME small enum, so this header lives once in common/ rather
// than being duplicated per platform/<name>/ directory.
//
// CONFIRM/CANCEL each collapse two keys the desktop build treats as
// interchangeable (Return+Space, Escape+Backspace) into one logical button,
// since every call site in the old single-file main.c always checked both
// together -- see platform/linux/platform.c's own mapping.
#ifndef NFM_BUTTONS_H
#define NFM_BUTTONS_H

typedef enum {
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_RIGHT,
  BTN_CONFIRM,  // Return/Space on desktop, Cross on Vita
  BTN_CANCEL,   // Escape/Backspace on desktop, Circle on Vita
  BTN_ABANDON,  // F10 on desktop -- dev-only manual race-abandon hotkey
  // In-race toggles, all edge-detected by game.c. These mirror the keys
  // GameSparker.java's own keyDown() handles (:3590-3639) and that the
  // Instructions screen's "other controls" page advertises, which this
  // port previously documented without implementing:
  BTN_VIEW,       // V -- cycle camera view 0/1/2 (follow / around / watch)
  BTN_MUTE_MUSIC, // M -- toggle `mutem`
  BTN_MUTE_SFX,   // N -- toggle `mutes`
  BTN_ARRACE,     // A -- toggle `arrace` (guidance arrow: track <-> cars), GameSparker.java:3618-3625
  BTN_RADAR,      // S -- toggle `radar` (map overlay + speedo), GameSparker.java:3626-3633
  // Pause. xtGraphics.java:7579-7585 enters fase -6 (and from there the
  // pause menu, fase -7) on `control.enter || control.exit` during a
  // race. It needs its own button rather than reusing BTN_CONFIRM,
  // because BTN_CONFIRM also covers Space on desktop and Space is the
  // handbrake -- braking must not pause the game. Return only.
  BTN_PAUSE,
  BTN_COUNT
} Button;

#endif
