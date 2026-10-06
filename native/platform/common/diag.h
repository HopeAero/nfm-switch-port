// Freeze and crash reports. The game loop leaves breadcrumbs (what it is
// doing right now, and on which screen / stage / tick); a watchdog thread
// notices when the loop stops advancing and writes freeze.txt beside the
// save, and the Switch's exception handler writes crash.txt. Both are meant
// to be pasted back into a bug report.
#ifndef NFM_DIAG_H
#define NFM_DIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *volatile phase;   // what the loop is in the middle of (a string literal)
  volatile uint32_t heartbeat;  // bumped once per loop iteration
  volatile int32_t state, stage, gmode, frame, ticks, car, nplayers;
  volatile int32_t aux[8];      // phase-specific values (e.g. the car being driven)
} DiagInfo;

extern DiagInfo g_diag;

#define DIAG_PHASE(s) (g_diag.phase = (s))

/** Where reports go: the directory of the save file (no trailing slash). */
void diag_init(const char *save_path);

/** Starts the freeze watchdog (where the platform has threads). */
void diag_start_watchdog(void);

/** Writes <dir>/<name> with the breadcrumbs, plus `extra` (may be NULL).
 * Plain stdio only, so it can run from a watchdog or an exception handler. */
void diag_write_report(const char *name, const char *title, const char *extra);

#ifdef __cplusplus
}
#endif

#endif
