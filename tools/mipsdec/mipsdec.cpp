#include "symbols.h"
#include "instruction.h"
#include "codegen.h"
#include "optimize.h"
#include "common.h"

#ifndef _WIN32
void __doFail(const char *pCondition, const char *pFile, const char *pFunction, unsigned int uLine)
{
	printf("Assertion (%s) failed in file: %s, function: %s, line %d\n", pCondition, pFile, pFunction, uLine);
}
#endif

int main(int argc, char **argv)
{
	if(argc != 4)
	{
		printf("Syntax: %s <function> <binary> <map>\n", argv[0]);
		return 1;
	}

	std::string strFunction = argv[1];
	std::string strBinaryFile = argv[2];
	std::string strMapFile = argv[3];

	if(!Symbols::parseSymFile(strMapFile))
	{
		return 0;
	}

	tInstList collInstList;
	
	parseFunction(strFunction, strBinaryFile, collInstList);
	//dumpInstructions(collInstList);
	resolveDelaySlots(collInstList);
	//dumpInstructions(collInstList);

	unsigned uCompletePassesDone = 0;

	while(uCompletePassesDone < 2)
	{
		do {
			updateJumpTargets(collInstList);
			//dumpInstructions(collInstList);
		} 
		while(optimizeInstructions(collInstList, uCompletePassesDone));

		uCompletePassesDone++;
	}

	updateJumpTargets(collInstList);
	//dumpInstructions(collInstList);

	FILE *pCodeFile = fopen("code.c", "w");
	generateCode(pCodeFile, strFunction, collInstList);
	fclose(pCodeFile);

	return 0;
}
