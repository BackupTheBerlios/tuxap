#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <vector>
#include "register.h"
#include "symbols.h"

typedef enum {
	IF_UNKNOWN,
	IF_NOARG,
	IF_RSRTRD,
	IF_RSRT,
	IF_RSRTSI,
	IF_RSSI,
	IF_RSRTUI,
	IF_RTUI,
	IF_RTRDSA,
	IF_RSRD,
	IF_RD,
	IF_RS,
	IF_RTRDSEL,
} tInstructionFormat;

typedef enum {
	IDS_NONE,
	IDS_CONDITIONAL,
	IDS_UNCONDITIONAL,
} tInstructionDelaySlot;

typedef enum {
	IC_BRANCH,
	IC_JUMP,
	IC_OTHER,
} tInstructionClass;

typedef enum {
	IT_ADDIU,		// No delay slot
	IT_ADDU,		// No delay slot
	IT_AND,			// No delay slot
	IT_ANDI,		// No delay slot
	IT_BEQ,			// Delay slot
	IT_BEQL,		// Conditional delay slot
	IT_BLTZ,		// Delay slot
	IT_BNE,			// Delay slot
	IT_BNEL,		// Conditional delay slot
	IT_MFHI,		// No delay slot
	IT_J,				// Delay slot
	IT_JALR,		// Delay slot
	IT_JALR_HB,	// Delay slot
	IT_JR,			// Delay slot
	IT_JR_HB,		// Delay slot
	IT_LBU,			// No delay slot
	IT_LH,			// No delay slot
	IT_LHU,			// No delay slot
	IT_LUI,			// No delay slot
	IT_LW,			// No delay slot
	IT_LWL,			// No delay slot
	IT_LWR,			// No delay slot
	IT_MFC0,		// No delay slot
	IT_MTC0,		// No delay slot
	IT_MULT,		// No delay slot
	IT_NOP,			// No delay slot
	IT_OR,			// No delay slot
	IT_ORI,			// No delay slot
	IT_SB,			// No delay slot
	IT_SH,			// No delay slot
	IT_SLL,			// No delay slot
	IT_SLTI,		// No delay slot
	IT_SLTIU,		// No delay slot
	IT_SLTU,		// No delay slot
	IT_SRA,			// No delay slot
	IT_SSNOP,		// No delay slot
	IT_SUBU,		// No delay slot
	IT_SW,			// No delay slot
	IT_SWL,			// No delay slot
	IT_SWR,			// No delay slot
	IT_XORI,		// No delay slot
} tInstructionType;

class Instruction;
typedef std::vector<Instruction> tInstList;

class Instruction
{
public:
	Instruction(void) :
			bDelaySlotReordered(false),
			bIgnoreJump(false)
	{
	}

	void encodeAbsoluteJump(unsigned uAddress);
	void swap(Instruction &aOtherInstruction, bool bSwapAddress);
	bool parse(unsigned uInstructionData, unsigned uInstructionAddress);
	bool modifiesRegister(tRegister eRegister) const;
	void encodeRegisterMove(tRegister eSrcRegister, tRegister eDstRegister);
	tInstructionDelaySlot getDelaySlotType(void);
	tInstructionClass getClassType(void);

	unsigned uAddress;
	tInstructionType eType;
	tInstructionFormat eFormat;
	tRegister eRS;
	tRegister eRT;
	tRegister eRD;
	unsigned short uUI;
	signed short iSI;
	unsigned char uSEL;
	unsigned char uSA;
	unsigned int uJumpAddress;

	tInstList collIfBranch;
	tInstList collElseBranch;

	bool bDelaySlotReordered;
	bool bIgnoreJump;
	bool bIsJumpTarget;
	
private:
	void setDefaults(void);
	void decodeNOARG(void);
	void decodeRTRDSEL(unsigned uInstructionData);
	void decodeRTUI(unsigned uInstructionData);
	void decodeRTRDSA(unsigned uInstructionData);
	void decodeRSRTRD(unsigned uInstructionData);
	void decodeRSRD(unsigned uInstructionData);
	void decodeRSRT(unsigned uInstructionData);
	void decodeRSRTSI(unsigned uInstructionData);
	void decodeRSSI(unsigned uInstructionData);
	void decodeRSRTUI(unsigned uInstructionData);
	void decodeRD(unsigned uInstructionData);
	void decodeRS(unsigned uInstructionData);
};

bool parseFunction(const std::string &strFuncName, const std::string &strBinFile, tInstList &collInstList);
void updateJumpTargets(tInstList &collInstList);
bool resolveRegisterValue(const tInstList &collInstList, unsigned uInstIdx, tRegister eRegister, unsigned &uRegisterVal);
void dumpInstructions(const tInstList &collInstList);
const char *getInstrName(const Instruction &aInstruction);

#endif
