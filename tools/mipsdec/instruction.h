#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <vector>
#include "register.h"
#include "symbols.h"

typedef enum {
	IF_UNKNOWN,
	IF_NOARG,
	IF_RSRTRD,
	IF_RSRTSI,
	IF_RTSI,
	IF_RSSI,
	IF_RSRTUI,
	IF_RTUI,
	IF_RSRD,
	IF_RS,
	IF_RTRDSEL,
} tInstructionFormat;

typedef enum {
	IT_ADDIU,		// No delay slot
	IT_ADDU,		// No delay slot
	IT_AND,			// No delay slot
	IT_ANDI,		// No delay slot
	IT_BEQ,			// Delay slot
	IT_BLTZ,		// Delay slot
	IT_BNE,			// Delay slot
	IT_BNEL,
	IT_J,				// Delay slot
	IT_JALR,		// Delay slot
	IT_JALR_HB,	// Delay slot
	IT_JR,			// Delay slot
	IT_JR_HB,		// Delay slot
	IT_LUI,			// No delay slot
	IT_LW,			// No delay slot
	IT_MFC0,		// No delay slot
	IT_MTC0,		// No delay slot
	IT_NOP,			// No delay slot
	IT_OR,			// No delay slot
	IT_ORI,			// No delay slot
	IT_SLL,			// No delay slot
	IT_SLTI,		// No delay slot
	IT_SLTIU,		// No delay slot
	IT_SSNOP,		// No delay slot
	IT_SW,			// No delay slot
	IT_XORI,		// No delay slot
} tInstructionType;

class Instruction;
typedef std::vector<Instruction> tInstList;

class Instruction
{
public:
	Instruction(void) :
			bDelaySlotReordered(false)
	{
	}

	void encodeAbsoluteJump(unsigned uAddress);
	void swap(Instruction &aOtherInstruction, bool bSwapAddress);
	bool parse(unsigned uInstructionData, unsigned uInstructionAddress);
	bool modifiesRegister(tRegister eRegister) const;
	void encodeRegisterMove(tRegister eSrcRegister, tRegister eDstRegister);

	unsigned uAddress;
	tInstructionType eType;
	tInstructionFormat eFormat;
	tRegister eRS;
	tRegister eRT;
	tRegister eRD;
	unsigned short uUI;
	signed short iSI;
	unsigned char uSEL;
	unsigned int uJumpAddress;

	tInstList collIfBranch;
	tInstList collElseBranch;

	bool bDelaySlotReordered;
	bool bIsJumpTarget;
	
private:
	void setDefaults(void);
	void decodeNOARG(void);
	void decodeRTRDSEL(unsigned uInstructionData);
	void decodeRTUI(unsigned uInstructionData);
	void decodeRSRTRD(unsigned uInstructionData);
	void decodeRSRD(unsigned uInstructionData);
	void decodeRSRTSI(unsigned uInstructionData);
	void decodeRSSI(unsigned uInstructionData);
	void decodeRSRTUI(unsigned uInstructionData);
	void decodeRS(unsigned uInstructionData);
};

bool parseFunction(const std::string &strFuncName, const std::string &strBinFile, tInstList &collInstList);
void updateJumpTargets(tInstList &collInstList);
bool resolveRegisterValue(const tInstList &collInstList, unsigned uInstIdx, tRegister eRegister, unsigned &uRegisterVal);
void dumpInstructions(const tInstList &collInstList);
const char *getInstrName(const Instruction &aInstruction);

#endif
