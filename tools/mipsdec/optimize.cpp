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
		tInstList::iterator itNext = itCurr;
		itNext++;

		if((!itCurr->bDelaySlotReordered) && (itCurr->getDelaySlotType() != IDS_NONE))
		{
			M_ASSERT(itNext != itEnd);
			itCurr->bDelaySlotReordered = true;

			switch(itCurr->getDelaySlotType())
			{
				case IDS_CONDITIONAL:
				{
					M_ASSERT(itCurr->getClassType() == IC_BRANCH);

					/* Move delay slot instruction into if branch */
					itCurr->collIfBranch.push_back(*itNext);
					collInstList.erase(itNext);
					break;
				}
				case IDS_UNCONDITIONAL:
					if((itCurr->getClassType() == IC_BRANCH) && (delaySlotClash(*itCurr, *itNext)))
					{
						/* Move delay slot instruction into if and else branch */
						itCurr->collIfBranch.push_back(*itNext);
						itCurr->collElseBranch.push_back(*itNext);
						collInstList.erase(itNext);
					}
					else
					{
						/* Swap master and delay slot instruction */
						itCurr->swap(*itNext, false);
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

static unsigned getCalcNumJumpSources(tInstList &collInstList, unsigned uAddress, Instruction **pLastJumpSource = NULL, unsigned uDepth = 0)
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

			if(pLastJumpSource)
			{
				*pLastJumpSource = &(*itCurr);
			}
		}

		uNumJumpSources += getCalcNumJumpSources(itCurr->collIfBranch, uAddress, pLastJumpSource, uDepth + 1);
		uNumJumpSources += getCalcNumJumpSources(itCurr->collElseBranch, uAddress, pLastJumpSource, uDepth + 1);

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
static bool reassembleSingleJumpBlocks(Function &aFunction, tInstList &aCurrBranch)
{
	tInstList::iterator itCurr = aCurrBranch.begin();
	tInstList::iterator itEnd = aCurrBranch.end();

	while(itCurr != itEnd)
	{
		if(itCurr->eType == IT_J)
		{
			tInstList::iterator itBlockBegin = itCurr;
			itBlockBegin++;

			/* Found jump instruction next has to be a jump target */
			if((itBlockBegin != itEnd) && (itBlockBegin->bIsJumpTarget))
			{
				tInstList::iterator itBlockEnd = itBlockBegin;
				itBlockEnd++;

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
						Instruction *pLastJumpSource = NULL;

						unsigned uNumJumpSources = getCalcNumJumpSources(aFunction.m_collInstList, itBlockBegin->uAddress, &pLastJumpSource);
						if(uNumJumpSources == 1)
						{
							tInstList::iterator itBlockEndNext = itBlockEnd;
							itBlockEndNext++;

							/* Yeah! Found one... move block into if branch*/
							M_ASSERT(pLastJumpSource->collInsertAfter.size() == 0);
							pLastJumpSource->deleteDelayed();
							pLastJumpSource->collInsertAfter.insert(pLastJumpSource->collInsertAfter.end(), itBlockBegin, itBlockEndNext);
							aCurrBranch.erase(itBlockBegin, itBlockEndNext);

							return true;
						}
					}

					itBlockEnd++;
				}
			}
		}

		if(reassembleSingleJumpBlocks(aFunction, itCurr->collIfBranch))
		{
			return true;
		}

		if(reassembleSingleJumpBlocks(aFunction, itCurr->collElseBranch))
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
				tInstList::iterator itBlockBegin = itCurr;
				itBlockBegin++;

				tInstList::iterator itBlockEnd = itCurr;
				itBlockEnd++;

				while(itBlockEnd != itEnd)
				{
					tInstList::iterator itBlockNext = itBlockEnd;
					itBlockNext++;

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

					if(itBlockNext == itEnd)
					{
						/* Move block into else branch */
						itCurr->collElseBranch.insert(itCurr->collElseBranch.begin(), itBlockBegin, itBlockNext);
						collInstList.erase(itBlockBegin, itBlockNext);
						return true;
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
		case IT_BEQL:
			aInstruction.eType = IT_BNEL;
			break;
		case IT_BNE:
			aInstruction.eType = IT_BEQ;
			break;
		case IT_BGTZ:
			aInstruction.eType = IT_BLEZ;
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
		if((itCurr->uJumpAddress == uJumpTarget) && (!itCurr->bIgnoreJump))
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
	tInstList::iterator itNext = collInstList.begin();
	itNext++;

	while((itCurr != itEnd) && (itNext != itEnd))
	{
		if(((itCurr->collIfBranch.size() > 0) || (itCurr->collElseBranch.size() > 0)) && (itNext->bIsJumpTarget))
		{
			if(removeClosingJump(itCurr->collIfBranch, itNext->uAddress))
			{
				return true;
			}

			if(removeClosingJump(itCurr->collElseBranch, itNext->uAddress))
			{
				return true;
			}
		}

		itCurr++;
		itNext++;
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
			M_ASSERT((unsigned)abs(itCurr->iSI) == uStackOffset);

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
				case 20:
				case 24:
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

	return stripEpilogProlog(aFunction.m_collInstList, aFunction.uStackOffset);
}

static bool stripNops(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if((itCurr->isNOP()) && (!itCurr->bIsJumpTarget))
		{
			collInstList.erase(itCurr);
			return true;
		}

		if(stripNops(itCurr->collIfBranch))
		{
			return true;
		}

		if(stripNops(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}

	return false;
}

static bool detectReturningIfBranch(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if((itCurr->collIfBranch.size() > 0) && (itCurr->collElseBranch.size() > 0))
		{
			tInstList::reverse_iterator itLastIf = itCurr->collIfBranch.rbegin();
		
			if((itLastIf->eType == IT_JR) && (itLastIf->eRS == R_RA))
			{
				tInstList::iterator itNext = itCurr;
				itNext++;
				
				collInstList.insert(itNext, itCurr->collElseBranch.begin(), itCurr->collElseBranch.end());
				itCurr->collElseBranch.clear();
				
				return true;
			}
		}
		
		if(detectReturningIfBranch(itCurr->collIfBranch))
		{
			return true;
		}

		if(detectReturningIfBranch(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}
	
	return false;
}

static bool detectIdenticalIfElseBranch(tInstList &collInstList)
{
	tInstList::iterator itCurr = collInstList.begin();
	tInstList::iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		if((itCurr->collIfBranch.size() > 0) && (itCurr->collElseBranch.size() > 0))
		{
			tInstList::reverse_iterator itLastIf = itCurr->collIfBranch.rbegin();
			tInstList::reverse_iterator itLastElse = itCurr->collElseBranch.rbegin();
		
			if((itLastIf->collIfBranch.size() == 0) && (itLastElse->collIfBranch.size() == 0) && (itLastIf->isSame(*itLastElse)))
			{
				M_ASSERT(itLastIf->collElseBranch.size() == 0);
				M_ASSERT(itLastElse->collElseBranch.size() == 0);

				Instruction aInstruction = *itLastIf;
				tInstList::iterator itNext = itCurr;
				itNext++;

				itCurr->collIfBranch.pop_back();
				itCurr->collElseBranch.pop_back();

				collInstList.insert(itNext, aInstruction);
			
				return true;
			}
		}
		
		if(detectIdenticalIfElseBranch(itCurr->collIfBranch))
		{
			return true;
		}

		if(detectIdenticalIfElseBranch(itCurr->collElseBranch))
		{
			return true;
		}

		itCurr++;
	}
	
	return false;
}

static unsigned getInstructionCount(const tInstList::iterator &itBlockBegin, const tInstList::iterator &itBlockEnd)
{
	unsigned uInstructionCount = 0;
	tInstList::iterator itCurr = itBlockBegin;

	while(itCurr != itBlockEnd)
	{
		if(!itCurr->isNOP())
		{
			uInstructionCount++;
			uInstructionCount += getInstructionCount(itCurr->collIfBranch.begin(), itCurr->collIfBranch.end());
			uInstructionCount += getInstructionCount(itCurr->collElseBranch.begin(), itCurr->collElseBranch.end());
		}

		itCurr++;
	}

	return uInstructionCount;
}

#define MAX_CLONE_INSTRUCTIONS 10

static bool detectAndCloneSmallBlocks(Function &aFunction, tInstList &aCurrBranch)
{
	tInstList::iterator itCurr = aCurrBranch.begin();
	tInstList::iterator itEnd = aCurrBranch.end();

	while(itCurr != itEnd)
	{
		if(itCurr->bIsJumpTarget)
		{
			tInstList::iterator itBlockBegin = itCurr;
			tInstList::iterator itBlockEnd = itCurr;

			while(itBlockEnd != itEnd)
			{
				if((itBlockEnd->eType == IT_J) || (itBlockEnd->eType == IT_JR))
				{
					tInstList::iterator itBlockEndNext = itBlockEnd;
					itBlockEndNext++;

					unsigned uInstructionCount = getInstructionCount(itBlockBegin, itBlockEndNext);

					if(uInstructionCount <= MAX_CLONE_INSTRUCTIONS)
					{
						Instruction *pLastJumpSource = NULL;

						unsigned uNumJumpSources = getCalcNumJumpSources(aFunction.m_collInstList, itBlockBegin->uAddress, &pLastJumpSource);
						M_ASSERT(uNumJumpSources >= 1);
						M_ASSERT(pLastJumpSource->collInsertAfter.size() == 0);

						pLastJumpSource->deleteDelayed();
						pLastJumpSource->collInsertAfter.insert(pLastJumpSource->collInsertAfter.end(), itBlockBegin, itBlockEndNext);

						return true;
					}

					break;
				}

				itBlockEnd++;
			}
		}

		itCurr++;
	}

	return false;
}

bool optimizeInstructions(Function &aFunction)
{
#if 1
	if(reassembleSingleJumpBlocks(aFunction, aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectElseBranch(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectOnlyElseBranch(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectEndOfBranchJumps(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectEpilogProlog(aFunction))
	{
		return true;
	}
#endif

#if 1
	if(stripNops(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectReturningIfBranch(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectIdenticalIfElseBranch(aFunction.m_collInstList))
	{
		return true;
	}
#endif

#if 1
	if(detectAndCloneSmallBlocks(aFunction, aFunction.m_collInstList))
	{
		return true;
	}
#endif

	return false;
}
