#include "symbols.h"
#include "common.h"

tSymList Symbols::m_collSymbolList;

bool Symbols::parseSymFile(const std::string &strSymFile)
{
	m_collSymbolList.clear();

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
			printf("Error in symbol line: %d (\"%s\")\n", m_collSymbolList.size(), strSymLine.c_str());
			return false;
		}
		
		while((strSymLine[strSymLine.length() - 1] == '\n') || (strSymLine[strSymLine.length() - 1] == '\r'))
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

		m_collSymbolList.push_back(aSymEntry);
	}
	
	fclose(pSymFile);
	
	return true;

}

bool Symbols::lookup(const std::string &strSymName, unsigned &uSymIdx)
{
	unsigned uNumSymbols = m_collSymbolList.size();

	for(unsigned uIdx = 0; uIdx < uNumSymbols; uIdx++)
	{
		if(m_collSymbolList[uIdx].strName == strSymName)
		{
			uSymIdx = uIdx;
			return true;
		}
	}
	
	return false;
}

unsigned Symbols::getCount(void)
{
	return m_collSymbolList.size();
}

const tSymbolEntry *Symbols::get(unsigned uSymIdx)
{
	M_ASSERT(uSymIdx < getCount());
	return &m_collSymbolList[uSymIdx];
}
