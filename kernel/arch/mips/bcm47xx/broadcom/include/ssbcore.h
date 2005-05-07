#ifndef SSBCORE_H
#define SSBCORE_H

#define MAX_BUS_ID_LEN 64

/* forward declarations */
struct sb_bus;
struct list_head;	
struct pci_dev;	

struct sb_core {
	struct sb_bus *bus;
	u32 usage;
	u32 mem;
	unsigned char index;
	u32 code;
	u32 vendor;
	u32 rev;
};

struct sb_bus {
	struct list_head entry;
	char id[MAX_BUS_ID_LEN];
	int type;
	void *mem;
	struct pci_dev *pcidev;
	unsigned char core_count;
	struct sb_core cores[SB_MAXCORES];
	struct sb_core *selected;
	struct sb_core *gpio;
};

#define ssbregaddr(ssbc, type, offset, register)	(&(((type *)(ssbc->mem + offset))->register))
#define ssbconfregaddr(ssbc, register)			ssbregaddr(ssbc, sbconfig_t, SBCONFIGOFF, register)
#define ssbextifregaddr(ssbc, register)			ssbregaddr(ssbc, extifregs_t, 0, register)
#define ssbpciregaddr(ssbc, register)			ssbregaddr(ssbc, sbpciregs_t, 0, register)
#define ssbchipcregaddr(ssbc, register)			ssbregaddr(ssbc, chipcregs_t, 0, register)
#define ssbr32(ssbc, register)				readl(ssbconfregaddr(ssbc, register))
#define ssbw32(ssbc, register, value)			writel(value, ssbconfregaddr(ssbc, register))

extern struct sb_core *sb_core_attach(struct sb_bus *bus, int code, int index);
extern int sb_core_detach(struct sb_core *ssbc);
//extern void sb_core_disable(struct sb_core *ssbc);
//extern u32 sb_core_flags(struct sb_core *ssbc, u32 mask, u32 val);
//extern int sb_core_is_up(struct sb_core *ssbc);
//extern void sb_core_reset(struct sb_core *ssbc);
//extern void sb_core_tofixup(struct sb_core *ssbc);

extern int sb_bus_add(struct sb_bus *bus, int type, void *mem_phys, u32 mem_len, char *id, struct pci_dev *pcidev);
//extern u32 sb_bus_gpio_out(struct sb_bus *bus, u32 mask, u32 val);
//extern u32 sb_bus_gpio_outen(struct sb_bus *bus, u32 mask, u32 val);

#endif
