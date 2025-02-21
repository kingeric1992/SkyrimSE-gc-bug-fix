# SkyrimSE/AE Garbage Collector Logger & Bug Fix

Ported from [fallout4-gc-bug-fix (created by Nukem9)](https://github.com/Nukem9/fallout4-gc-bug-fix).

An SKSE plugin dll to address a bug in papyrus VM garbage collector that cause game to only cleanup single array item per VM frame, which will eventually overload VM
when allocating too many arrays *and discarding them immediately*.

### Incremental GC pass:
Incremental GC runs once per Papyrus VM frame. ProcessArrayCleanup() individually allotted 1% of the frame time budget. If a VM frame update takes 1 millisecond, they'll each have 10 microseconds to execute. It's *very* easy to allocate an excess of objects per frame to the point that the garbage collector can't keep up.

### Full GC pass (FullGarbageCollectionPass):
Full GC runs on save or load. A "stop the world" GC pass will loop until there's no objects left to destroy. SS2(fo4)'s Long Save Bug happens when this cleanup occurs. The aforementioned loop, combined with the mistakes inside ProcessCleanup, cause time complexity to quickly spiral out of control.

### ProcessArrayCleanup:
Due to convoluted "last index" tracking, a bug was introduced that could cause both functions to prematurely break out of a loop and forgo their time slices after a single object was destroyed.

[Clayne](https://github.com/clayne) has provided an excellent perl script demo of the buggy ProcessCleanup function [here](https://github.com/clayne/random/blob/master/bin/fo4-gc-test).

# Table of contents

* [Requirements](#Requirements)
* [Quick start](#Quick-start)
* [Notes](#Notes)
    * [Papyrus reproduction code](#Papyrus-reproduction-code)
    * [Original function pseudocode with bug included](#Original-function-pseudocode-with-bug-included)
    * [Rewritten function pseudocode with bug corrected](#Rewritten-function-pseudocode-with-bug-corrected)
    * [Full garbage collector pass pseudocode](#Full-garbage-collector-pass-pseudocode)
    * [Log format](#Log-format)
* [License](#License)

# Requirements

* Building
    * Microsoft Visual Studio 2022 or later
    * CMake 3.26 or later
    * vcpkg
* Installation
    * [SKSE](http://skse.silverlock.org/)
    * [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
    * [64-bit Visual C++ 2019/2022 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

# Quick start

1. Clone this repository and any submodules
2. Run `cmake --preset skse-release & cmake --build --preset skse-release --target skse_package`
3. Copy DLL and INI build artifacts to the game folder

# Notes

## Papyrus reproduction code
```papyrus
Function TestKillTheGarbageManWithArrays
    int i = 0
    while (i < 2000)
        ObjectReference[] a = new ObjectReference[20]
        i += 1
    endWhile
EndFunction ; all vars immediately discarded

Function ScriptEntryPoint()
    TestKillTheGarbageManWithArrays()
EndFunction
```

## Original function pseudocode with bug included
```c++
template<typename T>
void ProcessEntries(float TimeBudget, BSTArray<BSTSmartPointer<T>>& Elements, uint32_t& NextIndexToClean)
{
    const uint64_t startTime = BSPrecisionTimer::GetTimer();
    const uint64_t budget = static_cast<uint64_t>(BSPrecisionTimer::FrequencyMS() * TimeBudget);

    // NextIndexToClean stores the last checked entry for iterative GC purposes.
    if (NextIndexToClean >= Elements.size())
        NextIndexToClean = Elements.size() - 1;

    uint32_t index = NextIndexToClean;

    if (!Elements.empty()) {
        do {
            if (Elements[index]->QRefCount() == 1) {
                Elements.RemoveFast(index);

                if (!Elements.empty() && NextIndexToClean >= Elements.size())
                    NextIndexToClean = Elements.size() - 1;

                // Note 1: 'index' isn't incremented when an entry is deleted. There's a chance that 'NextIndexToClean' == 'index' on the first loop.
            } else {
                index++;
            }

            // Note 2: Wrap around to the beginning of the array like a ring buffer.
            if (index >= Elements.size())
                index = 0;

        } while (
            index != NextIndexToClean && // Break when 'index' == 'NextIndexToClean'. Refer to note 1. BUG: This CAN exit after a single object is collected, forgoing the remaining budget/timeslice.
            (TimeBudget <= 0 || (BSPrecisionTimer::GetTimer() - startTime) <= budget) &&
            !Elements.empty());
    }
    NextIndexToClean = index;
}
```

The bug, when triggered, limit the cleanup routine to only release single item per VM frame. Array size quickly piled up.

```Coffee
[01:41:14:762 TID 37600] GC process arrays (0.044000 / 0.048469 ms)... Arr Size: 345 -> 318, Queue: 27, Cleanup: 27
[01:41:14:779 TID 42252] GC process arrays (0.049000 / 0.048465 ms)... Arr Size: 347 -> 318, Queue: 29, Cleanup: 29
[01:41:14:795 TID 34688] GC process arrays (0.046000 / 0.048474 ms)... Arr Size: 347 -> 319, Queue: 28, Cleanup: 28
[01:41:14:812 TID 37600] GC process arrays (0.047000 / 0.048434 ms)... Arr Size: 347 -> 318, Queue: 29, Cleanup: 29
[01:41:14:829 TID  3188] GC process arrays (0.049000 / 0.048469 ms)... Arr Size: 347 -> 325, Queue: 29, Cleanup: 22

// bug is triggered.
[01:41:14:845 TID  3188] GC process arrays (0.002000 / 0.048465 ms)... Arr Size: 353 -> 352, Queue: 35, Cleanup: 1
[01:41:14:862 TID 37600] GC process arrays (0.002000 / 0.048469 ms)... Arr Size: 381 -> 380, Queue: 63, Cleanup: 1
[01:41:14:879 TID 37600] GC process arrays (0.002000 / 0.048434 ms)... Arr Size: 408 -> 407, Queue: 90, Cleanup: 1
[01:41:14:896 TID 42252] GC process arrays (0.002000 / 0.048460 ms)... Arr Size: 436 -> 435, Queue: 118, Cleanup: 1
[01:41:14:912 TID 37600] GC process arrays (0.002000 / 0.048456 ms)... Arr Size: 464 -> 463, Queue: 145, Cleanup: 1
[01:41:14:929 TID 33476] GC process arrays (0.002000 / 0.046858 ms)... Arr Size: 491 -> 490, Queue: 173, Cleanup: 1
[01:41:14:945 TID 37600] GC process arrays (0.002000 / 0.048451 ms)... Arr Size: 519 -> 518, Queue: 201, Cleanup: 1
[01:41:14:962 TID 34688] GC process arrays (0.002000 / 0.048429 ms)... Arr Size: 546 -> 545, Queue: 228, Cleanup: 1
[01:41:14:979 TID 33476] GC process arrays (0.002000 / 0.048460 ms)... Arr Size: 574 -> 573, Queue: 256, Cleanup: 1
[01:41:14:995 TID 42300] GC process arrays (0.002000 / 0.048438 ms)... Arr Size: 601 -> 600, Queue: 283, Cleanup: 1
[01:41:15:012 TID 33476] GC process arrays (0.002000 / 0.047088 ms)... Arr Size: 629 -> 628, Queue: 311, Cleanup: 1
[01:41:15:029 TID 42300] GC process arrays (0.002000 / 0.048442 ms)... Arr Size: 657 -> 656, Queue: 338, Cleanup: 1
[01:41:15:045 TID 37600] GC process arrays (0.002000 / 0.047300 ms)... Arr Size: 684 -> 683, Queue: 366, Cleanup: 1
[01:41:15:062 TID 42252] GC process arrays (0.003000 / 0.046431 ms)... Arr Size: 712 -> 711, Queue: 394, Cleanup: 1
[01:41:15:080 TID 34688] GC process arrays (0.002000 / 0.048460 ms)... Arr Size: 739 -> 738, Queue: 421, Cleanup: 1
[01:41:15:095 TID 42252] GC process arrays (0.002000 / 0.048469 ms)... Arr Size: 767 -> 766, Queue: 449, Cleanup: 1
[01:41:15:112 TID  3188] GC process arrays (0.002000 / 0.048465 ms)... Arr Size: 794 -> 793, Queue: 476, Cleanup: 1
[01:41:15:129 TID 42300] GC process arrays (0.002000 / 0.046877 ms)... Arr Size: 822 -> 821, Queue: 504, Cleanup: 1
[01:41:15:146 TID 37600] GC process arrays (0.003000 / 0.047704 ms)... Arr Size: 850 -> 849, Queue: 531, Cleanup: 1
[01:41:15:162 TID 33476] GC process arrays (0.003000 / 0.046868 ms)... Arr Size: 877 -> 876, Queue: 559, Cleanup: 1
[01:41:15:179 TID 42252] GC process arrays (0.002000 / 0.047196 ms)... Arr Size: 905 -> 904, Queue: 587, Cleanup: 1
[01:41:15:196 TID 42300] GC process arrays (0.002000 / 0.048442 ms)... Arr Size: 932 -> 931, Queue: 614, Cleanup: 1
[01:41:15:212 TID 42300] GC process arrays (0.002000 / 0.048429 ms)... Arr Size: 960 -> 959, Queue: 642, Cleanup: 1
[01:41:15:230 TID 37600] GC process arrays (0.003000 / 0.048438 ms)... Arr Size: 987 -> 986, Queue: 669, Cleanup: 1
```

## Rewritten function pseudocode with bug corrected
```c++
template<typename T>
void ProcessEntries(float TimeBudget, BSTArray<BSTSmartPointer<T>>& Elements, uint32_t& NextIndexToClean)
{
    const uint64_t startTime = BSPrecisionTimer::GetTimer();
    const uint64_t budget = static_cast<uint64_t>(BSPrecisionTimer::FrequencyMS() * TimeBudget);

    if (NextIndexToClean >= Elements.size())
        NextIndexToClean = Elements.size() - 1;

    uint32_t index = NextIndexToClean;

    if (!Elements.empty()) {
        do {
            if (Elements[index]->QRefCount() == 1) {
                Elements.RemoveFast(index);

                if (!Elements.empty() && NextIndexToClean >= Elements.size())
                    NextIndexToClean = Elements.size() - 1;
            }
            index++;

            if (index >= Elements.size())
                index = 0;

        } while (
            index != NextIndexToClean &&
            (TimeBudget <= 0 || (BSPrecisionTimer::GetTimer() - startTime) <= budget) &&
            !Elements.empty());
    }
    NextIndexToClean = index;
}
```

After the patch, the time spent on GC is more align with given time budget, and it allows VM to handle burst of temp array more easily with more items released per frame.
```Coffee
[01:48:31:788 TID 34872] GC process arrays (0.025000 / 0.048465 ms)... Arr Size: 345 -> 331, Queue: 27, Cleanup: 14
[01:48:31:805 TID  1444] GC process arrays (0.038000 / 0.048447 ms)... Arr Size: 360 -> 338, Queue: 42, Cleanup: 22
[01:48:31:822 TID 33116] GC process arrays (0.039000 / 0.048447 ms)... Arr Size: 367 -> 342, Queue: 48, Cleanup: 25
[01:48:31:838 TID  1444] GC process arrays (0.043000 / 0.048469 ms)... Arr Size: 370 -> 343, Queue: 52, Cleanup: 27
[01:48:31:855 TID 23832] GC process arrays (0.041000 / 0.048451 ms)... Arr Size: 372 -> 344, Queue: 54, Cleanup: 28
[01:48:31:871 TID  1444] GC process arrays (0.045000 / 0.048447 ms)... Arr Size: 372 -> 344, Queue: 54, Cleanup: 28
[01:48:31:888 TID 38452] GC process arrays (0.043000 / 0.048438 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:31:905 TID 34872] GC process arrays (0.079000 / 0.048429 ms)... Arr Size: 373 -> 348, Queue: 55, Cleanup: 25
[01:48:31:921 TID  1444] GC process arrays (0.048000 / 0.048465 ms)... Arr Size: 377 -> 346, Queue: 59, Cleanup: 31
[01:48:31:938 TID 38452] GC process arrays (0.049000 / 0.047794 ms)... Arr Size: 375 -> 344, Queue: 56, Cleanup: 31
[01:48:31:955 TID 23832] GC process arrays (0.049000 / 0.047893 ms)... Arr Size: 372 -> 341, Queue: 54, Cleanup: 31
[01:48:31:971 TID 31688] GC process arrays (0.046000 / 0.048447 ms)... Arr Size: 370 -> 340, Queue: 52, Cleanup: 30
[01:48:31:988 TID 23832] GC process arrays (0.046000 / 0.048456 ms)... Arr Size: 368 -> 339, Queue: 50, Cleanup: 29
[01:48:32:004 TID  1444] GC process arrays (0.046000 / 0.048424 ms)... Arr Size: 368 -> 339, Queue: 50, Cleanup: 29
[01:48:32:021 TID 38452] GC process arrays (0.044000 / 0.048469 ms)... Arr Size: 367 -> 338, Queue: 49, Cleanup: 29
[01:48:32:038 TID  1444] GC process arrays (0.046000 / 0.047938 ms)... Arr Size: 367 -> 338, Queue: 49, Cleanup: 29
[01:48:32:055 TID 34872] GC process arrays (0.046000 / 0.047929 ms)... Arr Size: 367 -> 338, Queue: 48, Cleanup: 29
[01:48:32:071 TID 38452] GC process arrays (0.044000 / 0.048451 ms)... Arr Size: 366 -> 338, Queue: 48, Cleanup: 28
[01:48:32:088 TID 38452] GC process arrays (0.043000 / 0.048429 ms)... Arr Size: 367 -> 338, Queue: 49, Cleanup: 29
[01:48:32:105 TID 33116] GC process arrays (0.044000 / 0.048478 ms)... Arr Size: 366 -> 338, Queue: 48, Cleanup: 28
[01:48:32:121 TID 23832] GC process arrays (0.044000 / 0.048456 ms)... Arr Size: 367 -> 338, Queue: 49, Cleanup: 29
[01:48:32:138 TID 34872] GC process arrays (0.049000 / 0.047857 ms)... Arr Size: 366 -> 338, Queue: 48, Cleanup: 28
[01:48:32:155 TID  1444] GC process arrays (0.044000 / 0.047929 ms)... Arr Size: 367 -> 338, Queue: 49, Cleanup: 29
[01:48:32:171 TID 31688] GC process arrays (0.048000 / 0.048451 ms)... Arr Size: 367 -> 345, Queue: 48, Cleanup: 22
[01:48:32:188 TID 23832] GC process arrays (0.044000 / 0.048415 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:32:205 TID 23832] GC process arrays (0.044000 / 0.048451 ms)... Arr Size: 374 -> 345, Queue: 56, Cleanup: 29
[01:48:32:221 TID 23832] GC process arrays (0.046000 / 0.048460 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:32:238 TID 23832] GC process arrays (0.044000 / 0.048456 ms)... Arr Size: 374 -> 345, Queue: 56, Cleanup: 29
[01:48:32:254 TID 38452] GC process arrays (0.045000 / 0.048415 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:32:271 TID 31688] GC process arrays (0.046000 / 0.048434 ms)... Arr Size: 374 -> 345, Queue: 56, Cleanup: 29
[01:48:32:288 TID  1444] GC process arrays (0.046000 / 0.048434 ms)... Arr Size: 374 -> 345, Queue: 55, Cleanup: 29
[01:48:32:304 TID 38452] GC process arrays (0.046000 / 0.048442 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:32:321 TID 31688] GC process arrays (0.045000 / 0.048451 ms)... Arr Size: 374 -> 345, Queue: 56, Cleanup: 29
[01:48:32:338 TID 23832] GC process arrays (0.044000 / 0.048456 ms)... Arr Size: 373 -> 345, Queue: 55, Cleanup: 28
[01:48:32:354 TID 38452] GC process arrays (0.046000 / 0.048456 ms)... Arr Size: 374 -> 345, Queue: 56, Cleanup: 29
```

## Log format
* logging is disabled by default.

### Timestamps
```
[19:38:20:145 TID 32280]

19 - Hour
38 - Minute
20 - Second
145 - Milliseconds
32280 - Thread ID
```

### Context breakdown
```log
[19:17:21:217 TID  5664] GCBugFix v1.0.0 <---- Line 1. Game startup.
;...
[05:22:44:440 TID 29508] Released an object: 0x0000022023693A80 [MS02QuestScript MS02 (00040A5E)]
[05:22:44:440 TID 29508] Released an object: 0x0000022075BE2F50 [QF_MS02_00040A5E MS02 (00040A5E)]
[05:22:44:441 TID 29508] Released an object: 0x00000220233D8600 [mq302script MQ302 (00045923)]
[05:22:44:441 TID 29508] Released an object: 0x0000022075BDE1F0 [QF_MQ302_00045923 MQ302 (00045923)]
[05:22:44:441 TID 29508] Released an object: 0x0000022075BC87A0 [da02script DA02 (0004D8D6)]
[05:22:44:441 TID 29508] Released an object: 0x0000022075BCA680 [QF_DA02_0004D8D6 DA02 (0004D8D6)]

; The start of this log includes events all the way back to the main menu when a save is initially loaded. You'll see plenty of
; objects being created and destroyed. They're not very relevant to the bug. It's included for completeness.

[01:48:31:871 TID  1444] GC process arrays (0.045000 / 0.048447 ms)... Arr Size: 372 -> 344, Queue: 54, Cleanup: 28

; Note: This describes per-VM-frame incremental garbage collector with actuall runtime and given budget, array size changes and
; queue for the cleanup before operation.

[01:48:58:789 TID 33116] GC process objects (0.002000 / 0.048442 ms)...Arr Size: 1 -> 0, Cleanup: 1
[01:48:58:789 TID 33116] 	TIF__000340B9 <---- Script Object that has ben cleaned.

; Note: The Script Object GC patch is added in SSE/AE port, though I can't find a way to manually trigger the cleanup.
```

```log
; Note: Full garbage collection like following can happen multiple times - typically on transitions (loading screens) or saves.

[19:38:20:145 TID 32280]: =============================================== <---- Demarks the start of a full GC pass. This is also the BEGINNING of the Long Hang.
[19:38:20:145 TID 32280]: Begin full garbage collection
[19:38:20:145 TID 32280]: ===============================================

; SSE/AE full gc doesn't has detail log.

[19:50:31:178 TID 32280]: =============================================== <---- Demarks the end of a full GC pass. This is also the END of the Long Hang.
[19:50:31:178 TID 32280]: End full garbage collection (891908 ms)
[19:50:31:178 TID 32280]: ===============================================

; Random script noise after the save completes.
```

# Changes from Fo4 GC fix

* SSE doesn't have ActiveObject and Struct for GC.
* Change the patch from reimplimenting gc subroutine to binary patch.
* Add patch to similar routine GC_Object.

# Change log

* Feb.22.2025 first release.
* Aug.23.2024 init port. Patch is working, no demo script.

# License

* [GPLv3](COPYING) with [exceptions](EXCEPTIONS)
