#include "function.h"
#include "common.h"

void Function::detectStackOffset(void)
{
	tInstList::const_iterator itCurr = m_collInstList.begin();
	tInstList::const_iterator itEnd = m_collInstList.end();

	while(itCurr != itEnd)
	{
		if(itCurr->modifiesRegister(R_SP))
		{
			M_ASSERT(itCurr->eType == IT_ADDIU);
			M_ASSERT(itCurr->iSI < 0);

			uStackOffset = abs(itCurr->iSI);
			bHasStackOffset = true;

			return;
		}

		itCurr++;
	}

	uStackOffset = 0;
	bHasStackOffset = false;
}

static unsigned calcSymFileOffset(unsigned uSymAddress)
{
	return uSymAddress - 0x80000000;
}

bool Function::parseFromFile(const std::string &strFuncName, const std::string &strBinFile)
{
	unsigned uSymIdx;
	strName = strFuncName;

	if(!Symbols::lookup(strFuncName, uSymIdx))
	{
		printf("Unable to locate function %s\n", strFuncName.c_str());
		return false;
	}

	unsigned uInstructionCount;

	if((uSymIdx + 1) == Symbols::getCount())
	{
		M_ASSERT(false); // FIXME: TODO
		uInstructionCount = 0;
	}
	else
	{
		uInstructionCount = (Symbols::get(uSymIdx + 1)->uAddress - Symbols::get(uSymIdx)->uAddress) / 4;
	}
	
	FILE *pBinFile = fopen(strBinFile.c_str(), "rb");
	
	if(!pBinFile)
	{
		printf("Can't open binary file: %s\n", strBinFile.c_str());
		return false;
	}
	
	fseek(pBinFile, calcSymFileOffset(Symbols::get(uSymIdx)->uAddress), SEEK_SET);
	
	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		unsigned uData = 0;

		for(unsigned uByteIdx = 0; uByteIdx < sizeof(uData); uByteIdx++)
		{
			unsigned char uByte;

			if(fread(&uByte, 1, sizeof(uByte), pBinFile) != sizeof(uByte))
			{
				printf("Short binary file read!\n");
				return false;
			}
			
			uData = (uData >> 8) | (uByte << 24);
		}
		
		Instruction aInstruction;

		aInstruction.eFormat = IF_UNKNOWN;

		if(!aInstruction.parse(uData, Symbols::get(uSymIdx)->uAddress + (4 * uInstructionIdx)))
		{
			return false;
		}
		else
		{
			m_collInstList.push_back(aInstruction);
		}
	}
	
	fclose(pBinFile);

	return true;
}

void Function::applyDelayedDeletes(void)
{
	while(applyDelayedDeletesRecursive(m_collInstList))
	{
	}
}

bool Function::applyDelayedDeletesRecursive(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if(itCurr->bDelayedDelete)
		{
			M_ASSERT(itCurr->collInsertAfter.size() == 0);
			collInstList.erase(itCurr);

			return true;
		}

		if(applyDelayedDeletesRecursive(itCurr->collIfBranch))
		{
			return true;
		}

		if(applyDelayedDeletesRecursive(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}
