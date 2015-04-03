#ifndef USB_FUNCTIONS_H
#define USB_FUNCTIONS_H

#define ENODEV          19      /* No such device */
#define 	ERROR_OK   (0)
#define 	ERROR_JTAG_INIT_FAILED   (-100)
#define     ERROR_JTAG_QUEUE_FAILED   (-104)

#include "stdafx.h"
#include "stdint.h"

static int jtag_libusb_match(struct usb_device *dev,
		const uint16_t vid, const uint16_t pid);
int jtag_libusb_open(uint16_t vid, uint16_t pid,
		struct usb_dev_handle **out);
void jtag_libusb_close(struct usb_dev_handle *dev);
int jtag_libusb_bulk_write(struct usb_dev_handle *dev, int ep, char *bytes,
		int size, int timeout);
int jtag_libusb_bulk_read(struct usb_dev_handle *dev, int ep, char *bytes,
		int size, int timeout);
int jtag_libusb_set_configuration(struct usb_dev_handle *devh,	int configuration);

int jtag_libusb_claim_interface(struct usb_dev_handle *devh, int iface);


#endif