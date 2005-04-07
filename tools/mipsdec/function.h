#ifndef FUNCTION_H
#define FUNCTION_H

#include <vector>
#include "instruction.h"
#include "register.h"
#include "symbols.h"

class Function
{
public:
	Function(void) :
		bHasStackOffset(false),
		uStackOffset(0)
	{
	}

	void detectStackOffset(void);
	bool parseFromFile(const std::string &strFuncName, const std::string &strBinFile);

	tInstList collInstList;
	bool bHasStackOffset;
	unsigned uStackOffset;
	std::string strName;
};

#endif
