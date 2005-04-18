#ifndef PARAMETER_H
#define PARAMETER_H

typedef enum {
	TF_NONE = 0,
	TF_BY_REFERENCE = 1,
	TF_CONST = 2,
} tTypeFlags;

struct TypeInfo;

typedef struct
{
	const char *pName;
	tTypeFlags eFlags;
	const struct TypeInfo *pInfo;
} tParameter;

typedef struct TypeInfo
{
	unsigned uSize;
	const char *pName;
	tParameter subFields[100];
} tTypeInfo;

#endif
