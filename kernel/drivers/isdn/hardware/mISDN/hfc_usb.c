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

#include "hfc_usb.h"

#define HFC_USB_NAME "hfc_usb"

#define HFC_USB_ERR KERN_ERR HFC_USB_NAME ": "
#define HFC_USB_INFO KERN_INFO HFC_USB_NAME ": "
#define HFC_USB_DEBUG KERN_INFO HFC_USB_NAME ": "

static const char hfc_usb_name[] = HFC_USB_NAME;
static const char hfc_usb_revision[] = "1.0";
static mISDNobject_t hfc_usb;

#define hfc_usb_object hfc_usb

static int debug;
static int HFC_cnt;

/******************************************************************************/
/* Hardware related functions                                                 */
/******************************************************************************/

static inline int hfc_usb_write_reg(hfcusb_data *hfc, u8 reg, u8 val)
{
	return usb_control_msg(hfc->dev, hfc->ctrl_out_pipe, HFC_REG_WR, 0x40, val, reg, NULL, 0, HFC_CTRL_TIMEOUT);
}

static inline int hfc_usb_read_reg(hfcusb_data *hfc, u8 reg, u8 *val)
{
	return usb_control_msg(hfc->dev, hfc->ctrl_in_pipe, HFC_REG_RD, 0xC0, 0, reg, val, sizeof(*val), HFC_CTRL_TIMEOUT);
}

static void
hfc_usb_submit_reg_xs(hfcusb_data *hfc)
{
	int err;
	unsigned long flags;
	struct reg_request *req = NULL;

	spin_lock_irqsave(&hfc->reg_request_lock, flags);
        if ((!hfc->reg_request_busy) && (!list_empty(&hfc->reg_request_list))) {
                req = list_entry(hfc->reg_request_list.next, struct reg_request, list);
		list_del(&req->list);
		hfc->reg_request_busy = 1;
	}
	spin_unlock_irqrestore(&hfc->reg_request_lock, flags);

	if (!req)
		return;

	hfc->ctrl_write.wIndex = req->reg;
	hfc->ctrl_write.wValue = req->val;

	kfree(req);

	hfc->ctrl_urb->pipe = hfc->ctrl_out_pipe;
	hfc->ctrl_urb->setup_packet = (u_char *)&hfc->ctrl_write;
	hfc->ctrl_urb->transfer_buffer = NULL;
	hfc->ctrl_urb->transfer_buffer_length = 0;
	
	err = usb_submit_urb(hfc->ctrl_urb, GFP_ATOMIC);
	printk(HFC_USB_DEBUG "ctrl_start_transfer: submit %d\n", err);
}

static void
hfc_usb_complete_reg_xs(struct urb *urb, struct pt_regs *regs)
{
	unsigned long flags;
	hfcusb_data *hfc = (hfcusb_data *)urb->context;

	printk(HFC_USB_DEBUG "Register access completed\n");

	spin_lock_irqsave(&hfc->reg_request_lock, flags);
	hfc->reg_request_busy = 0;
	spin_unlock_irqrestore(&hfc->reg_request_lock, flags);

	hfc_usb_submit_reg_xs(hfc);
}

static int
hfc_usb_queue_reg_xs(hfcusb_data *hfc, u8 reg, u8 val)
{
	struct reg_request *req;
	unsigned long flags;
	
	printk(HFC_USB_DEBUG "Delayed register access (reg: 0x%01X, val: 0x%01X)\n", reg, val);
	
	if (!(req = kmalloc(sizeof(*req), GFP_ATOMIC))) {
		printk(HFC_USB_ERR "Unable to allocate register request memory\n");
		return -ENOMEM;
	}

	req->reg = reg;
	req->val = val;
	
	spin_lock_irqsave(&hfc->reg_request_lock, flags);
	list_add_tail(&req->list, &hfc->reg_request_list);
	spin_unlock_irqrestore(&hfc->reg_request_lock, flags);
	
	hfc_usb_submit_reg_xs(hfc);
	
	return 0;
}

static int
hfc_usb_set_statemachine(hfcusb_data *hfc, u8 activate)
{
	u8 val = 0;
	
	if (activate)
		val |= 0x03 << 5;
	else
		val |= 0x02 << 5;

	hfc_usb_queue_reg_xs(hfc, HFCUSB_STATES, val);
	
	return 0;
}

static int
hfc_usb_set_fifo_mode(hfcusb_data *hfc, u8 fifo_idx, u8 enable, u8 transparent)
{
	u8 val = 0;
	u8 par_val = 0;

	if (fifo_idx >= HFCUSB_NUM_FIFOS) {
		printk(HFC_USB_ERR "Invalid fifo idx=%d\n", fifo_idx);
		return -EINVAL;
	}

	if ((fifo_idx == HFCUSB_D_TX) || (fifo_idx == HFCUSB_D_RX)) {
		val |= 0x01 << 0;
		par_val |= 0x02;
	}
	
	if (transparent)
		val |= 0x01 << 1;
	
	if (enable)
		val |= 0x02 << 2;

	hfc_usb_write_reg(hfc, HFCUSB_FIFO, fifo_idx);	/* select the desired fifo */
	hfc_usb_write_reg(hfc, HFCUSB_HDLC_PAR, par_val);
	hfc_usb_write_reg(hfc, HFCUSB_CON_HDLC, val);
	hfc_usb_write_reg(hfc, HFCUSB_INC_RES_F, 2);	/* reset the fifo */
	
	return 0;
}

