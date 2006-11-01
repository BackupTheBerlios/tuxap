#ifndef ELFSPEC_H
#define ELFSPEC_H

typedef unsigned short Elf32_Half;
typedef unsigned int Elf32_Word;
typedef unsigned int Elf32_Addr;
typedef unsigned int Elf32_Off;

#define EI_MAG0 0 // File identification 
#define EI_MAG1 1 // File identification 
#define EI_MAG2 2 // File identification 
#define EI_MAG3 3 // File identification 
#define EI_CLASS 4 // File class 
#define EI_DATA 5 // Data encoding 
#define EI_VERSION 6 // File version 
#define EI_OSABI 7 // Operating system/ABI identification 
#define EI_ABIVERSION 8 // ABI version 
#define EI_PAD 9 // Start of padding bytes 
#define EI_NIDENT 16 // Size of e_ident[] 

typedef struct {
	unsigned char   e_ident[EI_NIDENT];
	Elf32_Half      e_type;
	Elf32_Half      e_machine;
	Elf32_Word      e_version;
	Elf32_Addr      e_entry;
	Elf32_Off       e_phoff;
	Elf32_Off       e_shoff;
	Elf32_Word      e_flags;
	Elf32_Half      e_ehsize;
	Elf32_Half      e_phentsize;
	Elf32_Half      e_phnum;
	Elf32_Half      e_shentsize;
	Elf32_Half      e_shnum;
	Elf32_Half      e_shstrndx;
} Elf32_Ehdr;

#define SHN_UNDEF 0 
#define SHN_LORESERVE 0xff00 
#define SHN_LOPROC 0xff00 
#define SHN_HIPROC 0xff1f 
#define SHN_LOOS 0xff20 
#define SHN_HIOS 0xff3f 
#define SHN_ABS 0xfff1 
#define SHN_COMMON 0xfff2 
#define SHN_HIRESERVE 0xffff 

#define SHT_NULL 0 
#define SHT_PROGBITS 1 
#define SHT_SYMTAB 2 
#define SHT_STRTAB 3 
#define SHT_RELA 4 
#define SHT_HASH 5 
#define SHT_DYNAMIC 6 
#define SHT_NOTE 7 
#define SHT_NOBITS 8 
#define SHT_REL 9 
#define SHT_SHLIB 10 
#define SHT_DYNSYM 11 
#define SHT_LOOS 0x60000000 
#define SHT_HIOS 0x6fffffff 
#define SHT_LOPROC 0x70000000 
#define SHT_HIPROC 0x7fffffff 
#define SHT_LOUSER 0x80000000 
#define SHT_HIUSER 0xffffffff 

typedef struct {
       Elf32_Word      sh_name;
       Elf32_Word      sh_type;
       Elf32_Word      sh_flags;
       Elf32_Addr      sh_addr;
       Elf32_Off       sh_offset;
       Elf32_Word      sh_size;
       Elf32_Word      sh_link;
       Elf32_Word      sh_info;
       Elf32_Word      sh_addralign;
       Elf32_Word      sh_entsize;
} Elf32_Shdr;

#endif