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
	IF_UA,
	IF_RS,
	IF_RTRDSEL,
} tInstructionFormat;

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

typedef struct {
	unsigned uAddress;
	unsigned uRaw;
	tInstructionType eType;
	tInstructionFormat eFormat;
	tRegister eRS;
	tRegister eRT;
	tRegister eRD;
	unsigned short uUI;
	signed short iSI;
	unsigned int uUA;
	unsigned char uSEL;
} tInstruction;

typedef std::vector<tInstruction> tInstList;

bool parseFunction(const std::string &strFuncName, const tSymList &collSymList, const std::string &strBinFile, tInstList &collInstList);
void dumpInstructions(const tInstList &collInstList);

#endif
