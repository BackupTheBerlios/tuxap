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

struct reg_request {
	struct list_head list;
	u8 reg;
	u8 val;
};

#define HFC_USB_DEVICE(vendor_id, product_id, led_info_idx) USB_DEVICE(vendor_id, product_id), .driver_info = led_info_idx

#define HFC_REG_WR	0x0000
#define HFC_REG_RD	0x0001

#endif
