/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: mthca_main.c 1396 2004-12-28 04:10:27Z roland $
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#ifdef CONFIG_INFINIBAND_MTHCA_SSE_DOORBELL
#include <asm/cpufeature.h>
#endif

#include "mthca_dev.h"
#include "mthca_config_reg.h"
#include "mthca_cmd.h"
#include "mthca_profile.h"
#include "mthca_memfree.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("Mellanox InfiniBand HCA low-level driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

#ifdef CONFIG_PCI_MSI

static int msi_x = 0;
module_param(msi_x, int, 0444);
MODULE_PARM_DESC(msi_x, "attempt to use MSI-X if nonzero");

static int msi = 0;
module_param(msi, int, 0444);
MODULE_PARM_DESC(msi, "attempt to use MSI if nonzero");

#else /* CONFIG_PCI_MSI */

#define msi_x (0)
#define msi   (0)

#endif /* CONFIG_PCI_MSI */

static const char mthca_version[] __devinitdata =
	"ib_mthca: Mellanox InfiniBand HCA driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

static struct mthca_profile default_profile = {
	.num_qp     = 1 << 16,
	.rdb_per_qp = 4,
	.num_cq     = 1 << 16,
	.num_mcg    = 1 << 13,
	.num_mpt    = 1 << 17,
	.num_mtt    = 1 << 20
};

enum {
	MTHCA_TAVOR_NUM_UDAV  = 1 << 15,
	MTHCA_ARBEL_UARC_SIZE = 1 << 18
};

static int __devinit mthca_tune_pci(struct mthca_dev *mdev)
{
	int cap;
	u16 val;

	/* First try to max out Read Byte Count */
	cap = pci_find_capability(mdev->pdev, PCI_CAP_ID_PCIX);
	if (cap) {
		if (pci_read_config_word(mdev->pdev, cap + PCI_X_CMD, &val)) {
			mthca_err(mdev, "Couldn't read PCI-X command register, "
				  "aborting.\n");
			return -ENODEV;
		}
		val = (val & ~PCI_X_CMD_MAX_READ) | (3 << 2);
		if (pci_write_config_word(mdev->pdev, cap + PCI_X_CMD, val)) {
			mthca_err(mdev, "Couldn't write PCI-X command register, "
				  "aborting.\n");
			return -ENODEV;
		}
	} else if (mdev->hca_type == TAVOR)
		mthca_info(mdev, "No PCI-X capability, not setting RBC.\n");

	cap = pci_find_capability(mdev->pdev, PCI_CAP_ID_EXP);
	if (cap) {
		if (pci_read_config_word(mdev->pdev, cap + PCI_EXP_DEVCTL, &val)) {
			mthca_err(mdev, "Couldn't read PCI Express device control "
				  "register, aborting.\n");
			return -ENODEV;
		}
		val = (val & ~PCI_EXP_DEVCTL_READRQ) | (5 << 12);
		if (pci_write_config_word(mdev->pdev, cap + PCI_EXP_DEVCTL, val)) {
			mthca_err(mdev, "Couldn't write PCI Express device control "
				  "register, aborting.\n");
			return -ENODEV;
		}
	} else if (mdev->hca_type == ARBEL_NATIVE ||
		   mdev->hca_type == ARBEL_COMPAT)
		mthca_info(mdev, "No PCI Express capability, "
			   "not setting Max Read Request Size.\n");

	return 0;
}

