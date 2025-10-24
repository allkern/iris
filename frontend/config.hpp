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

#define STR1(m) #m
#define STR(m) STR1(m)

#define IRIS_TITLE "Iris (" STR(_IRIS_VERSION) ")"
#define IRIS_VULKAN_API_VERSION VK_API_VERSION_1_2