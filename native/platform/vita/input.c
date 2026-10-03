// PS Vita sceCtrl -> Control. Implements common/input.h's neutral
// input_poll() -- see that header for the shared contract. NOT a port of
// any web/*.js file (see PORT_SPEC.md §5, same category as main.c). The
// keymap is a pad layout (triggers / left stick / CROSS + D-pad for
// stunts / right stick camera), documented in input_poll() below.
// Written against the documented sceCtrl API but not compiled anywhere
// in this repo -- no VitaSDK in this environment,
// see ../../TASKS_NATIVE.md's Part 18 entry; verify the button-bit names
// on a real toolchain before trusting them.
//
// Touch-drag steering and the rear touchpad remain NOT wired -- out of scope for "is the port drivable on
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
  // pad.lx/ly/rx are 0..255, 128 = centered -- see platform.c's own
  // platform_poll() doc comment for why this deadzone is as generous as
  // it is (this game's own input is boolean held/not-held, no analog
  // throttle or steering gradient to preserve with a tighter one).
  const int32_t kStickDeadzone = 40;
  bool stick_left = pad.lx < (128 - kStickDeadzone);
  bool stick_right = pad.lx > (128 + kStickDeadzone);

  // The original drives everything off FOUR arrow keys: on the ground
  // up/down are throttle/brake-reverse and left/right steer, and in the
  // air -- once the handbrake is held, which is what arms a stunt in
  // mad_drive() (`handb && !wtouch` -> loop 1 -> loop 2) -- the same four
  // keys pitch and roll the car. On a pad those two jobs want different
  // controls, so the Vita splits them (user-requested scheme):
  //   R / L trigger      throttle / brake-reverse   -> up / down
  //   left stick x       steering                   -> left / right
  //   CROSS              handbrake, and held in the air, the stunt key
  //   CROSS + D-pad      stunts                     -> up/down/left/right
  // While CROSS is held the triggers do NOT feed up/down: a player who
  // keeps R squeezed through a jump and then presses CROSS to stunt
  // would otherwise get a forward flip they never asked for. Steering
  // stays on the stick as well, so a ground handbrake turn still steers.
  bool cross = (pad.buttons & SCE_CTRL_CROSS) != 0;
  bool dpad_up = (pad.buttons & SCE_CTRL_UP) != 0;
  bool dpad_down = (pad.buttons & SCE_CTRL_DOWN) != 0;
  bool dpad_left = (pad.buttons & SCE_CTRL_LEFT) != 0;
  bool dpad_right = (pad.buttons & SCE_CTRL_RIGHT) != 0;
  control->handb = cross;
  if (cross) {
    control->up = dpad_up;
    control->down = dpad_down;
    control->left = dpad_left || stick_left;
    control->right = dpad_right || stick_right;
  } else {
    control->up = (pad.buttons & SCE_CTRL_RTRIGGER) != 0;
    control->down = (pad.buttons & SCE_CTRL_LTRIGGER) != 0;
    control->left = stick_left;
    control->right = stick_right;
  }

  // Camera: the right stick's x axis swings the chase camera around the
  // car while held and lets it ease back when released -- the original's
  // look-behind (Z/X, GameSparker.java:3596-3601), which medium_follow()
  // turns into exactly that orbit (bcxz, up to 180 degrees either way).
  // +1 orbits the camera toward the car's right. The stick's y axis is
  // the arrace/radar toggles, handled in platform.c.
  control->lookback = pad.rx > (128 + kStickDeadzone) ? 1
                     : (pad.rx < (128 - kStickDeadzone) ? -1 : 0);
}
