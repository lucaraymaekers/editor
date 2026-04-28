#if !defined(BASE_CORE_H)
#define BASE_CORE_H

// detect OS
#if ANDROID
# define OS_ANDROID 1
#elif __linux__
# define OS_LINUX 1
#elif _WIN32
# define OS_WINDOWS 1
#endif

// Detect compiler
#if __clang__
# define COMPILER_CLANG 1
#elif _MSC_VER
# define COMPILER_MSVC 1
#elif __GNUC__
# define  COMPILER_GNU 1
#endif

// Detect language
#if defined(__cplusplus)
# define LANG_CPP 1
#else
# define LANG_C 1
#endif

// Zero undefined
#if !defined(OS_LINUX)
# define OS_LINUX 0
#endif
#if !defined(OS_WINDOWS)
# define OS_WINDOWS 0
#endif
#if !defined(OS_ANDROID)
# define OS_ANDROID 0
#endif
#if !defined(COMPILER_MSVC)
# define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_LLVM)
# define COMPILER_LLVM 0
#endif
#if !defined(COMPILER_GNU)
# define COMPILER_GNU 0
#endif
#if !defined(LANG_C)
# define LANG_C 0
#endif
#if !defined(LANG_CPP)
# define LANG_CPP 0
#endif
#if !defined(BASE_CONSOLE_APPLICATION)
# define BASE_CONSOLE_APPLICATION 0
#endif
#if !defined(BASE_FORCE_THREADS_COUNT)
# define BASE_FORCE_THREADS_COUNT 0
#endif
#if !defined(BASE_PROFILE)
# define BASE_PROFILE 0
#endif

//~ OS
#include <stdint.h>
#include <stddef.h>
#include <string.h> // memset

#if OS_WINDOWS

# define _CRT_SECURE_NO_WARNINGS 1
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
# include <stdio.h>
# define RADDBG_MARKUP_IMPLEMENTATION
# define ssize_t SSIZE_T
#else
# define RADDBG_MARKUP_STUBS
# include <sys/types.h>
#endif

#if defined(COMPILER_MSVC)
#  if defined(__cplusplus)
// NOTE(luca): Decay to numeric type.
#    define TypeOf(x) decltype((x)+0)
#  else
#    define TypeOf(x) __typeof__(x)
#  endif
#elif defined(COMPILER_GNU) || defined(COMPILER_CLANG)
#  define TypeOf(x) typeof(x)
#else
#  error "Unsupported compiler"
#endif

//~ Macros
#define ERROR_FMT "%s(%d): ERROR: "
#define ERROR_ARG __FILE__, __LINE__

//-
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define CeilIntegerDiv(A,B) (((A) + (B) - 1)/(B))

//-
#define Stringify_(S) #S
#define Stringify(S) Stringify_(S)

#define Glue_(A,B) A##B
#define Glue(A,B) Glue_(A,B)

//-
#define Min(A, B) (((A) < (B)) ? (A) : (B))
#define Max(A, B) (((A) > (B)) ? (A) : (B))
#define ClampTop(A, X) Min(A,X)
#define ClampBot(X, B) Max(X,B)
#define Clamp(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) : (X))

#if LANG_C
# define Swap(A, B) do { TypeOf((A)) (temp) = (TypeOf((A)))(A); (A) = (B); (B) = (temp); } while(0)
#else
template <typename t> inline void
Swap(t& A, t& B) { t T = A; A = B; B = T; }
#endif

//-
#if LANG_C
# define ZeroStruct {0}
#elif LANG_CPP
# define ZeroStruct {0}
#endif

#if OS_LINUX || OS_ANDROID
# define OS_SlashChar '/'
# define SLASH "/"
#elif OS_WINDOWS
# define OS_SlashChar '\\'
# define SLASH "\\"
#endif

#if (COMPILER_CLANG || COMPILER_GNU)
# if defined(__arm__) || defined(__aarch64__)
#  define Trap() __asm__ volatile("brk #0");
# elif defined(__i386__) || defined(__x86_64__)
#  define Trap() __asm__ volatile("int3");
# endif
#elif COMPILER_MSVC
# define Trap() (__debugbreak());
#else
# define Trap() *(int *)0 = 0;
#endif

#if COMPILER_MSVC
# define ReadWriteBarrier() _ReadBarrier(); _WriteBarrier();
#elif COMPILER_GNU || COMPILER_CLANG
# define ReadWriteBarrier() __asm__ __volatile__ ("" : : : "memory")
#endif
#define NoOp() do { int Glue(X, __LINE__) = 3; } while(0)

