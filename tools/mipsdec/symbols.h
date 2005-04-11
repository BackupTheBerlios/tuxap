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

class Symbols
{
public:
	static bool parseSymFile(const std::string &strSymFile);
	static bool lookup(const std::string &strSymName, unsigned &uSymIdx);
	static bool lookup(unsigned uAddress, unsigned &uSymIdx);
	static size_t getCount(void);
	static const tSymbolEntry *get(unsigned uSymIdx);

private:
	static tSymList m_collSymbolList;
};

#endif
