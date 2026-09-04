# 1. [Table of Contents](https://www.computerenhance.com/p/table-of-contents)

# 2. [Performance-Aware Programming Series Begins February 1st](https://www.computerenhance.com/p/performance-aware-programming-series)
# 3. [Welcome to the Performance-Aware Programming Series!](https://www.computerenhance.com/p/welcome-to-the-performance-aware)
## Optimization
optimization = Maximizing performance of software on hardware.
- used to be complicated (years ago) = traditional

## Problem
People think performance is:
- not worth it
- too hard

True for traditional optimzation.
- actually results in extremely slow programming.

## What
Only learning about how performance works is enough.
- What decisions affect performance?

## Affecting performance
1. amount of instructions: reduce number of instructions
2. speed of instructions: make CPU process instructions faster

## Decline
- CPUs became more complicated.
- unaware of the resulting instructions of higher-level languages

## Solution
- Keep result of instructions in mind, not code
- Learn what the maximum speed of something should be

# 4. [Waste](https://www.computerenhance.com/p/waste)
Instructions that do not need to be there.
- Often the biggest multiplier
```asm
LEA C, [A+B]
```
- Load Effective Adress
- separated from the destination

You can find waste by looking at the output assembly code.

Eliminating waste is a form of *reducing instructions*.

In the case of python the interpreter code is the waste.
- do not use python for bulk operations (i.e. loops)

Key points:
- recognize waste
- in python: find ways to offload to code with less waste (e.g. C,
                                                           numpy, ...)
- to measure overhead + loop we can measure cycles
- more instructions != more time

Python had 180x instructions and was 130x slower.

# 5. [Instructions Per Clock](https://www.computerenhance.com/p/instructions-per-clock)
*speed of instructions*

## IPC/ILP
- **Instructions Per Clock**
- instructions per clock cycle
- specific to one instruction
- **Instruction Level Parallelism**
- accounting all instructions
```c
for (i = 0; i < count; i +=1)
{
    sum += input[i];
}
```
- more instructions than only "adds"
- no way to get to 1x add per cycle
- loop overhead

Reducing ratio of loop overhead / work
- example: loop unrolling
```c
for (i = 0; i < count; i +=2)
{
    sum += input[i];
    sum += input[i + 1];
}
```
Weird that it would go until to 1x add per cycle.
- what are the chances? overhead??


Multiple instructions can be executed at the same time.
- CPU recognizes their *independency*, (e.g. different locations)
- = parallelism
- "If the destination is the same as the input it cannot be executed at
the same time."
- = serial dependency chain

Multiple chains can help break through limits.
- "boosting the IPL"

CPUs are designed for more computation so boosting IPL in a loop that
does not do a lot of computation will bring less benefits.

# 6. [Monday Q&A (2023-02-05)](https://www.computerenhance.com/p/monday-q-and-a-2023-02-05)
## JIT
- compile code "upfront"
- have extra information
- knows context in which it is compiled
- can branch off functions
- Javascript uses it by default

Waste:
- Instructions that are not required

JAVA may have waste as well, but the bytecode was designed as
"something that could be executed faster".

## No test harness yet.
Read Time Stamp Counter (rtsc):
- allows to read chip-wide counter
- dangerous against turbo boosts


We have to move by stage so we can focus on everything step by step.

The reality is that there already is a well grown userbase.
- who cares!  We will make it performant.

## Micro-OPs go to execution ports
- each port can do X operations
- different instructions can be contenders for a port
- looking at the port usage will show for unrolling a loop
- there are tools that can simulate this
- sometimes there is a limit on how instructions can be sent


## Unrolling
- most compilers can unroll the loop for you
- clang can screw this up


## Why *minimum* adds per cycle
- thinking about "opportunities"
- mean & median will not converge to the actual cycle count the CPU can do them.
- *fastest* will get to the actual adds/cycle the CPU can do
- median & mean gives you typical behavior

Assembly -> Micro-OPs -> CPU

```c
input[index + 0]
input[index + 1]
input[index + 2]
input[index + 3]
```
Is slower than
```c
input[0]
input[1]
input[2]
input[3]
input += 4
```
## Three-based addition:
- common technique to work out a dependency chain

# 7. [Single Instruction, Multiple Data](https://www.computerenhance.com/p/single-instruction-multiple-data)
*Amount of instructions*

## SIMD
- *Single Instruction, Multiple Data*
- One instruction can act on multiple data
- SSE in x64
- can be used together with IPC

## PADDD
- Packed ADD D-word
- "Wide" instruction
- can use multiple accumulators
- Saves work
- e.g. extracting dependency chains
- ![vector example](img/vector_paddd.png)

