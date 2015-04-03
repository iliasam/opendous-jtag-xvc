/***************************************************************************
 *   Copyright (C) 2009 by Zachary T Welch <zw@superlucidity.net>          *
 *                                                                         *
 *   Copyright (C) 2011 by Mauro Gamba <maurillo71@gmail.com>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
//Code in this file is particly taken from OpenOCD project

#include "stdafx.h"
#include "usb_functions.h"
#include <stdio.h>


static int jtag_libusb_match(struct usb_device *dev,
		const uint16_t vid, const uint16_t pid)
{
	if (dev->descriptor.idVendor == vid &&dev->descriptor.idProduct == pid) 
	{
		return 1;
	}
	
	return 0;
}

int jtag_libusb_open(uint16_t vid, uint16_t pid,
		struct usb_dev_handle **out)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_init();

	usb_find_busses();
	usb_find_devices();


	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next){
			if (!jtag_libusb_match(dev, vid, pid))
				continue;

			*out = usb_open(dev);
			if (NULL == *out)
				return -errno;
			return 0;
		}
	}
	return -ENODEV;
}

void jtag_libusb_close(struct usb_dev_handle *dev)
{
	/* Close device */
	usb_close(dev);
}

int jtag_libusb_control_transfer(struct usb_dev_handle *dev, uint8_t requestType,
		uint8_t request, uint16_t wValue, uint16_t wIndex, char *bytes,
		uint16_t size, unsigned int timeout)
{
	int transferred = 0;

	transferred = usb_control_msg(dev, requestType, request, wValue, wIndex,
				bytes, size, timeout);

	if (transferred < 0)
		transferred = 0;

	return transferred;
}



int jtag_libusb_bulk_write(struct usb_dev_handle *dev, int ep, char *bytes,
		int size, int timeout)
{
	return usb_bulk_write(dev, ep, bytes, size, timeout);
}

int jtag_libusb_bulk_read(struct usb_dev_handle *dev, int ep, char *bytes,
		int size, int timeout)
{
	return usb_bulk_read(dev, ep, bytes, size, timeout);
}

int jtag_libusb_set_configuration(struct usb_dev_handle *devh,	int configuration)
{
	struct usb_device *udev = usb_device(devh);
	return usb_set_configuration(devh, udev->config[configuration].bConfigurationValue);
}


int jtag_libusb_claim_interface(struct usb_dev_handle *devh, int iface)
{
	return usb_claim_interface(devh, iface);
};