static int __devinit mthca_dev_lim(struct mthca_dev *mdev, struct mthca_dev_lim *dev_lim)
{
	int err;
	u8 status;

	err = mthca_QUERY_DEV_LIM(mdev, dev_lim, &status);
	if (err) {
		mthca_err(mdev, "QUERY_DEV_LIM command failed, aborting.\n");
		return err;
	}
	if (status) {
		mthca_err(mdev, "QUERY_DEV_LIM returned status 0x%02x, "
			  "aborting.\n", status);
		return -EINVAL;
	}
	if (dev_lim->min_page_sz > PAGE_SIZE) {
		mthca_err(mdev, "HCA minimum page size of %d bigger than "
			  "kernel PAGE_SIZE of %ld, aborting.\n",
			  dev_lim->min_page_sz, PAGE_SIZE);
		return -ENODEV;
	}
	if (dev_lim->num_ports > MTHCA_MAX_PORTS) {
		mthca_err(mdev, "HCA has %d ports, but we only support %d, "
			  "aborting.\n",
			  dev_lim->num_ports, MTHCA_MAX_PORTS);
		return -ENODEV;
	}

	mdev->limits.num_ports      	= dev_lim->num_ports;
	mdev->limits.vl_cap             = dev_lim->max_vl;
	mdev->limits.mtu_cap            = dev_lim->max_mtu;
	mdev->limits.gid_table_len  	= dev_lim->max_gids;
	mdev->limits.pkey_table_len 	= dev_lim->max_pkeys;
	mdev->limits.local_ca_ack_delay = dev_lim->local_ca_ack_delay;
	mdev->limits.max_sg             = dev_lim->max_sg;
	mdev->limits.reserved_qps       = dev_lim->reserved_qps;
	mdev->limits.reserved_srqs      = dev_lim->reserved_srqs;
	mdev->limits.reserved_eecs      = dev_lim->reserved_eecs;
	mdev->limits.reserved_cqs       = dev_lim->reserved_cqs;
	mdev->limits.reserved_eqs       = dev_lim->reserved_eqs;
	mdev->limits.reserved_mtts      = dev_lim->reserved_mtts;
	mdev->limits.reserved_mrws      = dev_lim->reserved_mrws;
	mdev->limits.reserved_uars      = dev_lim->reserved_uars;
	mdev->limits.reserved_pds       = dev_lim->reserved_pds;

	if (dev_lim->flags & DEV_LIM_FLAG_SRQ)
		mdev->mthca_flags |= MTHCA_FLAG_SRQ;

	return 0;
}

static int __devinit mthca_init_tavor(struct mthca_dev *mdev)
{
	u8 status;
	int err;
	struct mthca_dev_lim        dev_lim;
	struct mthca_profile        profile;
	struct mthca_init_hca_param init_hca;
	struct mthca_adapter        adapter;

	err = mthca_SYS_EN(mdev, &status);
	if (err) {
		mthca_err(mdev, "SYS_EN command failed, aborting.\n");
		return err;
	}
	if (status) {
		mthca_err(mdev, "SYS_EN returned status 0x%02x, "
			  "aborting.\n", status);
		return -EINVAL;
	}

	err = mthca_QUERY_FW(mdev, &status);
	if (err) {
		mthca_err(mdev, "QUERY_FW command failed, aborting.\n");
		goto err_out_disable;
	}
	if (status) {
		mthca_err(mdev, "QUERY_FW returned status 0x%02x, "
			  "aborting.\n", status);
		err = -EINVAL;
		goto err_out_disable;
	}
	err = mthca_QUERY_DDR(mdev, &status);
	if (err) {
		mthca_err(mdev, "QUERY_DDR command failed, aborting.\n");
		goto err_out_disable;
	}
	if (status) {
		mthca_err(mdev, "QUERY_DDR returned status 0x%02x, "
			  "aborting.\n", status);
		err = -EINVAL;
		goto err_out_disable;
	}

	err = mthca_dev_lim(mdev, &dev_lim);

	profile = default_profile;
	profile.num_uar  = dev_lim.uar_size / PAGE_SIZE;
	profile.num_udav = MTHCA_TAVOR_NUM_UDAV;

	err = mthca_make_profile(mdev, &profile, &dev_lim, &init_hca);
	if (err)
		goto err_out_disable;

	err = mthca_INIT_HCA(mdev, &init_hca, &status);
	if (err) {
		mthca_err(mdev, "INIT_HCA command failed, aborting.\n");
		goto err_out_disable;
	}
	if (status) {
		mthca_err(mdev, "INIT_HCA returned status 0x%02x, "
			  "aborting.\n", status);
		err = -EINVAL;
		goto err_out_disable;
	}

	err = mthca_QUERY_ADAPTER(mdev, &adapter, &status);
	if (err) {
		mthca_err(mdev, "QUERY_ADAPTER command failed, aborting.\n");
		goto err_out_disable;
	}
	if (status) {
		mthca_err(mdev, "QUERY_ADAPTER returned status 0x%02x, "
			  "aborting.\n", status);
		err = -EINVAL;
		goto err_out_close;
	}

	mdev->eq_table.inta_pin = adapter.inta_pin;
	mdev->rev_id            = adapter.revision_id;

	return 0;

err_out_close:
	mthca_CLOSE_HCA(mdev, 0, &status);

err_out_disable:
	mthca_SYS_DIS(mdev, &status);

	return err;
}

