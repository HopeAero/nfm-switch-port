// Resolves native/core/gfx_gl.c's GL calls to vitaGL on PS Vita -- see
// native/platform/linux/gl_include.h for the Linux counterpart. Unverified:
// no VitaSDK/vitaGL in the environment that wrote this, confirm vitaGL.h
// actually provides GLubyte/GL_TRIANGLES/glColor4ub/glVertex2f with these
// signatures before trusting gfx_gl.c compiles against it unmodified.
#include <vitaGL.h>
