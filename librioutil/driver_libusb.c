/**
 *   (c) 2002-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 driver_libusb.c
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

#include <stdio.h>
#include <errno.h>

#include <usb.h>

#include "driver.h"

char driver_method[] = "libusb";

int usb_open_rio (rios_t *rio, int number) {
  struct rioutil_usbdevice *plyr;

  struct usb_bus *bus = NULL;
  struct usb_device *dev = NULL;

  int current = 0, ret;
  struct player_device_info *p;

  struct usb_device *plyr_device = NULL;

  usb_init();

  usb_find_busses();
  usb_find_devices();

  /* find a suitable device based on device table and player number */
  for (bus = usb_busses ; bus && !plyr_device ; bus = bus->next)
    for (dev = bus->devices ; dev && !plyr_device; dev = dev->next) {
      rio_log (rio, 0, "USB Device: idVendor = %08x, idProduct = %08x\n", dev->descriptor.idVendor,
	       dev->descriptor.idProduct);

      for (p = &player_devices[0] ; p->vendor_id && !plyr_device ; p++) {
	if (dev->descriptor.idVendor == p->vendor_id && dev->descriptor.idProduct == p->product_id &&
	    current++ == number)
	  plyr_device = dev;
      }
    }
  
  if (plyr_device == NULL || p->product_id == 0)
    return -ENOENT;

  plyr = (struct rioutil_usbdevice *) calloc (1, sizeof (struct rioutil_usbdevice));
  if (plyr == NULL) {
    perror ("rio_open");
    
    return errno;
  }

  plyr->entry = p;

  /* open the device */
  plyr->dev   = (void *) usb_open (plyr_device);
  if (plyr->dev == NULL)
    return -ENOENT;

  usb_set_configuration (plyr->dev, 1);

  ret = usb_claim_interface (plyr->dev, 0);
  if (ret < 0) {
    usb_close (plyr->dev);
    free (plyr);

    return ret;
  }

  rio->dev    = (void *)plyr;

  rio_log (rio, 0, "Rio device ready\n");

  return 0;
}

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  usb_release_interface (dev->dev, 0);
  usb_close (dev->dev);

  free (dev);
}

/* direction is unused  here */
int control_msg(rios_t *rio, u_int8_t request, u_int16_t value,
		u_int16_t index, u_int16_t length, unsigned char *buffer) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  unsigned char requesttype = 0;
  int ret;
  struct usb_dev_handle *ud;

  ud = dev->dev;

  requesttype = 0x80 | USB_TYPE_VENDOR | USB_RECIP_DEVICE;

  ret = usb_control_msg(ud, requesttype, request, value, index, (char *)buffer, length, 15000);

  if (ret == length)
    return URIO_SUCCESS;
  else
    return ret;
}

int write_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int write_ep;
  struct usb_dev_handle *ud;

  write_ep = dev->entry->oep;
  ud = dev->dev;

  return usb_bulk_write(ud, write_ep, (char *)buffer, buffer_size, 8000);
}

int read_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret;
  int read_ep;
  struct usb_dev_handle *ud;

  read_ep = dev->entry->iep | 0x80;
  ud = dev->dev;

  ret = usb_bulk_read(ud, read_ep, (char *)buffer, buffer_size, 8000);
  if (ret < 0) {
    rio_log (rio, ret, "error reading from device (%i). resetting..\n", ret);
    rio_log (rio, ret, "size = %i\n", buffer_size);
    usb_reset (ud);
  }
  
  return ret;
}

void usb_setdebug (int i) {
  usb_set_debug (i);
}
