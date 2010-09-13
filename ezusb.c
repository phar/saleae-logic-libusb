/*
 * This file is part of the sigrok project.
 *
 * Copyright (C) 2010 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Helper functions for the Cypress EZ-USB / FX2 series chips.
 */

#include <libusb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "slogic.h"
#include "usbutil.h"
#include "log.h"
//#include "config.h"

int ezusb_reset(struct libusb_device_handle *hdl, int set_clear){
int err;
unsigned char buf[1];

	err = 0;
	log_printf(DEBUG,"setting CPU reset mode %s...\n", set_clear ? "on" : "off");
	buf[0] = set_clear ? 1 : 0;
	if((err = libusb_control_transfer(hdl, LIBUSB_REQUEST_TYPE_VENDOR, 0xa0, 0xe600, 0x0000, buf, 1, 100)) < 0){
		perror("ezusb_reset");
		
	}
	return err;
}

int ezusb_install_firmware(libusb_device_handle *hdl, const char *filename){
FILE *fw;
int offset, chunksize, err, result;
unsigned char buf[4096];

	log_printf(DEBUG,"Uploading firmware at %s\n", filename);
	if ((fw = fopen(filename, "r")) == NULL) {
		perror(filename);
		return 1;
	}

	result = offset = 0;
	while (1) {
		chunksize = fread(buf, 1, sizeof(buf), fw);
		if (chunksize == 0)
			break;
		if((err = libusb_control_transfer(hdl, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT, 0xa0, offset, 0x0000, buf, chunksize, 100)) < 0) {
			//g_warning("Unable to send firmware to device: %d", err);
			result = 1;
			break;
		}
		log_printf(DEBUG,"Uploaded %d bytes\n", chunksize);
		offset += chunksize;
	}
	fclose(fw);
	log_printf(DEBUG,"Firmware upload done\n");

	return result;
}

