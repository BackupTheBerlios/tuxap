#include "instruction.h"
#include "register.h"
#include "common.h"
#include "symbols.h"

static unsigned calcSymFileOffset(unsigned uSymAddress)
{
	return uSymAddress - 0x80000000;
}

void Instruction::encodeAbsoluteJump(void)
{
	setDefaults();
	eType = IT_J;
	eFormat = IF_UA;
	bDelaySlotReordered = true;
	//M_ASSERT(false); // FIXME: Complete!
}

void Instruction::swap(Instruction &aOtherInstruction)
{
	Instruction aInstructionTmp = aOtherInstruction;
	aOtherInstruction = *this;
	*this = aInstructionTmp;
}

bool Instruction::parse(unsigned uInstructionData, unsigned uAddress)
{
	uAddress = uAddress;

	if(uAddress == 0x8016bb64)
	{
		int i = 1;
	}

	unsigned uOpcode = uInstructionData >> 26;

	switch(uOpcode)
	{
		case 0x00: /* SPECIAL instructions */
		{
			unsigned uSpecialOpcode = uInstructionData & 0x3F;

			switch(uSpecialOpcode)
			{
				case 0x00:
				{
					if(((uInstructionData >> 11) & 0x7FFF) == 0)
					{
						unsigned uSllOpcode = (uInstructionData >> 6) & 0x1F;

						switch(uSllOpcode)
						{
							case 0x00:
								decodeNOARG();
								eType = IT_NOP;
								break;
							case 0x01:
								decodeNOARG();
								eType = IT_SSNOP;
								break;
							default:
								M_ASSERT(false);
								printf("Unkown SLLOPCODE: 0x%08X (I-Data: 0x%08X)\n", uSllOpcode, uInstructionData);
								return false;
						}
					}
					else
					{
						eType = IT_SLL;
					}
					break;
				}
				case 0x08:
					decodeRS(uInstructionData);
					if(uInstructionData & (1 << 10))
					{
						eType = IT_JR_HB;
					}
					else
					{
						eType = IT_JR;
					}
					break;
				case 0x09:
					decodeRSRD(uInstructionData);
					if(uInstructionData & (1 << 10))
					{
						eType = IT_JALR_HB;
					}
					else
					{
						eType = IT_JALR;
					}
					break;
				case 0x21:
					decodeRSRTRD(uInstructionData);
					eType = IT_ADDU;
					break;
				case 0x24:
					decodeRSRTRD(uInstructionData);
					eType = IT_AND;
					break;
				case 0x25:
					decodeRSRTRD(uInstructionData);
					eType = IT_OR;
					break;
				default:
				{
					unsigned uOp1 = uSpecialOpcode >> 3;
					unsigned uOp2 = uSpecialOpcode & 7;

					M_ASSERT(false);
					printf("Unkown SECIALOPCODE: 0x%08X (I-Data: 0x%08X)\n", uSpecialOpcode, uInstructionData);

					return false;
				}
			}
			break;
		}
		case 0x01: /* REGIMM instructions */
		{
			unsigned uRegimmOpcode = (uInstructionData >> 16) & 0x1F;

			switch(uRegimmOpcode)
			{
				case 0x00:
					decodeRSSI(uInstructionData);
					eType = IT_BLTZ;
					break;
				default:
				{
					unsigned uOp1 = uRegimmOpcode >> 3;
					unsigned uOp2 = uRegimmOpcode & 7;

					M_ASSERT(false);
					printf("Unkown REGIMMOPCODE: 0x%08X (I-Data: 0x%08X)\n", uRegimmOpcode, uInstructionData);
					return false;
				}
			}
			break;
		}
		case 0x02:
			decodeUA(uInstructionData);
			eType = IT_J;
			break;
		case 0x04:
			decodeRSRTSI(uInstructionData);
			eType = IT_BEQ;
			break;
		case 0x05:
			decodeRSRTSI(uInstructionData);
			eType = IT_BNE;
			break;
		case 0x09:
			decodeRSRTSI(uInstructionData);
			eType = IT_ADDIU;
			break;
		case 0x0A:
			decodeRSRTSI(uInstructionData);
			eType = IT_SLTI;
			break;
		case 0x0B:
			decodeRSRTSI(uInstructionData);
			eType = IT_SLTIU;
			break;
		case 0x0C:
			decodeRSRTUI(uInstructionData);
			eType = IT_ANDI;
			break;
		case 0x0D:
			// CHECKME: signed or not?
			decodeRSRTUI(uInstructionData);
			eType = IT_ORI;
			break;
		case 0x0E:
			decodeRSRTUI(uInstructionData);
			eType = IT_XORI;
			break;
		case 0x0F:
			decodeRTUI(uInstructionData);
			eType = IT_LUI;
			break;
		case 0x10: /* COP0 instructions */
		{
			unsigned uCop0Opcode = (uInstructionData >> 21) & 0x1F;

			switch(uCop0Opcode)
			{
				case 0x00:
					eType = IT_MFC0;
					break;
				case 0x04:
					eType = IT_MTC0;
					break;
				default:
				{
					unsigned uOp1 = uCop0Opcode >> 3;
					unsigned uOp2 = uCop0Opcode & 7;

					M_ASSERT(false);
					printf("Unkown COP0OPCODE: 0x%08X (I-Data: 0x%08X)\n", uCop0Opcode, uInstructionData);
					return false;
				}
			}
			break;
		}
		case 0x15:
			decodeRSRTSI(uInstructionData);
			eType = IT_BNEL;
			break;
		case 0x23:
			decodeRSRTSI(uInstructionData);
			eType = IT_LW;
			break;
		case 0x2B:
			decodeRSRTSI(uInstructionData);
			eType = IT_SW;
			break;
		default:
		{
			unsigned uOp1 = uOpcode >> 3;
			unsigned uOp2 = uOpcode & 7;

			M_ASSERT(false);
			printf("Unkown OPCODE: 0x%08X (I-Data: 0x%08X)\n", uOpcode, uInstructionData);
			return false;
		}
	}
	
	return true;
}

