#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <asm/pci.h>

#include <typedefs.h>
#include <sbpci.h>

extern void *sbh;
//extern spinlock_t bcm47xx_sbh_lock;

static int
sb_pci_read_config(struct pci_bus *bus, unsigned int devfn,
		     int reg, int size, u32 *val)
{
	//unsigned long flags;
	int ret;


	//spin_lock_irqsave(&sbh_lock, flags);
	ret = sbpci_read_config(sbh, bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), reg, val, size);
	//spin_unlock_irqrestore(&sbh_lock, flags);

	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

static int
sb_pci_write_config(struct pci_bus *bus, unsigned int devfn,
		      int reg, int size, u32 val)
{
//	unsigned long flags;
	int ret;

//	spin_lock_irqsave(&sbh_lock, flags);
	ret = sbpci_write_config(sbh, bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), reg, &val, size);
//	spin_unlock_irqrestore(&sbh_lock, flags);
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

struct pci_ops sb_pci_ops = {
	.read = sb_pci_read_config,
	.write = sb_pci_write_config,
};
