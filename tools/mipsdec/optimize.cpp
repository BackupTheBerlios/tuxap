#include "optimize.h"
#include "instruction.h"
#include "common.h"

static bool delaySlotClash(const Instruction &aMasterInstruction, const Instruction &aDelaySlotInstruction)
{
	switch(aMasterInstruction.eFormat)
	{
		case IF_RSRTRD:
			return (aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRS) ||
					aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRT) ||
					aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRD));
		case IF_RSRT:
		case IF_RSRTSI:
		case IF_RSRTUI:
			return (aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRS) || 
					aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRT));
		case IF_RSSI:
		case IF_RS:
			return aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRS);
		case IF_RTUI:
			return aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRT);
		case IF_RTRDSA:
		case IF_RTRDSEL:
			return (aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRT) || 
					aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRD));
		case IF_RSRD:
			return (aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRS) || 
					aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRD));
		case IF_RD:
			return aDelaySlotInstruction.modifiesRegister(aMasterInstruction.eRD);
			break;
		case IF_NOARG:
			return false;
		case IF_UNKNOWN:
		default:
			M_ASSERT(false);
			return false;
	}
}

bool resolveDelaySlots(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if((!itCurr->bDelaySlotReordered) && (itCurr->getDelaySlotType() != IDS_NONE))
		{
			M_ASSERT((itCurr + 1) < itEnd);
			itCurr->bDelaySlotReordered = true;

			switch(itCurr->getDelaySlotType())
			{
				case IDS_CONDITIONAL:
				{
					M_ASSERT(itCurr->getClassType() == IC_BRANCH);

					/* Move delay slot instruction into if branch */
					itCurr->collIfBranch.push_back(*(itCurr + 1));
					collInstList.erase(itCurr + 1);
					break;
				}
				case IDS_UNCONDITIONAL:
					if((itCurr->getClassType() == IC_BRANCH) && (delaySlotClash(*itCurr, *(itCurr + 1))))
					{
						/* Move delay slot instruction into if and else branch */
						itCurr->collIfBranch.push_back(*(itCurr + 1));
						itCurr->collElseBranch.push_back(*(itCurr + 1));
						collInstList.erase(itCurr + 1);
					}
					else
					{
						/* Swap master and delay slot instruction */
						itCurr->swap(*(itCurr + 1), false);
						itCurr++;
					}
					break;
				default:
					M_ASSERT(false);
					return false;
			}

			if(itCurr->getClassType() != IC_JUMP)
			{
				/* Close if branch using the original jump address */
				Instruction aInstruction;
				aInstruction.encodeAbsoluteJump(itCurr->uJumpAddress);
				itCurr->collIfBranch.push_back(aInstruction);
			}

			/* Restart from beginning - iterators might be broken */
			itCurr = collInstList.begin();
			itEnd = collInstList.end();
		}

		itCurr++;
	}

	return true;
}

bool optimizeInstructions(tInstList &collInstList)
{
	return false;
}