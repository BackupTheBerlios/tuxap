#include "symbols.h"
#include <windows.h>

typedef enum {
	IT_ADDIU,
	IT_ADDU,
	IT_AND,
	IT_ANDI,
	IT_BEQ,
	IT_BLTZ,
	IT_BNE,
	IT_BNEL,
	IT_J,
	IT_JALR,
	IT_JALR_HB,
	IT_JR,
	IT_JR_HB,
	IT_LI,
	IT_LUI,
	IT_LW,
	IT_MFC0,
	IT_MOVE,
	IT_MTC0,
	IT_NOP,
	IT_OR,
	IT_ORI,
	IT_SLL,
	IT_SLTI,
	IT_SLTIU,
	IT_SSNOP,
	IT_SW,
	IT_XORI,
} tInstructionType;

typedef enum {
	R_ZERO,
	R_A0,
	R_A1,
	R_A2,
	R_A3,
	R_AT,
	R_GP,
	R_RA,
	R_S0,
	R_S1,
	R_S2,
	R_S3,
	R_SP,
	R_V0,
	R_V1,
	R_UNKNOWN,
} tRegister;

typedef enum {
	IF_UNKNOWN,
	IF_RSRTRD,
	IF_RDRS,
	IF_RSRTSI,
	IF_RTSI,
	IF_RSRTUI,
	IF_RTUI,
} tInstructionFormat;

typedef struct {
	unsigned uAddress;
	unsigned uRaw;
	tInstructionType eType;
	tInstructionFormat eFormat;
	tRegister eRS;
	tRegister eRT;
	tRegister eRD;
	unsigned short uUI;
	signed short uSI;
} tInstruction;

typedef std::vector<tInstruction> tInstList;

unsigned calcSymFileOffset(unsigned uSymAddress)
{
	return uSymAddress - 0x80000000;
}

tRegister decodeRegister(unsigned uRawData)
{
	switch(uRawData)
	{
		case 0x00:
			return R_ZERO;
		case 0x01:
			return R_AT;
		case 0x02:
			return R_V0;
		case 0x03:
			return R_V1;
		case 0x04:
			return R_A0;
		case 0x05:
			return R_A1;
		case 0x06:
			return R_A2;
		case 0x07:
			return R_A3;
		case 0x10:
			return R_S0;
		case 0x11:
			return R_S1;
		case 0x12:
			return R_S2;
		case 0x13:
			return R_S3;
		case 0x1C:
			return R_GP;
		case 0x1D:
			return R_SP;
		case 0x1F:
			return R_RA;
		default:
			return R_UNKNOWN;
	}
}

void decodeRTUI(tInstruction &aInstruction)
{
	aInstruction.eFormat = IF_RTUI;
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.uUI = aInstruction.uRaw & 0xFFFF;
}

void decodeRSRTRD(tInstruction &aInstruction)
{
	aInstruction.eFormat = IF_RSRTRD;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.eRD = decodeRegister((aInstruction.uRaw >> 11) & 0x1F);
}

void decodeRSRTSI(tInstruction &aInstruction)
{
	aInstruction.eFormat = IF_RSRTSI;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.uSI = aInstruction.uRaw & 0xFFFF;
}

void decodeRSRTUI(tInstruction &aInstruction)
{
	aInstruction.eFormat = IF_RSRTUI;
	aInstruction.eRS = decodeRegister((aInstruction.uRaw >> 21) & 0x1F);
	aInstruction.eRT = decodeRegister((aInstruction.uRaw >> 16) & 0x1F);
	aInstruction.uUI = aInstruction.uRaw & 0xFFFF;
}

static uNumUnknownInstructions = 0;