/***************************************************/
/* write led data to auxport & invert if necessary */
/***************************************************/
static void 
hfc_usb_write_led(hfcusb_data *hfc, u8 led_state)
{
	if (led_state != hfc->led_state) {
		hfc->led_state = led_state;
		hfc_usb_queue_reg_xs(hfc, HFCUSB_P_DATA, (hfc->led->scheme == LED_INVERTED) ? ~led_state : led_state);
	}
}

/******************************************/
/* invert B-channel LEDs if data is sent  */
/******************************************/
static void
hfc_usb_led_timer(hfcusb_data * hfc)
{
   	static int cnt = 0;
	u8 led_state = hfc->led_state;

	if (cnt) {
		if (hfc->led_b_active & 1)
			led_state |= hfc->led->bits[2];
		if (hfc->led_b_active & 2)
			led_state |= hfc->led->bits[3];
	} else {
		if ((!(hfc->led_b_active & 1) || hfc->led_new_data & 1))
			led_state &= ~hfc->led->bits[2];
		if ((!(hfc->led_b_active & 2) || hfc->led_new_data & 2))
			led_state &= ~hfc->led->bits[3];
	}

	hfc_usb_write_led(hfc, led_state);
	hfc->led_new_data = 0;

	cnt = !cnt;

	// restart 4 hz timer
	hfc->led_timer.expires = jiffies + (LED_TIME * HZ) / 1000;
	if (!timer_pending(&hfc->led_timer))
		add_timer(&hfc->led_timer);
}

/**************************/
/* handle LED requests    */
/**************************/
static void
hfc_usb_handle_led(hfcusb_data *hfc, int event)
{
	u8 led_state = hfc->led_state;

	// if no scheme -> no LED action
   	if (hfc->led->scheme == LED_OFF)
		return;

	switch(event) {
		case LED_POWER_ON:
			led_state |= hfc->led->bits[0];
			break;
		case LED_POWER_OFF: // no Power off handling
			break;
		case LED_S0_ON:
			led_state |= hfc->led->bits[1];
			break;
		case LED_S0_OFF:
			led_state &= ~hfc->led->bits[1];
			break;
		case LED_B1_ON:
			hfc->led_b_active |= 1;
			break;
		case LED_B1_OFF:
			hfc->led_b_active &= ~1;
			break;
		case LED_B1_DATA:
			hfc->led_new_data |= 1;
			break;
		case LED_B2_ON:
			hfc->led_b_active |= 2;
			break;
		case LED_B2_OFF:
			hfc->led_b_active &= ~2;
			break;
		case LED_B2_DATA:
			hfc->led_new_data |= 2;
			break;
	}

	hfc_usb_write_led(hfc, led_state);
}

static void 
hfc_usb_process_state(hfcusb_data *hfc, u8 new_state)
{
	if (hfc->l1_state == new_state)
		return;

	printk(HFC_USB_INFO "State transition from %d to %d\n", hfc->l1_state, new_state);

	switch(new_state) {
		case 3:
			break;
		case 7:
			hfc_usb_set_statemachine(hfc, 1);
			hfc->l1_activated = 1;
			hfc_usb_handle_led(hfc, LED_S0_ON);
			break;
		default:
			printk(HFC_USB_ERR "Unhandled state %d\n", new_state);
	
	}
	
	hfc->l1_state = new_state;
}

#if 0
static void
collect_rx_frame(usb_fifo *fifo, u8 *data, int len, int finish)
{
	hfcusb_data *hfc = fifo->hfc;

	int transp_mode, fifon;

	fifon = fifo->fifonum;
	transp_mode=0;

	if ((fifon < 4) && (hfc->b_mode[fifon / 2] == L1_MODE_TRANS))
		transp_mode = TRUE;

	//printk(KERN_INFO "HFC-USB: got %d bytes finish:%d max_size:%d fifo:%d\n",len,finish,fifo->max_size,fifon);
	if (!fifo->skbuff)
	{
		// allocate sk buffer
		fifo->skbuff = dev_alloc_skb(fifo->max_size + 3);
		if (!fifo->skbuff) {
			printk(HFC_USB_ERR "Can not allocate buffer (dev_alloc_skb) fifo:%d\n", fifon);
			return;
		}
		
	}

	if (len && ((fifo->skbuff->len + len) < fifo->max_size))
		memcpy(skb_put(fifo->skbuff,len),data,len);
	else
		printk(KERN_INFO "HCF-USB: got frame exceeded fifo->max_size:%d\n", fifo->max_size);

	// give transparent data up, when 128 byte are available
	if (transp_mode && fifo->skbuff->len >= 128)
	{
//FIXMEFSC		fifo->hif->l1l2(fifo->hif,PH_DATA | INDICATION,fifo->skbuff);
		fifo->skbuff = NULL;  // buffer was freed from upper layer
		return;
	}

	// we have a complete hdlc packet
	if (finish) {
		if (!fifo->skbuff->data[fifo->skbuff->len - 1]) {
			skb_trim(fifo->skbuff,fifo->skbuff->len-3);  // remove CRC & status

			//printk(KERN_INFO "HFC-USB: got frame %d bytes on fifo:%d\n",fifo->skbuff->len,fifon);

//FIXMEFSC			if(fifon==HFCUSB_PCM_RX) fifo->hif->l1l2(fifo->hif,PH_DATA_E | INDICATION,fifo->skbuff);
//FIXMEFSC			else fifo->hif->l1l2(fifo->hif,PH_DATA | INDICATION,fifo->skbuff);

			fifo->skbuff = NULL;  // buffer was freed from upper layer
		}
		else
		{
			printk(KERN_INFO "HFC-USB: got frame %d bytes but CRC ERROR!!!\n",fifo->skbuff->len);

			skb_trim(fifo->skbuff,0);  // clear whole buffer
		}
	}

	// LED flashing only in HDLC mode
	if (!transp_mode) {
		if (fifon == HFCUSB_B1_RX)
			hfc_usb_handle_led(hfc, LED_B1_DATA);
		if (fifon == HFCUSB_B2_RX)
			hfc_usb_handle_led(hfc, LED_B2_DATA);
	}
}

