#ifndef ELFBINARYLOADER_H
#define ELFBINARYLOADER_H

#ifndef LOADERBASE_H
#include "LoaderBase.h"
#endif

#ifndef ELFSPEC_H
#include "ElfSpec.h"
#endif

#include <vector>
#include <map>

class ElfSectionHeader
{
public:
	typedef std::map<size_t, std::string> tStringTable;

	ElfSectionHeader(const Elf32_Shdr &aRawSectionHeader)
	{
		m_aSectionHeader = aRawSectionHeader;
	}

	const Elf32_Shdr &getHeader() { return m_aSectionHeader; }
	void clearStrings() { return m_collStringTable.clear(); }
	size_t getNumStrings() { return m_collStringTable.size(); }
	void addString(size_t uOffset, const std::string &strContent) { m_collStringTable[uOffset] = strContent; }
	bool getString(size_t uOffset, std::string &strContent)
	{
		tStringTable::iterator it = m_collStringTable.find(uOffset);

		if(it == m_collStringTable.end())
		{
			return false;
		}

		strContent = it->second;
		return true;
	}

private:
	Elf32_Shdr m_aSectionHeader;
	tStringTable m_collStringTable;
};

class ElfBinaryLoader : public LoaderBase
{
public:
	ElfBinaryLoader();

	bool loadFile(const std::string &strFileName);
	eArchType getArchType() { return m_enArchType; }
	bool getCodeSectionByName(const std::string &strName, tCollData &collData);
	bool getCodeSectionByAddress(unsigned uAddress, tCollData &collData);
	bool getMainCodeSection(tCollData &collData) { return getCodeSectionByName(".text", collData); }

private:
	bool parseElfHeader();
	bool parseSectionHeaders(unsigned uOffset, unsigned uNumSections, unsigned uNumBytesPerSection);
	bool postProcessSectionHeaders();

	bool readStringTable(ElfSectionHeader &aSectionHeader);
	std::string getStringTableEntry(unsigned uOffset);

	std::vector<unsigned char> m_collBinaryData;
	std::vector<ElfSectionHeader> m_collSectionHeaders;
	eArchType m_enArchType;
	bool m_bHaveStringTable;
	unsigned m_uStringTableSectionIdx;
};

#endif