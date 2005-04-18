#include "parameter.h"
#include "common.h"

#define DECLARE_BASIC_TYPE(size, name) { size, name	}

const tTypeInfo aTypeInt = DECLARE_BASIC_TYPE(4, "int");
const tTypeInfo aTypeUnsignedInt = DECLARE_BASIC_TYPE(4, "unsigned int");
const tTypeInfo aTypeUnsignedShort = DECLARE_BASIC_TYPE(2, "unsigned short");
const tTypeInfo aTypeUnsignedLong = DECLARE_BASIC_TYPE(4, "unsigned long");

const tTypeInfo aTypePciDevice = { 0, "struct pci_device_id",
	{
		{"vendor", TF_NONE, &aTypeUnsignedInt},
		{"device", TF_NONE, &aTypeUnsignedInt},
		{"subvendor", TF_NONE, &aTypeUnsignedInt},
		{"subdevice", TF_NONE, &aTypeUnsignedInt},
		{"class", TF_NONE, &aTypeUnsignedInt},
		{"class_mask", TF_NONE, &aTypeUnsignedInt},
		{"driver_data", TF_NONE, &aTypeUnsignedLong},
	}
};

const tTypeInfo aTypeListHead = { 0, "struct list_head",
	{
		{"next", TF_BY_REFERENCE, &aTypeListHead},
		{"prev", TF_BY_REFERENCE, &aTypeListHead},
	}
};

extern const tTypeInfo aTypePciDev;

const tTypeInfo aTypePciBus = { 0, "struct pci_bus",
	{
		{"node", TF_NONE, &aTypeListHead},
		{"parent", TF_BY_REFERENCE, &aTypePciBus},
		{"children", TF_NONE, &aTypeListHead},
		{"devices", TF_NONE, &aTypeListHead},
		{"self", TF_BY_REFERENCE, &aTypePciDev},
	}
};

#if 0
        struct resource *resource[4];   /* address space routed to this bus */

        struct pci_ops  *ops;           /* configuration access functions */
        void            *sysdata;       /* hook for sys-specific extension */
        struct proc_dir_entry *procdir; /* directory entry in /proc/bus/pci */

        unsigned char   number;         /* bus number */
        unsigned char   primary;        /* number of primary bridge */
        unsigned char   secondary;      /* number of secondary bridge */
        unsigned char   subordinate;    /* max number of subordinate buses */

        char            name[48];
        unsigned short  vendor;
        unsigned short  device;
        unsigned int    serial;         /* serial number */
        unsigned char   pnpver;         /* Plug & Play version */
        unsigned char   productver;     /* product version */
        unsigned char   checksum;       /* if zero - checksum passed */
        unsigned char   pad1;
};
#endif

const tTypeInfo aTypePciDev = { 0, "struct pci_dev",
	{
		{"global_list", TF_NONE, &aTypeListHead},
		{"bus_list", TF_NONE, &aTypeListHead},
	}
};

#if 0
struct pci_dev {
        struct list_head global_list;   /* node in list of all PCI devices */
        struct list_head bus_list;      /* node in per-bus list */
        struct pci_bus  *bus;           /* bus this device is on */
        struct pci_bus  *subordinate;   /* bus this device bridges to */

        void            *sysdata;       /* hook for sys-specific extension */
        struct proc_dir_entry *procent; /* device entry in /proc/bus/pci */

        unsigned int    devfn;          /* encoded device & function index */
        unsigned short  vendor;
        unsigned short  device;
        unsigned short  subsystem_vendor;
        unsigned short  subsystem_device;
        unsigned int    class;          /* 3 bytes: (base,sub,prog-if) */
        u8              hdr_type;       /* PCI header type (`multi' flag masked out) */
        u8              rom_base_reg;   /* which config register controls the ROM */

        struct pci_driver *driver;      /* which driver has allocated this device */
        void            *driver_data;   /* data private to the driver */
        u64             dma_mask;       /* Mask of the bits of bus address this
                                           device implements.  Normally this is
                                           0xffffffff.  You only need to change
                                           this if your device has broken DMA
                                           or supports 64-bit transfers.  */

        u32             current_state;  /* Current operating state. In ACPI-speak,
                                           this is D0-D3, D0 being fully functional,
                                           and D3 being off. */

        /* device is compatible with these IDs */
        unsigned short vendor_compatible[DEVICE_COUNT_COMPATIBLE];
        unsigned short device_compatible[DEVICE_COUNT_COMPATIBLE];

        /*
         * Instead of touching interrupt line and base address registers
         * directly, use the values stored here. They might be different!
         */
        unsigned int    irq;
        struct resource resource[DEVICE_COUNT_RESOURCE]; /* I/O and memory regions + expansion ROMs */
        struct resource dma_resource[DEVICE_COUNT_DMA];
        struct resource irq_resource[DEVICE_COUNT_IRQ];

        char            name[90];       /* device name */
        char            slot_name[8];   /* slot name */
        int             active;         /* ISAPnP: device is active */
        int             ro;             /* ISAPnP: read only */
        unsigned short  regs;           /* ISAPnP: supported registers */

        /* These fields are used by common fixups */
        unsigned short  transparent:1;  /* Transparent PCI bridge */

        int (*prepare)(struct pci_dev *dev);    /* ISAPnP hooks */
        int (*activate)(struct pci_dev *dev);
        int (*deactivate)(struct pci_dev *dev);
};
#endif
