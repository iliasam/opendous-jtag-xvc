#ifndef OPENDOUS_H
#define OPENDOUS_H

/* JTAG usb commands */
#define JTAG_CMD_TAP_OUTPUT		0x0
#define JTAG_CMD_SET_TRST		0x1
#define JTAG_CMD_SET_SRST		0x2
#define JTAG_CMD_READ_INPUT		0x3
#define JTAG_CMD_TAP_OUTPUT_EMU	0x4
#define JTAG_CMD_SET_DELAY		0x5
#define JTAG_CMD_SET_SRST_TRST	0x6
#define JTAG_CMD_READ_CONFIG	0x7

//opendous_jtag *opendous_usb_open(void);
//void opendous_usb_close(opendous_jtag *opendous_jtag);

int opendous_init(void);
int opendous_quit(void);

int opendous_usb_write(struct opendous_jtag *opendous_jtag, int out_length);
int opendous_usb_read(struct opendous_jtag *opendous_jtag);
int opendous_usb_message(struct opendous_jtag *opendous_jtag, int out_length, int in_length);
int opendous_send_data(int tap_length);
int io_scan(unsigned char  *TMS, unsigned char *TDI, unsigned char  *TDO, int bits);
void enable_delay(void);
#endif