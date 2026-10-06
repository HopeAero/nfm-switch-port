#include "diag.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef NFM_TARGET_VITA
#include <SDL.h>
#endif

DiagInfo g_diag = {"boot", 0, 0, 0, 0, 0, 0, 0, 0, {0}};

static char g_dir[512] = ".";

void diag_init(const char *save_path) {
  if (!save_path || !save_path[0]) return;
  snprintf(g_dir, sizeof(g_dir), "%s", save_path);
  char *slash = strrchr(g_dir, '/');
  if (slash) *slash = '\0';
}

void diag_write_report(const char *name, const char *title, const char *extra) {
  char path[600];
  snprintf(path, sizeof(path), "%s/%s", g_dir, name);
  FILE *f = fopen(path, "w");
  if (!f) return;
  const time_t now = time(NULL);
  fprintf(f, "NFM %s\n", title);
  fprintf(f, "build %s %s, written at unix time %lld\n", __DATE__, __TIME__, (long long)now);
  fprintf(f, "phase: %s\n", g_diag.phase ? g_diag.phase : "?");
  fprintf(f, "state %d, stage %d, mode %d, car %d, players %d\n", (int)g_diag.state, (int)g_diag.stage,
          (int)g_diag.gmode, (int)g_diag.car, (int)g_diag.nplayers);
  fprintf(f, "frame %d, race ticks %d, heartbeat %u\n", (int)g_diag.frame, (int)g_diag.ticks,
          (unsigned)g_diag.heartbeat);
  fprintf(f, "aux:");
  for (int i = 0; i < 8; i++) fprintf(f, " %d", (int)g_diag.aux[i]);
  fprintf(f, "\n");
  if (extra) fprintf(f, "\n%s\n", extra);
  fclose(f);
  fprintf(stderr, "diag: wrote %s (%s, phase %s)\n", path, title, g_diag.phase ? g_diag.phase : "?");
}

#ifndef NFM_TARGET_VITA
// The loop draws at 60 Hz and never blocks for long (a stage loads in well
// under a second), so 8 s without a heartbeat is a hang, not a slow frame.
#define DIAG_FREEZE_SECONDS 8

static int watchdog(void *arg) {
  (void)arg;
  uint32_t last = g_diag.heartbeat;
  int still = 0;
  int reported = 0;
  for (;;) {
    SDL_Delay(1000);
    const uint32_t hb = g_diag.heartbeat;
    if (hb != last) {
      last = hb;
      still = 0;
      reported = 0;
      continue;
    }
    if (++still >= DIAG_FREEZE_SECONDS && !reported) {
      reported = 1;
      diag_write_report("freeze.txt", "freeze report: the game loop stopped advancing", NULL);
    }
  }
  return 0;
}

void diag_start_watchdog(void) {
  SDL_Thread *t = SDL_CreateThread(watchdog, "nfm-watchdog", NULL);
  if (t) SDL_DetachThread(t);
}
#else
void diag_start_watchdog(void) {}
#endif
