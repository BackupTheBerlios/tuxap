#include "instruction.h"
#include "register.h"
#include "common.h"
#include "symbols.h"

typedef enum
{
	RF_NONE,
	RF_RD,
	RF_RS,
	RF_RT,
} tResultField;

typedef struct
{
	unsigned uOpcode;
	unsigned uSpecialOpcode;
	unsigned uRegimmOpcode;
	const char *pName;
	tInstructionType eType;
	tInstructionFormat eFormat;
	tResultField eResultField;
	tInstructionDelaySlot eDelaySlot;
} tInstructionInfo;

#define DECLARE_OPCODE(type, format, result_field, delayslot) {~0, ~0, ~0, #type, IT_##type, format, result_field, delayslot}
#define DECLARE_REGULAR_OPCODE(opcode, type, format, result_field, delayslot) {opcode, ~0, ~0, #type, IT_##type, format, result_field, delayslot}
#define DECLARE_SPECIAL_OPCODE(type, format, result_field, delayslot) {0x00, ~0, ~0, #type, IT_##type, format, result_field, delayslot}
#define DECLARE_REGIMM_OPCODE(regdimmopcode, type, format, result_field, delayslot) {0x01, ~0, regdimmopcode, #type, IT_##type, format, result_field, delayslot}

static tInstructionInfo s_InstructionInfo[] = 
{
	DECLARE_OPCODE(ADDU,	IF_RSRTRD,	RF_RD,		IDS_NONE),
	DECLARE_REGULAR_OPCODE(0x09, ADDIU,	IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(AND,		IF_RSRTRD,	RF_RD,		IDS_NONE),
	DECLARE_REGULAR_OPCODE(0x0C, ANDI,	IF_RSRTUI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(BEQ,		IF_RSRTSI,	RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(BEQL,	IF_RSRTSI,	RF_NONE,	IDS_CONDITIONAL),
	DECLARE_REGIMM_OPCODE (0x00, BLTZ,	IF_RSSI,	RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(BNE,		IF_RSRTSI,	RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(BNEL,	IF_RSRTSI,	RF_NONE,	IDS_CONDITIONAL),
	DECLARE_OPCODE(J,		IF_NOARG,	RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(JALR,	IF_RSRD,	RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(JR,		IF_RS,		RF_NONE,	IDS_UNCONDITIONAL),
	DECLARE_OPCODE(LBU,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LH,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LHU,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LUI,		IF_RTUI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LW,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LWL,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(LWR,		IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(MFC0,	IF_RTRDSEL,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(MFHI,	IF_RD,		RF_RD,		IDS_NONE),
	DECLARE_OPCODE(MTC0,	IF_RTRDSEL,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(MULT,	IF_RSRT,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(NOP,		IF_NOARG,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(OR,		IF_RSRTRD,	RF_RD,		IDS_NONE),
	DECLARE_OPCODE(ORI,		IF_RSRTUI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(SB,		IF_RSRTSI,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(SH,		IF_RSRTSI,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(SLL,		IF_RTRDSA,	RF_RD,		IDS_NONE),
	DECLARE_OPCODE(SLTI,	IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(SLTIU,	IF_RSRTSI,	RF_RT,		IDS_NONE),
	DECLARE_OPCODE(SLTU,	IF_RSRTRD,	RF_RD,		IDS_NONE),
	DECLARE_OPCODE(SRA,		IF_RTRDSA,	RF_RD,		IDS_NONE),
	DECLARE_OPCODE(SSNOP,	IF_NOARG,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(SUBU,	IF_RSRTRD,	RF_RD,		IDS_NONE),
	DECLARE_OPCODE(SW,		IF_RSRTSI,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(SWL,		IF_RSRTSI,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(SWR,		IF_RSRTSI,	RF_NONE,	IDS_NONE),
	DECLARE_OPCODE(XORI,	IF_RSRTUI,	RF_RT,		IDS_NONE),
};

#define NUM_INSTRUCTION_INFO_ENTRIES (sizeof(s_InstructionInfo) / sizeof(s_InstructionInfo[0]))

static unsigned getInstructionInfoIdx(tInstructionType eType)
{
	unsigned uInfoIdx;

	for(uInfoIdx = 0; uInfoIdx < NUM_INSTRUCTION_INFO_ENTRIES; uInfoIdx++)
	{
		if(s_InstructionInfo[uInfoIdx].eType == eType)
		{
			return uInfoIdx;
		}
	}

	M_ASSERT(false);

	return 0;
}

static unsigned calcSymFileOffset(unsigned uSymAddress)
{
	return uSymAddress - 0x80000000;
}

void Instruction::encodeAbsoluteJump(unsigned uJAddress)
{
	M_ASSERT(uJAddress != 0);

	setDefaults();
	uAddress = 0xFFFFFFFF;
	eType = IT_J;
	eFormat = IF_NOARG;
	uJumpAddress = uJAddress;
	bDelaySlotReordered = true;
}

void Instruction::swap(Instruction &aOtherInstruction, bool bSwapAddress)
{
	Instruction aInstructionTmp = aOtherInstruction;
	aOtherInstruction = *this;
	*this = aInstructionTmp;
	
	if(!bSwapAddress)
	{
		unsigned uTmpAddress = uAddress;
		uAddress = aOtherInstruction.uAddress;
		aOtherInstruction.uAddress = uTmpAddress;
	}
}

bool decodeData(unsigned uInstructionData, unsigned &uInfoIdx)
{
	unsigned uOpcode = uInstructionData >> 26;

	for(uInfoIdx = 0; uInfoIdx < NUM_INSTRUCTION_INFO_ENTRIES; uInfoIdx++)
	{
		if(s_InstructionInfo[uInfoIdx].uOpcode == uOpcode)
		{
			switch(uOpcode)
			{
				case 0x00: /* SPECIAL instructions */
				{
					unsigned uSpecialOpcode = uInstructionData & 0x3F;

					if(s_InstructionInfo[uInfoIdx].uSpecialOpcode == uSpecialOpcode)
					{
					}
					break;
				}
				case 0x01: /* REGIMM instructions */
				{
					unsigned uRegimmOpcode = (uInstructionData >> 16) & 0x1F;

					if(s_InstructionInfo[uInfoIdx].uRegimmOpcode == uRegimmOpcode)
					{
						return true;
					}
					break;
				}
				default:
					return true;
			}
		}
	}

	return false;
}

bool Instruction::parse(unsigned uInstructionData, unsigned uInstructionAddress)
{
	uAddress = uInstructionAddress;

	unsigned uInfoIdx;
	if(decodeData(uInstructionData, uInfoIdx))
	{
		eType = s_InstructionInfo[uInfoIdx].eType;

		switch(s_InstructionInfo[uInfoIdx].eFormat)
		{
			default:
				M_ASSERT(false);
		}

		return true;
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
						decodeRTRDSA(uInstructionData);
						eType = IT_SLL;
					}
					break;
				}
				case 0x03:
					decodeRTRDSA(uInstructionData);
					eType = IT_SRA;
					break;
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
				case 0x10:
					decodeRD(uInstructionData);
					eType = IT_MFHI;
					break;
				case 0x18:
					decodeRSRT(uInstructionData);
					eType = IT_MULT;
					break;
				case 0x21:
					decodeRSRTRD(uInstructionData);
					eType = IT_ADDU;
					break;
				case 0x23:
					decodeRSRTRD(uInstructionData);
					eType = IT_SUBU;
					break;
				case 0x24:
					decodeRSRTRD(uInstructionData);
					eType = IT_AND;
					break;
				case 0x25:
					decodeRSRTRD(uInstructionData);
					eType = IT_OR;
					break;
				case 0x2B:
					decodeRSRTRD(uInstructionData);
					eType = IT_SLTU;
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
		case 0x02:
			decodeNOARG();
			eType = IT_J;
			uJumpAddress = ((uAddress + 4) & ~0xFFFFFFF) | ((uInstructionData & 0x3FFFFFF) << 2);
			break;
		case 0x04:
			decodeRSRTSI(uInstructionData);
			eType = IT_BEQ;
			uJumpAddress = uAddress + 4 + (iSI << 2);
			break;
		case 0x05:
			decodeRSRTSI(uInstructionData);
			eType = IT_BNE;
			uJumpAddress = uAddress + 4 + (iSI << 2);
			break;
		/*case 0x09:
			decodeRSRTSI(uInstructionData);
			eType = IT_ADDIU;
			break;*/
		case 0x0A:
			decodeRSRTSI(uInstructionData);
			eType = IT_SLTI;
			break;
		case 0x0B:
			decodeRSRTSI(uInstructionData);
			eType = IT_SLTIU;
			break;
		/*case 0x0C:
			decodeRSRTUI(uInstructionData);
			eType = IT_ANDI;
			break;*/
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
					decodeRTRDSEL(uInstructionData);
					eType = IT_MFC0;
					break;
				case 0x04:
					decodeRTRDSEL(uInstructionData);
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
		case 0x14:
			decodeRSRTSI(uInstructionData);
			eType = IT_BEQL;
			uJumpAddress = uAddress + 4 + (iSI << 2);
			break;
		case 0x15:
			decodeRSRTSI(uInstructionData);
			eType = IT_BNEL;
			uJumpAddress = uAddress + 4 + (iSI << 2);
			break;
		case 0x21:
			decodeRSRTSI(uInstructionData);
			eType = IT_LH;
			break;
		case 0x22:
			decodeRSRTSI(uInstructionData);
			eType = IT_LWL;
			break;
		case 0x23:
			decodeRSRTSI(uInstructionData);
			eType = IT_LW;
			break;
		case 0x24:
			decodeRSRTSI(uInstructionData);
			eType = IT_LBU;
			break;
		case 0x25:
			decodeRSRTSI(uInstructionData);
			eType = IT_LHU;
			break;
		case 0x26:
			decodeRSRTSI(uInstructionData);
			eType = IT_LWR;
			break;
		case 0x28:
			decodeRSRTSI(uInstructionData);
			eType = IT_SB;
			break;
		case 0x29:
			decodeRSRTSI(uInstructionData);
			eType = IT_SH;
			break;
		case 0x2A:
			decodeRSRTSI(uInstructionData);
			eType = IT_SWL;
			break;
		case 0x2B:
			decodeRSRTSI(uInstructionData);
			eType = IT_SW;
			break;
		case 0x2E:
			decodeRSRTSI(uInstructionData);
			eType = IT_SWR;
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

bool Instruction::modifiesRegister(tRegister eRegister) const
{
	M_ASSERT(eRegister != R_UNKNOWN);

	switch(s_InstructionInfo[getInstructionInfoIdx(eType)].eResultField)
	{
		case RF_NONE:
			return false;
		case RF_RD:
			M_ASSERT(eRD != R_UNKNOWN);
			return eRD == eRegister;
			break;
		case RF_RS:
			M_ASSERT(eRS != R_UNKNOWN);
			return eRS == eRegister;
			break;
		case RF_RT:
			M_ASSERT(eRT != R_UNKNOWN);
			return eRT == eRegister;
			break;
		default:
			M_ASSERT(false);
			return true;
	}
}

void Instruction::encodeRegisterMove(tRegister eSrcRegister, tRegister eDstRegister)
{
	M_ASSERT(eSrcRegister != R_UNKNOWN);
	M_ASSERT(eDstRegister != R_UNKNOWN);

	setDefaults();
	uAddress = 0xFFFFFFFF;
	eType = IT_ADDU;
	eFormat = IF_RSRTRD;
	eRS = eSrcRegister;
	eRT = R_ZERO;
	eRD = eDstRegister;
}

void Instruction::setDefaults(void)
{
	eFormat = IF_UNKNOWN;
	eRS = R_UNKNOWN;
	eRT = R_UNKNOWN;
	eRD = R_UNKNOWN;
	uUI = 0;
	iSI = 0;
	uSEL = 0;
	bDelaySlotReordered = false;
	collIfBranch.clear();
	collElseBranch.clear();
	uJumpAddress = 0;
	bIsJumpTarget = false;
}

void Instruction::decodeNOARG(void)
{
	setDefaults();
	eFormat = IF_NOARG;
}

void Instruction::decodeRTRDSA(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RTRDSA;
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
	uSA = (uInstructionData >> 6) & 0x1F;
}

void Instruction::decodeRTRDSEL(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RTRDSEL;
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
	//eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
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

void Instruction::decodeRSRT(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RSRT;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
	eRT = decodeRegister((uInstructionData >> 16) & 0x1F);
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

void Instruction::decodeRD(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RD;
	eRD = decodeRegister((uInstructionData >> 11) & 0x1F);
}

void Instruction::decodeRS(unsigned uInstructionData)
{
	setDefaults();
	eFormat = IF_RS;
	eRS = decodeRegister((uInstructionData >> 21) & 0x1F);
}

bool parseFunction(const std::string &strFuncName, const std::string &strBinFile, tInstList &collInstList)
{
	unsigned uSymIdx;

	if(!Symbols::lookup(strFuncName, uSymIdx))
	{
		printf("Unable to locate function %s\n", strFuncName.c_str());
		return false;
	}

	unsigned uInstructionCount;

	if((uSymIdx + 1) == Symbols::getCount())
	{
		M_ASSERT(false); // FIXME: TODO
	}
	else
	{
		uInstructionCount = (Symbols::get(uSymIdx + 1)->uAddress - Symbols::get(uSymIdx)->uAddress) / 4;
	}
	
	FILE *pBinFile = fopen(strBinFile.c_str(), "rb");
	
	if(!pBinFile)
	{
		printf("Can't open binary file: %s\n", strBinFile.c_str());
		return false;
	}
	
	fseek(pBinFile, calcSymFileOffset(Symbols::get(uSymIdx)->uAddress), SEEK_SET);
	
	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		unsigned uData = 0;

		for(unsigned uByteIdx = 0; uByteIdx < sizeof(uData); uByteIdx++)
		{
			unsigned char uByte;

			if(fread(&uByte, 1, sizeof(uByte), pBinFile) != sizeof(uByte))
			{
				printf("Short binary file read!\n");
				return false;
			}
			
			uData = (uData >> 8) | (uByte << 24);
		}
		
		Instruction aInstruction;

		aInstruction.eFormat = IF_UNKNOWN;

		if(!aInstruction.parse(uData, Symbols::get(uSymIdx)->uAddress + (4 * uInstructionIdx)))
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

static bool isJumpTarget(const tInstList &collInstList, unsigned uAddress)
{
	unsigned uInstructionCount = collInstList.size();

	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		if(collInstList[uInstructionIdx].uJumpAddress == uAddress)
		{
			return true;
		}

		if(isJumpTarget(collInstList[uInstructionIdx].collIfBranch, uAddress))
		{
			return true;
		}

		if(isJumpTarget(collInstList[uInstructionIdx].collElseBranch, uAddress))
		{
			return true;
		}
	}
	
	return false;
}

void updateJumpTargets(tInstList &collInstList)
{
	unsigned uInstructionCount = collInstList.size();

	for(unsigned uInstructionIdx = 0; uInstructionIdx < uInstructionCount; uInstructionIdx++)
	{
		collInstList[uInstructionIdx].bIsJumpTarget = isJumpTarget(collInstList, collInstList[uInstructionIdx].uAddress);

		updateJumpTargets(collInstList[uInstructionIdx].collIfBranch);
		updateJumpTargets(collInstList[uInstructionIdx].collElseBranch);
	}
}

bool resolveRegisterValue(const tInstList &collInstList, unsigned uInstIdx, tRegister eRegister, unsigned &uRegisterVal)
{
	bool bGotAbsoluteVal = false;
	unsigned uPos = uInstIdx;

	do
	{
		switch(collInstList[uPos].eType)
		{
			case IT_LUI:
				if(collInstList[uPos].eRT == eRegister)
				{
					uRegisterVal = collInstList[uPos].uUI << 16;
					bGotAbsoluteVal = true;
				}
				break;
			case IT_ADDU:
				if(collInstList[uPos].eRD == eRegister)
				{
					unsigned uVal1 = 0;

					if((collInstList[uPos].eRS != R_ZERO) && (!resolveRegisterValue(collInstList, uPos, collInstList[uPos].eRS, uVal1)))
					{
						return false;
					}

					unsigned uVal2 = 0;

					if((collInstList[uPos].eRT != R_ZERO) && (!resolveRegisterValue(collInstList, uPos, collInstList[uPos].eRT, uVal2)))
					{
						return false;
					}

					uRegisterVal = uVal1 + uVal2;

					return true;
				}
		}

		if(!bGotAbsoluteVal)
		{
			if(collInstList[uPos].bIsJumpTarget)
			{
				return false;
			}

			uPos--;
		}
	}
	while((!bGotAbsoluteVal) && (uPos > 0));
	
	if(!bGotAbsoluteVal)
	{
		return false;
	}
	
	/* Skip absolute set instruction */
	uPos++;

	while(uPos < uInstIdx)
	{
		const Instruction *pInstruction = &collInstList[uPos];

		switch(collInstList[uPos].eType)
		{
			case IT_ADDIU:
				if(collInstList[uPos].eRT == eRegister)
				{
					M_ASSERT(collInstList[uPos].eRS == eRegister);
					uRegisterVal += collInstList[uPos].iSI;
				}
				break;
			default:
			{
				if(collInstList[uPos].modifiesRegister(eRegister))
				{
					M_ASSERT(false);
					return false;
				}
				break;
			}
		}

		uPos++;
	}
	
	return true;
}

const char *getInstrName(const Instruction &aInstruction)
{
	return s_InstructionInfo[getInstructionInfoIdx(aInstruction.eType)].pName;
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
			case IF_RS:
				sprintf(pBuf, "\t%s", getRegName(collInstList[uInstructionIdx].eRS));
				strArg = pBuf;
				break;
			case IF_RTRDSEL:
				//sprintf(pBuf, "\t%s,%s,%d", getRegName(collInstList[uInstructionIdx].eRT), getRegName(collInstList[uInstructionIdx].eRD), collInstList[uInstructionIdx].uSEL);
				sprintf(pBuf, "\t%s,%d,%d", getRegName(collInstList[uInstructionIdx].eRT), collInstList[uInstructionIdx].eRD, collInstList[uInstructionIdx].uSEL);
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
