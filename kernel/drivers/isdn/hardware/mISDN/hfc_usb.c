/*
 * hfc_usb.c
 *
 * modular mISDN driver for Colognechip HFC-USB chip
 *
 * Author : Florian Schirmer <jolt@tuxbox.org>
 *
 *          based on the second i4l hfc_usb driver of
 *            Peter Sprenger  (sprenger@moving-byters.de)
 *            Martin Bachem   (info@colognechip.com)
 *
 *          based on the first i4l hfc_usb driver of 
 *            Werner Cornelius (werner@isdn-development.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
*/

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <linux/mISDNif.h>
#include "dchannel.h"
#include "bchannel.h"
#include "helper.h"
#define SPIN_DEBUG
#define LOCK_STATISTIC
#include "hw_lock.h"

#define HFC_USB_NAME "hfc_usb"

#define HFC_USB_ERR KERN_ERR HFC_USB_NAME ": "
#define HFC_USB_INFO KERN_INFO HFC_USB_NAME ": "

static const char hfc_usb_name[] = HFC_USB_NAME;
static const char hfc_usb_revision[] = "1.0";
static mISDNobject_t hfc_usb;

#define hfc_usb_object hfc_usb

static int debug;
static int HFC_cnt;


/* begin old stuff */
struct hfcusb_data;
struct usb_fifo;
static void hfc_usb_stop_isoc_chain(struct usb_fifo *fifo);
static int hfc_usb_lock_dev(void *data, int nowait);
static void hfc_usb_unlock_dev(void *data);
static int hfc_usb_init_dchannel(struct hfcusb_data *ta);
static int hfc_usb_init_bchannels(struct hfcusb_data *ta);
static int hfc_usb_register_channels(struct hfcusb_data *ta);
static int hfc_usb_hw_init(struct hfcusb_data *ta);
#include "hfc_usb_old.c"
/* end old stuff */

/******************************************************************************/
/* Hardware related functions                                                 */
/******************************************************************************/

/* stops running iso chain and frees their pending urbs */
static void 
hfc_usb_stop_isoc_chain(usb_fifo *fifo)
{
	int i;

	for(i = 0; i < 2; i++)
	{
		if(fifo->iso[i].purb)
		{
			usb_unlink_urb(fifo->iso[i].purb);
			usb_free_urb(fifo->iso[i].purb);
			fifo->iso[i].purb = NULL;
		}
	}

	if (fifo->urb) {
		usb_unlink_urb(fifo->urb);
		usb_free_urb(fifo->urb);
		fifo->urb = NULL;
	}

	fifo->active = 0;
}

/* allocs urbs and start isoc transfer with two pending urbs to avoid gaps in the transfer chain */
static int 
hfc_usb_start_isoc_chain(usb_fifo *fifo, int num_packets_per_urb, usb_complete_t complete, int packet_size)
{
	int i;
	int packet_nr;
	int err;

	// allocate Memory for Iso out Urbs
	for (i = 0; i < 2; i++) {
		if (!(fifo->iso[i].purb)) {
			fifo->iso[i].purb = usb_alloc_urb(num_packets_per_urb, GFP_KERNEL);
			fifo->iso[i].owner_fifo = fifo;

			// Init the first iso
			if ((fifo->usb_packet_maxlen * num_packets_per_urb) <= ISO_BUFFER_SIZE)	{
				fill_isoc_urb(fifo->iso[i].purb, fifo->hfc->dev, fifo->pipe, fifo->iso[i].buffer,
					num_packets_per_urb, fifo->usb_packet_maxlen, fifo->intervall,
					complete, &fifo->iso[i]);

				memset(fifo->iso[i].buffer, 0, sizeof(fifo->iso[i].buffer));

				// defining packet delimeters in fifo->buffer
				for(packet_nr = 0; packet_nr < num_packets_per_urb; packet_nr++)
				{
					fifo->iso[i].purb->iso_frame_desc[packet_nr].offset = packet_nr * packet_size;
					fifo->iso[i].purb->iso_frame_desc[packet_nr].length = packet_size;
				}
			}
		}

		fifo->bit_line = BITLINE_INF;
		err = usb_submit_urb(fifo->iso[i].purb, GFP_KERNEL);
		fifo->active = (err == 0) ? 1 : 0;

		if(err < 0) {
			printk(HFC_USB_ERR "Error submitting ISO URB %i (error = %d)\n",  i, err);
			break;
		}
	}

	return err;
}

