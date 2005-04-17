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
	void applyDelayedDeletes(void);
	void applyDelayedInserts(void);

	tInstList m_collInstList;
	bool bHasStackOffset;
	unsigned uStackOffset;
	std::string strName;

private:
	bool applyDelayedDeletesRecursive(tInstList &collInstList);
	bool applyDelayedInsertsRecursive(tInstList &collInstList);
};

#endif
