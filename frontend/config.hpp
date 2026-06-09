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