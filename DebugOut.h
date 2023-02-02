#pragma once
#include "../libcommon/libcommon/DbgPrintf.h"

//#define SHOW_DEBUG_PRINT

#ifdef SHOW_DEBUG_PRINT
#define DEBUG_PRINT(LPCTSTR, ...) DbgPrintf(LPCTSTR, __VA_ARGS__)
#else
#define DEBUG_PRINT(LPCTSTR, ...)
#endif
