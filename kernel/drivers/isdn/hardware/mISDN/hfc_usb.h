/*
 * hfc_usb.h
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

#ifndef HFC_USB_H
#define HFC_USB_H

struct led_info {
	u8 scheme;    // led display scheme
	u8 bits[4];   // array of 4 possible LED bitmask settings
};

#define LED_OFF		0 // No LED support
#define LED_NORMAL	1 // LEDs are normal
#define LED_INVERTED	2 // LEDs are inverted

#define LED_POWER_ON	1
#define LED_POWER_OFF	2
#define LED_S0_ON	3
#define LED_S0_OFF	4
#define LED_B1_ON	5
#define LED_B1_OFF	6
#define LED_B1_DATA	7
#define LED_B2_ON	8
#define LED_B2_OFF	9
#define LED_B2_DATA	10

// time for LED flashing
#define LED_TIME      250

#define EP_NUL -1    			// Endpoint at this position not allowed
#define EP_NOP -2			// all type of endpoints allowed at this position
#define EP_ISO USB_ENDPOINT_XFER_ISOC	// Isochron endpoint mandatory at this position
#define EP_BLK USB_ENDPOINT_XFER_BULK	// Bulk endpoint mandatory at this position
#define EP_INT USB_ENDPOINT_XFER_INT	// Interrupt endpoint mandatory at this position

/*
	to enable much mire debug messages in this driver, define
			VERBOSE_USB_DEBUG and VERBOSE_ISDN_DEBUG
	below
*/

//FIXMEFSC
#define MAX_DFRAME_LEN  260

#define L1_MODE_NULL    0
#define L1_MODE_TRANS   1
#define L1_MODE_HDLC    2
#define L1_MODE_EXTRN   3
#define L1_MODE_HDLC_56K 4
#define L1_MODE_MODEM   7
#define L1_MODE_V32     8
#define L1_MODE_FAX     9

#define VERBOSE_USB_DEBUG
#define VERBOSE_ISDN_DEBUG

#define INCLUDE_INLINE_FUNCS

#define TRUE  1
#define FALSE 0


/***********/
/* defines */
/***********/
#define HFC_CTRL_TIMEOUT	20  //(HZ * USB_CTRL_GET_TIMEOUT)
/* 5ms timeout writing/reading regs */
#define HFC_TIMER_T3     8000      /* timeout for l1 activation timer */
#define HFC_TIMER_T4     500       /* time for state change interval */

#define HFCUSB_L1_STATECHANGE   0  /* L1 state changed */
#define HFCUSB_L1_DRX           1  /* D-frame received */
#define HFCUSB_L1_ERX           2  /* E-frame received */
#define HFCUSB_L1_DTX           4  /* D-frames completed */

#define MAX_BCH_SIZE        2048   /* allowed B-channel packet size */

#define HFCUSB_RX_THRESHOLD 64     /* threshold for fifo report bit rx */
#define HFCUSB_TX_THRESHOLD 64     /* threshold for fifo report bit tx */

#define HFCUSB_CHIP_ID    0x16     /* Chip ID register index */
#define HFCUSB_CIRM       0x00     /* cirm register index */
#define HFCUSB_USB_SIZE   0x07     /* int length register */
#define HFCUSB_USB_SIZE_I 0x06     /* iso length register */
#define HFCUSB_F_CROSS    0x0b     /* bit order register */
#define HFCUSB_CLKDEL     0x37     /* bit delay register */
#define HFCUSB_CON_HDLC   0xfa     /* channel connect register */
#define HFCUSB_HDLC_PAR   0xfb
#define HFCUSB_SCTRL      0x31     /* S-bus control register (tx) */
#define HFCUSB_SCTRL_E    0x32     /* same for E and special funcs */
#define HFCUSB_SCTRL_R    0x33     /* S-bus control register (rx) */
#define HFCUSB_F_THRES    0x0c     /* threshold register */
#define HFCUSB_FIFO       0x0f     /* fifo select register */
#define HFCUSB_F_USAGE    0x1a     /* fifo usage register */
#define HFCUSB_MST_MODE0  0x14
#define HFCUSB_MST_MODE1  0x15
#define HFCUSB_P_DATA     0x1f
#define HFCUSB_INC_RES_F  0x0e
#define HFCUSB_STATES     0x30

#define HFCUSB_CHIPID 0x40         /* ID value of HFC-USB */

