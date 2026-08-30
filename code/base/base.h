#if !defined(BASE_H)
#define BASE_H

#include "base_build.h"
#if __has_include(".base_build.h")
# include ".base_build.h"
#endif

#include "base_core.h"
#include "base_math.h"
#include "base_strings.h"
#include "base_arenas.h"
#include "base_lanes.h"
#include "base_os.h"
#include "base_intrinsics.h"

#endif // BASE_H