## Subsets
|---------+-----------+-----------|
| subset  | bit width | supported |
|---------+-----------+-----------|
| SSE     |        4x | Common    |
| AVX     |        8x | Common    |
| AVX-512 |       16x | Uncommon  |
|---------+-----------+-----------|
#32 bit integer/float
- 4*32 = 128bits
- Making smaller instructions can use bit width.
- Typical that you cannot get full x improvements (x2, x4, ...)

## Difficulty
- SIMD does not care about how data is organized
- easy with adds

# 8. [Caching](https://www.computerenhance.com/p/caching)
*speed of instructions*

Load/Store:
- how CPU gets (load) or puts (store) data from memory

Every add is dependent on the load
- it needs data from previous load

Because there are many dependencies on loads it is very important.
- *Cache*!
- Way faster than main memory (DIMMs)

## Cache
- Register file :
- produce values really quickly and feed them to registers
- maximum speed
- few hundred values at most

# 9. [Monday Q&A #2 (2023-02-12)](https://www.computerenhance.com/p/monday-q-and-a-2-2023-02-12)
## Why would the register renamer not solve the dependencies?
- Because there is a "literal" dependency
- *register renamer* fixes "fake" dependencies

## Python over cpp
- cpp is quite bad, but allows control over the output
- python is good for sketching and libraries

## Hardware jungle
- Coding towards the minimum specification
- generally true
- Design hotspots for platforms (e.g. Xbox, PS5, ...)
- vectorizable, enough in loop for IPC
- focus on things that work on all CPUs

## More complicated loops
- For now it's demonstrations
- everything can be optimized (: )

## How can you tell if you are wasteful?
- profiling
- "how much time spending in this loop?"

## Lanes and bottlenecks during design
- how many of "those" can I do per second
- which is the limiting factor = bottleneck

## Asymptotic performance
- also important

## Power usage reduction
- in general achieved through reduced instructions
- same thing the *majority* of the time
- reducing waste

## Signed and unsigned integers
- are the same because of *two's complement*
- except for:
- mul/div/gt
- saturated add :: stops at lowest/highest value
- name of instructions tells the compiler which instruction to use
- unsigned/signed is not needed and could be replaced by a different operator

## Can compilers SIMD?
- gcc and clang are agressive at vectorization
- generally better than nothing

## SIMD: AMD vs Intel
- no. (long-term)

## Are unused registers ignored?
- modern chips (CPU/GPU) have two types of registers:
- Scalar
- slot
- more than Vector ones
- SIMD ("Vector")
- 8/16/32/64/128/256 of the 256bits
- Using larger bits is more expensive
- VZeroUpper after using different sizes
- Special considerations per register
- tip: *SIMD if you can*


## CPU vs GPU
- GPUs are the same for parallellism but with different trade-offs
- 1024bit Vectors / Wide execution units
- CPU
- high clock
- high IPC/IPL
- GPU
- more ALUs (on the chip)
- more queues (pipelining)
- more hyperthreads
- CPU were designed for single core execution
- GPU does not look ahead, but is told
- *Both* are SIMD CPUs
- *benefits:*
- massive parallellization
- lots of math ops
- Switch can be difficult (talkin between both)
- unless APU

## Non-deterministic architectures
- You cannot depend on timings
- Potentially depends on the room's temperature
- Things run until they cannot melt

## Arm
- everything transfers
- Instructions name change

## SIMD without SIMD
- leave one bit for overflow
- SIMD registers handle overflows and carrying

## Slowdown when 256/512 registers
- Most machines downclocks when using AVX

## Hardware Jungle: SIMD edition
- 'cpu_id' tells what instructions sets the CPU supports
- set function pointers accordingly
- SHIM? 


## Micro-OP debugging
- assembly instructions are a layer of debugging on top of micro-OPs
- You cannot micro-op debug 

## Out-of-order CPUs
- only if no one can tell the order was violated
- inside a window
- limited:
- how many things at once
- but, retiring finished instructions happen in order
- rolling window
- waiting for instructions to be done

## SIMD dataset requirements
- "Shot"
- "Tail"
- You can padd you inputs
- Mass loads/stores (on modern instruction sets)
- will write bit per lane
- along with masks you can choose lanes
- Vector/Packed instructions set
- Packed: operate on x elements (with a mask)
- Vector: VLen (Vector Length) says how much elements
- Scaler loop in worst case scenario

## Cost of SIMD
- 128 can always be used
- clock penalty goes away over time
- more latent

## Latency vs Pipelining
- Latency can be beaten by pipelining
- if the instructions are independent then the latency does not matter

