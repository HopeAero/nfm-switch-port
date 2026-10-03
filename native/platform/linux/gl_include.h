// Resolves native/core/gfx_gl.c's GL calls to real desktop OpenGL on Linux.
// See native/platform/vita/gl_include.h for the Vita counterpart -- each
// platform's CMakeLists adds its own directory to the include path so
// gfx_gl.c's `#include "gl_include.h"` picks up the right one.
//
// glext.h adds the framebuffer-object entry points gfx_gl.c's render-
// target support needs (glGenFramebuffers/glBindFramebuffer/
// glFramebufferTexture2D/glCheckFramebufferStatus -- GL 3.0 core /
// ARB_framebuffer_object, not part of the GL1.1 subset PORT_SPEC.md §5
// otherwise holds this renderer to; see gfx_gl.h's own doc comment on
// gfx_gl_render_target_blit for why this one exception exists). These
// need to resolve as directly linkable symbols against libGL.so (no
// glXGetProcAddress loading is set up) -- verified against this
// sandbox's Mesa/llvmpipe build; flag this if a desktop build ever hits
// an undefined-reference on one of them, since that would mean some
// other driver doesn't export them the same way.
#define GL_GLEXT_PROTOTYPES // glext.h only declares prototypes (not just the
                            // typedefs/enums) with this defined -- see
                            // above, we're relying on libGL.so exporting
                            // these directly rather than loading them via
                            // glXGetProcAddress.
#include <GL/gl.h>
#include <GL/glext.h>
