#include "instruction.h"
#include "register.h"
#include "common.h"
#include "symbols.h"

static unsigned calcSymFileOffset(unsigned uSymAddress)
{
	return uSymAddress - 0x80000000;
}

static void setDefaults(tInstruction &aInstruction)
{
	aInstruction.eFormat = IF_UNKNOWN;
	aInstruction.eRS = R_UNKNOWN;
	aInstruction.eRT = R_UNKNOWN;
	aInstruction.eRD = R_UNKNOWN;
	aInstruction.uUI = 0;
	aInstruction.iSI = 0;
	aInstruction.uUA = 0;
	aInstruction.uSEL = 0;
}

static void decodeNOARG(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_NOARG;
}

static void decodeUA(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_UA;
	aInstruction.uUA = aInstruction.uRaw & 0x3FFFFFF;
}

static void decodeRTRDSEL(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RTRDSEL;
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.eRD = decodeRegister((aInstruction.uRaw >> 11) & 0x1F);
	aInstruction.uSEL = aInstruction.uRaw & 0x7;
}

static void decodeRTUI(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RTUI;
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.uUI = aInstruction.uRaw & 0xFFFF;
}

static void decodeRSRTRD(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RSRTRD;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.eRD = decodeRegister((aInstruction.uRaw >> 11) & 0x1F);
}

static void decodeRSRD(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RSRD;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRD = decodeRegister((aInstruction.uRaw >> 11) & 0x1F);
}

static void decodeRSRTSI(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RSRTSI;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.iSI = aInstruction.uRaw & 0xFFFF;
}

static void decodeRSSI(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RSSI;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.iSI = aInstruction.uRaw & 0xFFFF;
}

static void decodeRSRTUI(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RSRTUI;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.uUI = aInstruction.uRaw & 0xFFFF;
}

static void decodeRS(tInstruction &aInstruction)
{
	setDefaults(aInstruction);
	aInstruction.eFormat = IF_RS;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
}

static bool parseInstruction(unsigned uInstructionData, unsigned uAddress, tInstruction &aInstruction)
{
	aInstruction.uAddress = uAddress;
	aInstruction.uRaw = uInstructionData;

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
								decodeNOARG(aInstruction);
								aInstruction.eType = IT_NOP;
								break;
							case 0x01:
								decodeNOARG(aInstruction);
								aInstruction.eType = IT_SSNOP;
								break;
							default:
								M_ASSERT(false);
								printf("Unkown SLLOPCODE: 0x%08X (I-Data: 0x%08X)\n", uSllOpcode, uInstructionData);
								return false;
						}
					}
					else
					{
						aInstruction.eType = IT_SLL;
					}
					break;
				}
				case 0x08:
					decodeRS(aInstruction);
					if(uInstructionData & (1 << 10))
					{
						aInstruction.eType = IT_JR_HB;
					}
					else
					{
						aInstruction.eType = IT_JR;
					}
					break;
				case 0x09:
					decodeRSRD(aInstruction);
					if(uInstructionData & (1 << 10))
					{
						aInstruction.eType = IT_JALR_HB;
					}
					else
					{
						aInstruction.eType = IT_JALR;
					}
					break;
				case 0x21:
					decodeRSRTRD(aInstruction);
					aInstruction.eType = IT_ADDU;
					break;
				case 0x24:
					decodeRSRTRD(aInstruction);
					aInstruction.eType = IT_AND;
					break;
				case 0x25:
					decodeRSRTRD(aInstruction);
					aInstruction.eType = IT_OR;
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
					decodeRSSI(aInstruction);
					aInstruction.eType = IT_BLTZ;
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
			decodeUA(aInstruction);
			aInstruction.eType = IT_J;
			break;
		case 0x04:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_BEQ;
			break;
		case 0x05:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_BNE;
			break;
		case 0x09:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_ADDIU;
			break;
		case 0x0A:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_SLTI;
			break;
		case 0x0B:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_SLTIU;
			break;
		case 0x0C:
			decodeRSRTUI(aInstruction);
			aInstruction.eType = IT_ANDI;
			break;
		case 0x0D:
			// CHECKME: signed or not?
			decodeRSRTUI(aInstruction);
			aInstruction.eType = IT_ORI;
			break;
		case 0x0E:
			decodeRSRTUI(aInstruction);
			aInstruction.eType = IT_XORI;
			break;
		case 0x0F:
			decodeRTUI(aInstruction);
			aInstruction.eType = IT_LUI;
			break;
		case 0x10: /* COP0 instructions */
		{
			unsigned uCop0Opcode = (uInstructionData >> 21) & 0x1F;

			switch(uCop0Opcode)
			{
				case 0x00:
					aInstruction.eType = IT_MFC0;
					break;
				case 0x04:
					aInstruction.eType = IT_MTC0;
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
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_BNEL;
			break;
		case 0x23:
			decodeRSRTUI(aInstruction);
			aInstruction.eType = IT_LW;
			break;
		case 0x2B:
			decodeRSRTUI(aInstruction);
			aInstruction.eType = IT_SW;
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

static void optimizeInstruction(tInstruction &aInstruction)
{
	switch(aInstruction.eType)
	{
		case IT_ADDU:
			if(aInstruction.eRT == R_ZERO)
			{
				aInstruction.eType = IT_MOVE;
				aInstruction.eFormat = IF_RSRD;
			}
			break;
		case IT_ADDIU:
			if(aInstruction.eRS == R_ZERO)
			{
				aInstruction.eType = IT_LI;
				aInstruction.eFormat = IF_RTSI;
			}
			break;
		case IT_ORI:
			if(aInstruction.eRS == R_ZERO)
			{
				aInstruction.eType = IT_LI;
				aInstruction.eFormat = IF_RTUI;
			}
			break;
	}
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
		
		tInstruction aInstruction;

		aInstruction.eFormat = IF_UNKNOWN;

		if(!parseInstruction(uData, collSymList[uSymIdx].uAddress + (4 * uInstructionIdx), aInstruction))
		{
		//	return false;
		}
		else
		{
			optimizeInstruction(aInstruction);
			collInstList.push_back(aInstruction);
		}
	}
	
	fclose(pBinFile);

	return true;
}

static const char *getInstrName(const tInstruction &aInstruction)
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
		case IT_LI:
			return "li";
		case IT_LUI:
			return "lui";
		case IT_LW:
			return "lw";
		case IT_MFC0:
			return "mfc0";
		case IT_MOVE:
			return "move";
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

		tInstruction aInstruction = collInstList[uInstructionIdx];

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

		printf("%08x:\t%08x\t%s%s\n", collInstList[uInstructionIdx].uAddress, collInstList[uInstructionIdx].uRaw, getInstrName(collInstList[uInstructionIdx]), strArg.c_str());
	}
}