static void
hfc_usb_rx_complete_old(struct urb *urb, struct pt_regs *regs)
{
	usb_fifo *fifo = (usb_fifo *)urb->context;
	hfcusb_data *hfc = fifo->hfc;
	
	printk(HFC_USB_INFO "RX event on fifo %d\n", fifo->fifonum);

	int len;
	u8 *buf;

	urb->dev = hfc->dev;	/* security init */

	if ((!fifo->active) || (urb->status)) {
#ifdef VERBOSE_USB_DEBUG
		printk(KERN_INFO "HFC-USB: RX-Fifo %i is going down (%i)\n", fifo->fifonum, urb->status);
#endif
		fifo->urb->interval = 0;	/* cancel automatic rescheduling */
		if (fifo->skbuff) {
			dev_kfree_skb_any(fifo->skbuff);
			fifo->skbuff = NULL;
		}

		return;
	}

	len = urb->actual_length;
	buf = fifo->buffer;

	if (fifo->last_urblen != fifo->usb_packet_maxlen) {
		//if ((buf[0] >> 4) == 3) {
		//	printk(HFC_USB_INFO "Activating L1\n");
		//	hfc_usb_set_statemachine(hfc, 1);
		//}

		// the threshold mask is in the 2nd status byte
		hfc->threshold_mask = buf[1];
		// the S0 state is in the upper half of the 1st status byte
		state_handler(hfc, buf[0] >> 4);
		// if we have more than the 2 status bytes -> collect data
		if (len > 2) {
			printk(HFC_USB_INFO "RX data (end)\n");
			collect_rx_frame(fifo, buf + 2, urb->actual_length - 2, buf[0] & 1);
		}
	} else {
		printk(HFC_USB_INFO "RX data (continued)\n");
		collect_rx_frame(fifo, buf, urb->actual_length, 0);
	}

	fifo->last_urblen = urb->actual_length;
}

static void
hfc_usb_rx_complete(struct urb *urb, struct pt_regs *regs)
{
	usb_fifo *fifo = (usb_fifo *)urb->context;
	hfcusb_data *hfc = fifo->hfc;

	printk(HFC_USB_INFO "RX event on fifo %d\n", fifo->fifonum);

	if (!fifo->active) {
		printk(HFC_USB_ERR "Event on inactive FIFO %d\n", fifo->fifonum);
		return;
	}
	
	if (urb->status) {
		printk(HFC_USB_ERR "Invalid URB status %d\n", urb->status);
		return;
	}
	
	printk(HFC_USB_INFO "Got %d URB bytes\n", urb->actual_length);
	
	if (urb->actual_length >= 2) {
		printk(HFC_USB_INFO "State=0x%01X Err=%d EOF=%d Fill0x%01X\n", fifo->buffer[0] >> 4, (fifo->buffer[0] >> 1) & 0x01, fifo->buffer[0] & 0x01, fifo->buffer[1]);
		hfc_usb_process_state(hfc, fifo->buffer[0] >> 4);
	}
}
#endif

#include "hfc_usb_old.c"

static void
hfc_usb_rx_complete(struct urb *urb, struct pt_regs *regs)
{
	rx_complete(urb, regs);
}

/* stops running iso chain and frees their pending urbs */
static void 
hfc_usb_stop_isoc_chain(usb_fifo *fifo)
{
	int i;

	for (i = 0; i < 2; i++)
	{
		if (fifo->iso[i].purb)
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
				for(packet_nr = 0; packet_nr < num_packets_per_urb; packet_nr++) {
					fifo->iso[i].purb->iso_frame_desc[packet_nr].offset = packet_nr * packet_size;
					fifo->iso[i].purb->iso_frame_desc[packet_nr].length = packet_size;
				}
			}
		}

		fifo->bit_line = BITLINE_INF;
		err = usb_submit_urb(fifo->iso[i].purb, GFP_KERNEL);
		fifo->active = (err == 0) ? 1 : 0;

		if (err < 0) {
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
		if (!fifo->urb) {
			printk(HFC_USB_ERR "Unable allocate URB\n");
			return -ENOMEM;
		}
	}

	usb_fill_int_urb(fifo->urb, fifo->hfc->dev, fifo->pipe, fifo->buffer,
				 fifo->usb_packet_maxlen, hfc_usb_rx_complete, fifo, fifo->intervall);
	fifo->active = 1;		/* must be marked active */
	err = usb_submit_urb(fifo->urb, GFP_KERNEL);

	if(err)	{
		printk(HFC_USB_ERR "Unable to submit URB (error = %d)\n",  err);
		fifo->active = 0;
		fifo->skbuff = NULL;
	}
	
	return err;
}

