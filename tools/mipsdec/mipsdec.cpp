#include "symbols.h"
#include "instruction.h"
#include "codegen.h"
#include "optimize.h"
#include "common.h"

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
	optimizeInstructions(collInstList);
	//dumpInstructions(collInstList);
	generateCode(stdout, collInstList);

	while(1)
	{
//		Sleep(100);
	}
	
	return 0;
}
