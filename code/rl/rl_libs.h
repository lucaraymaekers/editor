#ifndef RL_LIBS_H
#define RL_LIBS_H

#include "base/base_core.h"

#if RL_LIBS_IMPLEMENTATION
C_LINKAGE_BEGIN
# define GLAD_GL_IMPLEMENTATION
# define STB_IMAGE_IMPLEMENTATION
# define STB_IMAGE_WRITE_IMPLEMENTATION
# define STB_TRUETYPE_IMPLEMENTATION
# define STB_SPRINTF_IMPLEMENTATION
# define RL_FONT_IMPLEMENTATION

# define XXH_STATIC_LINKING_ONLY
# define XXH_IMPLEMENTATION
// TODO(luca): Does not work because firstly, this implies XXH_IMPLEMENTATION and secondly, it will make the function signatures mismatch
//# define XXH_INLINE_ALL

#else
void GLADDisableCallbacks();
void GLADEnableCallbacks();
#endif

NO_WARNINGS_BEGIN
# include "lib/gl_core_3_3_debug.h"
# include "lib/stb_image.h"
# include "lib/stb_image_write.h"
# include "lib/stb_sprintf.h"
# include "lib/stb_truetype.h"
# include "lib/xxHash/xxhash.h"
NO_WARNINGS_END

//~ GLAD helper functions

#if RL_LIBS_IMPLEMENTATION

void GLADNullPreCallback(const char *name, GLADapiproc apiproc, int len_args, ...) {}
void GLADNullPostCallback(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {}

void GLADDisableCallbacks()
{
    _pre_call_gl_callback = GLADNullPreCallback;
    _post_call_gl_callback = GLADNullPostCallback;
}

void GLADEnableCallbacks()
{
    _pre_call_gl_callback = _pre_call_gl_callback_default;
    _post_call_gl_callback = _post_call_gl_callback_default;
}

C_LINKAGE_END
#endif // RL_LIBS_IMPLEMENTATION

#endif // RL_LIBS_H