static int
hfc_usb_start_int_fifo(usb_fifo * fifo)
{
	int err;

	if (!fifo->urb) {
		fifo->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!fifo->urb)
			return -ENOMEM;
	}

	usb_fill_int_urb(fifo->urb, fifo->hfc->dev, fifo->pipe, fifo->buffer,
				 fifo->usb_packet_maxlen, rx_complete, fifo, fifo->intervall);
	fifo->active = 1;		/* must be marked active */
	err = usb_submit_urb(fifo->urb, GFP_KERNEL);

	if(err)
	{
		printk(HFC_USB_ERR "Unable to submit URB (error = %d)\n",  err);
		fifo->active = 0;
		fifo->skbuff = NULL;
	}
	
	return err;
}

static int 
hfc_usb_start_rx_fifo(usb_fifo *fifo, int num_packets_per_urb)
{
	switch(fifo->hfc->cfg_used) {
		case CNF_3INT3ISO:
		case CNF_4INT3ISO:
			return hfc_usb_start_int_fifo(fifo);
			break;
		case CNF_3ISO3ISO:
		case CNF_4ISO3ISO:
#warning TODO: Look at fifo and computate num_pakets_per_urb
			return hfc_usb_start_isoc_chain(fifo, num_packets_per_urb, rx_iso_complete, 16);
			break;
		default:
			printk(HFC_USB_ERR "Invalid FIFO configuration!\n");
			return -EINVAL;
	}
}

static int 
hfc_usb_start_fifos(hfcusb_data *hfc)
{
	/* RX FIFO */
	hfc_usb_start_rx_fifo(&hfc->fifos[HFCUSB_D_RX], ISOC_PACKETS_D);
	if (hfc->fifos[HFCUSB_PCM_RX].pipe)
		hfc_usb_start_rx_fifo(&hfc->fifos[HFCUSB_PCM_RX], ISOC_PACKETS_D);
	hfc_usb_start_rx_fifo(&hfc->fifos[HFCUSB_B1_RX], ISOC_PACKETS_B);
	hfc_usb_start_rx_fifo(&hfc->fifos[HFCUSB_B2_RX], ISOC_PACKETS_B);

	/* TX FIFO */
	hfc_usb_start_isoc_chain(&hfc->fifos[HFCUSB_D_TX], ISOC_PACKETS_D, tx_iso_complete, 1);
	hfc_usb_start_isoc_chain(&hfc->fifos[HFCUSB_B1_TX], ISOC_PACKETS_B, tx_iso_complete, 1);
	hfc_usb_start_isoc_chain(&hfc->fifos[HFCUSB_B2_TX], ISOC_PACKETS_B, tx_iso_complete, 1);
	
	return 0;
}

/******************************************************************************/
/* mISDN related functions                                                    */
/******************************************************************************/

static int
hfc_usb_l1hw(mISDNif_t *hif, struct sk_buff *skb)
{
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");
	printk(HFC_USB_ERR "l1hw called!\n");

	return 0;
}

static int
hfc_usb_l2l1(mISDNif_t *hif, struct sk_buff *skb)
{
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");
	printk(HFC_USB_ERR "l2l1 called!\n");

	return 0;
}

