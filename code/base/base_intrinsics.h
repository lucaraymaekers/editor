/* date = April 14th 2026 3:51 pm */

#ifndef BASE_INTRINSICS_H
#define BASE_INTRINSICS_H

#if COMPILER_GNU || COMPILER_CLANG
# define AtomicAddEvalU64(Pointer, Value) \
(__sync_fetch_and_add((Pointer), (Value), __ATOMIC_SEQ_CST) + (Value))
#elif COMPILER_MSVC
# define AtomicAddEvalU64(Pointer, Value) \
InterlockedAdd64((__int64 *)(Pointer), (Value))
#endif

#endif //BASE_INTRINSICS_H
