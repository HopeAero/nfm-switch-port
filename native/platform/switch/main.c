// Nintendo Switch entry point -- see ../common/game.c for what actually runs.
// With -DNFM_NXLINK=ON, stdout/stderr go over the network to `nxlink -s`
// on the PC (the platform's printf debugging). With -DNFM_SVCLOG=ON, stderr
// goes to svcOutputDebugString instead, which emulators (Eden, yuzu) print
// in their own log.
#include "game.h"

#if defined(NFM_NXLINK) || defined(NFM_SVCLOG)
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
#ifdef NFM_SVCLOG
  consoleDebugInit(debugDevice_SVC);
  // The headless hooks game.c reads from the environment (NFM_SCREENSHOT_PPM,
  // NFM_SCREENSHOT_FRAME, NFM_SCREENSHOT_MENU, ...): homebrew has no
  // environment, so this diagnostic build takes KEY=VALUE lines from the SD
  // card instead. A plain build never reads the file.
  FILE *env = fopen("sdmc:/switch/nfm/debug_env.txt", "r");
  if (env) {
    char line[256];
    while (fgets(line, sizeof(line), env)) {
      line[strcspn(line, "\r\n")] = '\0';
      char *eq = strchr(line, '=');
      if (eq && line[0] != '#') {
        *eq = '\0';
        setenv(line, eq + 1, 1);
        fprintf(stderr, "debug_env: %s=%s\n", line, eq + 1);
      }
    }
    fclose(env);
  }
#endif
#ifdef NFM_NXLINK
  socketInitializeDefault();
  int sock = nxlinkStdio();
#endif
  int rc = game_run();
#ifdef NFM_NXLINK
  if (sock >= 0) close(sock);
  socketExit();
#endif
  return rc;
}
