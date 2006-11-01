#ifndef LOADERBASE_H
#define LOADERBASE_H

#include <string>
#include <vector>

class LoaderBase
{
public:
	enum eArchType
	{
		ARCH_TYPE_UNKNOWN = 0,
		ARCH_TYPE_X86 = 1,
		ARCH_TYPE_MIPS32 = 2
	};

	typedef std::vector<unsigned char> tCollData;

	virtual bool loadFile(const std::string &strFileName) = 0;
	virtual eArchType getArchType() = 0;
	virtual bool getCodeSectionByName(const std::string &strName, tCollData &collData) = 0;
	virtual bool getCodeSectionByAddress(unsigned uAddress, tCollData &collData) = 0;
	virtual bool getMainCodeSection(tCollData &collData) = 0;
};

#endif