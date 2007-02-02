/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 driver_standard.c
 *
 *   Allows rioutil to communicate with the rio through kernel-level drivers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <errno.h>

#include <sys/ioctl.h>

#include "driver.h"

char driver_method[] = "device file";

/* Duplicated from rio_usb.h */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define RIO_RECV_COMMAND _IOWR('U', 201, struct RioCommand)
#else
#define RIO_RECV_COMMAND 0x2
#endif

/* RIODEVICE moved here since it is only used in this file */
#ifdef linux
#include <linux/usb.h>
#define RIODEVICE  "/dev/usb/rio"
#elif defined(__FreeBSD__)
#define RIODEVICE "/dev/urio"
#elif defined(__NetBSD__)
#define RIODEVICE "/dev/urio"
#endif

#if !defined(linux)
/* Device descriptor */
struct usb_device_descriptor {
  u_int8_t  bLength;
  u_int8_t  bDescriptorType;
  u_int16_t bcdUSB;
  u_int8_t  bDeviceClass;
  u_int8_t  bDeviceSubClass;
  u_int8_t  bDeviceProtocol;
  u_int8_t  bMaxPacketSize0;
  u_int16_t idVendor;
  u_int16_t idProduct;
  u_int16_t bcdDevice;
  u_int8_t  iManufacturer;
  u_int8_t  iProduct;
  u_int8_t  iSerialNumber;
  u_int8_t  bNumConfigurations;
};
#endif

void usb_close_rio (rios_t *rio) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  close ((int)dev->dev);
  free (dev);
}

int read_bulk (rios_t *rio, unsigned char *buffer, u_int32_t size) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;
  
  if (dev)
    return read ((int)dev->dev, buffer, size);
  else
    return -1;
}

int write_bulk (rios_t *rio, unsigned char *buffer, u_int32_t size) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  if (dev)
    return write ((int)dev->dev, buffer, size);
  else
    return -1;
}

int usb_open_rio (rios_t *rio, int number) {
  char fileName[FILENAME_MAX+2];
  struct rioutil_usbdevice *plyr;
  int fd;

  struct player_device_info *p;

  struct usb_device_descriptor desc;
  
  int id_product, id_vendor;

  snprintf(fileName, FILENAME_MAX, "%s%i", RIODEVICE, number); 

  fd = open(fileName, O_RDWR, 0666);
  if (fd < 0) {
    perror ("rio_open");

    return -1;
  }
  
  if (ioctl(fd, USB_REQ_GET_DESCRIPTOR, desc) < 0) {
    rio_log (rio, -errno, "couldn't get device descriptor for %s: %s\n", fileName, strerror(errno));
    close (fd);
    return -errno;
  }

  id_product = little16_2_arch16 (dev.idProduct);
  id_vendor  = little16_2_arch16 (dev.idVendor);

  for (p = &player_devices[0] ; p->vendor_id ; p++)
    if (desc.idVendor == p->vendor_id &&
	desc.idProduct == p->product_id)
      break;

  if (p->vendor_id == 0) {
    close (fd);
    return -1;
  }

  plyr        = (struct rioutil_usbdevice *)calloc(1, sizeof(struct rioutil_usbdevice));
  plyr->dev   = (void *)fd;
  plyr->entry = p;
  rio->dev    = (void *)plyr;

  return 0;
}

int control_msg(rios_t *rio, u_int8_t request, u_int16_t value,
		u_int16_t index, u_int16_t length, unsigned char *buffer) {
  struct rioutil_usbdevice *dev = (struct rioutil_usbdevice *)rio->dev;

  struct RioCommand cmd;
  int ret = 0;
  
  cmd.timeout           = 50;
  cmd.requesttype       = 0;            /* low level usb req. type. */
  cmd.request           = request;      /* the actualy request      */
  cmd.value             = value;        /* value??                  */
  cmd.index             = index;        /* indexy thingy            */
  cmd.length            = 0x0c;         /* length?                  */
  cmd.buffer            = buffer;
  
  if ( ioctl ((int)dev->dev, RIO_RECV_COMMAND, &cmd) < 0)
    ret = errno;
  
  return URIO_SUCCESS;
}

void usb_setdebug(int i) {
}