static int __devinit mthca_load_fw(struct mthca_dev *mdev)
{
	u8 status;
	int err;

	/* FIXME: use HCA-attached memory for FW if present */

	mdev->fw.arbel.icm =
		mthca_alloc_icm(mdev, mdev->fw.arbel.fw_pages,
				GFP_HIGHUSER | __GFP_NOWARN);
	if (!mdev->fw.arbel.icm) {
		mthca_err(mdev, "Couldn't allocate FW area, aborting.\n");
		return -ENOMEM;
	}

	err = mthca_MAP_FA(mdev, mdev->fw.arbel.icm, &status);
	if (err) {
		mthca_err(mdev, "MAP_FA command failed, aborting.\n");
		goto err_free;
	}
	if (status) {
		mthca_err(mdev, "MAP_FA returned status 0x%02x, aborting.\n", status);
		err = -EINVAL;
		goto err_free;
	}
	err = mthca_RUN_FW(mdev, &status);
	if (err) {
		mthca_err(mdev, "RUN_FW command failed, aborting.\n");
		goto err_unmap_fa;
	}
	if (status) {
		mthca_err(mdev, "RUN_FW returned status 0x%02x, aborting.\n", status);
		err = -EINVAL;
		goto err_unmap_fa;
	}

	return 0;

err_unmap_fa:
	mthca_UNMAP_FA(mdev, &status);

err_free:
	mthca_free_icm(mdev, mdev->fw.arbel.icm);
	return err;
}

static int __devinit mthca_init_arbel(struct mthca_dev *mdev)
{
	struct mthca_dev_lim dev_lim;
	u8 status;
	int err;

	err = mthca_QUERY_FW(mdev, &status);
	if (err) {
		mthca_err(mdev, "QUERY_FW command failed, aborting.\n");
		return err;
	}
	if (status) {
		mthca_err(mdev, "QUERY_FW returned status 0x%02x, "
			  "aborting.\n", status);
		return -EINVAL;
	}

	err = mthca_ENABLE_LAM(mdev, &status);
	if (err) {
		mthca_err(mdev, "ENABLE_LAM command failed, aborting.\n");
		return err;
	}
	if (status == MTHCA_CMD_STAT_LAM_NOT_PRE) {
		mthca_dbg(mdev, "No HCA-attached memory (running in MemFree mode)\n");
		mdev->mthca_flags |= MTHCA_FLAG_NO_LAM;
	} else if (status) {
		mthca_err(mdev, "ENABLE_LAM returned status 0x%02x, "
			  "aborting.\n", status);
		return -EINVAL;
	}

	err = mthca_load_fw(mdev);
	if (err) {
		mthca_err(mdev, "Failed to start FW, aborting.\n");
		goto err_out_disable;
	}

	err = mthca_dev_lim(mdev, &dev_lim);
	if (err) {
		mthca_err(mdev, "QUERY_DEV_LIM command failed, aborting.\n");
		goto err_out_stop_fw;
	}

	mthca_warn(mdev, "Sorry, native MT25208 mode support is not done, "
		   "aborting.\n");
	err = -ENODEV;

err_out_stop_fw:
	mthca_UNMAP_FA(mdev, &status);
	mthca_free_icm(mdev, mdev->fw.arbel.icm);

err_out_disable:
	if (!(mdev->mthca_flags & MTHCA_FLAG_NO_LAM))
		mthca_DISABLE_LAM(mdev, &status);
	return err;
}

