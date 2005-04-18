#ifndef PARAMETER_H
#define PARAMETER_H

#include <string>

typedef enum {
	TF_NONE = 0,
	TF_BY_REFERENCE = 1,
	TF_CONST = 2,
} tTypeFlags;

struct TypeInfo;

typedef struct
{
	const char *pName;
	unsigned int uFlags;
	const struct TypeInfo *pInfo;
	unsigned uArrayElements;
} tParameter;

typedef struct TypeInfo
{
	unsigned uSize;
	const char *pName;
	tParameter subFields[100];
} tTypeInfo;

typedef struct {
	const char *pName;
	tParameter collParameters[100];
} tFunctionParameters;

tParameter *getFunctionParameters(const std::string &strFunctionName);

#endif