## Instructions Limits
- there is a limit on the number of micro-ops a cycle
- 5 micro-ops cannot be adds
- load tides up the execution port
- Registers are next to the lanes

## Cache control
- responsive, guesses
- "hints"
- prefetch instruction :: tries to get the memory in cache
- what level (not always followed, hint!)
- when look ahead is not going to see the loads
- streaming instruction :: forbid to load the data in the cache
- opposite of prefetch
- streaming store :: a store that is not going to be read in the future
- only matters when you have data you *do* want to cache


## Data going around cache
- bandwith is way bigger than the amount that needs to be passed through
- bandwith becomes narrower
- [cache_wideness][img/cache_wideness.png]
- the place having the data decides the bandwith

## Prefetches
- hardware and software
- hardware ::
- looks at the pattern of pages
- linearly (ascending/descending/skipping/...) 
- eliminates latency
- throughput stays the same (cache bandwith)

## Other programs
- processor does not care about *what* it caches
- you lose depending on the core
- programms waking up takes up cache
- speed decrease is not in ^2 but size of bytes per cycle

## Cache runtime
- The slower the routine, the less the cache is important

## Cache lines
- every 64 bytes
- being memory aligned penalties :: (not very high)
- 4096B (page boundaries)
- can pollute cache
- waste of cache space
- [[file:./img/cache_lines.png][cache_lines]]
- best to use all the cache lines (all 64 bytes)
- via data structures

## Checking the cache
- checking and getting happens in one instruction

## Cache fights
- in multithreaded beware of shared caching
- pulling in the cache can evict the current data

## L1 cache supremacy
- constrained by distance

## Instructions in cache
- Front end ::
- ICache
- Back end ::
- DCache
- Separate for the L1
- Unpredicted instructions can slow down the program
- In L2/L3 cache instructions take space.

## Performance for different sizes
- smaller sizes are more likely to be cached
- if the cache is *primed* then yes

## Cache behaviour
- branch predictor :: which way
- sophisticated
- hardware prefetcher :: which memory is going to be touched
- recognizes patterns and puts *next* memory in cache
- not smart
- "warmed up"

## Persistent cache
- OS wipes out with new memory map

## Ask for cache
- Evict train
- evict, ask L2, evict, ask L3, evict ask memory, fill evicts
- DMA (Direct Memory Acces)

## Inclusive vs Exclusive Caches
- exclusive cache ::
- data is not in L2
- only when evicted from L1
- inclusive cache ::
- L1 and L2 are filled with the data
- per chip

# 10. [Multithreading](https://www.computerenhance.com/p/multithreading)
*Increasing speed of instructions*

## Multithreading
- Core :: different computers
- physical
- Threads :: interface to access cores through the OS
- OS

## Speeding up
- not x2 x4, actually account cache for speed up
- more SIMD registers, instructions cache, ...
- shared caches add up
- memory bandwidth can be bottleneck
- sometimes does not add up

## Forcing out of memory
- bandwith does not increase a lot when using main memory
- depending on the chip
- L3 cache and main memory are shared (not big speed ups)

# 11. [Python Revisited](https://www.computerenhance.com/p/python-revisited)
Assembly is what determines the speed.

## Python
- doing every sum in python is slow
- numpy is faster when you have supplied the array with a type
# 12. [Monday Q&A #3 (2023-02-20)](https://www.computerenhance.com/p/monday-q-and-a-3-2023-02-20)
## Hyperthreading & Branch prediction
- hyperthreads ::
- [[./img/hyperthreading.png][hyperthreading]]
- polling for more than one instructions
- very important in GPUs
- fill the execution ports with multiple instruction streams
- both go to the front end
- branch prediction ::
- [[file:./img/branch_prediction.png][branch_prediction]]
- uops arrive faster than they are executed
- they can be processed
- 1. stall on jumps
- flush uops (10-14 cycles)
- bad for out-of-order/IPL
- 2. guess
- wrong = stall
- front end feeds instructions into micro-ops to the back end
- IPC: more execution ports filled


## Multithreaded
- code so that threads do not talk to each other
- communication is a mistake
- sync the code


## Max multiplier mulitthreading
- fetching memory is slower than computation
- look at all-core-bandwith
- total bandwith to all cores
- divided by cores = max memory per cycle


## Logical processors vs Cores
- Cores = computers
- Logical processors
- OS / threads / instruction streams


## thread count > L1 cache
- oversubscription ::
- when the program asks for more threads than available
- lot of eviction
- OS overhead
- bad *always*
- unless waiting thread
- OS tries to run thread as long as possible


## Green thread / fibers
- software control swapping of the OS


## Multithreadeding with disks
- micromanagement
- when CPU has to decrypt
- depends on how disk works
- autonomous/not
- threads can make non blocking code


