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
					if(delaySlotClash(*itCurr, *(itCurr + 1)))
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

#if 0
			switch(itCurr->eType)
			{
				case IT_J:
				case IT_JALR:
				case IT_JALR_HB:
				case IT_JR:
				case IT_JR_HB:
				{
					M_ASSERT((itCurr + 1) < itEnd);

					itCurr->bDelaySlotReordered = true;
					itCurr->swap(*(itCurr + 1), false);

					/* If the instruction in the delay slot modifies the */
					/* jump address we need to keep a backup to handle   */
					/* the jump in the correct way...                    */
					switch((itCurr + 1)->eType)
					{
						case IT_J:
							/* Jump address not in register */
							break;
						case IT_JALR:
						case IT_JALR_HB:
						case IT_JR:
						case IT_JR_HB:
							/* Jump address in register in RS */
							if(itCurr->modifiesRegister((itCurr + 1)->eRS))
							{
								Instruction aInstruction;
								aInstruction.encodeRegisterMove((itCurr + 1)->eRS, getDSBRegister((itCurr + 1)->eRS));
								(itCurr + 1)->eRS = getDSBRegister((itCurr + 1)->eRS);
								collInstList.insert(itCurr, aInstruction);
							}
							break;
						default:
							M_ASSERT(false);
					}

					itCurr = collInstList.begin();
					itEnd = collInstList.end();

					continue;
				}

				case IT_BEQ:
				case IT_BLTZ:
				case IT_BNE:
				{
					M_ASSERT((itCurr + 1) < itEnd);

					itCurr->bDelaySlotReordered = true;

					/* Close if branch using the original jump address */
					Instruction aInstruction;
					aInstruction.encodeAbsoluteJump(itCurr->uJumpAddress);
					itCurr->collIfBranch.push_back(aInstruction);

					itCurr->swap(*(itCurr + 1), false);

					/* If the instruction in the delay slot modifies registers */
					/* used in the branch instruction we need to keep a backup */
					/* to handle the branch in the correct way...              */
					switch((itCurr + 1)->eType)
					{
						case IT_BEQ:
						case IT_BNE:
							if(itCurr->modifiesRegister((itCurr + 1)->eRT))
							{
								Instruction aInstruction;
								aInstruction.encodeRegisterMove((itCurr + 1)->eRT, getDSBRegister((itCurr + 1)->eRT));
								(itCurr + 1)->eRT = getDSBRegister((itCurr + 1)->eRT);
								collInstList.insert(itCurr, aInstruction);
							}
							/* Fall through */
						case IT_BLTZ:
							if(itCurr->modifiesRegister((itCurr + 1)->eRS))
							{
								Instruction aInstruction;
								aInstruction.encodeRegisterMove((itCurr + 1)->eRS, getDSBRegister((itCurr + 1)->eRS));
								(itCurr + 1)->eRS = getDSBRegister((itCurr + 1)->eRS);
								collInstList.insert(itCurr, aInstruction);
							}
							break;
						default:
							M_ASSERT(false);
					}

					itCurr = collInstList.begin();
					itEnd = collInstList.end();

					continue;
				}
				case IT_BEQL:
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
#endif
		}

		itCurr++;
	}

	return true;
}

bool optimizeInstructions(tInstList &collInstList)
{
	return true;
}