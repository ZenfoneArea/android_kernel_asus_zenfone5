/*
 * platform_otg_pci.c: USB OTG platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/pci.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>
#include <asm/spid.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_USB_DWC3_OTG
#include <linux/usb/dwc3-intel-mid.h>
static struct intel_dwc_otg_pdata dwc_otg_pdata;

static bool dwc_otg_get_usbspecoverride(void)
{
	void __iomem *usb_comp_iomap;
	bool usb_spec_override;

	/* Read MISCFLAGS byte from offset 0x717 */
	usb_comp_iomap = ioremap_nocache(0xFFFCE717, 4);
	/* MISCFLAGS.BIT[6] indicates USB spec override */
	usb_spec_override = ioread8(usb_comp_iomap) & 0x40;
	iounmap(usb_comp_iomap);

	return usb_spec_override;
}

/* Read SCCB_USB_CFG.bit14 to get the current phy select setting */
static enum usb_phy_intf get_usb2_phy_type(void)
{
	void __iomem *addr;
	u32 val;

	addr = ioremap_nocache(SCCB_USB_CFG, 4);
	val = readl(addr) & SCCB_USB_CFG_SELECT_ULPI;
	iounmap(addr);

	if (val)
		return USB2_PHY_ULPI;
	else
		return USB2_PHY_UTMI;
}

static struct intel_dwc_otg_pdata *get_otg_platform_data(struct pci_dev *pdev)
{
	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MRFL_DWC3_OTG:
		if (INTEL_MID_BOARD(1, PHONE, MOFD)) {
			dwc_otg_pdata.pmic_type = SHADY_COVE;
			dwc_otg_pdata.charger_detect_enable = 0;
			dwc_otg_pdata.usb2_phy_type = get_usb2_phy_type();
			if (dwc_otg_pdata.usb2_phy_type == USB2_PHY_ULPI)
				dwc_otg_pdata.charger_detect_enable = 1;

		} else if (INTEL_MID_BOARD(1, PHONE, MRFL)) {
			dwc_otg_pdata.pmic_type = BASIN_COVE;
			dwc_otg_pdata.charger_detect_enable = 1;

			dwc_otg_pdata.charging_compliance =
				dwc_otg_get_usbspecoverride();
			dwc_otg_pdata.usb2_phy_type = USB2_PHY_ULPI;

		} else if (intel_mid_identify_sim() ==
				INTEL_MID_CPU_SIMULATION_HVP) {
			dwc_otg_pdata.pmic_type = NO_PMIC;
			dwc_otg_pdata.is_hvp = 1;
			dwc_otg_pdata.charger_detect_enable = 0;
			dwc_otg_pdata.usb2_phy_type = USB2_PHY_ULPI;
		}
		return &dwc_otg_pdata;
	case PCI_DEVICE_ID_INTEL_BYT_OTG:
		/* FIXME: Hardcode now, but need to use ACPI table for GPIO */
		if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, RVP3) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, RVP3)) {
			dwc_otg_pdata.gpio_cs = 156;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x4f;
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, 8PR0) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, 8PR0)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x4f;
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, 8PR1) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, 8PR1)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x7f;
			dwc_otg_pdata.sdp_charging = 1;
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, CRV2) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, CRV2)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x7f;
			dwc_otg_pdata.gpio_id = 156;
			dwc_otg_pdata.sdp_charging = 1;
		}
		return &dwc_otg_pdata;
	default:
		break;
	}

	return NULL;
}
#endif

#ifdef CONFIG_USB_PENWELL_OTG
#include <linux/usb/penwell_otg.h>
static struct intel_mid_otg_pdata otg_pdata = {
	.gpio_vbus = 0,
	.gpio_cs = 0,
	.gpio_reset = 0,
	.charging_compliance = 0,
	.hnp_poll_support = 0,
	.power_budget = 500
};

static struct intel_mid_otg_pdata *get_otg_platform_data(struct pci_dev *pdev)
{
	struct intel_mid_otg_pdata *pdata = &otg_pdata;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_OTG:
		if (INTEL_MID_BOARD(2, TABLET, MFLD, SLP, PRO) ||
			INTEL_MID_BOARD(2, TABLET, MFLD, SLP, ENG))
			pdata->gpio_vbus = 54;

		if (!INTEL_MID_BOARD(2, TABLET, MFLD, RR, PRO) &&
			!INTEL_MID_BOARD(2, TABLET, MFLD, RR, ENG))
			pdata->power_budget = 200;
		break;

	case PCI_DEVICE_ID_INTEL_CLV_OTG:
		pdata->gpio_cs = get_gpio_by_name("usb_otg_phy_cs");
		if (pdata->gpio_cs == -1) {
			pr_err("%s: No gpio pin usb_otg_phy_cs\n", __func__);
			return NULL;
		}
		pdata->gpio_reset = get_gpio_by_name("usb_otg_phy_rst");
		if (pdata->gpio_reset == -1) {
			pr_err("%s: No gpio pin usb_otg_phy_rst\n", __func__);
			return NULL;
		}
		break;

	default:
		break;
	}

	return pdata;
}
#endif

static void otg_pci_early_quirks(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = get_otg_platform_data(pci_dev);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MFD_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CLV_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MRFL_DWC3_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_OTG,
			otg_pci_early_quirks);

static void quirk_byt_otg_d3_delay(struct pci_dev *dev)
{
	dev->d3_delay = 10;
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_OTG,
			quirk_byt_otg_d3_delay);