static int 
hfc_usb_start_rx_fifo(usb_fifo *fifo, int num_packets_per_urb)
{
	switch(fifo->transfer_mode) {
		case USB_ENDPOINT_XFER_INT:
			return hfc_usb_start_int_fifo(fifo);
		case USB_ENDPOINT_XFER_ISOC:
#warning TODO: Look at fifo and computate num_pakets_per_urb
			return hfc_usb_start_isoc_chain(fifo, num_packets_per_urb, rx_iso_complete, 16);
		default:
			printk(HFC_USB_ERR "Invalid FIFO configuration!\n");
			BUG();
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

static void
hfc_usb_stop_fifos(hfcusb_data *hfc)
{
	int i;

	/* tell all fifos to terminate */
	for (i = 0; i < HFCUSB_NUM_FIFOS; i++) {
		if (hfc->fifos[i].transfer_mode == USB_ENDPOINT_XFER_ISOC) {
			if (hfc->fifos[i].active > 0)
	    			hfc_usb_stop_isoc_chain(&hfc->fifos[i]);
		} else {
			if (hfc->fifos[i].active > 0)
				hfc->fifos[i].active = 0;

			if (hfc->fifos[i].urb) {
				usb_unlink_urb(hfc->fifos[i].urb);
				usb_free_urb(hfc->fifos[i].urb);
				hfc->fifos[i].urb = NULL;
			}
		}

		hfc->fifos[i].active = 0;
	}
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
	bchannel_t	*bch;
	int		ret = -EINVAL;
	mISDN_head_t	*hh;

	printk(HFC_USB_ERR "l2l1 called!\n");

	if ((!hif) || (!skb)) {
		BUG();
		return -EINVAL;
	}

	hh = mISDN_HEAD_P(skb);
	bch = hif->fdata;

	switch(hh->prim) {
		case PH_DATA_REQ:
			printk("PH_DATA_REQ\n");
		case DL_DATA | REQUEST:
			printk("DL_DATA | REQUEST\n");
//#warning TODO: hier muss abgefragt werden, ob skb->len <= 0 ist, und ggf. ein -EINVAL zurückliefern, sonst wird zwar einmal confirmed, aber es regt sich nichts mehr. dies bitte auch für den d-kanal überdenken, sowie für alle andere kartentreiber.
			if (bch->next_skb) {
				printk(KERN_WARNING "%s: next_skb exist ERROR\n", __FUNCTION__);
				return -EBUSY;
			}

			bch->inst.lock(bch->inst.data, 0);
			if (test_and_set_bit(BC_FLG_TX_BUSY, &bch->Flag)) {
				test_and_set_bit(BC_FLG_TX_NEXT, &bch->Flag);
				bch->next_skb = skb;
				bch->inst.unlock(bch->inst.data);
				return 0;
			} else {
				bch->tx_len = skb->len;
				memcpy(bch->tx_buf, skb->data, bch->tx_len);
				bch->tx_idx = 0;
				//hfcpci_fill_fifo(bch);
				bch->inst.unlock(bch->inst.data);
				if ((bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV)	&& bch->dev)
					hif = &bch->dev->rport.pif;
				else
					hif = &bch->inst.up;
				skb_trim(skb, 0);
				return if_newhead(hif, hh->prim | CONFIRM, hh->dinfo, skb);
			}
			break;
		case PH_ACTIVATE | REQUEST:
			printk("PH_ACTIVATE | REQUEST\n");
		case DL_ESTABLISH | REQUEST:
			printk("DL_ESTABLISH | REQUEST\n");
			if (test_and_set_bit(BC_FLG_ACTIV, &bch->Flag))
				ret = 0;
			else {
				bch->inst.lock(bch->inst.data, 0);
				//ret = mode_hfcpci(bch, bch->channel,
				//	bch->inst.pid.protocol[1]);
				bch->inst.unlock(bch->inst.data);
			}
			if (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV)
				if (bch->dev)
					if_link(&bch->dev->rport.pif,
						hh->prim | CONFIRM, 0, 0, NULL, 0);
			skb_trim(skb, 0);
			return (if_newhead(&bch->inst.up, hh->prim | CONFIRM, ret, skb));
		case PH_DEACTIVATE | REQUEST:
			printk("PH_DEACTIVATE | REQUEST\n");
		case DL_RELEASE | REQUEST:
			printk("DL_RELEASE | REQUEST\n");
		case MGR_DISCONNECT | REQUEST:
			printk("MGR_DISCONNECT | REQUEST\n");
			bch->inst.lock(bch->inst.data, 0);
			if (test_and_clear_bit(BC_FLG_TX_NEXT, &bch->Flag)) {
				dev_kfree_skb(bch->next_skb);
				bch->next_skb = NULL;
			}
			test_and_clear_bit(BC_FLG_TX_BUSY, &bch->Flag);
			//mode_hfcpci(bch, bch->channel, ISDN_PID_NONE);
			test_and_clear_bit(BC_FLG_ACTIV, &bch->Flag);
			bch->inst.unlock(bch->inst.data);
			skb_trim(skb, 0);
			if (hh->prim != (MGR_DISCONNECT | REQUEST)) {
				if (bch->inst.pid.protocol[2] == ISDN_PID_L2_B_RAWDEV)
					if (bch->dev)
						if_link(&bch->dev->rport.pif,
							hh->prim | CONFIRM, 0, 0, NULL, 0);
				if (!if_newhead(&bch->inst.up, hh->prim | CONFIRM, 0, skb))
					return(0);
			}
			ret = 0;
			break;
		case PH_CONTROL | REQUEST:
			printk("PH_CONTROL | REQUEST\n");
			bch->inst.lock(bch->inst.data, 0);
			if (hh->dinfo == HW_TESTRX_RAW) {
				//ret = set_hfcpci_rxtest(bch, ISDN_PID_L1_B_64TRANS, skb);
			} else if (hh->dinfo == HW_TESTRX_HDLC) {
				//ret = set_hfcpci_rxtest(bch, ISDN_PID_L1_B_64HDLC, skb);
			} else if (hh->dinfo == HW_TESTRX_OFF) {
				//mode_hfcpci(bch, bch->channel, ISDN_PID_NONE);
				ret = 0;
			} else
				ret = -EINVAL;
			bch->inst.unlock(bch->inst.data);
			if (!ret) {
				skb_trim(skb, 0);
				if (!if_newhead(&bch->inst.up, hh->prim | CONFIRM, hh->dinfo, skb))
					return(0);
			}
			break;
		default:
			printk(KERN_WARNING "%s: unknown prim(%x)\n", __FUNCTION__, hh->prim);
			ret = -EINVAL;
		}

	if (!ret)
		dev_kfree_skb(skb);

	return ret;
}

static void
hfc_usb_dhw(dchannel_t *dch)
{
	printk(HFC_USB_ERR "dhw called!\n");
	printk(HFC_USB_ERR "dhw called!\n");
	printk(HFC_USB_ERR "dhw called!\n");
	printk(HFC_USB_ERR "dhw called!\n");
}

static void
hfc_usb_dbusy_timer(hfcusb_data *hfc)
{
	printk(HFC_USB_ERR "dbusy_timer called!\n");
	printk(HFC_USB_ERR "dbusy_timer called!\n");
	printk(HFC_USB_ERR "dbusy_timer called!\n");
	printk(HFC_USB_ERR "dbusy_timer called!\n");
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
		if (channel == 2)
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
hfc_usb_init_dchannel(hfcusb_data *hfc)
{
	dchannel_t *dch = &hfc->dch;

	dch->debug = debug;
	dch->inst.lock = hfc_usb_lock_dev;
	dch->inst.unlock = hfc_usb_unlock_dev;

	mISDN_init_instance(&dch->inst, &hfc_usb, hfc);

	dch->inst.pid.layermask = ISDN_LAYER(0);
	sprintf(dch->inst.name, "HFC-USB%d", ++HFC_cnt);

	mISDN_init_dch(dch);

	hfc->dch.hw_bh = hfc_usb_dhw;
	hfc->dch.dbusytimer.function = (void *)hfc_usb_dbusy_timer;
	hfc->dch.dbusytimer.data = (long)&hfc->dch;
	init_timer(&hfc->dch.dbusytimer);

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

//		hc->chanlimit = 2;
//		mode_hfcpci(&hc->bch[0], 1, -1);
//		mode_hfcpci(&hc->bch[1], 2, -1);
	}

	return 0;
}

static int 
hfc_usb_register_channels(hfcusb_data *hfc)
{
	dchannel_t *dch = &hfc->dch;
	int err;
	int i;
	mISDN_pid_t pid;

	mISDN_set_dchannel_pid(&pid, hfc->protocol, hfc->layermask);

	if (hfc->protocol & 0x10) {
		hfc->dch.inst.pid.protocol[0] = ISDN_PID_L0_NT_S0;
		hfc->dch.inst.pid.protocol[1] = ISDN_PID_L1_NT_S0;
		pid.protocol[0] = ISDN_PID_L0_NT_S0;
		pid.protocol[1] = ISDN_PID_L1_NT_S0;
		hfc->dch.inst.pid.layermask |= ISDN_LAYER(1);
		pid.layermask |= ISDN_LAYER(1);
		if (hfc->layermask & ISDN_LAYER(2))
			pid.protocol[2] = ISDN_PID_L2_LAPD_NET;
		//card->hw.nt_mode = 1;
	} else {
		hfc->dch.inst.pid.protocol[0] = ISDN_PID_L0_TE_S0;
		//card->hw.nt_mode = 0;
	}

	if ((err = hfc_usb.ctrl(NULL, MGR_NEWSTACK | REQUEST, &dch->inst))) {
		printk(HFC_USB_ERR "MGR_NEWSTACK dch (error = %d)\n", err);
		return err;
	}

	for (i = 0; i < 2; i++) {
		if ((err = hfc_usb.ctrl(dch->inst.st, MGR_NEWSTACK | REQUEST, &hfc->bch[i].inst))) {
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
hfc_usb_init_hw(hfcusb_data *hfc, u32 packet_size, u32 iso_packet_size)
{
	usb_fifo *fifo;
	int i, err;
	u_char b;
	
	/* check the chip id */
	if (hfc_usb_read_reg(hfc, HFCUSB_CHIP_ID, &b) != 1) {
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
	hfc_usb_write_reg(hfc, HFCUSB_CIRM, 8);	    // do reset
	hfc_usb_write_reg(hfc, HFCUSB_CIRM, 0x10);	// aux = output, reset off

	// set USB_SIZE to match the the wMaxPacketSize for INT or BULK transfers
	hfc_usb_write_reg(hfc, HFCUSB_USB_SIZE, (packet_size / 8) | ((packet_size / 8) << 4));

	// set USB_SIZE_I to match the the wMaxPacketSize for ISO transfers
	hfc_usb_write_reg(hfc, HFCUSB_USB_SIZE_I, iso_packet_size);

	/* enable PCM/GCI master mode */
	hfc_usb_write_reg(hfc, HFCUSB_MST_MODE1, 0);	/* set default values */
	hfc_usb_write_reg(hfc, HFCUSB_MST_MODE0, 1);	/* enable master mode */

	/* init the fifos */
	hfc_usb_write_reg(hfc, HFCUSB_F_THRES, (HFCUSB_TX_THRESHOLD / 8) | ((HFCUSB_RX_THRESHOLD / 8) << 4));

	fifo = hfc->fifos;
	for(i = 0; i < HFCUSB_NUM_FIFOS; i++) {
		fifo[i].skbuff = NULL;	/* init buffer pointer */
		fifo[i].max_size = (i <= HFCUSB_B2_RX) ? MAX_BCH_SIZE : MAX_DFRAME_LEN;
		fifo[i].last_urblen = 0;
		
		if ((err = hfc_usb_set_fifo_mode(hfc, i, 1, 0)))
			printk(HFC_USB_ERR "Can't set FIFO mode for idx=%d, result=%d\n", i, err);
	}

	hfc_usb_write_reg(hfc, HFCUSB_CLKDEL, 0x0f);	 /* clock delay value */
	hfc_usb_write_reg(hfc, HFCUSB_STATES, 3 | 0x10); /* set deactivated mode */
	hfc_usb_write_reg(hfc, HFCUSB_STATES, 3);	     /* enable state machine */

	hfc_usb_write_reg(hfc, HFCUSB_SCTRL_R, 0);	     /* disable both B receivers */
	hfc_usb_write_reg(hfc, HFCUSB_SCTRL, 0x40);	     /* disable B transmitters + capacitive mode */

	// set both B-channel to not connected
	hfc->b_mode[0] = L1_MODE_NULL;
	hfc->b_mode[1] = L1_MODE_NULL;

	hfc->l1_activated = FALSE;
	hfc->led_state = 0;
	hfc->led_new_data = 0;

#if 0
	/* init the t3 timer */
	init_timer(&hfc->t3_timer);
	hfc->t3_timer.data = (long)hfc;
	hfc->t3_timer.function = (void *)l1_timer_expire_t3;

	/* init the t4 timer */
	init_timer(&hfc->t4_timer);
	hfc->t4_timer.data = (long)hfc;
	hfc->t4_timer.function = (void *)l1_timer_expire_t4;
#endif

	/* init the led timer */
	init_timer(&hfc->led_timer);
	hfc->led_timer.data = (long)hfc;
	hfc->led_timer.function = (void *)hfc_usb_led_timer;

	// trigger 4 hz led timer
	hfc->led_timer.expires = jiffies + (LED_TIME * HZ) / 1000;
	if (!timer_pending(&hfc->led_timer))
		add_timer(&hfc->led_timer);

	// init the background machinery for control requests
	hfc->ctrl_read.bRequestType = 0xc0;
	hfc->ctrl_read.bRequest = 1;
	hfc->ctrl_read.wLength = 1;
	hfc->ctrl_write.bRequestType = 0x40;
	hfc->ctrl_write.bRequest = 0;
	hfc->ctrl_write.wLength = 0;

	usb_fill_control_urb(hfc->ctrl_urb, hfc->dev, hfc->ctrl_out_pipe, (u_char *)&hfc->ctrl_write, NULL, 0, hfc_usb_complete_reg_xs, hfc);
					
	/* Init All Fifos */
	for(i = 0; i < HFCUSB_NUM_FIFOS; i++)
	{
		hfc->fifos[i].iso[0].purb = NULL;
		hfc->fifos[i].iso[1].purb = NULL;
		hfc->fifos[i].active = 0;
	}

#warning TODO: HACK! HACK!	
#if 0
	hfc->protocol = 0x12;
	hfc->layermask = 0x03;
#else
	hfc->protocol = 0x02;
	hfc->layermask = 0x0F;
#endif
	
	hfc_usb_handle_led(hfc, LED_POWER_ON);

	return 0;
}

static void 
hfc_usb_deinit_hw(hfcusb_data *hfc)
{
	/* Remove timers */
	if (timer_pending(&hfc->led_timer))
		del_timer(&hfc->led_timer);
}

/******************************************************************************/
/* USB related functions                                                      */
/******************************************************************************/

static struct led_info hfc_led_info[] = {
	{LED_OFF,	{0x04, 0x00, 0x02, 0x01}},
	{LED_INVERTED,	{0x08, 0x40, 0x20, 0x10}},
	{LED_NORMAL,	{0x04, 0x00, 0x02, 0x01}},
	{LED_NORMAL,	{0x02, 0x00, 0x01, 0x04}},
};

#if 0
	{0x07b0, 0x0007, "Billion tiny USB ISDN TA 128", LED_SCHEME1, {0x80,-64,-32,-16}},  /* Billion TA */
	{0x0675, 0x1688, "DrayTec USB ISDN TA",          LED_SCHEME1, {1,2,0,0}},           /* Draytec TA */
#endif

static struct usb_device_id hfc_usb_id_table[] = {
	{HFC_USB_DEVICE(0x07b0, 0x0007, 1)},	/* Billion USB TA 2 */
	{HFC_USB_DEVICE(0x0742, 0x2008, 2)},	/* Stollmann USB TA */
	{HFC_USB_DEVICE(0x0742, 0x2009, 2)},	/* Aceex USB ISDN TA */
	{HFC_USB_DEVICE(0x0742, 0x200A, 2)},	/* OEM USB ISDN TA */
	{HFC_USB_DEVICE(0x0959, 0x2bd0, 0)},	/* Colognechip USB eval TA */
	{HFC_USB_DEVICE(0x08e3, 0x0301, 3)},	/* OliTec ISDN USB */
	{HFC_USB_DEVICE(0x0675, 0x1688, 2)},	/* DrayTec ISDN USB */
	{HFC_USB_DEVICE(0x07fa, 0x0846, 1)},	/* Bewan ISDN USB TA */
	{HFC_USB_DEVICE(0x07fa, 0x0847, 1)},	/* Bewan TA */
	{HFC_USB_DEVICE(0x07b0, 0x0006, 1)},	/* Twister ISDN TA */
	{}					/* end with an all-zeroes entry */
};

// this array represents all endpoints possible in the HCF-USB
// the last entry is the the minimum interval for Interrupt endpoints
static int hfc_usb_ep_cfg[][17]=
{
	// INT in, ISO out config
	{EP_NUL,EP_INT,EP_NUL,EP_INT,EP_NUL,EP_INT,EP_NOP,EP_INT,EP_ISO,EP_NUL,EP_ISO,EP_NUL,EP_ISO,EP_NUL,EP_NUL,EP_NUL,2},
	{EP_NUL,EP_INT,EP_NUL,EP_INT,EP_NUL,EP_INT,EP_NUL,EP_NUL,EP_ISO,EP_NUL,EP_ISO,EP_NUL,EP_ISO,EP_NUL,EP_NUL,EP_NUL,2},
	// ISO in, ISO out config
	{EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_NOP,EP_ISO,2},
	{EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_NUL,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_ISO,EP_NUL,EP_NUL,2},
};

#define NUM_EP_CFG (sizeof(ep_cfg) / sizeof(ep_cfg[0]))

static inline int 
get_ep_cfg_idx(struct usb_host_endpoint *ep)
{
	int ep_cfg_idx = ((ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK) - 1) * 2;

	if (ep->desc.bEndpointAddress & USB_DIR_IN)
		ep_cfg_idx++;

	return ep_cfg_idx;
}

static int
hfc_usb_init_usb(hfcusb_data *hfc, struct usb_interface *intf, int alt_idx, int cfg_idx, int *packet_size, int *iso_packet_size)
{
	struct usb_host_endpoint *ep;
	int ep_cfg_idx;
	int ep_idx;
	usb_fifo *fifo;
	struct usb_host_interface *host_if = &intf->altsetting[alt_idx];
	
	*packet_size = 0;
	*iso_packet_size = 0;

	for (ep_idx = 0; ep_idx < host_if->desc.bNumEndpoints; ep_idx++) {
		ep = &host_if->endpoint[ep_idx];
		ep_cfg_idx = get_ep_cfg_idx(ep);
		fifo = &hfc->fifos[ep_cfg_idx & 7];

		// only initialize used endpoints
		if ((hfc_usb_ep_cfg[cfg_idx][ep_cfg_idx] == EP_NOP) || (hfc_usb_ep_cfg[cfg_idx][ep_cfg_idx] == EP_NUL))
			continue;

		switch(ep->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
			case USB_ENDPOINT_XFER_INT:
				fifo->pipe = usb_rcvintpipe(hfc->dev, ep->desc.bEndpointAddress);
				*packet_size = ep->desc.wMaxPacketSize;
				break;
			case USB_ENDPOINT_XFER_BULK:
				if (ep->desc.bEndpointAddress & USB_DIR_IN)
					fifo->pipe = usb_rcvbulkpipe(hfc->dev, ep->desc.bEndpointAddress);
				else
					fifo->pipe = usb_sndbulkpipe(hfc->dev, ep->desc.bEndpointAddress);
				*packet_size = ep->desc.wMaxPacketSize;
				break;
			case USB_ENDPOINT_XFER_ISOC:
				if (ep->desc.bEndpointAddress & USB_DIR_IN)
					fifo->pipe = usb_rcvisocpipe(hfc->dev, ep->desc.bEndpointAddress);
				else
					fifo->pipe = usb_sndisocpipe(hfc->dev, ep->desc.bEndpointAddress);
				*iso_packet_size = ep->desc.wMaxPacketSize;
				break;
			default:
				fifo->pipe = 0;
				continue;
		
		}

		fifo->fifonum = ep_cfg_idx & 7;
		fifo->hfc = hfc;
		fifo->usb_packet_maxlen = ep->desc.wMaxPacketSize;
		fifo->intervall = ep->desc.bInterval;
		fifo->skbuff = NULL;
		fifo->transfer_mode = ep->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	}

	/* create the control pipes needed for register access */
	hfc->ctrl_in_pipe = usb_rcvctrlpipe(hfc->dev, 0);
	hfc->ctrl_out_pipe = usb_sndctrlpipe(hfc->dev, 0);
	hfc->ctrl_urb = usb_alloc_urb(0, GFP_KERNEL);

	usb_set_intfdata(intf, hfc);

	return 0;
}

static void
hfc_usb_deinit_usb(hfcusb_data *hfc)
{
	usb_unlink_urb(hfc->ctrl_urb);
	usb_free_urb(hfc->ctrl_urb);
	hfc->ctrl_urb = NULL;
}

static int
hfc_usb_scan_intf(struct usb_interface *intf, int *res_alt_idx, int *res_cfg_idx)
{
	int alt_idx;
	int cfg_idx;
	struct usb_host_endpoint *ep;
	int *ep_cfg;
	int ep_cfg_idx;
	int ep_idx;
	struct usb_host_interface *host_if;
	int num_valid_ep;
	
	for (alt_idx = 0; alt_idx < intf->num_altsetting; alt_idx++) {
		num_valid_ep = 0;
		host_if = &intf->altsetting[alt_idx];
	
		for (cfg_idx = 0; cfg_idx < NUM_EP_CFG; cfg_idx++) {
			ep_cfg = hfc_usb_ep_cfg[cfg_idx];

			for (ep_idx = 0; ep_idx < host_if->desc.bNumEndpoints; ep_idx++) {
				ep = &host_if->endpoint[ep_idx];
				ep_cfg_idx = get_ep_cfg_idx(ep);
				
				if (ep_cfg[ep_cfg_idx] == EP_NOP)
					continue;

				if ((ep_cfg[ep_cfg_idx] == EP_NUL) || (ep_cfg[ep_cfg_idx] != ep->desc.bmAttributes) || 
					((ep->desc.bmAttributes == USB_ENDPOINT_XFER_INT) && (ep->desc.bInterval < ep_cfg[16]))) {
					num_valid_ep = 0;
					break;
				}
				
				num_valid_ep++;
			}
			
			/* Found valid configuration */
			if (num_valid_ep) {
				*res_alt_idx = alt_idx;
				*res_cfg_idx = cfg_idx;
				
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int 
hfc_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int alt_idx;
	int cfg_idx;
	int err;
	hfcusb_data *hfc;
	int iso_packet_size;
	int packet_size;

	/* Try to locate usable alt and cfg index */
	if ((err = hfc_usb_scan_intf(intf, &alt_idx, &cfg_idx)))
		return err;

	if (!(hfc = kmalloc(sizeof(*hfc), GFP_KERNEL)))
		return -ENOMEM;

	memset(hfc, 0, sizeof(*hfc));

	hfc->dev = interface_to_usbdev(intf);
	hfc->led = &hfc_led_info[id->driver_info];
	hfc->if_used = intf->altsetting[alt_idx].desc.bInterfaceNumber;
	hfc->alt_used = alt_idx;
	INIT_LIST_HEAD(&hfc->reg_request_list);
	spin_lock_init(&hfc->reg_request_lock);

	/* init usb settings */
	if ((err = hfc_usb_init_usb(hfc, intf, alt_idx, cfg_idx, &packet_size, &iso_packet_size))) {
		kfree(hfc);
		return err;
	}

	/* init the chip */
	if ((err = hfc_usb_init_hw(hfc, packet_size, iso_packet_size))) {
		hfc_usb_deinit_usb(hfc);
		kfree(hfc);
		return err;
	}

	if ((err = hfc_usb_start_fifos(hfc))) {
		hfc_usb_deinit_hw(hfc);
		hfc_usb_deinit_usb(hfc);
		kfree(hfc);
		return err;
	}
	
//	hfc_usb_set_statemachine(hfc, 1);

//	list_add_tail(&hfc->list, &hfc_usb.ilist);
//	lock_HW_init(&hfc->lock);

//	hfc_usb_init_dchannel(hfc);
//	hfc_usb_init_bchannels(hfc);
//	hfc_usb_register_channels(hfc);

	return 0;
}

static void 
hfc_usb_disconnect(struct usb_interface *intf)
{
	hfcusb_data *hfc = usb_get_intfdata(intf);

	printk(HFC_USB_INFO "Device disconnect\n");
	
	usb_set_intfdata(intf, NULL);

	hfc_usb_stop_fifos(hfc);
	hfc_usb_deinit_hw(hfc);
	hfc_usb_deinit_usb(hfc);

	kfree(hfc);
}

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

	if ((err = hfc_usb_misdn_register())) {
		return err;
	}

	if ((err = usb_register(&hfc_usb_driver))) {
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