## How to get memory bandwidth
- https://github.com/cmuratori/blandwidth

# 13. [The Haversine Distance Problem](https://www.computerenhance.com/p/the-haversine-distance-problem)
- Computing arc length between two coordinates.
- You want to do the math first.
- CPU is made for it
- Second is the *Input*
- Reading the data can take a long time.

# 14. ["Clean" Code, Horrible Performance](https://www.computerenhance.com/p/clean-code-horrible-performance)

# 15. [Instruction Decoding on the 8086](https://www.computerenhance.com/p/instruction-decoding-on-the-8086)
The 8086 instruction set architecture is easier.
- Better for understanding concepts.

## Register
- place to store information
- 16 bits on the 8086

## Operations
1. load memory
- copy into register
2. compute
3. write to memory

## Instruction Decode
Turning the instruction stream into hardware operations.

## Instructions
- mov ::
- move, but actually a /copy/
- are assembled into binary that the /Instruction Decoder/ can use to
execute the instruction
- stored in 2x 8bits
- [[./img/instruction_encoding.png][image]]
- instruction (6) :: code for the instruction
- flags
- D (1) :: whether REG is source or destination
- W (1) :: 16bits or not
- second byte:
- MOD (2) :: memory or register operation
- REG (3) :: encodes register
- R/M (3) :: register/memory operation
- operand
- AX/AL/AH ::
- X: wide
- L: low bits
- H: high bits

Binary Instruction stream
- only register to register moves

Exercise:
- read binary in
- bit manipulation to extract the bits
- *assemble the listings
- load 2 bytes and disassemble that instruction
- outputs the instructions

# 16. [Decoding Multiple Instructions and Suffixes](https://www.computerenhance.com/p/decoding-multiple-instructions-and)
1st byte tells if there's a second, 2nd if there's a 3rd, ...
-> makes decoding dependent process, A cost on the CPU

The D bit is the difference between a store and a load

- Effective address calculation :: Adress that needs to be computed
before it can be resolved, e.g. [BP + 75] (this is also a displacement)

MOD field 

- displacement ::
- [ ... + n] where n is a n-bit number either 0, 8 or 16 bits
- defined by the MOD field
- direct address still has a displacement (MOD = 00)
- BP 110, has this 16bits
- [displacement][img/displacement.png]

- Some registers can be adressed as their low or high bits (L/H)
- [[file:./img/l_h_registers.png][l_h_registers]]

The R/M field encodes what type of displacement. (BP, BX, SI, DI)

There are two sets of registers, ones where you can address or low and high parts freely
like (AH, AL, AX: A to D).
And SP, BP, SI, DI.
*Some registers are not created equal.*

Special case when MOD is 00 and R/M is 110 -> 16 bit displacement.

Immediately (available) value.

## Assignment
Also implement memory to register.
- going to have to read the displacement bits (DISP)

When reassembling signed/unsigned information will be lost.
- easier to test

## Challenge (extra)
- look up how negative displacements work in the manual
- byte/word is a different move
- different instruction for accumulator
- to save a byte
# 17. [Monday Q&A #4 (2023-03-06)](https://www.computerenhance.com/p/monday-q-and-a-4-2023-03-06)

# 18. [Opcode Patterns in 8086 Arithmetic](https://www.computerenhance.com/p/opcode-patterns-in-8086-arithmetic)

# 19. [Monday Q&A #5 (2023-03-13)](https://www.computerenhance.com/p/monday-q-and-a-5-2023-03-13)

# 20. [8086 Decoder Code Review](https://www.computerenhance.com/p/8086-decoder-code-review)
Enum + bits size, eg. (Byte_Lit, 6)
Using a segmented access so access to memory can be controlled.
Printable instructions / Non-printable for things like segment prefixes.  Which also require a context to be passed since you can have any amount of them prefixing an instruction.
MOD R/M field is encoded as effective_address_expression from effective_address_base.
Register access with offset & count for having for accessing low/high 8 bits or 16 bits.
Instruction operand as an union and type.
1MB memory buffer, with assert for out of bounds memory access.
Separate text code for printing out instructions into text assembly.
Compiler warns if not all enums are handled in a switch statement.
Appending superfluous "word" or "byte" simplifies the logic.
Implicit field for shortcut instructions like "mov to accumulator".
A shift value can be used to shift instructions around in case of instructions spreading multiple bytes like the escape instruction.
The table's bytes are big endian, but the bytes are little endian.
Last operand is the operand that was not used. 
# 21. [Monday Q&A #6 (2023-03-20)](https://www.computerenhance.com/p/monday-q-and-a-6-2023-03-20)
Bytecode is packed bytes that tells a CPU what to do.
# 22. [Using the Reference Decoder as a Shared Library](https://www.computerenhance.com/p/using-the-reference-decoder-as-a)
# 23. [Simulating Non-memory MOVs](https://www.computerenhance.com/p/simulating-non-memory-movs)
CPU only understand memory in register and simple operations on these bytes.1
# 24. [Homework Poll!](https://www.computerenhance.com/p/homework-poll)
# 25. [New Schedule Experiment](https://www.computerenhance.com/p/new-schedule-experiment)
# 26. [Simulating ADD, SUB, and CMP](https://www.computerenhance.com/p/simulating-add-jmp-and-cmp)
Signed flag is set when the highest bit (sign bit) is set.
`cmp` instructions are ways to turn arithmetic instructions in comparison.
Arithmetics setting the flags is so that you could save on cmp instructions.