static int
hfc_usb_manager(void * data, u_int prim, void *arg)
{
	printk(HFC_USB_INFO "Manager called!\n");

	hfcusb_data *hfc;
	mISDNinstance_t	*inst = data;
	struct sk_buff *skb;
	int channel = -1;

	if (!data) {
		MGR_HASPROTOCOL_HANDLER(prim, arg, &hfc_usb_object)
		printk(KERN_ERR "%s: no data prim %x arg %p\n",
			__FUNCTION__, prim, arg);
		return -EINVAL;
	}

	list_for_each_entry(hfc, &hfc_usb_object.ilist, list) {
		if (&hfc->dch.inst == inst) {
			channel = 2;
			break;
		}

		if (&hfc->bch[0].inst == inst) {
			channel = 0;
			break;
		}

		if (&hfc->bch[1].inst == inst) {
			inst = &hfc->bch[1].inst;
			channel = 1;
			break;
		}
	}

	if (channel < 0) {
		printk(KERN_ERR "%s: no channel data %p prim %x arg %p\n",
			__FUNCTION__, data, prim, arg);
		return -EINVAL;
	}

	switch(prim) {
	    case MGR_REGLAYER | CONFIRM:
		if (channel == 2)
			dch_set_para(&hfc->dch, &inst->st->para);
		else
			bch_set_para(&hfc->bch[channel], &inst->st->para);
		break;
	    case MGR_UNREGLAYER | REQUEST:
		if (channel == 2) {
			inst->down.fdata = &hfc->dch;
			if ((skb = create_link_skb(PH_CONTROL | REQUEST,
				HW_DEACTIVATE, 0, NULL, 0))) {
				if (hfc_usb_l1hw(&inst->down, skb))
					dev_kfree_skb(skb);
			}
		} else {
			inst->down.fdata = &hfc->bch[channel];
			if ((skb = create_link_skb(MGR_DISCONNECT | REQUEST,
				0, 0, NULL, 0))) {
				if (hfc_usb_l2l1(&inst->down, skb))
					dev_kfree_skb(skb);
			}
		}
		hfc_usb_object.ctrl(inst->up.peer, MGR_DISCONNECT | REQUEST, &inst->up);
		hfc_usb_object.ctrl(inst, MGR_UNREGLAYER | REQUEST, NULL);
		break;
	    case MGR_CLRSTPARA | INDICATION:
		arg = NULL;
	    case MGR_ADDSTPARA | INDICATION:
		if (channel == 2)
			dch_set_para(&hfc->dch, arg);
		else
			bch_set_para(&hfc->bch[channel], arg);
		break;
	    case MGR_RELEASE | INDICATION:
		if (channel == 2) {
			printk("Implement release_card!!!!\n");
			//release_card(hfc);
		} else {
			hfc_usb_object.refcnt--;
		}
		break;
	    case MGR_CONNECT | REQUEST:
		return(mISDN_ConnectIF(inst, arg));
		break;
	    case MGR_SETIF | REQUEST:
	    case MGR_SETIF | INDICATION:
		if (channel==2)
			return mISDN_SetIF(inst, arg, prim, hfc_usb_l1hw, NULL,
				&hfc->dch);
		else
			return mISDN_SetIF(inst, arg, prim, hfc_usb_l2l1, NULL,
				&hfc->bch[channel]);
		break;
	    case MGR_DISCONNECT | REQUEST:
	    case MGR_DISCONNECT | INDICATION:
		return(mISDN_DisConnectIF(inst, arg));
		break;
	    case MGR_SELCHANNEL | REQUEST:
		if (channel != 2) {
			printk(KERN_WARNING "%s: selchannel not dinst\n",
				__FUNCTION__);
			return -EINVAL;
		}
		printk("Implement SelFreeBChannel!!!!\n");
		//return(SelFreeBChannel(hfc, arg));
		break;
	    case MGR_SETSTACK | CONFIRM:
		if ((channel!=2) && (inst->pid.global == 2)) {
			inst->down.fdata = &hfc->bch[channel];
			if ((skb = create_link_skb(PH_ACTIVATE | REQUEST,
				0, 0, NULL, 0))) {
				if (hfc_usb_l2l1(&inst->down, skb))
					dev_kfree_skb(skb);
			}
			if (inst->pid.protocol[2] == ISDN_PID_L2_B_TRANS)
				if_link(&inst->up, DL_ESTABLISH | INDICATION,
					0, 0, NULL, 0);
			else
				if_link(&inst->up, PH_ACTIVATE | INDICATION,
					0, 0, NULL, 0);
		}
		break;
	    PRIM_NOT_HANDLED(MGR_CTRLREADY | INDICATION);
	    PRIM_NOT_HANDLED(MGR_GLOBALOPT | REQUEST);
	    default:
		printk(KERN_WARNING "%s: prim %x not handled\n",
			__FUNCTION__, prim);
		return -EINVAL;
	}

	return 0;
}

static int 
hfc_usb_lock_dev(void *data, int nowait)
{
	register mISDN_HWlock_t	*lock = &((hfcusb_data *)data)->lock;
	
	return (lock_HW(lock, nowait));
} 

