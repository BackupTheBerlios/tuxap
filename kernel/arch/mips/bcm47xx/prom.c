#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/pmon.h>

const char *get_system_type(void)
{
	return "Broadcom BCM47xx";
}

void __init prom_init(void)
{
	unsigned long mem;

        mips_machgroup = MACH_GROUP_BRCM;
        mips_machtype = MACH_BCM47XX;

	/* Figure out memory size by finding aliases */
	for (mem = (1 << 20); mem < (128 << 20); mem += (1 << 20)) {
		if (*(unsigned long *)((unsigned long)(prom_init) + mem) == 
		    *(unsigned long *)(prom_init))
			break;
	}

	add_memory_region(0, mem, BOOT_MEM_RAM);
}

unsigned long __init prom_free_prom_memory(void)
{
	return 0;
}
