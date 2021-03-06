/**********************************************************************
 *
 * Copyright (c) 2015 Magnus Lundin
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
***********************************************************************/

#include "usb_dev.h"

usb_dev_state_type usb_device_state = USB_UNCONNECTED;
uint32_t  usb_configuration_number = 0;
uint32_t  usb_interface_number = 0;

//__attribute__ ((section(".commsram"))) usb_ep_buffer_t ep_buffers[16];
__attribute__ ((section(".commbuffer"))) usb_ep_buffer_t ep_buffers[USB_EPBUF_NUM];
usb_ep_config_t config_ep_in[3] = { {0, ep_buffers, NULL}, {0, ep_buffers+1, NULL}, {0, ep_buffers+2, NULL} };
usb_ep_config_t config_ep_out[3] = { {0, ep_buffers+3, NULL}, {0, ep_buffers+4, NULL}, {0, ep_buffers+5, NULL} };


/* Hardware independent routines */

/* Class, driver and EP dependent request handlers are wealky defined to be overloaded */
void usb_class_request(USB_SETUP_PACKET * request) __attribute__((weak));
void usb_class_request(USB_SETUP_PACKET * request) {
	usb_send_status_stall();
}

void usb_set_configuration(uint32_t wValue) __attribute__((weak));

void usb_set_configuration(uint32_t wValue) {
	usb_configuration_number = wValue;
	usb_device_state = USB_CONFIGURED;
}

void usb_set_interface(uint32_t wValue) __attribute__((weak));

void usb_set_interface(uint32_t wValue) {
	usb_interface_number = wValue;
}

/*Basic request handling */

int usb_get_descriptor(USB_SETUP_PACKET * request) {
	uint32_t i;
	for (i=0; i<descriptor_table_length; i++) {
		if ((descriptor_table[i].wValue==request->wValue) && (descriptor_table[i].wIndex==request->wIndex)) {
			uint32_t length = descriptor_table[i].length;
			if (request->wLength < length) length = request->wLength;
			usb_tx_ep0(descriptor_table[i].address, length );
			usb_read_status_OUT();
			return 1;				
		}
	}
	usb_send_status_stall();
	return 0;
}

void usb_control(USB_SETUP_PACKET * request) {
	/* Standard Requests */
	if (request->bmRequestType == (STANDARD_REQUEST|DEVICE_RECIPIENT|USB_DIR_OUT)) {
		if (request->bRequest == SET_ADDRESS) {
			usb_send_status_IN();
			usb_set_address(request->wValue);
			usb_device_state = USB_ADDRESSED;
			return;
		}
		if (request->bRequest == SET_CONFIGURATION) {
			usb_set_configuration(request->wValue);
		}
		/* Any unhandled request, we nod and send OK status */
		usb_send_status_IN();
		return;
	}
	if (request->bmRequestType == (STANDARD_REQUEST|INTERFACE_RECIPIENT|USB_DIR_OUT)) {
		if (request->bRequest == SET_INTERFACE) {
			usb_set_interface(request->wValue);			
		}
		usb_send_status_IN();
		return;
	}
	if (request->bmRequestType == (STANDARD_REQUEST|DEVICE_RECIPIENT|USB_DIR_IN)) {
		if (request->bRequest == GET_STATUS) {
			uint32_t zero = 0;
			usb_tx_ep0((uint8_t *)(&zero), 2);
			usb_read_status_OUT();
		}
		if (request->bRequest == GET_DESCRIPTOR) {
			usb_get_descriptor(request);
			/* We should handle requests for nonexistent descriptors with stall or nak */
		}
		return;
	}
	/* Class Requests */
	if ((request->bmRequestType & REQUEST_TYPE) == (CLASS_REQUEST)) {
		usb_class_request(request);
		return;
	}
	/* At this point there is an unsupported request, stall the control EP */
	usb_send_status_stall();
}
