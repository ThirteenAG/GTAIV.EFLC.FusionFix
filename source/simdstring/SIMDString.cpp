/*
MIT License

Copyright (c) 2022 Morgan McGuire and Zander Majercik

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <cstdlib>

#ifdef _WIN32
#   define OS_WINDOWS
#elif defined(__APPLE__)
#   define OS_OSX

// Prevent OS X fp.h header from being included; it defines
// pi as a constant, which creates a conflict
#   define __FP__
#elif  defined(__FreeBSD__) || defined(__OpenBSD__)
#   define OS_FREEBSD
#   define OS_LINUX
#elif defined(__linux__)
#   define OS_LINUX
#else
#   error Unknown platform
#endif

#if defined(OS_LINUX) || (OS_FREEBSD)
// etext is defined by the linker as the end of the text segment
// edata is defined by the linker as the end of the initialized data segment
// the edata section comes after the etext segment
extern char etext, edata;
#endif

/** Returns true if this C string pointer is definitely located in the constant program data segment
and does not require memory management. Used by SIMDString. */
bool inConstSegment(const char* c) {
#ifdef OS_LINUX
    // We can't use __builtin_constant(c) because that checks whether the expression
    // is a constexpr, not where the address is. Here, the expression is never constant
    // (it is a runtime value).

    // check if the address is in the initialized data section (edata)
    return (c > &etext) && (c < &edata);
#else

    static const char* testStr = "__A Unique ConstSeg String__";
    static const uintptr_t PROBED_CONST_SEG_ADDR = uintptr_t(testStr);

    // MSVC assigns the const_seg to very high addresses that are grouped together.
    // Beware that when using explicit SEGMENT https://docs.microsoft.com/en-us/cpp/assembler/masm/segment?view=vs-2017
    // or const_seg https://docs.microsoft.com/en-us/cpp/preprocessor/const-seg?view=vs-2017, this does not work.
    // However, that simply disables some optimizations in G3DString, it doesn't break it.
    //
    // Assume it is at least 5 MB long.
    return (std::labs(static_cast<long>(uintptr_t(c) - PROBED_CONST_SEG_ADDR)) < 5000000L);
#endif
}
