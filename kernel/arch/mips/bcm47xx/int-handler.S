#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set	noreorder
	.set	noat
	.align	5

	NESTED(bcm47xx_irq_handler, PT_SIZE, sp)
	SAVE_ALL
	CLI

	.set	at
	.set    noreorder

	jal	bcm47xx_irq_dispatch
	move	a0, sp

	j	ret_from_irq
	nop
		
	END(bcm47xx_irq_handler)
