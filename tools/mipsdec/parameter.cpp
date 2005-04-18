#include "parameter.h"
#include "common.h"
#include <string>

#define DECLARE_BASIC_TYPE(size, name) { size, name	}

static const tTypeInfo aTypeVoid = DECLARE_BASIC_TYPE(0, "void");
static const tTypeInfo aTypeChar = DECLARE_BASIC_TYPE(1, "char");
static const tTypeInfo aTypeUnsignedChar = DECLARE_BASIC_TYPE(1, "unsigned char");
static const tTypeInfo aTypeInt = DECLARE_BASIC_TYPE(4, "int");
static const tTypeInfo aTypeUnsignedInt = DECLARE_BASIC_TYPE(4, "unsigned int");
static const tTypeInfo aTypeUnsignedShort = DECLARE_BASIC_TYPE(2, "unsigned short");
static const tTypeInfo aTypeUnsignedLong = DECLARE_BASIC_TYPE(4, "unsigned long");
static const tTypeInfo aTypeTodo = DECLARE_BASIC_TYPE(4, "void");

static const tTypeInfo aTypeListHead = { 0, "struct list_head",
	{
		{"next", TF_BY_REFERENCE, &aTypeListHead},
		{"prev", TF_BY_REFERENCE, &aTypeListHead},
	}
};

static const tTypeInfo aTypeResource = { 0, "struct resource",
	{
		{"name", TF_BY_REFERENCE | TF_CONST, &aTypeChar},
		{"start", TF_NONE, &aTypeUnsignedLong},
		{"end", TF_NONE, &aTypeUnsignedLong},
		{"flags", TF_NONE, &aTypeUnsignedLong},
		{"parent", TF_BY_REFERENCE, &aTypeResource},
		{"silbing", TF_BY_REFERENCE, &aTypeResource},
		{"child", TF_BY_REFERENCE, &aTypeResource},
	}
};

static const tTypeInfo aTypePciDeviceId = { 0, "struct pci_device_id",
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

extern const tTypeInfo aTypePciDev;

static const tTypeInfo aTypePciBus = { 0, "struct pci_bus",
	{
		{"node", TF_NONE, &aTypeListHead},
		{"parent", TF_BY_REFERENCE, &aTypePciBus},
		{"children", TF_NONE, &aTypeListHead},
		{"devices", TF_NONE, &aTypeListHead},
		{"self", TF_BY_REFERENCE, &aTypePciDev},
		{"resource", TF_BY_REFERENCE, &aTypeResource, 4},
		{"ops", TF_BY_REFERENCE, &aTypeTodo}, // struct pci_ops
		{"sysdata", TF_BY_REFERENCE, &aTypeVoid},
		{"procdir", TF_BY_REFERENCE, &aTypeTodo}, // struct proc_dir_entry
		{"number", TF_NONE, &aTypeUnsignedChar},
		{"primary", TF_NONE, &aTypeUnsignedChar},
		{"secondary", TF_NONE, &aTypeUnsignedChar},
		{"subordinate", TF_NONE, &aTypeUnsignedChar},
		{"name", TF_NONE, &aTypeUnsignedChar, 48},
		{"vendor", TF_NONE, &aTypeUnsignedShort},
		{"device", TF_NONE, &aTypeUnsignedShort},
		{"serial", TF_NONE, &aTypeUnsignedInt},
		{"pnpver", TF_NONE, &aTypeUnsignedChar},
		{"productver", TF_NONE, &aTypeUnsignedChar},
		{"checksum", TF_NONE, &aTypeUnsignedChar},
		{"pad1", TF_NONE, &aTypeUnsignedChar},
	}
};

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

static tFunctionParameters collFunctionParameters[] = 
{
#include "user_function_parameter.inc"
};

#define NUM_FUNCTION_PARAMETERS (sizeof(collFunctionParameters) / sizeof(collFunctionParameters[0]))

tParameter *getFunctionParameters(const std::string &strFunctionName)
{
	for(unsigned uIdx = 0; uIdx < NUM_FUNCTION_PARAMETERS; uIdx++)
	{
		if(strFunctionName == collFunctionParameters[uIdx].pName)
		{
			return collFunctionParameters[uIdx].collParameters;
		}
	}

	return NULL;
}