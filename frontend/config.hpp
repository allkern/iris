/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

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

#pragma once

#ifndef _IRIS_VERSION
#define _IRIS_VERSION latest
#endif

#ifndef _IRIS_COMMIT
#define _IRIS_COMMIT latest
#endif

#ifndef _IRIS_OSVERSION
#define _IRIS_OSVERSION unknown
#endif

#ifndef _IRIS_PROCESSOR
#define _IRIS_PROCESSOR unknown
#endif

#define STR1(m) #m
#define STR(m) STR1(m)

#define IRIS_TITLE "Iris (" STR(_IRIS_VERSION) ")"
#define IRIS_COMMIT STR(_IRIS_COMMIT)
#define IRIS_VULKAN_API_VERSION VK_API_VERSION_1_2

#if defined(__clang__)
    #define IRIS_COMPILER_NAME "Clang"
    #define IRIS_COMPILER_VERSION STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
#elif defined(__GNUC__) || defined(__GNUG__)
    #define IRIS_COMPILER_NAME "GCC"
    #define IRIS_COMPILER_VERSION STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define IRIS_COMPILER_NAME "MSVC"
    // _MSC_VER is typically 19xx for VS 2015-2022
    #define IRIS_COMPILER_VERSION STR(_MSC_VER) 
#else
    #define IRIS_COMPILER_NAME "Unknown"
    #define IRIS_COMPILER_VERSION "Unknown"
#endif