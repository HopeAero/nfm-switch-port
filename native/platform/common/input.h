// Driving-input polling -- NOT a port of anything in web/*.js (see
// PORT_SPEC.md §5). The DECLARATION is genuinely platform-neutral (it
// only names the already-shared `Control` type from core/control.h), so
// it lives once in common/ rather than being duplicated per
// platform/<name>/ directory -- only the .c implementation (which real
// device/key state it reads) differs per platform.
#ifndef NFM_INPUT_H
#define NFM_INPUT_H

#include "control.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Polls the current held-button state and writes control->left/right/up/
 * down/handb accordingly -- every other Control field is left untouched.
 * Call once per frame, after platform_poll() (see platform.h) has pumped
 * this frame's hardware state, before mad_drive(). Implemented by
 * platform/linux/input.c (SDL keyboard) and, once written,
 * platform/vita/input.c (sceCtrl). */
void input_poll(Control *control);

#ifdef __cplusplus
}
#endif

#endif
