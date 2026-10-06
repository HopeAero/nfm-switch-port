// Nintendo Switch entry point -- see ../common/game.c for what actually runs.
// With -DNFM_NXLINK=ON, stdout/stderr go over the network to `nxlink -s`
// on the PC (the platform's printf debugging). With -DNFM_SVCLOG=ON, stderr
// goes to svcOutputDebugString instead, which emulators (Eden, yuzu) print
// in their own log.
#include "game.h"

#if defined(NFM_NXLINK) || defined(NFM_SVCLOG)
#include <switch.h>
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
#ifdef NFM_SVCLOG
  consoleDebugInit(debugDevice_SVC);
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
