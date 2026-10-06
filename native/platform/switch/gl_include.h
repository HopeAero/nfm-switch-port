// Resolves native/core/gfx_gl.c's (and game.c's) GL calls on the Nintendo
// Switch. See native/platform/linux/gl_include.h for the desktop counterpart.
//
// The Switch has Mesa (nouveau) behind EGL, but no libGL to link the entry
// points against, and devkitPro's glad is generated for the GL 4.3 CORE
// profile -- no glBegin/glVertex/glColor, the GL 1.1 immediate mode this
// renderer is written in (PORT_SPEC.md §4). So each function the game calls
// becomes a pointer, filled from Mesa at startup (platform.c
// nfm_gl_load(), via SDL_GL_GetProcAddress, in a legacy 2.1 context where
// Mesa still serves the fixed-function pipeline). The prototypes still come
// from Mesa's own gl.h/glext.h, so every call is type-checked as before.
//
// NFM_GL_FUNCS must list every gl* function core/ and platform/common/ call:
// a missing one fails to link (no libGL), so it cannot be forgotten silently.
#ifndef NFM_SWITCH_GL_INCLUDE_H
#define NFM_SWITCH_GL_INCLUDE_H

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#define NFM_GL_FUNCS(X) \
  X(glBegin) X(glEnd) X(glVertex2f) X(glTexCoord2f) X(glColor4f) \
  X(glTexParameteri) X(glBindTexture) X(glGenTextures) X(glDeleteTextures) \
  X(glTexImage2D) X(glTexSubImage2D) X(glReadPixels) \
  X(glViewport) X(glEnable) X(glDisable) X(glClear) X(glClearColor) X(glColorMask) \
  X(glMatrixMode) X(glOrtho) X(glLoadIdentity) X(glBlendFunc) X(glFinish) \
  X(glEnableClientState) X(glDisableClientState) X(glVertexPointer) X(glColorPointer) \
  X(glDrawArrays) \
  X(glGenFramebuffers) X(glBindFramebuffer) X(glDeleteFramebuffers) \
  X(glFramebufferTexture2D) X(glCheckFramebufferStatus)

// The pointer types are taken here, while glBegin & co. still name Mesa's
// prototypes (the #defines below would turn them into the pointers).
#define NFM_GL_DECLARE(f) typedef __typeof__(&f) nfm_pfn_##f; extern nfm_pfn_##f nfm_##f;
NFM_GL_FUNCS(NFM_GL_DECLARE)
#undef NFM_GL_DECLARE

/** Fills every pointer above from the current GL context. Returns the name
 * of the first function Mesa did not provide, or NULL when all were found. */
const char *nfm_gl_load(void *(*get_proc)(const char *name));

// From here on, the game's calls go through the pointers.
#define glBegin nfm_glBegin
#define glEnd nfm_glEnd
#define glVertex2f nfm_glVertex2f
#define glTexCoord2f nfm_glTexCoord2f
#define glColor4f nfm_glColor4f
#define glTexParameteri nfm_glTexParameteri
#define glBindTexture nfm_glBindTexture
#define glGenTextures nfm_glGenTextures
#define glDeleteTextures nfm_glDeleteTextures
#define glTexImage2D nfm_glTexImage2D
#define glTexSubImage2D nfm_glTexSubImage2D
#define glReadPixels nfm_glReadPixels
#define glViewport nfm_glViewport
#define glEnable nfm_glEnable
#define glDisable nfm_glDisable
#define glClear nfm_glClear
#define glClearColor nfm_glClearColor
#define glColorMask nfm_glColorMask
#define glMatrixMode nfm_glMatrixMode
#define glOrtho nfm_glOrtho
#define glLoadIdentity nfm_glLoadIdentity
#define glBlendFunc nfm_glBlendFunc
#define glFinish nfm_glFinish
#define glEnableClientState nfm_glEnableClientState
#define glDisableClientState nfm_glDisableClientState
#define glVertexPointer nfm_glVertexPointer
#define glColorPointer nfm_glColorPointer
#define glDrawArrays nfm_glDrawArrays
#define glGenFramebuffers nfm_glGenFramebuffers
#define glBindFramebuffer nfm_glBindFramebuffer
#define glDeleteFramebuffers nfm_glDeleteFramebuffers
#define glFramebufferTexture2D nfm_glFramebufferTexture2D
#define glCheckFramebufferStatus nfm_glCheckFramebufferStatus

#endif
