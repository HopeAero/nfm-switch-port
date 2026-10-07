// Joy-Cons -> Control. Implements common/input.h's input_poll(); the scheme is
// platform/vita/input.c's, moved to the Switch's matching buttons:
//   ZR / ZL            throttle / brake-reverse    (Vita R / L)
//   left stick x       steering
//   B                  handbrake, and held in the air the stunt key (Vita Cross:
//                      the bottom face button)
//   B + left stick     stunts, in 8 directions (diagonals = two arrows)
//   right stick x      look around (the original's Z/X look-behind)
// Settings > Controls moves every button (g_bind, set by input_configure) and
// can swap the left stick for the D-pad (g_steer_dpad).
// platform_poll() (platform.c) has already called padUpdate() this frame.
#include "input.h"
#include "progress.h"
#include <switch.h>

extern PadState g_pad;

// Shared with platform.c, which reads the menu-level actions from it.
uint64_t g_bind[BIND_COUNT] = {
  HidNpadButton_ZR, HidNpadButton_ZL, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Up,
  HidNpadButton_Down, HidNpadButton_Plus, HidNpadButton_Y, HidNpadButton_Minus, HidNpadButton_R};
static bool g_steer_dpad;

void input_configure(const uint64_t mask[], bool steer_dpad) {
  for (int i = 0; i < BIND_COUNT; i++) g_bind[i] = mask[i];
  g_steer_dpad = steer_dpad;
}

static bool g_drive_up, g_drive_down, g_drive_left, g_drive_right;
static bool g_stunt_up, g_stunt_down, g_stunt_left, g_stunt_right;

// The left stick in the air, as eight directions: each axis counts while it
// is within 67.5 degrees of the stick (|component| >= sin 22.5 = 0.383 of the
// stick's length), so the four diagonal 45-degree sectors press two arrows at
// once -- the original's combined stunts (up+left, down+right, ...). A larger
// deadzone than steering, and hysteresis (an arrow already held stays down
// down to sin 15 = 0.259) so a stick resting on a sector edge does not
// chatter. The stick is +-32767 with up positive; dy comes in down-positive.
static void snap_stunt(int32_t dx, int32_t dy) {
  const int64_t kStuntDeadzone = 14336;     // 56/128 of the travel
  const int64_t x2 = (int64_t)dx * dx, y2 = (int64_t)dy * dy, mag2 = x2 + y2;
  static bool held_x = false, held_y = false;
  if (mag2 <= kStuntDeadzone * kStuntDeadzone) {
    held_x = held_y = false;
  } else {
    // c^2 >= k^2 * |v|^2, k^2 in ten-thousandths: 0.383^2 = .1464, 0.259^2 = .0670.
    held_x = x2 * 10000 >= (held_x ? 670 : 1464) * mag2;
    held_y = y2 * 10000 >= (held_y ? 670 : 1464) * mag2;
  }
  g_stunt_left = held_x && dx < 0;
  g_stunt_right = held_x && dx > 0;
  g_stunt_up = held_y && dy < 0;   // stick forward: forward loop
  g_stunt_down = held_y && dy > 0; // stick back: backward loop
}

void input_poll(Control *control) {
  const u64 b = padGetButtons(&g_pad);
  const HidAnalogStickState ls = padGetStickPos(&g_pad, 0);
  const HidAnalogStickState rs = padGetStickPos(&g_pad, 1);
  const s32 kStickDeadzone = 10240;   // the Vita's 40/128

  const bool handb = (b & g_bind[BIND_HANDB]) != 0;
  control->handb = handb;
  g_drive_up = (b & g_bind[BIND_ACCEL]) != 0;
  g_drive_down = (b & g_bind[BIND_BRAKE]) != 0;
  if (g_steer_dpad) {
    // Eight directions for free: a diagonal on the D-pad is two arrows.
    g_drive_left = (b & HidNpadButton_Left) != 0;
    g_drive_right = (b & HidNpadButton_Right) != 0;
    g_stunt_up = handb && (b & HidNpadButton_Up);
    g_stunt_down = handb && (b & HidNpadButton_Down);
    g_stunt_left = handb && g_drive_left;
    g_stunt_right = handb && g_drive_right;
  } else {
    g_drive_left = ls.x < -kStickDeadzone;
    g_drive_right = ls.x > kStickDeadzone;
    if (handb) snap_stunt(ls.x, -ls.y);
    else snap_stunt(0, 0);
  }
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
