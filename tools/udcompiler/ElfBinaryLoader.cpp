#include "ElfBinaryLoader.h"
#include <stdio.h>

ElfBinaryLoader::ElfBinaryLoader() :
	m_enArchType(ARCH_TYPE_UNKNOWN),
	m_bHaveStringTable(false)
{
}

bool ElfBinaryLoader::loadFile(const std::string &strFileName)
{
	FILE *pFileHandle = ::fopen(strFileName.c_str(), "rb");

	if(!pFileHandle)
	{
		return false;
	}

	::fseek(pFileHandle, 0, SEEK_END);
	long uFileSize = ::ftell(pFileHandle);
	::fseek(pFileHandle, 0, SEEK_SET);

	if(!uFileSize)
	{
		::fclose(pFileHandle);
		return false;
	}

	m_collBinaryData.resize(uFileSize);
	bool bResult = ::fread(&m_collBinaryData[0], 1, uFileSize, pFileHandle) == uFileSize;

	::fclose(pFileHandle);

	if(!bResult)
	{
		return false;
	}

	if(!parseElfHeader())
	{
		return false;
	}

	return true;
}

bool ElfBinaryLoader::getCodeSectionByName(const std::string &strName, tCollData &collData)
{
	for(unsigned uIdx = 0; uIdx < m_collSectionHeaders.size(); uIdx++)
	{
		const Elf32_Shdr &aHeader = m_collSectionHeaders[uIdx].getHeader();

		if(getStringTableEntry(aHeader.sh_name) == strName)
		{
			collData.resize(aHeader.sh_size);

			for(unsigned uIdx = 0; uIdx < aHeader.sh_size; uIdx++)
			{
				collData[uIdx] = m_collBinaryData[aHeader.sh_offset + uIdx];
			}

			return true;
		}
	}

	return false;
}

bool ElfBinaryLoader::getCodeSectionByAddress(unsigned uAddress, tCollData &collData)
{
	return true;
}

bool ElfBinaryLoader::parseElfHeader()
{
	Elf32_Ehdr *pElfHeader;

	if(m_collBinaryData.size() < sizeof(*pElfHeader))
	{
		return false;
	}

	pElfHeader = reinterpret_cast<Elf32_Ehdr *>(&m_collBinaryData[0]);

	if((pElfHeader->e_ident[EI_MAG0] != 0x7F) || (pElfHeader->e_ident[EI_MAG1] != 'E') ||
		(pElfHeader->e_ident[EI_MAG2] != 'L') || (pElfHeader->e_ident[EI_MAG3] != 'F'))
	{
		return false;
	}

	switch(pElfHeader->e_machine)
	{
		case 3: //EM_386
			m_enArchType = ARCH_TYPE_X86;
			break;
		case 8: //EM_MIPS
			m_enArchType = ARCH_TYPE_MIPS32;
			break;
		default:
			return false;
	}

	if(!parseSectionHeaders(pElfHeader->e_shoff, pElfHeader->e_shnum, pElfHeader->e_shentsize))
	{
		return false;
	}

	if((pElfHeader->e_shstrndx != SHN_UNDEF) && (pElfHeader->e_shstrndx > m_collSectionHeaders.size()))
	{
		return false;
	}

	m_bHaveStringTable = pElfHeader->e_shstrndx != SHN_UNDEF;
	m_uStringTableSectionIdx = pElfHeader->e_shstrndx;

	if(!postProcessSectionHeaders())
	{
		return false;
	}

	return true;
}

bool ElfBinaryLoader::parseSectionHeaders(unsigned uOffset, unsigned uNumSections, unsigned uNumBytesPerSection)
{
	m_collSectionHeaders.clear();

	if((!uOffset) || (!uNumSections))
	{
		return true;
	}

	if(!uNumBytesPerSection)
	{
		return false;
	}

	if(uNumBytesPerSection < sizeof(m_collSectionHeaders))
	{
		return false;
	}

	if((uOffset + (uNumSections * uNumBytesPerSection)) > m_collBinaryData.size())
	{
		return false;
	}

	for(unsigned uIdx = 0; uIdx < uNumSections; uIdx++)
	{
		Elf32_Shdr *pSectionHeader = reinterpret_cast<Elf32_Shdr *>(&m_collBinaryData[uOffset + (uIdx * uNumBytesPerSection)]);

		if((pSectionHeader->sh_type == SHT_NOBITS) && ((pSectionHeader->sh_offset + pSectionHeader->sh_size) > m_collBinaryData.size()))
		{
			return false;
		}

		// TODO: Inspect and validate each section header

		if(pSectionHeader->sh_type == SHT_STRTAB)
		{
			ElfSectionHeader aSectionHeader(*pSectionHeader);

			if(!readStringTable(aSectionHeader))
			{
				return false;
			}

			m_collSectionHeaders.push_back(aSectionHeader);
		}
		else
		{
			m_collSectionHeaders.push_back(*pSectionHeader);
		}
	}

	return true;
}

bool ElfBinaryLoader::postProcessSectionHeaders()
{
	for(unsigned uIdx = 0; uIdx < m_collSectionHeaders.size(); uIdx++)
	{
		const Elf32_Shdr &aHeader = m_collSectionHeaders[uIdx].getHeader();

		unsigned uVal1 = aHeader.sh_name;
		size_t uVal2 = m_collSectionHeaders[m_uStringTableSectionIdx].getNumStrings();
		std::string strDummy;

		if((aHeader.sh_name) && ((!m_bHaveStringTable) || (!m_collSectionHeaders[m_uStringTableSectionIdx].getString(aHeader.sh_name, strDummy))))
		{
			return false;
		}
	}

	return true;
}

bool ElfBinaryLoader::readStringTable(ElfSectionHeader &aSectionHeader)
{
	aSectionHeader.clearStrings();

	const Elf32_Shdr &aHeader = aSectionHeader.getHeader();

	if(aHeader.sh_type != SHT_STRTAB)
	{
		return false;
	}

	if((m_collBinaryData[aHeader.sh_offset]) || (m_collBinaryData[aHeader.sh_offset + aHeader.sh_size - 1]))
	{
		return false;
	}

	size_t uOffset = 0;

	while(uOffset < aHeader.sh_size)
	{
		std::string strContent = reinterpret_cast<const char *>(&m_collBinaryData[aHeader.sh_offset + uOffset]);
		aSectionHeader.addString(uOffset, strContent);
		uOffset += strContent.length() + 1;
	}

	return true;
}

std::string ElfBinaryLoader::getStringTableEntry(unsigned uOffset)
{
	if(!m_bHaveStringTable)
	{
		return "";
	}

	std::string strEntry;

	if(!m_collSectionHeaders[m_uStringTableSectionIdx].getString(uOffset, strEntry))
	{
		return "";
	}

	return strEntry;
}
