/*
 * Flash mapping for BCM947XX boards
 *
 * Copyright (C) 2001 Broadcom Corporation
 *
 * $Id: bcm47xx-flash.c,v 1.1 2004/10/21 07:18:31 jolt Exp $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>

//#define WINDOW_ADDR 0x1fc00000
#define WINDOW_ADDR 0x1c000000
#define WINDOW_SIZE (0x400000*2)
#define BUSWIDTH 2

static struct mtd_info *bcm947xx_mtd;

static void bcm947xx_map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
#define MIPS_MEMCPY_ALIGN 4
	map_word ret;
	ssize_t transfer;
	ssize_t done = 0;
	if ((len >= MIPS_MEMCPY_ALIGN) && (!(from & (MIPS_MEMCPY_ALIGN - 1))) && (!(((unsigned int)to & (MIPS_MEMCPY_ALIGN - 1))))) {
		done = len & ~(MIPS_MEMCPY_ALIGN - 1);
		memcpy_fromio(to, map->virt + from, done);
	}
	while (done < len) {
		ret = map->read(map, from + done);
		transfer = len - done;
		if (transfer > map->bankwidth)
			transfer = map->bankwidth;
		memcpy((void *)((unsigned long)to + done), &ret.x[0], transfer);
		done += transfer;
	}
}

static struct map_info bcm947xx_map = {
	name: "Physically mapped flash",
	size: WINDOW_SIZE,
	bankwidth: BUSWIDTH,
	phys: WINDOW_ADDR,
};

#define SECTORS *64*1024

#ifdef CONFIG_MTD_PARTITIONS
#if 0
static struct mtd_partition bcm947xx_parts[] = {
// 64 - 4 - 14 - 1 = 45 = 8 + 37
	{ name: "pmon",	offset: 0, size: 4 SECTORS, mask_flags: MTD_WRITEABLE },
	{ name: "linux", offset: MTDPART_OFS_APPEND, size: 14 SECTORS },
	{ name: "rescue", offset: MTDPART_OFS_APPEND, size: 8 SECTORS },
	{ name: "rootfs", offset: MTDPART_OFS_APPEND, size: 37 SECTORS },
	{ name: "nvram", offset: MTDPART_OFS_APPEND, size: 1 SECTORS, mask_flags: MTD_WRITEABLE },
};
#else
static struct mtd_partition bcm947xx_parts[] = {
 	{ name: "cfe",
           offset:  0,
           size:  384*1024,
           mask_flags: MTD_WRITEABLE
         },
 	{ name: "config",
           offset:  MTDPART_OFS_APPEND,
           size:  128*1024
         },
 	{ name: "linux",
           offset:  MTDPART_OFS_APPEND,
           size:  3*1024*1024
         },
 	{ name: "jffs",
           offset: MTDPART_OFS_APPEND,
           size: (8*1024*1024)-((384*1024)+(128*1024)+(3*1024*1024)+(128*1024)),
         },
 	{ name: "nvram",
           offset: MTDPART_OFS_APPEND,
           size: 128*1024,
           mask_flags: MTD_WRITEABLE
         },
};
#endif
#endif

int __init init_bcm947xx_map(void)
{
	bcm947xx_map.virt = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!bcm947xx_map.virt) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	simple_map_init(&bcm947xx_map);
	
	bcm947xx_map.copy_from = bcm947xx_map_copy_from;
	
	if (!(bcm947xx_mtd = do_map_probe("cfi_probe", &bcm947xx_map))) {
		printk("Failed to do_map_probe\n");
		iounmap((void *)bcm947xx_map.virt);
		return -ENXIO;
	}

	bcm947xx_mtd->owner = THIS_MODULE;

	printk(KERN_NOTICE "flash device: %x at %x\n", bcm947xx_mtd->size, WINDOW_ADDR);

#ifdef CONFIG_MTD_PARTITIONS
	return add_mtd_partitions(bcm947xx_mtd, bcm947xx_parts, sizeof(bcm947xx_parts)/sizeof(bcm947xx_parts[0]));
#else
	return 0;
#endif
}

void __exit cleanup_bcm947xx_map(void)
{
#ifdef CONFIG_MTD_PARTITIONS
	del_mtd_partitions(bcm947xx_mtd);
#endif
	map_destroy(bcm947xx_mtd);
	iounmap((void *)bcm947xx_map.virt);
}

module_init(init_bcm947xx_map);
module_exit(cleanup_bcm947xx_map);