void Instruction::setDefaults(void)
{
	eFormat = IF_UNKNOWN;
	eRS = R_UNKNOWN;
	eRT = R_UNKNOWN;
	eRD = R_UNKNOWN;
	uUI = 0;
	iSI = 0;
	uUA = 0;
	uSEL = 0;
	bDelaySlotReordered = false;
	collIfBranch.clear();
	collElseBranch.clear();
}

void Instruction::decodeNOARG(void)
{
	setDefaults();
	eFormat = IF_NOARG;
}

void Instruction::decodeUA(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_UA;
	uUA = uInstructionData & 0x3FFFFFF;
}

void Instruction::decodeRTRDSEL(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RTRDSEL;
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
	uSEL = uInstructionData & 0x7;
}

void Instruction::decodeRTUI(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RTUI;
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	uUI = uInstructionData & 0xFFFF;
}

void Instruction::decodeRSRTRD(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSRTRD;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
}

void Instruction::decodeRSRD(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSRD;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
}

void Instruction::decodeRSRTSI(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSRTSI;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	iSI = uInstructionData & 0xFFFF;
}

void Instruction::decodeRSSI(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSSI;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	iSI = uInstructionData & 0xFFFF;
}

void Instruction::decodeRSRTUI(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSRTUI;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	uUI = uInstructionData & 0xFFFF;
}

void Instruction::decodeRS(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RS;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
}

bool parseFunction(const std::string &strFuncName, const tSymList &collSymList, const std::string &strBinFile, tInstList &collInstList)
{
	unsigned uSymIdx;

	if(!lookupSymbol(strFuncName, collSymList, uSymIdx))
	{
		printf("Unable to locate function %s\n", strFuncName.c_str());
		return false;
	}

	unsigned uInstructionCount;

	if((uSymIdx + 1) == collSymList.size())
	{
	}
	else
	{
		uInstructionCount = (collSymList[uSymIdx + 1].uAddress - collSymList[uSymIdx].uAddress) / 4;
	}
	
	FILE *pBinFile = fopen(strBinFile.c_str(), "rb");
	
	if(!pBinFile)
	{
		printf("Can't open binary file: %s\n", strBinFile.c_str());
		return false;
	}
	
	fseek(pBinFile, calcSymFileOffset(collSymList[uSymIdx].uAddress), SEEK_SET);
	
	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		unsigned uData;
		
		if(fread(&uData, 1, sizeof(uData), pBinFile) != sizeof(uData))
		{
			printf("Short binary file read!\n");
			return false;
		}
		
		Instruction aInstruction;

		aInstruction.eFormat = IF_UNKNOWN;

		if(!aInstruction.parse(uData, collSymList[uSymIdx].uAddress + (4 * uInstructionIdx)))
		{
			return false;
		}
		else
		{
			collInstList.push_back(aInstruction);
		}
	}
	
	fclose(pBinFile);

	return true;
}

