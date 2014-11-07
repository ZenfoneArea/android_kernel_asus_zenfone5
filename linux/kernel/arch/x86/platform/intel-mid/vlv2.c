/*
 * vlv2.c: Intel ValleyView2 platform specific setup code
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Bin Gao <bin.gao@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <asm/setup.h>
#include <asm/time.h>
#include <asm/i8259.h>
#include <asm/intel-mid.h>
#include <asm/processor.h>
#include <asm/pci_x86.h>

#define ILB_BASE	0xfed08000
#define ILB_SIZE	0xa0
#define ILB_IR		0x20
#define PCI_DEV_NUM(x)	((x >> 3) & 0x1f)
#define IR_INTA(ir)	(ir & 0xf)

#define PIRQ2IRQ(x)	(x + 16)

/* Propagate PCI IRQ# */
static int vlv2_pci_enable_irq(struct pci_dev *pdev)
{

	u8 ir_val, dev;
	void __iomem *ilb_mem;
	struct io_apic_irq_attr irq_attr;

	ilb_mem = ioremap_nocache(ILB_BASE, ILB_SIZE);
	if (ilb_mem == NULL) {
		pr_err("%s(): can't map ILB_BASE(0x%x)\n",
			__func__, ILB_BASE);
		return -EIO;
	}

	dev = PCI_DEV_NUM(pdev->devfn);
	ir_val = ioread8(ilb_mem + ILB_IR + PCI_DEV_NUM(pdev->devfn) * 2);

	iounmap(ilb_mem);

	/* map INTA# only */
	pdev->irq = PIRQ2IRQ(IR_INTA(ir_val));
	pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, pdev->irq);

	irq_attr.ioapic = mp_find_ioapic(pdev->irq);
	irq_attr.ioapic_pin = pdev->irq;
	irq_attr.trigger = 1; /* level */
	irq_attr.polarity = 1; /* active low */
	io_apic_set_pci_routing(&pdev->dev, pdev->irq, &irq_attr);

	return 0;
}

#define VALLEYVIEW2_FAMILY	0x30670
#define CPUID_MASK		0xffff0
enum cpuid_regs {
	CR_EAX = 0,
	CR_ECX,
	CR_EDX,
	CR_EBX
};

static int is_valleyview()
{
	u32 regs[4];
	cpuid(1, &regs[CR_EAX], &regs[CR_EBX], &regs[CR_ECX], &regs[CR_EDX]);

	return ((regs[CR_EAX] & CPUID_MASK) == VALLEYVIEW2_FAMILY);
}

/*
 * ACPI DSDT table doesn't have correct PCI interrupt routing information
 * for some devices, so here we have a kernel workaround to fix this issue.
 * The workaround can be easily removed by not appending "vlv2" paramerter
 * to kernel command line.
 */
static int __init vlv2_pci_irq_fixup(void)
{
	/* HACK: intel_mid_identify_cpu() is not set yet */

	/* More hack: do cpuid instruction. This will be done here (helper is_valleyview) for now
	 * as temp w/a until reworked function is provided from Bin Gao.
	 */
	if (is_valleyview()) {
		pr_info("VLV2: fix up PCI interrupt routing\n");
		pcibios_enable_irq = vlv2_pci_enable_irq;
	}

	return 0;
}
fs_initcall(vlv2_pci_irq_fixup);

