// Desktop Linux keyboard -> Control. See common/input.h for the shared
// contract and PORT_SPEC.md §5 for why this isn't a web/*.js port.
#include "input.h"
#include <SDL.h>

void input_poll(Control *control) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  // Arrow keys ONLY -- the WASD aliases this used to also accept were an
  // addition of this port's, not something the original ever had, and they
  // directly collided with two keys the original DOES bind: A toggles
  // `arrace` (GameSparker.java:3618-3625) and S toggles `radar` (:3626-3633).
  // Keeping WASD meant those two could never be wired at all, so under the
  // 1:1 mandate the aliases lose and the original bindings win.
  control->up = keys[SDL_SCANCODE_UP];
  control->down = keys[SDL_SCANCODE_DOWN];
  control->left = keys[SDL_SCANCODE_LEFT];
  control->right = keys[SDL_SCANCODE_RIGHT];
  control->handb = keys[SDL_SCANCODE_SPACE];
  // GameSparker.java:3596-3601 sets lookback on Z/X keyDown and
  // :3675 clears it on keyUp -- a HELD state, not a toggle, so it reads
  // straight off the key state here rather than going through the
  // edge-detected button path the in-race toggles use. Z looks one way,
  // X the other; medium_follow() already consumes it (see game.c's
  // camera dispatch), it just never had anything to consume until now.
  control->lookback = keys[SDL_SCANCODE_Z] ? 1 : (keys[SDL_SCANCODE_X] ? -1 : 0);
}