# 27. [Simulating Conditional Jumps](https://www.computerenhance.com/p/simulating-conditional-jumps)
IP register holds where the next instruction is before it gets executed.
# 28. [Response to a Reporter Regarding "Clean Code, Horrible Performance"](https://www.computerenhance.com/p/response-to-a-reporter-regarding)
# 29. [Monday Q&A #7 (2023-04-10)](https://www.computerenhance.com/p/monday-q-and-a-7-2023-04-10)
Hacker's delight's `popcnt` can easily find the parity.
There are instructions that push and pop the flags register.
Registers don't have garbage value they can even have input values from the OS (like adresses you need).
AF is often used for math on ASCII or BCD, symbols represented as binary.
Order of bits in a register is not how they are stored physically.

Sign extension is creating a 16bit value by replicating the high bit in the high bit.

# 30. [Simulating Memory](https://www.computerenhance.com/p/simulating-memory)
Segment registers are used to access megabytes of memory.
```asm
mov xx, ds.[bp]
```
They specify which 64k(2^16) segment you want to address.  The can overlap.  Offsets stay at 16 bits.
Since they are shifted, you create 4bits boundaries.

# 31. [Simulating Real Programs](https://www.computerenhance.com/p/simulating-real-programs)

# 32. [Monday Q&A #8 (2023-04-17)](https://www.computerenhance.com/p/monday-q-and-a-8-2023-04-17)
Overwriting insrtuctions is harder on x64 because of the agressive caching.
The Frontend gathers instructions and the Backend executes them.
Intel chips are made for backwards compatibility in software making old software faster.  You cannot explicitly manage it, except on specific architectures (eg. ps3 cell processor).  There are special instructions which affect the cache.
If your code relies on a byte location in a register it can be affected by endianness.
You can have a magic number in your file format to detect endianness.
Little Endian is the default nowadays.
A turing machine is a way to prove that you can do all computations you need.  Useful for not leaving algorithmic "gaps".
Not knowing the memory layout introduces a lot of complexity.  Because there are less moving parts.

# 33. [Other Common Instructions](https://www.computerenhance.com/p/other-common-instructions)
There are arithmetic variants of instructions preserving the signed bit.
Variations on arithmetic ops without writing back the result.
Some ops are compacted instructions.

# 34. [The Stack](https://www.computerenhance.com/p/the-stack)
The `call` instruction affects the IP register and puts it on the stack.
Calling conventions and ABIs are rules so code can operate with other code when it's ambiguous (eg. function parameters).

# 35. [Monday Q&A #9 (2023-04-24)](https://www.computerenhance.com/p/monday-q-and-a-9-2023-04-24)
You can spot memory access by brackets '[]'.
Either a load/store or LEA  or prefetch.  LEA does a load with a temp registers.
You can use the stack as regular memory by saving `sp` (stack pointer) to a register and offsetting from it.
A stack frame is the part of the stack created by a function.

# 36. [Performance Excuses Debunked](https://www.computerenhance.com/p/performance-excuses-debunked)
# 37. [Estimating Cycles](https://www.computerenhance.com/p/estimating-cycles)
By using estimation you can know what your performance *should* be.
clocks=cycles

# 38. [Monday Q&A #10 (2023-05-08)](https://www.computerenhance.com/p/monday-q-and-a-10-2023-05-08)
With SIMD using smaller bit-widths will be faster.
For better cycle estimations it's better to try and simulate the microcode which has been reverse engineered from die shots.
2 transfers can mean read + write, eg. `add [bx], 20`.
There is microcode for loads and stores but some lines get processed and "skipped" which can account for a cycle.

