#include "symbols.h"
#include "instruction.h"
#include "common.h"

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
