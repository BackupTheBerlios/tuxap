#include "optimize.h"
#include "instruction.h"
#include "common.h"

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
				case IT_J:
				case IT_JALR:
				case IT_JALR_HB:
				case IT_JR:
				case IT_JR_HB:
				{
					itCurr->bDelaySlotReordered = true;

					M_ASSERT((itCurr + 1) < itEnd);
					itCurr->swap(*(itCurr + 1), false);

					itCurr = collInstList.begin();
					itEnd = collInstList.end();

					continue;
				}

				case IT_BEQ:
				case IT_BLTZ:
				case IT_BNE:
				{
					itCurr->bDelaySlotReordered = true;

					M_ASSERT((itCurr + 1) < itEnd);

					/* Close if branch using the original jump address */
					Instruction aInstruction;
					aInstruction.encodeAbsoluteJump(itCurr->uJumpAddress);
					itCurr->collIfBranch.push_back(aInstruction);

					itCurr->swap(*(itCurr + 1), false);

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
					Instruction aInstruction;
					aInstruction.encodeAbsoluteJump(itCurr->uJumpAddress);
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