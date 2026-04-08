#if OS_LINUX || OS_ANDROID
# include "base_os_linux.c"
#elif OS_WINDOWS
# include "base_os_windows.c"
#else 
# error "Operating system not provided or supported."
#endif

internal inline f64
OS_SecondsElapsed(f64 Start, f64 End)
{
    f64 Result = (End - Start);
    return Result;
}

internal inline f64
OS_MSElapsed(f64 Start, f64 End)
{
    f64 Result = ((End - Start)*1000.0);
    return Result;
}

internal void
OS_ProfileInit(char *Prefix)
{
    GlobalProfiler.Start = OS_GetWallClock();
    GlobalProfiler.End = GlobalProfiler.Start;
    GlobalProfilerPrefix = Prefix;
}

internal void
OS_ProfileAndPrint(char *Label)
{
    if(GlobalIsProfiling)
    {        
        GlobalProfiler.End = OS_GetWallClock();
        Log(" %s_%s: %.4f\n", GlobalProfilerPrefix, Label, OS_MSElapsed(GlobalProfiler.Start, GlobalProfiler.End));
        GlobalProfiler.Start = GlobalProfiler.End;
    }
}
