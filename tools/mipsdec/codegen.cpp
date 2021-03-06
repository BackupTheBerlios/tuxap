#include "instruction.h"
#include "function.h"
#include "parameter.h"
#include "common.h"

std::string getIndentStr(unsigned uDepth)
{
	std::string strIndent;

	while(uDepth > 0)
	{
		strIndent += '\t';
		uDepth--;
	}

	return strIndent;
}

std::string getRegVarName(tRegister eRegister)
{
	std::string strVarName = (eRegister == R_ZERO) ? "" : "REG_";
	strVarName += getRegName(eRegister);
	return strVarName;
}

void doIndent(FILE *pDestFile, unsigned uDepth)
{
	fprintf(pDestFile, "%s", getIndentStr(uDepth).c_str());
}

void generateInstructionCode(FILE *pDestFile, const tInstList &collInstList, unsigned uDepth)
{
	tInstList::const_iterator itCurr = collInstList.begin();
	tInstList::const_iterator itEnd = collInstList.end();

	while(itCurr != itEnd)
	{
		bool bBranchAllowed = false;
		const Instruction aInstruction = *itCurr;

		//fprintf(pDestFile, "%08X:\n", aInstruction.uAddress);

		if(aInstruction.bIsJumpTarget)
		{
			fprintf(pDestFile, "LABEL_%08X:\n", aInstruction.uAddress);
		}

		switch(aInstruction.eType)
		{
			case IT_ADDIU:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				if(aInstruction.eRS == R_ZERO)
				{
					fprintf(pDestFile, "%s = %d;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.iSI);
				}
				else if(aInstruction.eRT == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s %c= %d;\n", getRegVarName(aInstruction.eRT).c_str(), cSign, abs(aInstruction.iSI));
				}
				else
				{
					fprintf(pDestFile, "%s = %s %c %d;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				}
				break;
			}
			case IT_ADDU:
			{
				doIndent(pDestFile, uDepth);
				if((aInstruction.eRS == R_ZERO) && (aInstruction.eRT == R_ZERO))
				{
					fprintf(pDestFile, "%s = 0;\n", getRegVarName(aInstruction.eRD).c_str());
				}
				else if(aInstruction.eRT == R_ZERO)
				{
					fprintf(pDestFile, "%s = %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRS).c_str());
				}
				else if(aInstruction.eRD == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s += %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				else
				{
					fprintf(pDestFile, "%s = %s + %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				break;
			}
			case IT_AND:
			{
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRD == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s &= %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				else
				{
					fprintf(pDestFile, "%s = %s & %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				break;
			}
			case IT_ANDI:
			{
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRT == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s &= 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.uUI);
				}
				else
				{
					fprintf(pDestFile, "%s = %s & 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), aInstruction.uUI);
				}
				break;
			}
			case IT_BEQ:
			case IT_BEQL:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(%s == %s)\n", getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_BGEZ:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(((signed int)%s) >= 0)\n", getRegVarName(aInstruction.eRS).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_BGTZ:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(((signed int)%s) > 0)\n", getRegVarName(aInstruction.eRS).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_BLEZ:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(((signed int)%s) <= 0)\n", getRegVarName(aInstruction.eRS).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_BLTZ:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(((signed int)%s) < 0)\n", getRegVarName(aInstruction.eRS).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_BNE:
			case IT_BNEL:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "if(%s != %s)\n", getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				bBranchAllowed = true;
				break;
			}
			case IT_J:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "goto LABEL_%08X;\n", aInstruction.uJumpAddress);
				break;
			}
			case IT_JALR:
			{
				fprintf(pDestFile, "\n");
				doIndent(pDestFile, uDepth);
				M_ASSERT(aInstruction.eRD == R_RA);

				unsigned uValue = 0;
				unsigned uSymIdx;
				if((resolveRegisterValue(collInstList, itCurr, aInstruction.eRS, uValue)) && (Symbols::lookup(uValue, uSymIdx)))
				{
					fprintf(pDestFile, "%s = %s();\n\n", getRegVarName(R_V0).c_str(), Symbols::get(uSymIdx)->strName.c_str());									
				}
				else
				{
					fprintf(pDestFile, "/* FIXME: call UNKNOWN FUNCTION in %s (0x%08X); */\n\n", getRegVarName(aInstruction.eRS).c_str(), uValue);
				}
				break;
			}
			case IT_JR:
			{
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRS == R_RA)
				{
					fprintf(pDestFile, "return %s;\n", getRegVarName(R_V0).c_str());
				}
				else
				{
					fprintf(pDestFile, "/* FIXME: RET-CALL %s; */\n", getRegVarName(aInstruction.eRS).c_str());
				}
				break;
			}
			case IT_LB:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "%s = *((signed char *)(((unsigned int)(%s)) %c %d));\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				break;
			}
			case IT_LBU:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "%s = *((unsigned char *)(((unsigned int)(%s)) %c %d));\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				break;
			}
			case IT_LH:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "%s = *((signed short *)(((unsigned int)(%s)) %c %d));\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				break;
			}
			case IT_LHU:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "%s = *((unsigned short *)(((unsigned int)(%s)) %c %d));\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				break;
			}
			case IT_LUI:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = 0x%X0000;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.uUI);
				break;
			}
			case IT_LW:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "%s = *((unsigned int *)(((unsigned int)(%s)) %c %d));\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI));
				break;
			}
			case IT_MFC0:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = read_c0_status();\n", getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_MTC0:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "write_c0_status(%s);\n", getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_NOP:
				/* Ignore */
				break;
			case IT_OR:
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRD == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s |= %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				else
				{
					fprintf(pDestFile, "%s = %s | %s;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				}
				break;
			case IT_ORI:
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRS == R_ZERO)
				{
					fprintf(pDestFile, "%s = 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.uUI);
				}
				else if(aInstruction.eRT == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s |= 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.uUI);
				}
				else
				{
					fprintf(pDestFile, "%s = %s | 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), aInstruction.uUI);
				}
				break;
			case IT_SB:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "*((unsigned char *)(((unsigned int)(%s)) %c %d)) = %s & 0xFF;\n", getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI), getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_SH:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "*((unsigned short *)(((unsigned int)(%s)) %c %d)) = %s & 0xFF;\n", getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI), getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_SLL:
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRD == aInstruction.eRT)
				{
					fprintf(pDestFile, "%s <<= %d;\n", getRegVarName(aInstruction.eRD).c_str(), aInstruction.uSA);
				}
				else
				{
					fprintf(pDestFile, "%s = %s << %d;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str(), aInstruction.uSA);
				}
				break;
			case IT_SLTI:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = (((signed int)%s) < %d) ? 1 : 0;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), aInstruction.iSI);
				break;
			}
			case IT_SLTIU:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = (((unsigned int)%s) < %d) ? 1 : 0;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), aInstruction.iSI);
				break;
			}
			case IT_SLTU:
			{
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = (((unsigned int)%s) < %s) ? 1 : 0;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRS).c_str(), getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_SRA:
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = ((signed int)%s) >> %d;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str(), aInstruction.uSA);
				break;
			case IT_SRL:
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "%s = ((unsigned int)%s) >> %d;\n", getRegVarName(aInstruction.eRD).c_str(), getRegVarName(aInstruction.eRT).c_str(), aInstruction.uSA);
				break;
			case IT_SSNOP:
				/* Ignore */
				break;
			case IT_SW:
			{
				doIndent(pDestFile, uDepth);
				char cSign = aInstruction.iSI < 0 ? '-' : '+';
				fprintf(pDestFile, "*((unsigned int *)(((unsigned int)(%s)) %c %d)) = %s;\n", getRegVarName(aInstruction.eRS).c_str(), cSign, abs(aInstruction.iSI), getRegVarName(aInstruction.eRT).c_str());
				break;
			}
			case IT_XORI:
				doIndent(pDestFile, uDepth);
				if(aInstruction.eRT == aInstruction.eRS)
				{
					fprintf(pDestFile, "%s ^= 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), aInstruction.uUI);
				}
				else
				{
					fprintf(pDestFile, "%s = %s ^ 0x%X;\n", getRegVarName(aInstruction.eRT).c_str(), getRegVarName(aInstruction.eRS).c_str(), aInstruction.uUI);
				}
				break;
			default:
				doIndent(pDestFile, uDepth);
				fprintf(pDestFile, "/* TODO: %s */\n", getInstrName(aInstruction));
				break;
		}

		if(!bBranchAllowed)
		{
			M_ASSERT(aInstruction.collIfBranch.size() == 0);
			M_ASSERT(aInstruction.collElseBranch.size() == 0);
		}
		else
		{
			if(aInstruction.collIfBranch.size() > 0)
			{
				fprintf(pDestFile, "%s{\n", getIndentStr(uDepth).c_str());
				generateInstructionCode(pDestFile, aInstruction.collIfBranch, uDepth + 1);
				fprintf(pDestFile, "%s}\n", getIndentStr(uDepth).c_str());

				if(aInstruction.collElseBranch.size() > 0)
				{
					fprintf(pDestFile, "%selse\n", getIndentStr(uDepth).c_str());
					fprintf(pDestFile, "%s{\n", getIndentStr(uDepth).c_str());
					generateInstructionCode(pDestFile, aInstruction.collElseBranch, uDepth + 1);
					fprintf(pDestFile, "%s}\n", getIndentStr(uDepth).c_str());
				}

				fprintf(pDestFile, "\n");
			}
			else
			{
				M_ASSERT(false);
			}
		}

		itCurr++;
	}
}