# 39. [From 8086 to x64](https://www.computerenhance.com/p/from-8086-to-x64)
E prefix "widens" the register to 32 bits for backwards compatibility.  Analogously, the R prefix "widens" the register to 64 bits.
R8-15 or the new 64 bits registers.  Suffixes: B-8, W-16, D-32, byte, word, double word, quad word
You can use any registers for the 2 terms of effective addressing.  One of the terms can be a scalar for multiplying.  PTR is optional for specifying it's memory.

# 40. [8086 Internals Poll](https://www.computerenhance.com/p/8086-internals-poll)

# 41. [How to Play Trinity](https://www.computerenhance.com/p/how-to-play-trinity)

# 42. [Monday Q&A #11 (2023-05-15)](https://www.computerenhance.com/p/monday-q-and-a-11-2023-05-15)
`mov edi, edi` zeroes the upper bits of a register.  Since the ABI specifies edi as a parameter it needs to be 0.
Redundant register moves do not impact the backend performance (where dependency chains get resolved).
There are instructions that are not useful anymore.
Some instructions cannot be accessed.
Theres is `sgx` extension that allows to do encrypted memory, transactional memory system.
SIMD registers can be split in lanes.  But in "normal" registers this is not supported anymore.
A segfault is an interrupt from the interrupt table. Eg. paging in unmapped memory

# 43. [8086 Simulation Code Review](https://www.computerenhance.com/p/8086-simulation-code-review)

# 44. [Part One Q&A and Homework Showcase](https://www.computerenhance.com/p/part-one-q-and-a-and-homework-showcase)

# 45. [The First Magic Door](https://www.computerenhance.com/p/the-first-magic-door)

# 46. [Monday Q&A #12 (2023-05-22)](https://www.computerenhance.com/p/monday-q-and-a-12-2023-05-22)
TODO: more information about 8086 misc.

# 47. [Generating Haversine Input JSON](https://www.computerenhance.com/p/generating-haversine-input-json)
Random points on a sphere will be more present at the extremities.  A lot of random makes it non-random.
A better distribution will give a better average (e.g., clusters).

# 48. [Monday Q&A #13 (2023-05-29)](https://www.computerenhance.com/p/monday-q-and-a-13-2023-05-29)
Hashes are great for precise data, but floating point is imprecise so it would be hard.
f16 are used to save bandwidth everywhere but also because of a lot of other things.

# 49. [Writing a Simple Haversine Distance Processor](https://www.computerenhance.com/p/writing-a-simple-haversine-distance)
No libraries json processor.

# 50. [Monday Q&A #14 (2023-06-05)](https://www.computerenhance.com/p/monday-q-and-a-14-2023-06-05)
Reading a file completely into memory is not the most efficient.
You don't need to be completely bit-accurate for the exercise.
To use a text format for floating point you need very precise serialize-deserialize code (e.g., serialize-deserialize pairs).
For precision you should not need precision.
You can serialize into hex digits to have precise binary numbers.

# 51. [Initial Haversine Processor Code Review](https://www.computerenhance.com/p/initial-haversine-processor-code)
Generator
- JSF RNG
x You know pair count beforehand so you can use a coefficient
x Set uniform as default method
x MaxPairCount to avoid generating infinite pairs
x MaxAllowed(X/Y) values (random degree function)
x Collapsed both uniform and cluster codepaths by creating one cluster when uniform is selected
x No clusters array, just regenerate the 4 values for each cluster

Processor
x simple loop that mimics generator and returns the sum
x haversine_pair type
x minimum amount of json per pair -> you can get the max number of pairs from json file size

Cleanup times can be adressed.
Clearing data that holds pointers can catch use-after-free.

# 52. [Monday Q&A #15 (2023-06-12)](https://www.computerenhance.com/p/monday-q-and-a-15-2023-06-12)
Missing fields becoming 0s.

Aligment
- Old processor would require memory alignment for some instructions
- Nowadays unaligned loads have mostly no penalties
- The unit of operation for CPUs is a cache line i.e., 64 aligned bytes.
- This means if your data is unaligned you might end up using more cache lines than needed
- In a multicore setup threads will communicate via cache lines and their might be contension and threads
might be writing to the same cache lines.

cpuid will put information into registers so you can query the capabilities

# 53. [Introduction to RDTSC](https://www.computerenhance.com/p/introduction-to-rdtsc)
Timestamps are often aggregated (e.g., os program startup time).
Pre-Pentium processor would not track timer information.

Timestamp Counter has the count since the processor reset/boot.
You can write to it.

You might have to time rdtsc as well.
By repeating you can reduce overhead.

Cores can be turned off (e.g., power saving) and their frequencies are variable (e.g., turbo).
RDTSC became a wall clock. You need extra work to count cycles.
Other instructions for counting cycles do not exist everywhere or require privileged os access.
Reading the frequency is impossible on AMD.

