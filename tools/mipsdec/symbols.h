#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <vector>
#include <string>

typedef struct {
	unsigned uAddress;
	char cType;
	std::string strName;
} tSymbolEntry;

typedef std::vector<tSymbolEntry> tSymList;

bool parseSymFile(const std::string &strSymFile, tSymList &collSymList);
bool lookupSymbol(const std::string &strSymName, const tSymList &collSymList, unsigned &uSymIdx);

#endif