/******************/
/* fifo registers */
/******************/
#define HFCUSB_NUM_FIFOS   8       /* maximum number of fifos */
#define HFCUSB_B1_TX       0       /* index for B1 transmit bulk/int */
#define HFCUSB_B1_RX       1       /* index for B1 receive bulk/int */
#define HFCUSB_B2_TX       2
#define HFCUSB_B2_RX       3
#define HFCUSB_D_TX        4
#define HFCUSB_D_RX        5
#define HFCUSB_PCM_TX      6
#define HFCUSB_PCM_RX      7

#define ISOC_PACKETS_D	8
#define ISOC_PACKETS_B	8
#define ISO_BUFFER_SIZE	128

// ISO send definitions
#define SINK_MAX	68
#define SINK_MIN	48
#define SINK_DMIN	12
#define SINK_DMAX	18
#define BITLINE_INF	(-64*8)

/***************************************************************/
/* structure defining input+output fifos (interrupt/bulk mode) */
/***************************************************************/

struct usb_fifo;			/* forward definition */
typedef struct iso_urb_struct
{
	struct urb *purb;
	__u8 buffer[ISO_BUFFER_SIZE];	/* buffer incoming/outgoing data */
	struct usb_fifo *owner_fifo;	// pointer to owner fifo
} iso_urb_struct;


struct hfcusb_data;			/* forward definition */
typedef struct usb_fifo
{
	int transfer_mode;
	int num;			/* fifo index attached to this structure */
	int active;			/* fifo is currently active */
	int hdlc_complete;
	int got_hdr;
	struct hfcusb_data *hfc;	/* pointer to main structure */
	int pipe;			/* address of endpoint */
	__u8 usb_packet_maxlen;		/* maximum length for usb transfer */
	unsigned int max_size;		/* maximum size of receive/send packet */
	__u8 intervall;			/* interrupt interval */
	struct sk_buff *skbuff; 	/* actual used buffer */
	struct urb *urb;		/* transfer structure for usb routines */
	__u8 buffer[128];		/* buffer incoming/outgoing data */
	int bit_line;			/* how much bits are in the fifo? */

	iso_urb_struct iso[2];		/* need two urbs to have one always for pending */
	int delete_flg;			/* only delete skbuff once */
	int last_urblen;		/* remember length of last packet */

} usb_fifo;

/*********************************************/
/* structure holding all data for one device */
/*********************************************/
typedef struct hfcusb_data
{
	struct list_head list;
	mISDN_HWlock_t lock;
	dchannel_t dch;
	bchannel_t bch[2];
	int protocol;
	int layermask;
	struct led_info *led;
	struct usb_device *dev;			/* our device */
	struct list_head reg_request_list;
	spinlock_t reg_request_lock;
	int reg_request_busy;

	int disc_flag;      // TRUE if device was unplugged, so avoid some USB actions...

	int if_used;
	int alt_used;				/* used alternate config */
	int ctrl_in_pipe;			/* handles for control pipe */
	int ctrl_out_pipe;			/* handles for control pipe */

	int b_mode[2];				// B-channel mode

	int l1_activated;			// layer 1 activated

	/* control pipe background handling */
	struct urb *ctrl_urb;			/* transfer structure for control channel */

	struct usb_ctrlrequest ctrl_write;	/* buffer for control write request */
	struct usb_ctrlrequest ctrl_read;	/* same for read request */

	__u8 led_state,led_new_data,led_b_active;

	volatile __u8 threshold_mask;		/* threshold actually reported */
	volatile __u8 bch_enables;		/* or mask for sctrl_r and sctrl register values */

	usb_fifo fifos[HFCUSB_NUM_FIFOS];	/* structure holding all fifo data */

	volatile __u8 l1_state;			/* actual l1 state */
	struct timer_list t3_timer;		/* timer 3 for activation/deactivation */
	struct timer_list t4_timer;		/* timer 4 for activation/deactivation */
	struct timer_list led_timer;		/* timer flashing leds */

} hfcusb_data;

struct reg_request {
	struct list_head list;
	u8 reg;
	u8 val;
};

#define HFC_USB_DEVICE(vendor_id, product_id, led_info_idx) USB_DEVICE(vendor_id, product_id), .driver_info = led_info_idx

#define HFC_REG_WR	0x0000
#define HFC_REG_RD	0x0001

#endif
