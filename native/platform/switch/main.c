// Nintendo Switch entry point -- see ../common/game.c for what actually runs.
// With -DNFM_NXLINK=ON, stdout/stderr go over the network to `nxlink -s`
// on the PC (the platform's printf debugging). With -DNFM_SVCLOG=ON, stderr
// goes to svcOutputDebugString instead, which emulators (Eden, yuzu) print
// in their own log.
#include "game.h"
#include "diag.h"

#include <switch.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Crash reports: a fault in the game (bad pointer, bad access) lands here
// instead of the system's error screen. It writes crash.txt beside the save
// -- the breadcrumbs, the registers, and the code addresses as offsets into
// the .nro, which `aarch64-none-elf-addr2line -e nfm_switch.elf <offset>`
// turns into file:line -- then closes the game.
u32 __nx_exception_ignoredebug = 1;
alignas(16) u8 __nx_exception_stack[0x10000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
extern char __start__[];

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
  const u64 base = (u64)(uintptr_t)__start__;
  char extra[2048];
  int n = snprintf(extra, sizeof(extra),
                   "exception %u, pc offset 0x%lx, lr offset 0x%lx, fault address 0x%lx\n"
                   "sp 0x%lx, fp 0x%lx, esr 0x%x\nbacktrace (offsets):",
                   ctx->error_desc, (unsigned long)(ctx->pc.x - base), (unsigned long)(ctx->lr.x - base),
                   (unsigned long)ctx->far.x, (unsigned long)ctx->sp.x, (unsigned long)ctx->fp.x, ctx->esr);
  // Frame-pointer walk, only through frames that sit on this thread's stack
  // above sp and are 16-byte aligned, so a smashed chain stops it instead of
  // faulting again.
  u64 fp = ctx->fp.x;
  for (int i = 0; i < 16 && n > 0 && n < (int)sizeof(extra) - 32; i++) {
    if (fp < ctx->sp.x || fp > ctx->sp.x + 0x100000 || (fp & 0xF) != 0) break;
    const u64 *frame = (const u64 *)(uintptr_t)fp;
    const u64 ret = frame[1];
    if (ret < base) break;
    n += snprintf(extra + n, sizeof(extra) - (size_t)n, " 0x%lx", (unsigned long)(ret - base));
    fp = frame[0];
  }
  for (int r = 0; r < 29 && n > 0 && n < (int)sizeof(extra) - 40; r++) {
    n += snprintf(extra + n, sizeof(extra) - (size_t)n, "%sx%d=0x%lx", r % 4 == 0 ? "\n" : "  ", r,
                  (unsigned long)ctx->cpu_gprs[r].x);
  }
  diag_write_report("crash.txt", "crash report: the game hit a fault and was closed", extra);
  svcExitProcess();
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
#ifdef NFM_SVCLOG
  consoleDebugInit(debugDevice_SVC);
  // The headless hooks game.c reads from the environment (NFM_SCREENSHOT_PPM,
  // NFM_SCREENSHOT_FRAME, NFM_SCREENSHOT_MENU, ...): homebrew has no
  // environment, so this diagnostic build takes KEY=VALUE lines from the SD
  // card instead. A plain build never reads the file.
  FILE *env = fopen("sdmc:/switch/nfm-extended/debug_env.txt", "r");
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
