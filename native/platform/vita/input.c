// PS Vita sceCtrl -> Control. Implements common/input.h's neutral
// input_poll() -- see that header for the shared contract. NOT a port of
// any web/*.js file (see PORT_SPEC.md §5, same category as main.c). Same
// keymap SHAPE as platform/linux/input.c (D-pad = steer/throttle, one
// face button = handbrake) -- see that file's own doc comment for why
// (web/main.js's own installInput() binding, transplanted onto whichever
// physical buttons read most naturally on this device instead of
// reinventing a scheme). Written against the documented sceCtrl API but
// not compiled anywhere in this repo -- no VitaSDK in this environment,
// see ../../TASKS_NATIVE.md's Part 18 entry; verify the button-bit names
// on a real toolchain before trusting them.
//
// Left analog stick input (SceCtrlData's lx/ly) IS wired, on top of the
// d-pad rather than instead of it -- user-reported: driving only worked
// off the digital d-pad, not the stick most players reach for first on a
// racing game. The RIGHT stick is wired too, but in platform.c rather
// than here: it carries the arrace/radar toggles, which travel the
// button path, not this Control one. Touch-drag steering and the rear
// touchpad remain NOT wired -- out of scope for "is the port drivable on
// real hardware", not a scheme decision to make later, same class of
// omission platform/linux/input.c's own doc comment already accepts for
// gamepad/touch on that target.
//
// The lx/ly reads below only return real values because platform_init()
// calls sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) at startup; in the
// default DIGITAL mode they read 0, which the deadzone test below would
// report as a permanent hard left. See that call's own comment.
#include "input.h"
#include <psp2/ctrl.h>

void input_poll(Control *control) {
  SceCtrlData pad;
  sceCtrlPeekBufferPositive(0, &pad, 1);
  // pad.lx/ly are 0..255, 128 = centered -- see platform.c's own
  // platform_poll() doc comment for why this deadzone is as generous as
  // it is (this game's own input is boolean held/not-held, no analog
  // throttle or steering gradient to preserve with a tighter one).
  const int32_t kStickDeadzone = 40;
  bool stick_up = pad.ly < (128 - kStickDeadzone);
  bool stick_down = pad.ly > (128 + kStickDeadzone);
  bool stick_left = pad.lx < (128 - kStickDeadzone);
  bool stick_right = pad.lx > (128 + kStickDeadzone);
  control->up = ((pad.buttons & SCE_CTRL_UP) != 0) || stick_up;
  control->down = ((pad.buttons & SCE_CTRL_DOWN) != 0) || stick_down;
  control->left = ((pad.buttons & SCE_CTRL_LEFT) != 0) || stick_left;
  control->right = ((pad.buttons & SCE_CTRL_RIGHT) != 0) || stick_right;
  control->handb = (pad.buttons & SCE_CTRL_CROSS) != 0;
  // Look-behind (the original's Z/X, GameSparker.java:3596-3601) mapped
  // onto the shoulder triggers, which is where a held "glance" control
  // belongs on a pad. Held state, not a toggle, matching the original.
  control->lookback = (pad.buttons & SCE_CTRL_LTRIGGER) ? 1
                     : ((pad.buttons & SCE_CTRL_RTRIGGER) ? -1 : 0);
}