void optimizeInstruction(tInstruction &aInstruction)
{
	switch(aInstruction.eType)
	{
		case IT_ADDU:
			if(aInstruction.eRT == R_ZERO)
			{
				aInstruction.eType = IT_MOVE;
				aInstruction.eFormat = IF_RDRS;
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

bool parseInstruction(unsigned uInstructionData, unsigned uAddress, tInstruction &aInstruction)
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
								aInstruction.eType = IT_NOP;
								break;
							case 0x01:
								aInstruction.eType = IT_SSNOP;
								break;
							default:
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

					uNumUnknownInstructions++;

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
					aInstruction.eType = IT_BLTZ;
					break;
				default:
				{
					unsigned uOp1 = uRegimmOpcode >> 3;
					unsigned uOp2 = uRegimmOpcode & 7;

					uNumUnknownInstructions++;

					printf("Unkown REGIMMOPCODE: 0x%08X (I-Data: 0x%08X)\n", uRegimmOpcode, uInstructionData);
					return false;
				}
			}
			break;
		}
		case 0x02:
			aInstruction.eType = IT_J;
			break;
		case 0x04:
			aInstruction.eType = IT_BEQ;
			break;
		case 0x05:
			aInstruction.eType = IT_BNE;
			break;
		case 0x09:
			decodeRSRTSI(aInstruction);
			aInstruction.eType = IT_ADDIU;
			break;
		case 0x0A:
			aInstruction.eType = IT_SLTI;
			break;
		case 0x0B:
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

					uNumUnknownInstructions++;

					printf("Unkown COP0OPCODE: 0x%08X (I-Data: 0x%08X)\n", uCop0Opcode, uInstructionData);
					return false;
				}
			}
			break;
		}
		case 0x15:
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

			uNumUnknownInstructions++;

			printf("Unkown OPCODE: 0x%08X (I-Data: 0x%08X)\n", uOpcode, uInstructionData);
			return false;
		}
	}
	
	return true;
}

const char *getRegName(tRegister eRegister)
{
	switch(eRegister)
	{
		case R_ZERO:
			return "0";
		case R_A0:
			return "a0";
		case R_A1:
			return "a1";
		case R_A2:
			return "a2";
		case R_A3:
			return "a3";
		case R_AT:
			return "at";
		case R_GP:
			return "gp";
		case R_RA:
			return "ra";
		case R_S0:
			return "s0";
		case R_S1:
			return "s1";
		case R_S2:
			return "s2";
		case R_S3:
			return "s3";
		case R_SP:
			return "sp";
		case R_V0:
			return "v0";
		case R_V1:
			return "v1";
		case R_UNKNOWN:
			return "<UNKN>";
		default:
			return "<DEF>";
	}
}

const char *getInstrName(const tInstruction &aInstruction)
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
			case IF_RSRTRD:
				sprintf(pBuf, "\t%s,%s,%s", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRT), getRegName(collInstList[uInstructionIdx].eRD));
				strArg = pBuf;
				break;
			case IF_RDRS:
				sprintf(pBuf, "\t%s,%s", getRegName(collInstList[uInstructionIdx].eRD), getRegName(collInstList[uInstructionIdx].eRS));
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
				sprintf(pBuf, "\t%s,%d", getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].uSI);
				strArg = pBuf;
				break;
			case IF_RSRTSI:
				sprintf(pBuf, "\t%s,%s,%d", getRegName(collInstList[uInstructionIdx].eRS), getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].uSI);
				strArg = pBuf;
				break;
			case IF_UNKNOWN:
				break;
			default:
				strArg = "UGH!";
				break;
		}

		printf("%08x:\t%08x\t%s%s\n", collInstList[uInstructionIdx].uAddress, collInstList[uInstructionIdx].uRaw, getInstrName(collInstList[uInstructionIdx]), strArg.c_str());
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

	printf("Done with %d/%d known/unknown instructions\n", collInstList.size(), uNumUnknownInstructions);

	return true;
}

int main(void)
{
	tSymList collSymList;

	if(!parseSymFile("System.map", collSymList))
	{
		return 0;
	}

	tInstList collInstList;
	
	parseFunction("testfunc", collSymList, "vmlinux", collInstList);
	dumpInstructions(collInstList);

	while(1)
	{
		Sleep(100);
	}
	
	return 0;
}
