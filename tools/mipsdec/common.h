#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
#include <windows.h>
#include <assert.h>
#define M_ASSERT(x) assert(x)
#else
#define M_ASSERT(x) if(!(x)) printf("Assertion (%s) failed in file: %s, function: %s, line %d\n", #x, __FILE__, __FUNCTION__, __LINE__)
#endif

#endif