static int __devinit mthca_init_hca(struct mthca_dev *mdev)
{
	if (mdev->hca_type == ARBEL_NATIVE)
		return mthca_init_arbel(mdev);
	else
		return mthca_init_tavor(mdev);
}

static int __devinit mthca_setup_hca(struct mthca_dev *dev)
{
	int err;

	MTHCA_INIT_DOORBELL_LOCK(&dev->doorbell_lock);

	err = mthca_init_pd_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "protection domain table, aborting.\n");
		return err;
	}

	err = mthca_init_mr_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "memory region table, aborting.\n");
		goto err_out_pd_table_free;
	}

	err = mthca_pd_alloc(dev, &dev->driver_pd);
	if (err) {
		mthca_err(dev, "Failed to create driver PD, "
			  "aborting.\n");
		goto err_out_mr_table_free;
	}

	err = mthca_init_eq_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "event queue table, aborting.\n");
		goto err_out_pd_free;
	}

	err = mthca_cmd_use_events(dev);
	if (err) {
		mthca_err(dev, "Failed to switch to event-driven "
			  "firmware commands, aborting.\n");
		goto err_out_eq_table_free;
	}

	err = mthca_init_cq_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "completion queue table, aborting.\n");
		goto err_out_cmd_poll;
	}

	err = mthca_init_qp_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "queue pair table, aborting.\n");
		goto err_out_cq_table_free;
	}

	err = mthca_init_av_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "address vector table, aborting.\n");
		goto err_out_qp_table_free;
	}

	err = mthca_init_mcg_table(dev);
	if (err) {
		mthca_err(dev, "Failed to initialize "
			  "multicast group table, aborting.\n");
		goto err_out_av_table_free;
	}

	return 0;

err_out_av_table_free:
	mthca_cleanup_av_table(dev);

err_out_qp_table_free:
	mthca_cleanup_qp_table(dev);

err_out_cq_table_free:
	mthca_cleanup_cq_table(dev);

err_out_cmd_poll:
	mthca_cmd_use_polling(dev);

err_out_eq_table_free:
	mthca_cleanup_eq_table(dev);

err_out_pd_free:
	mthca_pd_free(dev, &dev->driver_pd);

err_out_mr_table_free:
	mthca_cleanup_mr_table(dev);

err_out_pd_table_free:
	mthca_cleanup_pd_table(dev);
	return err;
}

static int __devinit mthca_request_regions(struct pci_dev *pdev,
					   int ddr_hidden)
{
	int err;

	/*
	 * We request our first BAR in two chunks, since the MSI-X
	 * vector table is right in the middle.
	 *
	 * This is why we can't just use pci_request_regions() -- if
	 * we did then setting up MSI-X would fail, since the PCI core
	 * wants to do request_mem_region on the MSI-X vector table.
	 */
	if (!request_mem_region(pci_resource_start(pdev, 0) +
				MTHCA_HCR_BASE,
				MTHCA_MAP_HCR_SIZE,
				DRV_NAME))
		return -EBUSY;

	if (!request_mem_region(pci_resource_start(pdev, 0) +
				MTHCA_CLR_INT_BASE,
				MTHCA_CLR_INT_SIZE,
				DRV_NAME)) {
		err = -EBUSY;
		goto err_out_bar0_beg;
	}

	err = pci_request_region(pdev, 2, DRV_NAME);
	if (err)
		goto err_out_bar0_end;

	if (!ddr_hidden) {
		err = pci_request_region(pdev, 4, DRV_NAME);
		if (err)
			goto err_out_bar2;
	}

	return 0;

err_out_bar0_beg:
	release_mem_region(pci_resource_start(pdev, 0) +
			   MTHCA_HCR_BASE,
			   MTHCA_MAP_HCR_SIZE);

err_out_bar0_end:
	release_mem_region(pci_resource_start(pdev, 0) +
			   MTHCA_CLR_INT_BASE,
			   MTHCA_CLR_INT_SIZE);

err_out_bar2:
	pci_release_region(pdev, 2);
	return err;
}

