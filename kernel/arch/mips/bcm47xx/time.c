#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/serial_reg.h>
#include <linux/interrupt.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/time.h>

//#include <typedefs.h>
//#include <bcmnvram.h>
//#include <sbconfig.h>
//#include <sbextif.h>
//#include <sbutils.h>

/* Global SB handle */
//extern void *bcm947xx_sbh;
//extern spinlock_t bcm947xx_sbh_lock;

/* Convenience */
//#define sbh bcm947xx_sbh
//#define sbh_lock bcm947xx_sbh_lock

//extern int panic_timeout;
//static int watchdog = 0;
//static u8 *mcr = NULL;

void __init
bcm47xx_time_init(void)
{
	unsigned int hz;
//	extifregs_t *eir;

	/*
	 * Use deterministic values for initial counter interrupt
	 * so that calibrate delay avoids encountering a counter wrap.
	 */
	write_c0_count(0);
	write_c0_compare(0xffff);

//	if (!(hz = sb_mips_clock(sbh)))
//		hz = 100000000;
	hz = 200 * 1000 * 1000;

//	printk("CPU: BCM%04x rev %d at %d MHz\n", sb_chip(sbh), sb_chiprev(sbh),
//	       (hz + 500000) / 1000000);

	/* Set MIPS counter frequency for fixed_rate_gettimeoffset() */
	mips_hpt_frequency = hz / 2;

#if 0
	/* Set watchdog interval in ms */
	watchdog = simple_strtoul(nvram_safe_get("watchdog"), NULL, 0);

	/* Set panic timeout in seconds */
	panic_timeout = watchdog / 1000;
	panic_timeout *= 10;

	/* Setup blink */
	if ((eir = sb_setcore(sbh, SB_EXTIF, 0))) {
		sbconfig_t *sb = (sbconfig_t *)((unsigned int) eir + SBCONFIGOFF);
		unsigned long base = EXTIF_CFGIF_BASE(sb_base(readl(&sb->sbadmatch1)));
		mcr = (u8 *) ioremap_nocache(base + UART_MCR, 1);
	}
#endif
}

void __init
bcm47xx_timer_setup(struct irqaction *irq)
{
	/* Enable the timer interrupt */
	setup_irq(7, irq);
}
