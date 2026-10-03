// The whole game -- see game.c's own top comment for what this is and
// why it's shared by every platform target.
#ifndef NFM_GAME_H
#define NFM_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

/** Runs the full menu/race loop until the player quits. Returns the
 * process exit code (0 on a clean quit, 1 on an unrecoverable startup
 * failure -- missing mycars/Simple_Car.rad, data/models.zip, or a failed
 * platform_init()). Blocks until then -- call this last from each
 * platform's own main(). */
int game_run(void);

#ifdef __cplusplus
}
#endif

#endif