static void 
hfc_usb_unlock_dev(void *data)
{
	register mISDN_HWlock_t	*lock = &((hfcusb_data *)data)->lock;

	unlock_HW(lock);
}

static int 
hfc_usb_init_dchannel(hfcusb_data *ta)
{
	dchannel_t *dch = &ta->dch;

	dch->debug = debug;
	dch->inst.lock = hfc_usb_lock_dev;
	dch->inst.unlock = hfc_usb_unlock_dev;

	mISDN_init_instance(&dch->inst, &hfc_usb, ta);

	dch->inst.pid.layermask = ISDN_LAYER(0);
	sprintf(dch->inst.name, "HFC-USB%d", ++HFC_cnt);

	mISDN_init_dch(dch);

	return 0;
}

static int 
hfc_usb_init_bchannels(hfcusb_data *ta)
{
	int i;
	bchannel_t *bch;

	for (i = 0; i < 2; i++) {
		bch = &ta->bch[i];

		bch->channel = i + 1;

		mISDN_init_instance(&bch->inst, &hfc_usb, ta);

		bch->inst.pid.layermask = ISDN_LAYER(0);
		bch->inst.lock = hfc_usb_lock_dev;
		bch->inst.unlock = hfc_usb_unlock_dev;
		bch->debug = debug;
		sprintf(bch->inst.name, "%s B%d", ta->dch.inst.name, i + 1);

		mISDN_init_bch(bch);

		if (bch->dev) {
			bch->dev->wport.pif.func = hfc_usb_l2l1;
			bch->dev->wport.pif.fdata = bch;
		}
	}

	return 0;
}

static int 
hfc_usb_register_channels(hfcusb_data *ta)
{
	dchannel_t *dch = &ta->dch;
	int err;
	int i;
	mISDN_pid_t pid;

	mISDN_set_dchannel_pid(&pid, ta->protocol, ta->layermask);

	if ((err = hfc_usb.ctrl(NULL, MGR_NEWSTACK | REQUEST, &dch->inst))) {
		printk(HFC_USB_ERR "MGR_NEWSTACK dch (error = %d)\n", err);
		return err;
	}

	for (i = 0; i < 2; i++) {
		if ((err = hfc_usb.ctrl(dch->inst.st, MGR_NEWSTACK | REQUEST, &ta->bch[i].inst))) {
			printk(HFC_USB_ERR "MGR_NEWSTACK bch (error = %d)\n", err);
			hfc_usb.ctrl(dch->inst.st, MGR_DELSTACK | REQUEST, NULL);
			return err;
		}
	}
	
	if ((err = hfc_usb.ctrl(dch->inst.st, MGR_SETSTACK | REQUEST, &pid))) {
		printk(HFC_USB_ERR "MGR_SETSTACK REQUEST dch (error = %d)\n", err);
		hfc_usb.ctrl(dch->inst.st, MGR_DELSTACK | REQUEST, NULL);
		return err;
	}

	hfc_usb.ctrl(dch->inst.st, MGR_CTRLREADY | INDICATION, NULL);

	return 0;
}

static int 
hfc_usb_misdn_register(void)
{
	int err;

	INIT_LIST_HEAD(&hfc_usb_object.ilist);

	hfc_usb_object.owner = THIS_MODULE;
	hfc_usb_object.name = hfc_usb_name;
	hfc_usb_object.own_ctrl = hfc_usb_manager;
	hfc_usb_object.DPROTO.protocol[0] = ISDN_PID_L0_TE_S0 |
				     ISDN_PID_L0_NT_S0;
	hfc_usb_object.DPROTO.protocol[1] = ISDN_PID_L1_NT_S0;
	hfc_usb_object.BPROTO.protocol[1] = ISDN_PID_L1_B_64TRANS |
				     ISDN_PID_L1_B_64HDLC;
	hfc_usb_object.BPROTO.protocol[2] = ISDN_PID_L2_B_TRANS |
				     ISDN_PID_L2_B_RAWDEV;

	if ((err = mISDN_register(&hfc_usb_object))) {
		printk(HFC_USB_ERR "Can't register mISDN object (error: %d)\n", err);
		return err;
	}

	return 0;
}

