#include <linux/init.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <asm/time.h>
#include <asm/reboot.h>

#include <typedefs.h>
#include <sbutils.h>
#include <sbmips.h>
#include <sbpci.h>
#include <sbconfig.h>
#include <bcmdevs.h>

#include <ssbcore.h>

#if 1

//#define SER_PORT1(reg)	(*((volatile unsigned char *)(0xbf800000+reg)))
#define SER_PORT1(reg)	(*((volatile unsigned char *)(0xb8000400+reg)))

int putDebugChar(char c)
{
	while (!(SER_PORT1(UART_LSR) & UART_LSR_THRE));
	SER_PORT1(UART_TX) = c;
	
	return 1;
}

char getDebugChar(void)
{
	while (!(SER_PORT1(UART_LSR) & 1));
	return SER_PORT1(UART_RX);
}


static int ser_line = 0;

static void
serial_add(void *regs, uint irq, uint baud_base, uint reg_shift)
{
	struct uart_port s;

	memset(&s, 0, sizeof(s));

	s.line = ser_line++;
	s.membase = regs;
	s.irq = irq + 2;
	s.uartclk = baud_base;
	//s.baud_base = baud_base / 16;
	//s.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST | UPF_RESOURCES | ASYNC_AUTO_IRQ;
	s.flags = ASYNC_BOOT_AUTOCONF;
	s.iotype = SERIAL_IO_MEM;
	s.regshift = reg_shift;

	if (early_serial_setup(&s) != 0) {
		printk(KERN_ERR "Serial setup failed!\n");
	}
}
#endif

extern void bcm47xx_time_init(void);
extern void bcm47xx_timer_setup(struct irqaction *irq);

void *nvram_get(char *foo)
{
	return NULL;
}

void *sbh;

static void bcm47xx_machine_restart(char *command)
{
	/* Set the watchdog timer to reset immediately */
	cli();
	sb_watchdog(sbh, 1);
	while (1);
}

static void bcm47xx_machine_halt(void)
{
	/* Disable interrupts and watchdog and spin forever */
	cli();
	sb_watchdog(sbh, 0);
	while (1);
}

//static struct sb_bus bus;

static int __init bcm47xx_init(void)
{
//	sb_bus_add(&bus, SB_BUS, (void *)SB_ENUM_BASE, SB_ENUM_LIM - SB_ENUM_BASE, "bcm47xx", NULL);

	sbh = sb_kattach();
	sb_mips_init(sbh);
	sbpci_init(sbh);
	sb_serial_init(sbh, serial_add);

	_machine_restart = bcm47xx_machine_restart;
	_machine_halt = bcm47xx_machine_halt;
	_machine_power_off = bcm47xx_machine_halt;
	
	board_time_init = bcm47xx_time_init;
	board_timer_setup = bcm47xx_timer_setup;
	
	return 0;
}

early_initcall(bcm47xx_init);
