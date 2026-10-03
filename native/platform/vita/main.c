// PS Vita entry point -- see ../common/game.h/.c for what actually runs.
// This file's only job is to hand off to it; see ../common/game.c's own
// top comment for why the game itself lives there instead of here (Part
// 18, ../../TASKS_NATIVE.md). Not compiled anywhere in this repo -- no
// VitaSDK in this environment; verify sceUserMainThread* below matches
// the game's real memory/stack needs on a real toolchain before trusting
// them (previously a bare 60-line spinning-hexagon demo -- see git
// history -- that never needed either).
#include "game.h"

// Was 1MB, which real hardware proved is NOT enough: the first run on a
// Vita died with a Data Abort inside gif_decode()'s LZW decoder, on the
// very first write into a stack frame the prologue had just grown by
// 0x6000 bytes. Root cause is game_run() itself -- one enormous function
// whose own frame measures ~680KB (`gcc -fstack-usage`, and the same at
// every -O level, so it is declared storage rather than an optimizer
// artefact), leaving under 350KB for everything it calls. The LZW
// tables have since moved to the heap (core/gif_decode.c) and Trackers
// off the stack (game.c), which together cut ~385KB, but the margin was
// still far too thin to trust: vitaGL's own reload_ffp_shaders() alone
// wants 17KB, and nothing measures total depth at runtime.
//
// 4MB restores real headroom (~6x the measured worst case) and costs
// nothing that matters -- this is address space reserved for one thread
// on a device with hundreds of MB, not a resident allocation. NOTE the
// deeper issue is unfixed: ~646KB of game_run()'s frame could not be
// attributed to any named local (the declarations sum to ~34KB), so
// something in how that 3,800-line function is compiled is holding far
// more than its variables need. Worth splitting the function up rather
// than raising this number again -- see ../../TASKS_NATIVE.md.
unsigned int sceUserMainThreadStackSize = 4 * 1024 * 1024;

int main(void) {
  return game_run();
}
