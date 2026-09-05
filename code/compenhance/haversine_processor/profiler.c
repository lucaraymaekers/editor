#define Prof_TimeScope(Label) 

//- Profiling 
internal void 
Prof_PrintTimeElapsed(char const *Label, u64 TotalTSCElapsed, u64 Begin, u64 End)
{
 u64 Elapsed = End - Begin;
 f64 Percent = 100.0 * ((f64)Elapsed / (f64)TotalTSCElapsed);
 Log("  %s: %lu (%.2f%%)\n", Label, Elapsed, Percent);
}

internal u64 
Prof_GuessCPUFreq(u64 OSElapsed, u64 CPUElapsed, u64 OSFreq)
{
 u64 CPUFreq = 0;
 if(OSElapsed)
	{
		CPUFreq = OSFreq * CPUElapsed / OSElapsed;
	}
	
 return CPUFreq;
}