void generateCode(FILE *pDestFile, const Function &aFunction)
{
	fprintf(pDestFile, "#include \"../mipsdec_helper.h\"\n\n");
	fprintf(pDestFile, "/* Optimization passes: %4d */\n", aFunction.uOptimizationPasses);
	fprintf(pDestFile, "/* Stack offset.......: %4d */\n", aFunction.uStackOffset);

	tParameter *pParams = getFunctionParameters(aFunction.strName);

	if(!pParams)
	{
		fprintf(pDestFile, "void %s(void)\n", aFunction.strName.c_str());
	}
	else
	{
		fprintf(pDestFile, "%s %s(", pParams[0].pInfo->pName, aFunction.strName.c_str());
		unsigned uIdx = 1;
		while(pParams[uIdx].pName)
		{
			fprintf(pDestFile, "%s%s%s %s%s", 
				(uIdx > 1) ? ", " : "",
				(pParams[uIdx].uFlags & TF_CONST) ? "const " : "",
				pParams[uIdx].pInfo->pName,
				(pParams[uIdx].uFlags & TF_BY_REFERENCE) ? "*" : "",
				pParams[uIdx].pName);
			uIdx++;
		}
		fprintf(pDestFile, ")\n");
	}

	fprintf(pDestFile, "{\n");
	generateInstructionCode(pDestFile, aFunction.m_collInstList, 1);
	fprintf(pDestFile, "}\n");
}