static void mthca_release_regions(struct pci_dev *pdev,
				  int ddr_hidden)
{
	release_mem_region(pci_resource_start(pdev, 0) +
			   MTHCA_HCR_BASE,
			   MTHCA_MAP_HCR_SIZE);
	release_mem_region(pci_resource_start(pdev, 0) +
			   MTHCA_CLR_INT_BASE,
			   MTHCA_CLR_INT_SIZE);
	pci_release_region(pdev, 2);
	if (!ddr_hidden)
		pci_release_region(pdev, 4);
}

static int __devinit mthca_enable_msi_x(struct mthca_dev *mdev)
{
	struct msix_entry entries[3];
	int err;

	entries[0].entry = 0;
	entries[1].entry = 1;
	entries[2].entry = 2;

	err = pci_enable_msix(mdev->pdev, entries, ARRAY_SIZE(entries));
	if (err) {
		if (err > 0)
			mthca_info(mdev, "Only %d MSI-X vectors available, "
				   "not using MSI-X\n", err);
		return err;
	}

	mdev->eq_table.eq[MTHCA_EQ_COMP ].msi_x_vector = entries[0].vector;
	mdev->eq_table.eq[MTHCA_EQ_ASYNC].msi_x_vector = entries[1].vector;
	mdev->eq_table.eq[MTHCA_EQ_CMD  ].msi_x_vector = entries[2].vector;

	return 0;
}

static void mthca_close_hca(struct mthca_dev *mdev)
{
	u8 status;

	mthca_CLOSE_HCA(mdev, 0, &status);

	if (mdev->hca_type == ARBEL_NATIVE) {
		mthca_UNMAP_FA(mdev, &status);
		mthca_free_icm(mdev, mdev->fw.arbel.icm);

		if (!(mdev->mthca_flags & MTHCA_FLAG_NO_LAM))
			mthca_DISABLE_LAM(mdev, &status);
	} else
		mthca_SYS_DIS(mdev, &status);
}

