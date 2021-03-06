#ifndef _ASM_X86_64_TOPOLOGY_H
#define _ASM_X86_64_TOPOLOGY_H

#include <linux/config.h>

#ifdef CONFIG_DISCONTIGMEM

#include <asm/mpspec.h>
#include <asm/bitops.h>

/* Map the K8 CPU local memory controllers to a simple 1:1 CPU:NODE topology */

extern cpumask_t cpu_online_map;

extern unsigned char cpu_to_node[];
extern cpumask_t     node_to_cpumask[];
extern cpumask_t pci_bus_to_cpumask[];

#define cpu_to_node(cpu)		(cpu_to_node[cpu])
#define parent_node(node)		(node)
#define node_to_first_cpu(node) 	(__ffs(node_to_cpumask[node]))
#define node_to_cpumask(node)		(node_to_cpumask[node])

static inline cpumask_t __pcibus_to_cpumask(int bus)
{
	cpumask_t busmask = pci_bus_to_cpumask[bus];
	cpumask_t online = cpu_online_map;
	cpumask_t res;
	cpus_and(res, busmask, online);
	return res;
}
/* broken generic file uses #ifndef later on this */
#define pcibus_to_cpumask(bus) __pcibus_to_cpumask(bus)

#define NODE_BALANCE_RATE 30	/* CHECKME */ 

#endif

#include <asm-generic/topology.h>

#endif
