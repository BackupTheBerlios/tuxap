#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/types.h>

#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>

extern asmlinkage void bcm47xx_irq_handler(void);

void bcm47xx_irq_dispatch(struct pt_regs *regs)
{
	u32 cause;
	
	cause = read_c0_cause() & read_c0_status() & CAUSEF_IP;

#ifdef CONFIG_KERNPROF
	change_c0_status(cause | 1, 1);
#else
	clear_c0_status(cause);
#endif

	if (cause & CAUSEF_IP7)
		do_IRQ(7, regs);
	if (cause & CAUSEF_IP2)
		do_IRQ(2, regs);
	if (cause & CAUSEF_IP3)
		do_IRQ(3, regs);
	if (cause & CAUSEF_IP4)
		do_IRQ(4, regs);
	if (cause & CAUSEF_IP5)
		do_IRQ(5, regs);
	if (cause & CAUSEF_IP6)
		do_IRQ(6, regs);
}

void __init arch_init_irq(void)
{
	set_except_vector(0, bcm47xx_irq_handler);
	mips_cpu_irq_init(0);
}