There's no way to know directly what the timestamp counter frequency is.
You can infer it by dividing an os time period by rdtsc time period.

# 54. [Monday Q&A #16 (2023-06-19)](https://www.computerenhance.com/p/monday-q-and-a-16-2023-06-19)
The frequency of rdtsc is constant for the CPU. = *invariant TSC*
You can query this with `cpuid`.

If your program is running on different CPUs (e.g., vm). 
Offsets have to be correct and seed.

The processor needs to be able to tell that you are doing a lot of work. To get that boost.
RDTSCP respects the OOO (out-of-order) window.

Understand what things do so you can be minimally instrusive.
RDTSC does not work reliably on zen cores, there are other instructions. And is not good for nitty gritty optimizations.

The chance of getting pre-empted is small when taking casual timings. (e.g., single core).  The OS provides more information about this as well.

When cores communicate via the cache line they use protocols, (the original being MESI).
x64 has a very strong memory ordering protocol.
Cores can request exclusive access to cache lines.

False share happens when your memory is unaligned and cores end up modifying the same data without you knowing.

# 55. [How does QueryPerformanceCounter measure time?](https://www.computerenhance.com/p/how-does-queryperformancecounter)
RDTSC is available on mostly (if not all) CPUs.

It is better to use RDTSC yourself, because `QueryPerformanceCounter()` will multiply it by 10MHz. 

Uses a trick in hacker's delight to turn the division into a multiplication. 

# 56. [Monday Q&A #17 (2023-06-26)](https://www.computerenhance.com/p/monday-q-and-a-17-2023-06-26)
In win64 ABI first parameter is in RCX (if it can).

There are instructions for assembly instructions that you cannot do in C. E.g., _mulh(), _mul128()
Microsoft symbol server.

Low resolution timers can still be used if the implications are understood.

# 57. [Instrumentation-Based Profiling](https://www.computerenhance.com/p/instrumentation-based-profiling)
There are two comon ways to automate profiling
1. Block based with begin and end (also found in commercial profilers)
2. Marker based, where deltas are computed implicitly by the order they go in.

- BeginProfile()
- EndAndPrintProfile()
- static_assert(__COUNTER__ < ArrayCount(profiler::Anchors))

# 58. [Monday Q&A #18 (2023-07-03)](https://www.computerenhance.com/p/monday-q-and-a-18-2023-07-03)
Some games have locality (e.g., 1000 players are rarely in the same location)
Relative terms: "Who is doing better?"

Doing manual work + debug utility is fine. -> almost as good.
"When it gets really complicated that is often a sign that it is not supported.  The only code worth coding is simple code.  Just move one."

Reordering does happens mostly with small sets of instructions/effects.

Global namespace is easier to use.  Because you need to be able to deploy markers very easily.
It is going to be compiled out anyway.

__COUNTER__ is not related to slots.  You can still use another method to do slots.
int __Prof_ID == __COUNTER__;

# 59. [Profiling Nested Blocks](https://www.computerenhance.com/p/profiling-nested-blocks)
A good profiler needs to be easy to deploy, accurate and have low overhead.

- A marker has a hitcount
- Profiler global with start and end (and marker array)

You can track in which block you are so you can subtract that time from the outer block.

