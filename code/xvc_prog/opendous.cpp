/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2009 by Cahya Wirawan <cahya@gmx.at>                    *
 *   Based on opendous driver by Vladimir Fonov                            *
 *                                                                         *
 *   Copyright (C) 2009 by Vladimir Fonov <vladimir.fonov@gmai.com>        *
 *   Based on J-link driver by  Juergen Stuber                             *
 *                                                                         *
 *   Copyright (C) 2007 by Juergen Stuber <juergen@jstuber.net>            *
 *   based on Dominic Rath's and Benedikt Sauter's usbprog.c               *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
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
#include "opendous.h"

#define M_BUFFERSIZE 128
#define OPENDOUS_MAX_VIDS_PIDS 4
#define OPENDOUS_TAP_BUFFER_SIZE 65536
#define OPENDOUS_MAX_TAP_TRANSMIT	((M_BUFFERSIZE)-10)
#define OPENDOUS_MAX_INPUT_DATA		(OPENDOUS_MAX_TAP_TRANSMIT*4)
#define OPENDOUS_USB_BUFFER_SIZE  (M_BUFFERSIZE)
#define OPENDOUS_IN_BUFFER_SIZE   (OPENDOUS_USB_BUFFER_SIZE)
#define OPENDOUS_OUT_BUFFER_SIZE  (OPENDOUS_USB_BUFFER_SIZE)
#define OPENDOUS_USB_TIMEOUT      1000
#define OPENDOUS_WRITE_ENDPOINT   (0x02)
#define OPENDOUS_READ_ENDPOINT    (0x81)

struct opendous_probe {
	char *name;
	uint16_t VID[OPENDOUS_MAX_VIDS_PIDS];
	uint16_t PID[OPENDOUS_MAX_VIDS_PIDS];
	uint8_t READ_EP;
	uint8_t WRITE_EP;
	uint8_t CONTROL_TRANSFER;
	int BUFFERSIZE;
};

struct pending_scan_result {
	int first;	/* First bit position in tdo_buffer to read */
	int length; /* Number of bits to read */
	struct scan_command *command; /* Corresponding scan command */
	uint8_t *buffer;
};

struct opendous_jtag {
	struct usb_dev_handle *usb_handle;
};

static struct opendous_jtag *opendous_jtag_handle;
static struct opendous_probe *opendous_probe;

static uint8_t *usb_in_buffer;
static uint8_t *usb_out_buffer;

//static int tap_length;
static uint8_t tms_buffer[OPENDOUS_TAP_BUFFER_SIZE];
static uint8_t tdo_buffer[OPENDOUS_TAP_BUFFER_SIZE];

static int last_tms;

int last_received_bytes_cnt = 0;


opendous_jtag *opendous_usb_open(void)
{
	struct opendous_jtag *result;
	struct usb_dev_handle *devh;

	if (jtag_libusb_open(0x03eb, 0x204f, &devh) != ERROR_OK)
		return NULL;
		
	printf("Opendous opened\n");
	jtag_libusb_set_configuration(devh, 0);
	jtag_libusb_claim_interface(devh, 0);

	result = (opendous_jtag *) malloc(sizeof(opendous_jtag));
	result->usb_handle = devh;
	return result;
}


int opendous_init(void)
{
	opendous_jtag_handle = opendous_usb_open();

	usb_in_buffer = (uint8_t*)malloc(M_BUFFERSIZE * sizeof (uint8_t));
	usb_out_buffer = (uint8_t*)malloc(M_BUFFERSIZE * sizeof (uint8_t));

	if (opendous_jtag_handle == 0) {
		printf("Cannot find opendous Interface! Please check connection and permissions.\n");
		return ERROR_JTAG_INIT_FAILED;
	}

	//enable_delay();

	return ERROR_OK;
}
void opendous_usb_close(opendous_jtag *opendous_jtag)
{
	jtag_libusb_close(opendous_jtag->usb_handle);
	free(opendous_jtag);
}

int opendous_quit(void)
{
	opendous_usb_close(opendous_jtag_handle);

	if (usb_out_buffer) {
		free(usb_out_buffer);
		usb_out_buffer = NULL;
	}

	if (usb_in_buffer) {
		free(usb_in_buffer);
		usb_in_buffer = NULL;
	}

	return ERROR_OK;
}

/* Write data from out_buffer to USB. */
int opendous_usb_write(struct opendous_jtag *opendous_jtag, int out_length)
{
	int result;
	int i;

	if (out_length > OPENDOUS_OUT_BUFFER_SIZE) {
		printf("opendous_jtag_write illegal out_length=%d (max=%d)\n", out_length, OPENDOUS_OUT_BUFFER_SIZE);
		return -1;
	}
/*
	if (opendous_probe->CONTROL_TRANSFER) 
	{
		result = jtag_libusb_control_transfer(opendous_jtag->usb_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
			FUNC_WRITE_DATA, 0, 0, (char *) usb_out_buffer, out_length, OPENDOUS_USB_TIMEOUT);
	} else 
	*/
	{
		
		result = jtag_libusb_bulk_write(opendous_jtag->usb_handle, OPENDOUS_WRITE_ENDPOINT, \
			(char *)usb_out_buffer, out_length, OPENDOUS_USB_TIMEOUT);
			
	}
	return result;
}

