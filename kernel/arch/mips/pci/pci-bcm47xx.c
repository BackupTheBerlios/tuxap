#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>

#include <asm/cpu.h>
#include <asm/io.h>

#include <typedefs.h>
#include <sbconfig.h>

extern struct pci_ops sb_pci_ops;

static struct resource sb_pci_mem_resource = {
	.name   = "SB PCI Memory resources",
	.start  = SB_ENUM_BASE,
	.end    = SB_ENUM_LIM - 1,
	.flags  = IORESOURCE_MEM,
};

static struct resource sb_pci_io_resource = {
	.name   = "SB PCI I/O resources",
	.start  = 0x100,
	.end    = 0x1FF,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller bcm47xx_sb_pci_controller = {
	.pci_ops        = &sb_pci_ops,
	.mem_resource	= &sb_pci_mem_resource,
	.io_resource	= &sb_pci_io_resource,
};

static struct resource ext_pci_mem_resource = {
	.name   = "Ext PCI Memory resources",
	.start  = SB_PCI_DMA,
//	.end    = 0x7FFFFFFF,
	.end    = 0x40FFFFFF,
	.flags  = IORESOURCE_MEM,
};

static struct resource ext_pci_io_resource = {
	.name   = "Ext PCI I/O resources",
	.start  = 0x200,
	.end    = 0x2FF,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller bcm47xx_ext_pci_controller = {
	.pci_ops        = &sb_pci_ops,
	.mem_resource	= &ext_pci_mem_resource,
	.io_resource	= &ext_pci_io_resource,
};

static int __init bcm47xx_pci_init(void)
{
	register_pci_controller(&bcm47xx_sb_pci_controller);
	register_pci_controller(&bcm47xx_ext_pci_controller);
	return 0;
}

early_initcall(bcm47xx_pci_init);
