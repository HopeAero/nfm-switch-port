// Joy-Cons -> Control. Implements common/input.h's input_poll(); the scheme is
// platform/vita/input.c's, moved to the Switch's matching buttons:
//   ZR / ZL            throttle / brake-reverse    (Vita R / L)
//   left stick x       steering
//   B                  handbrake, and held in the air the stunt key (Vita Cross:
//                      the bottom face button)
//   B + left stick     stunts, snapped to 4 directions
//   right stick x      look around (the original's Z/X look-behind)
// platform_poll() (platform.c) has already called padUpdate() this frame.
#include "input.h"
#include <switch.h>

extern PadState g_pad;

static bool g_drive_up, g_drive_down, g_drive_left, g_drive_right;
static bool g_stunt_up, g_stunt_down, g_stunt_left, g_stunt_right;

// The Vita's snap_stunt (platform/vita/input.c has the reasoning): one of four
// directions, a larger deadzone than steering, an axis only when it clearly
// dominates (|major| >= 1.6x |minor|), hysteresis on the held axis. The stick
// here is +-32767 with up positive; dy is flipped to the Vita's down-positive.
static void snap_stunt(int32_t dx, int32_t dy) {
  const int32_t kStuntDeadzone = 14336;   // the Vita's 56/128
  static int32_t held_axis = 0;           // 0 none, 1 horizontal, 2 vertical
  const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  int32_t axis = 0;
  if (held_axis == 1 && ax > kStuntDeadzone && ax >= ay) axis = 1;
  else if (held_axis == 2 && ay > kStuntDeadzone && ay >= ax) axis = 2;
  else if (ax > kStuntDeadzone && (int64_t)ax * 10 >= (int64_t)ay * 16) axis = 1;
  else if (ay > kStuntDeadzone && (int64_t)ay * 10 >= (int64_t)ax * 16) axis = 2;
  held_axis = axis;
  g_stunt_left = axis == 1 && dx < 0;
  g_stunt_right = axis == 1 && dx > 0;
  g_stunt_up = axis == 2 && dy < 0;   // stick forward: forward loop
  g_stunt_down = axis == 2 && dy > 0; // stick back: backward loop
}

void input_poll(Control *control) {
  const u64 b = padGetButtons(&g_pad);
  const HidAnalogStickState ls = padGetStickPos(&g_pad, 0);
  const HidAnalogStickState rs = padGetStickPos(&g_pad, 1);
  const s32 kStickDeadzone = 10240;   // the Vita's 40/128

  const bool handb = (b & HidNpadButton_B) != 0;
  control->handb = handb;
  g_drive_up = (b & HidNpadButton_ZR) != 0;
  g_drive_down = (b & HidNpadButton_ZL) != 0;
  g_drive_left = ls.x < -kStickDeadzone;
  g_drive_right = ls.x > kStickDeadzone;
  if (handb) snap_stunt(ls.x, -ls.y);
  else snap_stunt(0, 0);
  input_set_stunting(control, false);

  control->lookback = rs.x > kStickDeadzone ? 1 : (rs.x < -kStickDeadzone ? -1 : 0);
}

void input_set_stunting(Control *control, bool stunting) {
  if (stunting) {
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