static void 
hfc_usb_misdn_unregister(void)
{
	int err;

	if ((err = mISDN_unregister(&hfc_usb_object)))
		printk(HFC_USB_ERR "Can't unregister mISDN object (error = %d)\n", err);
}

static int 
hfc_usb_hw_init(hfcusb_data *hfc)
{
	usb_fifo *fifo;
	int i, err;
	u_char b;
	
	/* check the chip id */
	if (read_usb(hfc, HFCUSB_CHIP_ID, &b) != 1) {
		printk(HFC_USB_ERR "Can not read chip id\n");
		return -EFAULT;
	}
	
	printk(HFC_USB_INFO "HFCUSB_CHIP_ID %x\n", b);

	if (b != HFCUSB_CHIPID) {
		printk(HFC_USB_ERR "Invalid chip id 0x%02x\n", b);
		return -EINVAL;
	}

	/* first set the needed config, interface and alternate */
	printk(KERN_INFO "usb_init 1\n");
//	usb_set_configuration(hfc->dev, 1);
	printk(KERN_INFO "usb_init 2\n");
	err = usb_set_interface(hfc->dev, hfc->if_used, hfc->alt_used);

	printk(KERN_INFO "usb_init usb_set_interface return %d\n", err);
	/* now we initialize the chip */
	write_usb(hfc, HFCUSB_CIRM, 8);	    // do reset
	write_usb(hfc, HFCUSB_CIRM, 0x10);	// aux = output, reset off

	// set USB_SIZE to match the the wMaxPacketSize for INT or BULK transfers
	write_usb(hfc, HFCUSB_USB_SIZE, (hfc->packet_size / 8) | ((hfc->packet_size / 8) << 4));

	// set USB_SIZE_I to match the the wMaxPacketSize for ISO transfers
	write_usb(hfc, HFCUSB_USB_SIZE_I, hfc->iso_packet_size);

	/* enable PCM/GCI master mode */
	write_usb(hfc, HFCUSB_MST_MODE1, 0);	/* set default values */
	write_usb(hfc, HFCUSB_MST_MODE0, 1);	/* enable master mode */

	/* init the fifos */
	write_usb(hfc, HFCUSB_F_THRES, (HFCUSB_TX_THRESHOLD/8) |((HFCUSB_RX_THRESHOLD/8) << 4));

	fifo = hfc->fifos;
	for(i = 0; i < HFCUSB_NUM_FIFOS; i++)
	{
		write_usb(hfc, HFCUSB_FIFO, i);	/* select the desired fifo */
		fifo[i].skbuff = NULL;	/* init buffer pointer */
		fifo[i].max_size = (i <= HFCUSB_B2_RX) ? MAX_BCH_SIZE : MAX_DFRAME_LEN;
		fifo[i].last_urblen=0;
		write_usb(hfc, HFCUSB_HDLC_PAR, ((i <= HFCUSB_B2_RX) ? 0 : 2));	    // set 2 bit for D- & E-channel
		write_usb(hfc, HFCUSB_CON_HDLC, ((i==HFCUSB_D_TX) ? 0x09 : 0x08));	// rx hdlc, enable IFF for D-channel
		write_usb(hfc, HFCUSB_INC_RES_F, 2);	/* reset the fifo */
	}

	write_usb(hfc, HFCUSB_CLKDEL, 0x0f);	 /* clock delay value */
	write_usb(hfc, HFCUSB_STATES, 3 | 0x10); /* set deactivated mode */
	write_usb(hfc, HFCUSB_STATES, 3);	     /* enable state machine */

	write_usb(hfc, HFCUSB_SCTRL_R, 0);	     /* disable both B receivers */
	write_usb(hfc, HFCUSB_SCTRL, 0x40);	     /* disable B transmitters + capacitive mode */

	// set both B-channel to not connected
	hfc->b_mode[0]=L1_MODE_NULL;
	hfc->b_mode[1]=L1_MODE_NULL;

	hfc->l1_activated=FALSE;
	hfc->led_state=0;
	hfc->led_new_data=0;

	/* init the t3 timer */
	init_timer(&hfc->t3_timer);
	hfc->t3_timer.data = (long) hfc;
	hfc->t3_timer.function = (void *) l1_timer_expire_t3;
	/* init the t4 timer */
	init_timer(&hfc->t4_timer);
	hfc->t4_timer.data = (long) hfc;
	hfc->t4_timer.function = (void *) l1_timer_expire_t4;
	/* init the led timer */
	init_timer(&hfc->led_timer);
	hfc->led_timer.data = (long) hfc;
	hfc->led_timer.function = (void *) led_timer;
	// trigger 4 hz led timer
	hfc->led_timer.expires = jiffies + (LED_TIME * HZ) / 1000;
	if(!timer_pending(&hfc->led_timer)) add_timer(&hfc->led_timer);

	// init the background machinery for control requests
	hfc->ctrl_read.bRequestType = 0xc0;
	hfc->ctrl_read.bRequest = 1;
	hfc->ctrl_read.wLength = 1;
	hfc->ctrl_write.bRequestType = 0x40;
	hfc->ctrl_write.bRequest = 0;
	hfc->ctrl_write.wLength = 0;
	usb_fill_control_urb(hfc->ctrl_urb, hfc->dev, hfc->ctrl_out_pipe,(u_char *) & hfc->ctrl_write, NULL, 0, ctrl_complete, hfc);
					
	/* Init All Fifos */
	for(i = 0; i < HFCUSB_NUM_FIFOS; i++)
	{
		hfc->fifos[i].iso[0].purb = NULL;
		hfc->fifos[i].iso[1].purb = NULL;
		hfc->fifos[i].active = 0;
	}

	list_add_tail(&hfc->list, &hfc_usb.ilist);
	lock_HW_init(&hfc->lock);

#warning TODO: HACK! HACK!	
	hfc->protocol = 0x12;
	hfc->layermask = 3;
	
	hfc_usb_init_dchannel(hfc);
	hfc_usb_init_bchannels(hfc);
	hfc_usb_register_channels(hfc);
	hfc_usb_start_fifos(hfc);

	handle_led(hfc,LED_POWER_ON);

	return 0;
}

