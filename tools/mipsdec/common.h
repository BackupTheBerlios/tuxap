#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
//#include <windows.h>
#include <assert.h>
#define M_ASSERT(x) assert(x)
#pragma warning(disable : 4996)
#else
void __doFail(const char *pCondition, const char *pFile, const char *pFunction, unsigned int uLine);
#define M_ASSERT(x) if(!(x)) __doFail(#x, __FILE__, __FUNCTION__, __LINE__)
#endif

#endif
