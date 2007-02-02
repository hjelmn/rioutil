/**
 *   (c) 2005-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 rio_ids.h
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

#if !defined(RIO_IDS_H)
#define RIO_IDS_H

/* supported rio devices */
#define VENDOR_DIAMOND01 0x045a

/* flash players */
#define PRODUCT_RIO600   0x5001
#define PRODUCT_RIO800   0x5002
#define PRODUCT_PSAPLAY  0x5003

#define PRODUCT_RIOS10   0x5005
#define PRODUCT_RIOS50   0x5006
#define PRODUCT_RIOS35   0x5007
#define PRODUCT_RIO900   0x5008
#define PRODUCT_RIOS30   0x5009
#define PRODUCT_FUSE     0x500d
#define PRODUCT_CHIBA    0x500e
#define PRODUCT_CALI     0x500f
#define PRODUCT_CALI256  0x503f
#define PRODUCT_RIOS11   0x5010

/* hard drive players */
#define PRODUCT_RIORIOT  0x5202
#define PRODUCT_NITRUS   0x5220

#endif