/******************************************************************************/
/* USB related functions                                                      */
/******************************************************************************/

static int 
hfc_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	return hfc_usb_probe_old(intf, id);
}

static void 
hfc_usb_disconnect(struct usb_interface *intf)
{
	hfc_usb_disconnect_old(intf);
}

static struct usb_device_id hfc_usb_id_table[] = {
	{USB_DEVICE(0x07b0, 0x0007)},	/* Billion USB TA 2 */
	{USB_DEVICE(0x0742, 0x2008)},	/* Stollmann USB TA */
	{USB_DEVICE(0x0959, 0x2bd0)},	/* Colognechip USB eval TA */
	{USB_DEVICE(0x08e3, 0x0301)},	/* OliTec ISDN USB */
	{USB_DEVICE(0x0675, 0x1688)},	/* DrayTec ISDN USB */
	{USB_DEVICE(0x07fa, 0x0846)},	/* Bewan ISDN USB TA */
	{}				/* end with an all-zeroes entry */
};

static struct usb_driver hfc_usb_driver = {
	.owner = THIS_MODULE,
	.name =	hfc_usb_name,
	.id_table = hfc_usb_id_table,
	.probe = hfc_usb_probe,
	.disconnect = hfc_usb_disconnect,
};

/******************************************************************************/
/* Module related functions                                                   */
/******************************************************************************/

static int __init 
hfc_usb_init(void)
{
	int err;

	printk(HFC_USB_INFO "Driver module revision %s loaded\n", hfc_usb_revision);

	if ((err = hfc_usb_misdn_register()))
	{
		return err;
	}

	if ((err = usb_register(&hfc_usb_driver)))
	{
		printk(HFC_USB_ERR "Unable to register module at usb stack (error = %d)\n", err);
		hfc_usb_misdn_unregister();

		return err;
	}

	return 0;
}

static void __exit 
hfc_usb_exit(void)
{
	usb_deregister(&hfc_usb_driver);
	hfc_usb_misdn_unregister();
}

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("HFC-USB mISDN driver");
MODULE_DEVICE_TABLE(usb, hfc_usb_id_table);
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "1i");

module_init(hfc_usb_init);
module_exit(hfc_usb_exit);
