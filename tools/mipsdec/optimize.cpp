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
				itCurr->bIgnoreJump = true;
			}

			/* Restart from beginning - iterators might be broken */
			itCurr = collInstList.begin();
			itEnd = collInstList.end();
		}

		itCurr++;
	}

	return true;
}

static unsigned getCalcNumJumpSources(tInstList &collInstList, unsigned uAddress, tInstList **pInstList = NULL, tInstList::iterator *pPosition = NULL)
{
	M_ASSERT(uAddress != 0);
	M_ASSERT(uAddress != 0xFFFFFFFF);

	unsigned uNumJumpSources = 0;
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if((itCurr->uJumpAddress == uAddress) && (!itCurr->bIgnoreJump))
		{
			uNumJumpSources++;

			if(pInstList)
			{
				*pInstList = &collInstList;
			}

			if(pPosition)
			{
				*pPosition = itCurr;
			}
		}

		uNumJumpSources += getCalcNumJumpSources(itCurr->collIfBranch, uAddress, pInstList, pPosition);
		uNumJumpSources += getCalcNumJumpSources(itCurr->collElseBranch, uAddress, pInstList, pPosition);

		itCurr++;
	}

	return uNumJumpSources;
}

/* Try to move code blocks referenced by only one jump instruction   */
/* back to the jump position                                         */
/* Preconditions:                                                    */
/* - Instruction before block is an absolute jump instruction        */
/* - First instruction in the block is a jump target                 */
/* - First instruction in the block has only one jump source         */
/* - Last instruction in the block is an absolute jump instruction   */
/* - No instruction in the block (except the first) is a jump target */
static bool reassembleSingleJumpBlocks(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if(itCurr->eType == IT_J)
		{
			tInstList::iterator itBlockBegin = itCurr + 1;

			/* Found jump instruction next has to be a jump target */
			if((itBlockBegin != itEnd) && (itBlockBegin->bIsJumpTarget))
			{
				tInstList::iterator itBlockEnd = itBlockBegin + 1;

				/* Now search for a closing jump instruction */
				while(itBlockEnd != itEnd)
				{
					/* No jump targets allowed within block */
					if(itBlockEnd->bIsJumpTarget)
					{
						break;
					}

					if(itBlockEnd->eType == IT_J)
					{
						/* Verify that the block has only one jump source */
						tInstList *pInstList;
						tInstList::iterator itSourcePosition;

						unsigned uNumJumpSources = getCalcNumJumpSources(collInstList, itBlockBegin->uAddress, &pInstList, &itSourcePosition);
						if(uNumJumpSources == 1)
						{
							/* Yeah! Found one... move block into if branch*/
							itSourcePosition = pInstList->erase(itSourcePosition);
							pInstList->insert(itSourcePosition, itBlockBegin, itBlockEnd + 1);
							collInstList.erase(itBlockBegin, itBlockEnd + 1);

							return true;
						}
					}

					itBlockEnd++;
				}
			}
		}

		if(reassembleSingleJumpBlocks(itCurr->collIfBranch))
		{
			return true;
		}

		if(reassembleSingleJumpBlocks(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}

bool optimizeInstructions(tInstList &collInstList)
{
	if(reassembleSingleJumpBlocks(collInstList))
	{
		return true;
	}

	return false;
}