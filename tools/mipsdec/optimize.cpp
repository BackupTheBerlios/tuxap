#include "optimize.h"
#include "instruction.h"
#include "common.h"

static void swapInstructions(tInstruction &aInstruction1, tInstruction &aInstruction2)
{
	tInstruction aInstructionTmp = aInstruction1;
	aInstruction1 = aInstruction2;
	aInstruction2 = aInstructionTmp;
}

bool resolveDelaySlots(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if(!itCurr->bDelaySlotReordered)
		{
			switch(itCurr->eType)
			{
				case IT_BEQ:
				case IT_BLTZ:
				case IT_BNE:
				case IT_J:
				case IT_JALR:
				case IT_JALR_HB:
				case IT_JR:
				case IT_JR_HB:
				{
					itCurr->bDelaySlotReordered = true;

					M_ASSERT((itCurr + 1) < itEnd);
					swapInstructions(*itCurr, *(itCurr + 1));

					itCurr = collInstList.begin();
					itEnd = collInstList.end();

					continue;
				}
				case IT_BNEL:
				{
					itCurr->bDelaySlotReordered = true;

					M_ASSERT((itCurr + 1) < itEnd);

					/* Move delay slot instruction into if branch */
					itCurr->collIfBranch.push_back(*(itCurr + 1));
					collInstList.erase(itCurr + 1);

					/* Close if branch using the original jump address */
					tInstruction aInstruction;
					aInstruction.eType = IT_J;
					M_ASSERT(false); // FIXME: Complete!
					itCurr->collIfBranch.push_back(aInstruction);

					itCurr = collInstList.begin();
					itEnd = collInstList.end();

					continue;
				}
			}
		}

		itCurr++;
	}

	return true;
}

bool optimizeInstructions(tInstList &collInstList)
{
	return true;
}