static int __devinit mthca_init_one(struct pci_dev *pdev,
				    const struct pci_device_id *id)
{
	static int mthca_version_printed = 0;
	int ddr_hidden = 0;
	int err;
	unsigned long mthca_base;
	struct mthca_dev *mdev;

	if (!mthca_version_printed) {
		printk(KERN_INFO "%s", mthca_version);
		++mthca_version_printed;
	}

	printk(KERN_INFO PFX "Initializing %s (%s)\n",
	       pci_pretty_name(pdev), pci_name(pdev));

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device, "
			"aborting.\n");
		return err;
	}

	/*
	 * Check for BARs.  We expect 0: 1MB, 2: 8MB, 4: DDR (may not
	 * be present)
	 */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM) ||
	    pci_resource_len(pdev, 0) != 1 << 20) {
		dev_err(&pdev->dev, "Missing DCS, aborting.");
		err = -ENODEV;
		goto err_out_disable_pdev;
	}
	if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM) ||
	    pci_resource_len(pdev, 2) != 1 << 23) {
		dev_err(&pdev->dev, "Missing UAR, aborting.");
		err = -ENODEV;
		goto err_out_disable_pdev;
	}
	if (!(pci_resource_flags(pdev, 4) & IORESOURCE_MEM))
		ddr_hidden = 1;

	err = mthca_request_regions(pdev, ddr_hidden);
	if (err) {
		dev_err(&pdev->dev, "Cannot obtain PCI resources, "
			"aborting.\n");
		goto err_out_disable_pdev;
	}

	pci_set_master(pdev);

	err = pci_set_dma_mask(pdev, DMA_64BIT_MASK);
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit PCI DMA mask.\n");
		err = pci_set_dma_mask(pdev, DMA_32BIT_MASK);
		if (err) {
			dev_err(&pdev->dev, "Can't set PCI DMA mask, aborting.\n");
			goto err_out_free_res;
		}
	}
	err = pci_set_consistent_dma_mask(pdev, DMA_64BIT_MASK);
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit "
			 "consistent PCI DMA mask.\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK);
		if (err) {
			dev_err(&pdev->dev, "Can't set consistent PCI DMA mask, "
				"aborting.\n");
			goto err_out_free_res;
		}
	}

	mdev = (struct mthca_dev *) ib_alloc_device(sizeof *mdev);
	if (!mdev) {
		dev_err(&pdev->dev, "Device struct alloc failed, "
			"aborting.\n");
		err = -ENOMEM;
		goto err_out_free_res;
	}

	mdev->pdev     = pdev;
	mdev->hca_type = id->driver_data;

	if (ddr_hidden)
		mdev->mthca_flags |= MTHCA_FLAG_DDR_HIDDEN;

	/*
	 * Now reset the HCA before we touch the PCI capabilities or
	 * attempt a firmware command, since a boot ROM may have left
	 * the HCA in an undefined state.
	 */
	err = mthca_reset(mdev);
	if (err) {
		mthca_err(mdev, "Failed to reset HCA, aborting.\n");
		goto err_out_free_dev;
	}

	if (msi_x && !mthca_enable_msi_x(mdev))
		mdev->mthca_flags |= MTHCA_FLAG_MSI_X;
	if (msi && !(mdev->mthca_flags & MTHCA_FLAG_MSI_X) &&
	    !pci_enable_msi(pdev))
		mdev->mthca_flags |= MTHCA_FLAG_MSI;

	sema_init(&mdev->cmd.hcr_sem, 1);
	sema_init(&mdev->cmd.poll_sem, 1);
	mdev->cmd.use_events = 0;

	mthca_base = pci_resource_start(pdev, 0);
	mdev->hcr = ioremap(mthca_base + MTHCA_HCR_BASE, MTHCA_MAP_HCR_SIZE);
	if (!mdev->hcr) {
		mthca_err(mdev, "Couldn't map command register, "
			  "aborting.\n");
		err = -ENOMEM;
		goto err_out_free_dev;
	}
	mdev->clr_base = ioremap(mthca_base + MTHCA_CLR_INT_BASE,
				 MTHCA_CLR_INT_SIZE);
	if (!mdev->clr_base) {
		mthca_err(mdev, "Couldn't map command register, "
			  "aborting.\n");
		err = -ENOMEM;
		goto err_out_iounmap;
	}

	mthca_base = pci_resource_start(pdev, 2);
	mdev->kar = ioremap(mthca_base + PAGE_SIZE * MTHCA_KAR_PAGE, PAGE_SIZE);
	if (!mdev->kar) {
		mthca_err(mdev, "Couldn't map kernel access region, "
			  "aborting.\n");
		err = -ENOMEM;
		goto err_out_iounmap_clr;
	}

	err = mthca_tune_pci(mdev);
	if (err)
		goto err_out_iounmap_kar;

	err = mthca_init_hca(mdev);
	if (err)
		goto err_out_iounmap_kar;

	err = mthca_setup_hca(mdev);
	if (err)
		goto err_out_close;

	err = mthca_register_device(mdev);
	if (err)
		goto err_out_cleanup;

	err = mthca_create_agents(mdev);
	if (err)
		goto err_out_unregister;

	pci_set_drvdata(pdev, mdev);

	return 0;

err_out_unregister:
	mthca_unregister_device(mdev);

err_out_cleanup:
	mthca_cleanup_mcg_table(mdev);
	mthca_cleanup_av_table(mdev);
	mthca_cleanup_qp_table(mdev);
	mthca_cleanup_cq_table(mdev);
	mthca_cmd_use_polling(mdev);
	mthca_cleanup_eq_table(mdev);

	mthca_pd_free(mdev, &mdev->driver_pd);

	mthca_cleanup_mr_table(mdev);
	mthca_cleanup_pd_table(mdev);

err_out_close:
	mthca_close_hca(mdev);

err_out_iounmap_kar:
	iounmap(mdev->kar);

err_out_iounmap_clr:
	iounmap(mdev->clr_base);

err_out_iounmap:
	iounmap(mdev->hcr);

