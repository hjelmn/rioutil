/*  ----------------------------------------------------------------------
    Copyright (C) 2001-2006 Nathan Hjelmn <hjelmn@users.sourceforge.net>

    Based of of rio500 driver:
      Copyright (C) 2000  Cesar Miquel  (miquel@df.uba.ar)

    Header for rio kernel driver.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    ---------------------------------------------------------------------- */
#if !defined (_RIO_USB_H)
#define _RIO_USB_H

#include <sys/types.h>

/* second generation players (199?) */
#define VENDOR_DIAMOND00  0x0841
#define PRODUCT_RIO500    0x0001

#define VENDOR_DIAMOND01  0x045a

/* third generation players (1999 ?) */
#define PRODUCT_RIO600    0x5001
#define PRODUCT_RIO800    0x5002
#define PRODUCT_PSAPLAY   0x5003

/* fouth generation players (2002) */
#define PRODUCT_RIOS10    0x5005
#define PRODUCT_RIOS50    0x5006
#define PRODUCT_RIOS35    0x5007
#define PRODUCT_RIO900    0x5008
#define PRODUCT_RIOS30    0x5009
#define PRODUCT_FUSE      0x500d
#define PRODUCT_CHIBA     0x500e
#define PRODUCT_CALI      0x500f
#define PRODUCT_CALI256   0x503f
#define PRODUCT_RIOS11    0x5010

#define PRODUCT_RIORIOT   0x5202

struct RioCommand {
	u_int16_t length;
	int request;
	int requesttype;
	int value;
	int index;
	void *buffer;
	int timeout;
};

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define RIO_SEND_COMMAND       _IOWR('U', 200, struct RioCommand)
#define RIO_RECV_COMMAND       _IOWR('U', 201, struct RioCommand)
#else
#define RIO_SEND_COMMAND                       0x1
#define RIO_RECV_COMMAND                       0x2
#endif

#define RIO_DIR_OUT                            0x0
#define RIO_DIR_IN                             0x1

#endif