# 60. [Monday Q&A #19 (2023-07-10)](https://www.computerenhance.com/p/monday-q-and-a-19-2023-07-10)
# 61. [Profiling Recursive Blocks](https://www.computerenhance.com/p/profiling-recursive-blocks)
# 62. [Monday Q&A #20 (2023-07-17)](https://www.computerenhance.com/p/monday-q-and-a-20-2023-07-17)
# 63. [A First Look at Profiling Overhead](https://www.computerenhance.com/p/a-first-look-at-profiling-overhead)
# 64. [New Q&A Process](https://www.computerenhance.com/p/new-q-and-a-process)
# 65. [A Tale of Two Radio Shacks](https://www.computerenhance.com/p/a-tale-of-two-radio-shacks)
# 66. [Comparing the Overhead of RDTSC and QueryPerformanceCounter](https://www.computerenhance.com/p/comparing-the-overhead-of-rdtsc-and)
# 67. [Monday Q&A #21 (2023-07-31)](https://www.computerenhance.com/p/monday-q-and-a-21-2023-07-31)
# 68. [The Four Programming Questions from My 1994 Microsoft Internship Interview](https://www.computerenhance.com/p/the-four-programming-questions-from)
# 69. [Microsoft Intern Interview Question #1: Rectangle Copy](https://www.computerenhance.com/p/microsoft-intern-interview-question)
# 70. [Microsoft Intern Interview Question #2: String Copy](https://www.computerenhance.com/p/microsoft-intern-interview-question-ab7)
# 71. [Microsoft Intern Interview Question #3: Flood Fill Detection](https://www.computerenhance.com/p/microsoft-intern-interview-question-a3f)
# 72. [Efficient DDA Circle Outlines](https://www.computerenhance.com/p/efficient-dda-circle-outlines)
# 73. [Q&A #22 (2023-08-15)](https://www.computerenhance.com/p/q-and-a-22-2023-08-15)
# 74. [Measuring Data Throughput](https://www.computerenhance.com/p/measuring-data-throughput)
# 75. [Q&A #23 (2023-08-21)](https://www.computerenhance.com/p/q-and-a-23-2023-08-21)
# 76. [Repetition Testing](https://www.computerenhance.com/p/repetition-testing)
# 77. [Q&A #24 (2023-08-28)](https://www.computerenhance.com/p/q-and-a-24-2023-08-28)
# 78. [Monitoring OS Performance Counters](https://www.computerenhance.com/p/monitoring-os-performance-counters)
# 79. [Q&A #25 (2023-09-04)](https://www.computerenhance.com/p/q-and-a-25-2023-09-04)
# 80. [Page Faults](https://www.computerenhance.com/p/page-faults)
# 81. [Q&A #26 (2023-09-11)](https://www.computerenhance.com/p/q-and-a-26-2023-09-11)
# 82. [Probing OS Page Fault Behavior](https://www.computerenhance.com/p/probing-os-page-fault-behavior)
# 83. [Game Development Post-Unity](https://www.computerenhance.com/p/game-development-post-unity)
# 84. [Q&A #27 (2023-09-18)](https://www.computerenhance.com/p/q-and-a-27-2023-09-18)
# 85. [Four-Level Paging](https://www.computerenhance.com/p/four-level-paging)
# 86. [Q&A #28 (2023-09-25)](https://www.computerenhance.com/p/q-and-a-28-2023-09-25)
# 87. [Analyzing Page Fault Anomalies](https://www.computerenhance.com/p/analyzing-page-fault-anomalies)
# 88. [Q&A #29 (2023-10-02)](https://www.computerenhance.com/p/q-and-a-29-2023-10-02)
# 89. [Powerful Page Mapping Techniques](https://www.computerenhance.com/p/powerful-page-mapping-techniques)
# 90. [Q&A #30 (2023-10-09)](https://www.computerenhance.com/p/q-and-a-30-2023-10-09)
# 91. [Faster Reads with Large Page Allocations](https://www.computerenhance.com/p/faster-reads-with-large-page-allocations)
# 92.  [#31 (2023-10-23)](Q&A)
# 93. [ttps://www.computerenhance.com/p/q-and-a-31-2023-10-23]()
# 94. [Memory-Mapped Files](https://www.computerenhance.com/p/memory-mapped-files)
# 95. [Q&A #32 (2023-10-30)](https://www.computerenhance.com/p/q-and-a-32-2023-10-30)
# 96. [Inspecting Loop Assembly](https://www.computerenhance.com/p/inspecting-loop-assembly)
# 97. [Q&A #33 (2023-11-06)](https://www.computerenhance.com/p/q-and-a-33-2023-11-06)
# 98. [Intuiting Latency and Throughput](https://www.computerenhance.com/p/intuiting-latency-and-throughput)
# 99. [Q&A #34 (2023-11-13)](https://www.computerenhance.com/p/q-and-a-34-2023-11-13)
# 100. [Analyzing Dependency Chains](https://www.computerenhance.com/p/analyzing-dependency-chains)
# 101. [Q&A #35 (2023-11-20)](https://www.computerenhance.com/p/q-and-a-35-2023-11-20)
# 102. [Linking Directly to ASM for Experimentation](https://www.computerenhance.com/p/linking-directly-to-asm-for-experimentation)
# 103. [Q&A #36 (2023-11-27)](https://www.computerenhance.com/p/q-and-a-36-2023-11-27)
# 104. [CPU Front End Basics](https://www.computerenhance.com/p/cpu-front-end-basics)
# 105. [A Few Quick Notes](https://www.computerenhance.com/p/a-few-quick-notes)
# 106. [Q&A #37 (2023-12-04)](https://www.computerenhance.com/p/q-and-a-37-2023-12-04)
# 107. [Branch Prediction](https://www.computerenhance.com/p/branch-prediction)
# 108. [Q&A #38 (2023-12-11)](https://www.computerenhance.com/p/q-and-a-38-2023-12-11)
# 109. [Code Alignment](https://www.computerenhance.com/p/code-alignment)