err_out_free_dev:
	if (mdev->mthca_flags & MTHCA_FLAG_MSI_X)
		pci_disable_msix(pdev);
	if (mdev->mthca_flags & MTHCA_FLAG_MSI)
		pci_disable_msi(pdev);

	ib_dealloc_device(&mdev->ib_dev);

err_out_free_res:
	mthca_release_regions(pdev, ddr_hidden);

err_out_disable_pdev:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	return err;
}

static void __devexit mthca_remove_one(struct pci_dev *pdev)
{
	struct mthca_dev *mdev = pci_get_drvdata(pdev);
	u8 status;
	int p;

	if (mdev) {
		mthca_free_agents(mdev);
		mthca_unregister_device(mdev);

		for (p = 1; p <= mdev->limits.num_ports; ++p)
			mthca_CLOSE_IB(mdev, p, &status);

		mthca_cleanup_mcg_table(mdev);
		mthca_cleanup_av_table(mdev);
		mthca_cleanup_qp_table(mdev);
		mthca_cleanup_cq_table(mdev);
		mthca_cmd_use_polling(mdev);
		mthca_cleanup_eq_table(mdev);

		mthca_pd_free(mdev, &mdev->driver_pd);

		mthca_cleanup_mr_table(mdev);
		mthca_cleanup_pd_table(mdev);

		mthca_close_hca(mdev);

		iounmap(mdev->hcr);
		iounmap(mdev->clr_base);

		if (mdev->mthca_flags & MTHCA_FLAG_MSI_X)
			pci_disable_msix(pdev);
		if (mdev->mthca_flags & MTHCA_FLAG_MSI)
			pci_disable_msi(pdev);

		ib_dealloc_device(&mdev->ib_dev);
		mthca_release_regions(pdev, mdev->mthca_flags &
				      MTHCA_FLAG_DDR_HIDDEN);
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
	}
}

static struct pci_device_id mthca_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_MELLANOX, PCI_DEVICE_ID_MELLANOX_TAVOR),
	  .driver_data = TAVOR },
	{ PCI_DEVICE(PCI_VENDOR_ID_TOPSPIN, PCI_DEVICE_ID_MELLANOX_TAVOR),
	  .driver_data = TAVOR },
	{ PCI_DEVICE(PCI_VENDOR_ID_MELLANOX, PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT),
	  .driver_data = ARBEL_COMPAT },
	{ PCI_DEVICE(PCI_VENDOR_ID_TOPSPIN, PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT),
	  .driver_data = ARBEL_COMPAT },
	{ PCI_DEVICE(PCI_VENDOR_ID_MELLANOX, PCI_DEVICE_ID_MELLANOX_ARBEL),
	  .driver_data = ARBEL_NATIVE },
	{ PCI_DEVICE(PCI_VENDOR_ID_TOPSPIN, PCI_DEVICE_ID_MELLANOX_ARBEL),
	  .driver_data = ARBEL_NATIVE },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mthca_pci_table);

static struct pci_driver mthca_driver = {
	.name		= "ib_mthca",
	.id_table	= mthca_pci_table,
	.probe		= mthca_init_one,
	.remove		= __devexit_p(mthca_remove_one)
};

static int __init mthca_init(void)
{
	int ret;

	/*
	 * TODO: measure whether dynamically choosing doorbell code at
	 * runtime affects our performance.  Is there a "magic" way to
	 * choose without having to follow a function pointer every
	 * time we ring a doorbell?
	 */
#ifdef CONFIG_INFINIBAND_MTHCA_SSE_DOORBELL
	if (!cpu_has_xmm) {
		printk(KERN_ERR PFX "mthca was compiled with SSE doorbell code, but\n");
		printk(KERN_ERR PFX "the current CPU does not support SSE.\n");
		printk(KERN_ERR PFX "Turn off CONFIG_INFINIBAND_MTHCA_SSE_DOORBELL "
		       "and recompile.\n");
		return -ENODEV;
	}
#endif

	ret = pci_register_driver(&mthca_driver);
	return ret < 0 ? ret : 0;
}

static void __exit mthca_cleanup(void)
{
	pci_unregister_driver(&mthca_driver);
}

module_init(mthca_init);
module_exit(mthca_cleanup);
