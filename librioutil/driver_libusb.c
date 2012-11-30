/**
 *   (c) 2002-2012 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.3 driver_libusb.c
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <libusb.h>

#include "driver.h"
#include "riolog.h"

char driver_method[] = "libusb";
static int usb_rio_open_count = 0;
static int usb_debug_level = 0;

int usb_open_rio (rios_t *rio, int number) {
  struct rioutil_usbdevice *plyr;

  libusb_device **device_list;
  struct libusb_device_descriptor descriptor;

  int current = 0, ret, i, count;
  struct player_device_info *p;

  libusb_device *plyr_device = NULL;

  debug("librioutil/driver_libusb.c:usb_open_rio(rio=%x,number=%d)", rio, number);

  do {
    if (!usb_rio_open_count++) {
      ret = libusb_init (NULL);
      if (LIBUSB_SUCCESS != ret)
        return -1;

      if (usb_debug_level)
	libusb_set_debug (NULL, usb_debug_level);
    }

    /* find a suitable device based on device table and player number */
    count = libusb_get_device_list (NULL, &device_list);
    if (0 > count) {
      error("librioutil/driver_libusb.c:usb_open_rio() error getting device list");
      break;
    }

    for (i = 0 ; device_list[i] && !plyr_device ; ++i) {
      ret = libusb_get_device_descriptor (device_list[i], &descriptor);

      debug("USB Device: idVendor = %08x, idProduct = %08x", descriptor.idVendor, descriptor.idProduct);

      for (p = &player_devices[0] ; p->vendor_id && !plyr_device ; p++) {
        if (descriptor.idVendor == p->vendor_id && descriptor.idProduct == p->product_id &&
            current++ == number)
          plyr_device = device_list[i];
      }

      /* found it */
      if (plyr_device) {
        /* reference this device so it isn't freed by libusb_free_device_list (, 1) */
        libusb_ref_device (plyr_device);
        break;
      }
    }

    /* have libusb unreference all devices */
    libusb_free_device_list (device_list, 1);

    if (plyr_device == NULL || p->product_id == 0) {
      ret = -ENOENT;
      break;
    }

    plyr = (struct rioutil_usbdevice *) calloc (1, sizeof (struct rioutil_usbdevice));
    if (plyr == NULL) {
      error("librioutil/driver_libusb.c:usb_open_rio() error allocating memory");
      ret = -ENOMEM;
      break;
    }

    plyr->entry = p;

    /* open the device */
    ret = libusb_open (plyr_device, (libusb_device_handle **) &plyr->dev);
    if (LIBUSB_SUCCESS != ret) {
      error("librioutil/driver_libusb.c:usb_open_rio() error opening usb device: %s", libusb_error_name(ret));
      ret = -1;
      break;
    }

    /* we can release our reference to the device now */
    libusb_unref_device (plyr_device);

    ret = libusb_set_configuration ((libusb_device_handle *) plyr->dev, 1);
    if (LIBUSB_SUCCESS != ret) {
      error("librioutil/driver_libusb.c:usb_open_rio() error setting configuration: %s", libusb_error_name(ret));
      ret = -1;
      break;
    }

    ret = libusb_claim_interface ((libusb_device_handle *) plyr->dev, 0);
    if (LIBUSB_SUCCESS != ret) {
      error("librioutil/driver_libusb.c:usb_open_rio() error claiming interface 0: %s", libusb_error_name(ret));
      ret = -1;
      break;
    }

    rio->dev    = (void *)plyr;

    debug("librioutil/driver_libusb.c:usb_open_rio() Success");

    return 0;
  } while (0);

  if (plyr_device) {
    libusb_unref_device (plyr_device);
  }

  usb_close_rio (rio);

  return ret;
}

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  if (NULL == dev) {
    return;
  }

  if (dev->dev) {
    (void) libusb_release_interface ((libusb_device_handle *) dev->dev, 0);
    (void) libusb_close ((libusb_device_handle *) dev->dev);
  }

  free (dev);

  rio->dev = NULL;

  if (!--usb_rio_open_count) {
    libusb_exit (NULL);
  }
}

/* direction is unused  here */
int control_msg(rios_t *rio, u_int8_t request, u_int16_t value,
		u_int16_t index, u_int16_t length, unsigned char *buffer) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *) rio->dev;

  unsigned char requesttype = 0;
  int ret;

  requesttype = 0x80 | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;

  ret = libusb_control_transfer ((libusb_device_handle *) dev->dev, requesttype, request, value,
                                 index, buffer, length, 15000);
  if (length == ret) {
    return URIO_SUCCESS;
  }

  return -1;
}

int write_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret, transferred;

  ret = libusb_bulk_transfer ((libusb_device_handle *) dev->dev, dev->entry->oep,
                              buffer, buffer_size, &transferred, 8000);
  if (LIBUSB_SUCCESS != ret) {
    return -1;
  }

  return transferred;
}

int read_bulk(rios_t *rio, unsigned char *buffer, u_int32_t buffer_size){
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  int ret, transferred;

  ret = libusb_bulk_transfer ((libusb_device_handle *) dev->dev, dev->entry->iep | 0x80,
                              buffer, buffer_size, &transferred, 8000);
  if (LIBUSB_SUCCESS != ret) {
    error("librioutil/driver_libusb.c:read_bulk() error reading from device (rc = %i). size = %i. resetting..\n", ret, buffer_size);

    libusb_reset_device ((libusb_device_handle *) dev->dev);
    return -1;
  }
  
  return transferred;
}

void usb_setdebug (int i) {
  usb_debug_level = i;
}
