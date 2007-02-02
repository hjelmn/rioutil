/**
 *   (c) 2003-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 driver.h
 *
 *   usb drivers header file
 *   
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU Library Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#if !defined (_DRIVER_H)
#define _DRIVER_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "rio.h"
#include "rio_ids.h"

/* I and read  are device->computer.
   O and write are computer->device. */
struct player_device_info {
  int vendor_id;
  int product_id;
  int iep;
  int oep;
  int type;
  int gen; /* device generation */
};

/* defined in rio.c */
extern struct player_device_info player_devices[];

struct rioutil_usbdevice {
  void *dev;
  struct player_device_info *entry;
};

extern char driver_method[];

int  usb_open_rio  (rios_t *rio, int number);
void usb_close_rio (rios_t *rio);

int  read_bulk  (rios_t *rio, unsigned char *buffer, u_int32_t size);
int  write_bulk (rios_t *rio, unsigned char *buffer, u_int32_t size);
int  control_msg(rios_t *rio, u_int8_t request, u_int16_t value,
		 u_int16_t index, u_int16_t length, unsigned char *buffer);

void usb_setdebug(int);
#endif
