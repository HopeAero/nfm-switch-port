// PS Vita sceCtrl -> Control. Implements common/input.h's neutral
// input_poll() -- see that header for the shared contract. NOT a port of
// any web/*.js file (see PORT_SPEC.md §5, same category as main.c). The
// keymap is a pad layout (triggers / left stick / CROSS + left stick for
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

// What the last poll read, for input_set_stunting(): the driving controls
// and the stunt directions are kept apart so the game can pick per tick.
static bool g_drive_up, g_drive_down, g_drive_left, g_drive_right;
static bool g_stunt_up, g_stunt_down, g_stunt_left, g_stunt_right;

// The left stick in the air, snapped to ONE of four directions. A stunt
// direction is a held key in the original, and the car keeps turning for as
// long as it is held, so a stick resting near a diagonal must not feed two
// at once (a loop and a roll together, which throws the car) nor flicker
// between them. So:
//   - a larger deadzone than steering, so small nudges do nothing;
//   - an axis only counts when it clearly dominates (|major| >= 1.6x
//     |minor|, about +-32 degrees around each axis); the diagonal bands
//     between the four sectors do nothing;
//   - hysteresis: a direction already held stays held while its axis still
//     leads, so riding the edge of a sector does not chatter.
static void snap_stunt(int32_t dx, int32_t dy) {
  const int32_t kStuntDeadzone = 56;
  static int32_t held_axis = 0; // 0 none, 1 horizontal, 2 vertical
  const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  int32_t axis = 0;
  if (held_axis == 1 && ax > kStuntDeadzone && ax >= ay) axis = 1;
  else if (held_axis == 2 && ay > kStuntDeadzone && ay >= ax) axis = 2;
  else if (ax > kStuntDeadzone && ax * 10 >= ay * 16) axis = 1;
  else if (ay > kStuntDeadzone && ay * 10 >= ax * 16) axis = 2;
  held_axis = axis;
  g_stunt_left = axis == 1 && dx < 0;
  g_stunt_right = axis == 1 && dx > 0;
  g_stunt_up = axis == 2 && dy < 0;   // stick forward: forward loop
  g_stunt_down = axis == 2 && dy > 0; // stick back: backward loop
}

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
  //   CROSS + left stick stunts, snapped to 4 directions (snap_stunt)
  // Which set reaches the car is decided per tick by input_set_stunting():
  // once a stunt is armed, mad_drive() reads EVERY direction as a stunt
  // until the car lands, so the triggers must not reach it then, or R/L
  // would loop the car. The D-pad no longer drives at all; in a race it
  // carries the guidance-arrow and radar toggles (platform.c).
  bool cross = (pad.buttons & SCE_CTRL_CROSS) != 0;
  control->handb = cross;
  g_drive_up = (pad.buttons & SCE_CTRL_RTRIGGER) != 0;
  g_drive_down = (pad.buttons & SCE_CTRL_LTRIGGER) != 0;
  g_drive_left = stick_left;
  g_drive_right = stick_right;
  if (cross) {
    snap_stunt((int32_t)pad.lx - 128, (int32_t)pad.ly - 128);
  } else {
    snap_stunt(0, 0);
  }
  input_set_stunting(control, false);

  // Camera: the right stick's x axis swings the chase camera around the
  // car while held and lets it ease back when released -- the original's
  // look-behind (Z/X, GameSparker.java:3596-3601), which medium_follow()
  // turns into exactly that orbit (bcxz, up to 180 degrees either way).
  // +1 orbits the camera toward the car's right. That is all the right
  // stick does.
  control->lookback = pad.rx > (128 + kStickDeadzone) ? 1
                     : (pad.rx < (128 - kStickDeadzone) ? -1 : 0);
}

void input_set_stunting(Control *control, bool stunting) {
  if (stunting) {
    // Only CROSS + the left stick; with CROSS released the car just
    // tumbles on with no input, as with no arrow key held.
    control->up = g_stunt_up;
    control->down = g_stunt_down;
    control->left = g_stunt_left;
    control->right = g_stunt_right;
  } else {
    control->up = g_drive_up;
    control->down = g_drive_down;
    control->left = g_drive_left;
    control->right = g_drive_right;
  }
}
