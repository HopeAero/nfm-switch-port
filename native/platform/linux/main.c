// Desktop Linux dev target for the native C port -- see
// ../common/game.h/.c for what actually runs. This file's only job is to
// hand off to it; see ../common/game.c's own top comment for why the
// game itself lives there instead of here (Part 18, ../../TASKS_NATIVE.md).
#include "game.h"

int main(void) {
  return game_run();
}
