#include "symbols.h"

bool parseSymFile(const std::string &strSymFile, tSymList &collSymList)
{
	collSymList.clear();

	FILE *pSymFile = fopen(strSymFile.c_str(), "r");
	
	if(!pSymFile)
	{
		printf("Can't open symbol file: %s\n", strSymFile.c_str());
		return false;
	}
	
	char pBuf[1024];
	
	while(fgets(pBuf, sizeof(pBuf), pSymFile))
	{
		std::string strSymLine = pBuf;
		
		if((strSymLine.length() < 12) || (strSymLine[8] != ' ') || (strSymLine[10] != ' '))
		{
			printf("Error in symbol line: %d (\"%s\")\n", collSymList.size(), strSymLine.c_str());
			return false;
		}
		
		while(strSymLine[strSymLine.length() - 1] == '\n')
		{
			strSymLine.erase(strSymLine.length() - 1, 1);
		}
		
		std::string strSymAddr = strSymLine.substr(0, 8);
		strSymLine.erase(0, 9);
		std::string strSymType = strSymLine.substr(0, 1);
		strSymLine.erase(0, 2);
		
		tSymbolEntry aSymEntry;
		
		aSymEntry.strName = strSymLine;
		aSymEntry.cType = strSymType[0];
		aSymEntry.uAddress = strtoul(strSymAddr.c_str(), NULL, 16);

		collSymList.push_back(aSymEntry);

		//printf("%s:%s:%s\n", strSymAddr.c_str(), strSymType.c_str(), strSymLine.c_str());
	}
	
	fclose(pSymFile);
	
	return true;

}

bool lookupSymbol(const std::string &strSymName, const tSymList &collSymList, unsigned &uSymIdx)
{
	unsigned uNumSymbols = collSymList.size();

	for(unsigned uIdx = 0; uIdx < uNumSymbols; uIdx++)
	{
		if(collSymList[uIdx].strName == strSymName)
		{
			uSymIdx = uIdx;
			return true;
		}
	}
	
	return false;
}
