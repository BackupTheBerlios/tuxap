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
						//case IT_J:
							/* Jump address not in register */
						//	break;
						case IT_BEQ:
						//case IT_BLTZ:
						//case IT_BNE:
							if(itCurr->modifiesRegister((itCurr + 1)->eRS))
							{
								Instruction aInstruction;
								aInstruction.encodeRegisterMove((itCurr + 1)->eRS, getDSBRegister((itCurr + 1)->eRS));
								(itCurr + 1)->eRS = getDSBRegister((itCurr + 1)->eRS);
								collInstList.insert(itCurr, aInstruction);
							}
							if(itCurr->modifiesRegister((itCurr + 1)->eRT))
							{
								Instruction aInstruction;
								aInstruction.encodeRegisterMove((itCurr + 1)->eRT, getDSBRegister((itCurr + 1)->eRT));
								(itCurr + 1)->eRT = getDSBRegister((itCurr + 1)->eRT);
								collInstList.insert(itCurr, aInstruction);
							}
							break;
						//default:
							//M_ASSERT(false);
					}

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