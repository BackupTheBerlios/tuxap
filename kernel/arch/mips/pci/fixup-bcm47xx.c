#include <linux/init.h>
#include <linux/pci.h>

int __init pcibios_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	u8 irq;
	
	if (dev->bus->number == 1)
		return 2;

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	return irq + 2;
}

struct pci_fixup pcibios_fixups[] __initdata = {
	{ 0 }
};