#define DebugBreak() do { if(GlobalDebuggerIsAttached) { Trap(); } } while(0)

#define Var(Name) Glue(Name, __LINE__)
#define DoOnce local_persist s32 Var(X) = 0; Var(X) += 1; if(Var(X) < 2)
#define DebugBreakOnce() do { DoOnce { DebugBreak(); }; } while(0);

#define DebugBreakMsg(Format, ...) do { ErrorLog(Format, ##__VA_ARGS__); DebugBreak(); } while(0)
#define TrapMsg(Format, ...) do { ErrorLog(Format, ##__VA_ARGS__); Trap(); } while(0)
#define AssertMsg(Expression, Format, ...) \
do { if(!(Expression)) DebugBreakMsg(Format, ##__VA_ARGS__); } while(0)
#define Assert(Expression) AssertMsg(Expression, "Hit assertion")

#define NotImplemented() DebugBreakMsg("Not Implemented!")
#define InvalidPath()    DebugBreakMsg("Invalid Path!")
#define StaticAssert(C, ID) global_variable u8 Glue(ID, __LINE__)[(C)?1:-1]

//~ Loop macros

#define DeferLoop_(Begin, End, c) \
for(int Glue(_i_, c) = ((Begin), 0); !Glue(_i_, c); Glue(_i_, c) += 1, (End))
#define DeferLoop(Begin, End) DeferLoop_(Begin, End, __COUNTER__)


#define EachIndexType(t, Index, Count) (t Index = 0; Index < (Count); Index += 1)
#define EachIndex(Index, Count)           EachIndexType(TypeOf((Count)), Index, Count)
#define EachElement(Index, Array)         EachIndexType(umm, Index, ArrayCount(Array))
#define EachInRange(Index, Range)         (TypeOf((Range).Min) Index = (Range).Min; Index < (Range).Max; Index += 1)
#define EachNode(Index, t, First)      (t *Index = First; Index != 0; Index = Index->Next)
#define EachCount(Count) EachIndex(Glue(_i_, __COUNTER__), Count)

#define MemoryCopy(Dest, Source, Count) memmove(Dest, Source, Count)
#define MemorySet(Dest, Value, Count)  memset(Dest, Value, Count)
#define MemoryZero(Value) MemorySet((Value), 0, sizeof((*Value)));
#define MemoryCopyArray(Dest, Source) MemoryCopy((Dest), (Source), sizeof((Dest)))
#define MemoryCopyStruct(Dest, Source) MemoryCopy((Dest), (Source), sizeof(*(Dest)))

//~ Attributes
#define internal static
#define local_persist static
#define global_variable static

#if COMPILER_MSVC
# define thread_static __declspec(thread)
#else
# define thread_static __thread
#endif

#if COMPILER_MSVC || (COMPILER_CLANG && OS_WINDOWS)
# pragma section(".rdata$", read)
# define read_only __declspec(allocate(".rdata$"))
#elif (COMPILER_CLANG && OS_LINUX)
# define read_only __attribute__((section(".rodata")))
#else
// NOTE(rjf): I don't know of a useful way to do this in GCC land.
// __attribute__((section(".rodata"))) looked promising, but it introduces a
// strange warning about malformed section attributes, and it doesn't look
// like writing to that section reliably produces access violations, strangely
// enough. (It does on Clang)
# define read_only
#endif

#if COMPILER_MSVC
# define PrintfFunc(FormatIdx, FirstArgIdx)
#elif COMPILER_CLANG || COMPILER_GNU
# define PrintfFunc(FormatIdx, FirstArgIdx) __attribute__((format(printf, FormatIdx, FirstArgIdx)))
#endif

#if COMPILER_MSVC
# define force_inline __forceinline
#elif COMPILER_CLANG || COMPILER_GNU
# define force_inline __attribute__((always_inline)
#else
# error force_inline not defined for this compiler.
#endif

#if COMPILER_MSVC
# define no_inline __declspec(noinline)
#elif COMPILER_CLANG || COMPILER_GNU
# define no_inline __attribute__((noinline))
#else
# error no_inline not defined for this compiler.
#endif

#if LANG_CPP
# define C_LINKAGE extern "C"
# define C_LINKAGE_BEGIN C_LINKAGE {
# define C_LINKAGE_END }
#else
# define C_LINKAGE
# define C_LINKAGE_BEGIN
# define C_LINKAGE_END
#endif

#define NO_STRUCT_PADDING_BEGIN _Pragma("pack(push, 1)")
#define NO_STRUCT_PADDING_END _Pragma("pack(pop)")


#if COMPILER_MSVC && !BUILD_DEBUG
# define NO_OPTIMIZE_BEGIN _Pragma("optimize(\"\", off)")
# define NO_OPTIMIZE_END _Pragma("optimize(\"\", on)")
#elif COMPILER_CLANG && !BUILD_DEBUG
# define NO_OPTIMIZE_BEGIN _Pragma("clang optimize off")
# define NO_OPTIMIZE_END _Pragma("clang optimize on")
#elif COMPILER_GNU && !BUILD_DEBUG
# define NO_OPTIMIZE_BEGIN _Pragma("GCC push_options") _Pragma("GCC optimize(\"O0\")")
# define NO_OPTIMIZE_END _Pragma("GCC pop_options")
#else
# define NO_OPTIMIZE_BEGIN
# define NO_OPTIMIZE_END
#endif

#if COMPILER_MSVC
# if defined(__SANITIZE_ADDRESS__)
#  define ASAN_ENABLED 1
#  define NO_ASAN __declspec(no_sanitize_address)
# else
#  define NO_ASAN
# endif
#elif COMPILER_CLANG
# if defined(__has_feature)
#  if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#   define ASAN_ENABLED 1
#  endif
# endif
# define NO_ASAN __attribute__((no_sanitize("address")))
#else
# define NO_ASAN
#endif

#if ASAN_ENABLED
C_LINKAGE void __asan_poison_memory_region(void const volatile *Address, size_t Size);
C_LINKAGE void __asan_unpoison_memory_region(void const volatile *Address, size_t Size);
# define AsanPoisonMemoryRegion(Address, Size) \
__asan_poison_memory_region((Address), (Size))
# define AsanUnpoisonMemoryRegion(Address,  Size) \
__asan_unpoison_memory_region((Address), (Size))
#else
# define AsanPoisonMemoryRegion(Address, Size) ((void)(Address), (void)(Size))
# define AsanUnpoisonMemoryRegion(Address, Size) ((void)(Address), (void)(Size))
#endif

// Alignment
#if COMPILER_MSVC
# define AlignOf(T) __alignof(T)
#elif COMPILER_CLANG
# define AlignOf(T) __alignof(T)
#elif COMPILER_GNU
# define AlignOf(T) __alignof__(T)
#else
# error AlignOf not defined for this compiler.
#endif
#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

// Push/Pop warnings
#if COMPILER_GNU
# define NO_WARNINGS_BEGIN \
_Pragma("GCC diagnostic push") \
_Pragma("GCC diagnostic ignored \"-Wall\"") \
_Pragma("GCC diagnostic ignored \"-Wextra\"") \
_Pragma("GCC diagnostic ignored \"-Wconversion\"") \
_Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"") \
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
_Pragma("GCC diagnostic ignored \"-Wsign-compare\"") \
_Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"") \
_Pragma("GCC diagnostic ignored \"-Wimplicit-fallthrough\"")
# define NO_WARNINGS_END _Pragma("GCC diagnostic pop")
#elif COMPILER_CLANG
# define NO_WARNINGS_BEGIN \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Weverything\"")
# define NO_WARNINGS_END _Pragma("clang diagnostic pop")
#elif COMPILER_MSVC
# define NO_WARNINGS_BEGIN \
__pragma(warning(push)) \
__pragma(warning(disable: 4267 4996)) // NOTE: Add specific warning numbers to disable as needed
# define NO_WARNINGS_END __pragma(warning(pop))
#else
# error "No compatible compiler found"
#endif


#define Pi32 3.14159265359f

#define KB(n)  (((u64 )(n)) << 10)
#define MB(n)  (((u64 )(n)) << 20)
#define GB(n)  (((u64 )(n)) << 30)
#define TB(n)  (((u64 )(n)) << 40)
#define Thousand(n)   ((n)*1000)
#define Million(n)    ((n)*1000000)
#define Billion(n)    ((n)*1000000000)

//~ Types
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t umm ;
typedef ssize_t smm;
typedef s32 rune; // utf8 codepoint

typedef float f32;
typedef double f64;
typedef unsigned int uint;

#define U8Max 0xff
#define U16Max 0xffff
#define S32Min ((s32)0x80000000)
#define S32Max ((s32)0x7fffffff)
#define U32Min 0
#define U32Max ((u32)-1)
#define U64Max ((u64)-1)

#define false 0
#define true  1

//~ Globals
global_variable b32 GlobalDebuggerIsAttached;

#endif // BASE_CORE_H
