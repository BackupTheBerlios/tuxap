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

/* Try to detect else branches and move code there to get rid of some  */
/* jumps. This will also remove the jump instruction at the end of the */
/* if branch if the jump target directly follows the if branch         */
/* Preconditions:                                                      */
/* - Last instruction in the if branch is an absolute jump instruction */
/* - No else branch                                                    */
/* - No instruction in the block is a jump target                      */
static bool detectElseBranch(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		/* Do we have an if branch and no else branch? */
		if((itCurr->collIfBranch.size() > 0) && (itCurr->collElseBranch.size() == 0))
		{
			tInstList::reverse_iterator itJumpInstruction = itCurr->collIfBranch.rbegin();

			/* Last if branch instruction a jump instruction? */
			if(itJumpInstruction->eType == IT_J)
			{
				tInstList::iterator itBlockBegin = itCurr + 1;
				tInstList::iterator itBlockEnd = itCurr + 1;

				while(itBlockEnd != itEnd)
				{
					if(itBlockEnd->uAddress == itJumpInstruction->uJumpAddress)
					{
						/* Remove jump from if branch */
						itCurr->collIfBranch.pop_back();

						/* Move block into else branch */
						itCurr->collElseBranch.insert(itCurr->collElseBranch.begin(), itBlockBegin, itBlockEnd);
						collInstList.erase(itBlockBegin, itBlockEnd);
						return true;
					}

					/* No jump targets allowed */
					if(itBlockEnd->bIsJumpTarget)
					{
						break;
					}

					itBlockEnd++;
				}
			}
		}

		if(detectElseBranch(itCurr->collIfBranch))
		{
			return true;
		}

		if(detectElseBranch(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}

static void invertCondition(Instruction &aInstruction)
{
	M_ASSERT(aInstruction.getClassType() == IC_BRANCH);

	switch(aInstruction.eType)
	{
		case IT_BEQ:
			aInstruction.eType = IT_BNE;
			break;
		case IT_BNE:
			aInstruction.eType = IT_BEQ;
			break;
		case IT_BLTZ:
			aInstruction.eType = IT_BGEZ;
			break;
		default:
			M_ASSERT(false);
	}
}

/* Detect branch instructions with empty if branches and non empty     */
/* else branches (caused/happens in different optimizer steps)         */
/* Preconditions:                                                      */
/* - Empty else branch                                                 */
/* - Non empty else branch                                             */
static bool detectOnlyElseBranch(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		/* Do we have an else branch and no if branch? */
		if((itCurr->collIfBranch.size() == 0) && (itCurr->collElseBranch.size() > 0))
		{
			invertCondition(*itCurr);
			itCurr->collIfBranch = itCurr->collElseBranch;
			itCurr->collElseBranch.clear();
			return true;
		}

		if(detectOnlyElseBranch(itCurr->collIfBranch))
		{
			return true;
		}

		if(detectOnlyElseBranch(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}

static bool removeClosingJump(tInstList &collInstList, unsigned uJumpTarget)
{
	M_ASSERT(uJumpTarget != 0);
	M_ASSERT(uJumpTarget != 0xFFFFFFFF);

	tInstList::reverse_iterator itCurr = collInstList.rbegin();
	tInstList::reverse_iterator itEnd = collInstList.rend();

	if(itCurr != itEnd)
	{
		if(itCurr->uJumpAddress == uJumpTarget)
		{
			collInstList.pop_back();
			return true;
		}

		if(removeClosingJump(itCurr->collIfBranch, uJumpTarget))
		{
			return true;
		}

		if(removeClosingJump(itCurr->collElseBranch, uJumpTarget))
		{
			return true;
		}
	}

	return false;
}

/* Detect situations where a jumps at the end of a branch jumps to a   */
/* target which is the next (outside) instruction after the branch     */
/* Preconditions:                                                      */
/* - Last instruction in the if/else branch                            */
/* - Jump target directly follows the branch                           */
static bool detectEndOfBranchJumps(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while((itCurr != itEnd) && ((itCurr + 1) != itEnd))
	{
		if(((itCurr->collIfBranch.size() > 0) || (itCurr->collElseBranch.size() > 0)) && ((itCurr + 1)->bIsJumpTarget))
		{
			if(removeClosingJump(itCurr->collIfBranch, (itCurr + 1)->uAddress))
			{
				return true;
			}

			if(removeClosingJump(itCurr->collElseBranch, (itCurr + 1)->uAddress))
			{
				return true;
			}
		}

		itCurr++;
	}

	return false;
}

static bool stripEpilogProlog(tInstList &collInstList, unsigned uStackOffset)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if(itCurr->modifiesRegister(R_SP))
		{
			M_ASSERT(itCurr->eType == IT_ADDIU);
			M_ASSERT(abs(itCurr->iSI) == uStackOffset);

			if(itCurr->bIsJumpTarget)
			{
				itCurr->makeNOP();
			}
			else
			{
				collInstList.erase(itCurr);
			}
			return true;
		}

		if(((itCurr->eType == IT_LW) || (itCurr->eType == IT_SW)) && (itCurr->eRS == R_SP))
		{
			int iDelta = uStackOffset - abs(itCurr->iSI);
			switch(iDelta)
			{
				case 4:
				case 8:
				case 12:
				case 16:
					if(itCurr->bIsJumpTarget)
					{
						itCurr->makeNOP();
					}
					else
					{
						collInstList.erase(itCurr);
					}
					return true;
					break;
				//default:
					//M_ASSERT(false);
			}
		}

		if(stripEpilogProlog(itCurr->collIfBranch, uStackOffset))
		{
			return true;
		}

		if(stripEpilogProlog(itCurr->collElseBranch, uStackOffset))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}

static bool detectEpilogProlog(Function &aFunction)
{
	if(!aFunction.bHasStackOffset)
	{
		return false;
	}

	return stripEpilogProlog(aFunction.collInstList, aFunction.uStackOffset);
}

bool optimizeInstructions(Function &aFunction, unsigned uCompletePassesDone)
{
	if(reassembleSingleJumpBlocks(aFunction.collInstList))
	{
		return true;
	}
	
	if(detectElseBranch(aFunction.collInstList))
	{
		return true;
	}
	
	if(detectOnlyElseBranch(aFunction.collInstList))
	{
		return true;
	}
	
	if(detectEndOfBranchJumps(aFunction.collInstList))
	{
		return true;
	}

	if(detectEpilogProlog(aFunction))
	{
		return true;
	}
	
	return false;
}
