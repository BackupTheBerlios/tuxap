#include "ElfBinaryLoader.h"
#include "X86RTLGenerator.h"

int main(int argc, char *argv[])
{
	ElfBinaryLoader aElfLoader;
	LoaderBase &aLoader = aElfLoader;

	if(!aLoader.loadFile("D:\\Dokumente und Einstellungen\\Florian Schirmer\\Desktop\\rec2\\gp.o"))
	{
		return -1;
	}

	LoaderBase::tCollData collData;

	if(!aLoader.getMainCodeSection(collData))
	{
		return -2;
	}

	X86RTLGenerator aX86RTLGenerator;
	RTLGeneratorBase *pRTLGenerator = NULL;

	switch(aLoader.getArchType())
	{
		case LoaderBase::ARCH_TYPE_X86:
			pRTLGenerator = &aX86RTLGenerator;
			break;
		//case LoaderBase::ARCH_TYPE_MIPS32:
		//	break;
		default:
			return -3;
	}

	RTLGeneratorBase::tCollRTLOps collRTLOps;

	if(!pRTLGenerator->generateRTL(collData, collRTLOps))
	{
		return -4;
	}

	if(!pRTLGenerator->dumpRTL(collRTLOps))
	{
		return -5;
	}

	return 0;
}