const char *getInstrName(const Instruction &aInstruction)
{
	switch(aInstruction.eType)
	{
		case IT_ADDIU:
			return "addiu";
		case IT_ADDU:
			return "addu";
		case IT_AND:
			return "and";
		case IT_ANDI:
			return "andi";
		case IT_BEQ:
			return "beq";
		case IT_BLTZ:
			return "bltz";
		case IT_BNE:
			return "bne";
		case IT_BNEL:
			return "bnel";
		case IT_J:
			return "j";
		case IT_JALR:
			return "jalr";
		case IT_JALR_HB:
			return "jalr.hb";
		case IT_JR:
			return "jr";
		case IT_JR_HB:
			return "jr.hb";
		case IT_LUI:
			return "lui";
		case IT_LW:
			return "lw";
		case IT_MFC0:
			return "mfc0";
		case IT_MTC0:
			return "mtc0";
		case IT_NOP:
			return "nop";
		case IT_OR:
			return "or";
		case IT_ORI:
			return "ori";
		case IT_SLL:
			return "sll";
		case IT_SLTI:
			return "slti";
		case IT_SLTIU:
			return "sltiu";
		case IT_SSNOP:
			return "ssnop";
		case IT_SW:
			return "sw";
		case IT_XORI:
			return "xori";
		default:
			M_ASSERT(false);
			return "unkn";
	}
}

void dumpInstructions(const tInstList &collInstList)
{
	unsigned uInstructionCount = collInstList.size();

	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		char pBuf[1000];
		std::string strArg;

		Instruction aInstruction = collInstList[uInstructionIdx];

		switch(collInstList[uInstructionIdx].eFormat)
		{
			case IF_NOARG:
				strArg = "\t<NOARG>";
				break;
			case IF_RSRTRD:
				sprintf(pBuf, "\t%s,%s,%s", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRT), getRegName(collInstList[uInstructionIdx].eRD));
				strArg = pBuf;
				break;
			case IF_RSRD:
				sprintf(pBuf, "\t%s,%s", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRD));
				strArg = pBuf;
				break;
			case IF_RSRTUI:
				sprintf(pBuf, "\t%s,%s,0x%X", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].uUI);
				strArg = pBuf;
				break;
			case IF_RTUI:
				sprintf(pBuf, "\t%s,0x%X", getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].uUI);
				strArg = pBuf;
				break;
			case IF_RTSI:
				sprintf(pBuf, "\t%s,%d", getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].iSI);
				strArg = pBuf;
				break;
			case IF_RSSI:
				sprintf(pBuf, "\t%s,%d", getRegName(collInstList[uInstructionIdx].eRS), collInstList[uInstructionIdx].iSI);
				strArg = pBuf;
				break;
			case IF_RSRTSI:
				sprintf(pBuf, "\t%s,%s,%d", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].iSI);
				strArg = pBuf;
				break;
			case IF_UA:
				sprintf(pBuf, "\t0x%X", collInstList[uInstructionIdx].uUA);
				strArg = pBuf;
				break;
			case IF_RS:
				sprintf(pBuf, "\t%s", getRegName(collInstList[uInstructionIdx].eRS));
				strArg = pBuf;
				break;
			case IF_UNKNOWN:
				break;
			default:
				M_ASSERT(false);
				strArg = "UGH!";
				break;
		}

		printf("%08x:\t%08x\t%s%s\n", collInstList[uInstructionIdx].uAddress, /*collInstList[uInstructionIdx].uRaw*/0, getInstrName(collInstList[uInstructionIdx]), strArg.c_str());
	}
}