/* Read data from USB into in_buffer. */
int opendous_usb_read(struct opendous_jtag *opendous_jtag)
{
	int result;
	/*
	if (opendous_probe->CONTROL_TRANSFER) {
		result = jtag_libusb_control_transfer(opendous_jtag->usb_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			FUNC_READ_DATA, 0, 0, (char *) usb_in_buffer, OPENDOUS_IN_BUFFER_SIZE, OPENDOUS_USB_TIMEOUT);
	} else 
	*/
	{
		result = jtag_libusb_bulk_read(opendous_jtag->usb_handle, OPENDOUS_READ_ENDPOINT,
			(char *)usb_in_buffer, OPENDOUS_IN_BUFFER_SIZE, OPENDOUS_USB_TIMEOUT);
	}
	


	return result;
}


/* Send a message and receive the reply. */
int opendous_usb_message(struct opendous_jtag *opendous_jtag, int out_length, int in_length)
{
	int result;

	result = opendous_usb_write(opendous_jtag, out_length);
	if (result == out_length) {
		result = opendous_usb_read(opendous_jtag);
		if (result == in_length)
			return result;
		else {
			printf("usb_bulk_read failed (requested=%d, result=%d)\n", in_length, result);
			return -1;
		}
	} else {
		printf("usb_bulk_write failed (requested=%d, result=%d)\n", out_length, result);
		return -1;
	}
}

int io_scan(unsigned char *TMS, unsigned char  *TDI, unsigned char  *TDO, int bits)
{
	int i;
	int curr_rd_byte;
	int curr_wr_byte;
	int curr_wr_TMS = 1;//0,2,4,6
	int curr_wr_TDI = 0;//1,3,5,7

	for (i=0;i<(bits/4+1);i++){tms_buffer[i] = 0;tdo_buffer[i] = 0;}

	for (i = 0; i < bits; i++)
	{
		curr_rd_byte = i/8;//number of byte from we read
		curr_wr_byte = i/4;//number of byte where we write

		if (TMS[curr_rd_byte] & (1<<(i&7))){
			tms_buffer[curr_wr_byte]|= (1<<curr_wr_TMS);}
		if (TDI[curr_rd_byte] & (1<<(i&7))){
			tms_buffer[curr_wr_byte]|= (1<<curr_wr_TDI);}

		curr_wr_TMS+= 2;
		curr_wr_TMS = curr_wr_TMS & 7;
		curr_wr_TDI+= 2;
		curr_wr_TDI = curr_wr_TDI & 7;
	}
	if (opendous_send_data(bits) != 0) return -1;

	memset(TDO, 0, (bits + 7) / 8);
	memmove(TDO, tdo_buffer, (bits + 7) / 8);

	return 0;
}

void enable_delay(void)
{
	int result;
	tms_buffer[0] = 4;
	tms_buffer[1] = 0;
	usb_out_buffer[2] = JTAG_CMD_SET_DELAY;
	usb_out_buffer[3] = 1;
	result = opendous_usb_message(opendous_jtag_handle, 4, 1);
}



int opendous_send_data(int tap_length)
{
	int byte_length;
	int i, j;
	int result;
	last_received_bytes_cnt = 0;
	int receive;
	//for (i=0;i<sizeof(usb_out_buffer);i++){usb_out_buffer[i] = 0;}


	if (tap_length > 0) 
	{

		byte_length = (tap_length + 3) / 4;
		for (j = 0, i = 0; j <  byte_length;) 
		{
			
			int transmit = byte_length - j;
			if (transmit > OPENDOUS_MAX_TAP_TRANSMIT) 
			{
				transmit = OPENDOUS_MAX_TAP_TRANSMIT;
				receive = (OPENDOUS_MAX_TAP_TRANSMIT) / 2;
				usb_out_buffer[2] = JTAG_CMD_TAP_OUTPUT;
			} else 
			{
				usb_out_buffer[2] = JTAG_CMD_TAP_OUTPUT | ((tap_length % 4) << 4);
				receive = (transmit + 1) / 2;
			}
			usb_out_buffer[0] = (transmit + 1) & 0xff;
			usb_out_buffer[1] = ((transmit + 1) >> 8) & 0xff;

			memmove(usb_out_buffer + 3, tms_buffer + j, transmit);
			result = opendous_usb_message(opendous_jtag_handle, 3 + transmit, receive);
			//printf("data was send\n");
			if (result != receive)
			{
				printf("opendous_tap_execute, wrong result %d, expected %d\n", result, receive);
				return ERROR_JTAG_QUEUE_FAILED;
			}

			memmove(tdo_buffer + i, usb_in_buffer, receive);
			i += receive;
			j += transmit;
		}
	}
	last_received_bytes_cnt = receive;
	return 0;
}




