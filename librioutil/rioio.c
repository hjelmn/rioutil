/**
 *   (c) 2001-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 rioio.c 
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
#include <string.h>

#include <errno.h>
#include <unistd.h>

#include "rioi.h"
#include "driver.h"

int read_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, u_int32_t block_size) {
  int i, ret;
  unsigned char *buffer;

  buffer = (ptr) ? ptr : rio->buffer;
  
  if (return_type_rio (rio) == RIONITRUS && block_size == RIO_FTS)
    block_size = 64;

  if (size > block_size)
    for (i = 0 ; i < size ; i += block_size)
      ret = read_bulk (rio, &buffer[i], block_size);
  else
    ret = read_bulk (rio, buffer, size);

  if (ret < 0)
    return ret;

  rio_log_data (rio, "In", buffer, size);
  
  return URIO_SUCCESS;
}

int write_cksum_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr) {
  unsigned int *intp;
  int ret;

  memset(rio->buffer, 0, 64);
  intp = (unsigned int *)rio->buffer;

  if (strcmp (cksum_hdr, "CRIOINFO") != 0) {
    if (ptr != NULL && return_type_rio (rio) != RIONITRUS)
      intp[2] = crc32_rio(ptr, size);
    else
      intp[2] = 0x00800000;
  }

  memcpy (rio->buffer, cksum_hdr, 8);

  ret = write_bulk (rio, rio->buffer, 64);
  if (ret < 0)
    return ret;
  
  rio_log_data (rio, "Out", rio->buffer, 64);

  return URIO_SUCCESS;
}

int write_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr) {
  int ret;

  if (!rio || !rio->dev)
    return -1;
  
  if (cksum_hdr != NULL) {
    if (rio->abort) {
      rio->abort = 0;
      rio_log (rio, 0, "librioutil/rioio.c write_block_rio: recieved abort. aborting transfer\n");
      return -EINTR;
    }

    if ((ret = write_cksum_rio (rio, ptr, size, cksum_hdr)) != URIO_SUCCESS)
      return ret;
  }

  ret = write_bulk (rio, ptr, size);    

  if (ret < 0)
    return ret;
  
  rio_log_data (rio, "Out", ptr, size);
  
  if (cksum_hdr != NULL)
    usleep(1000);
  
  ret = read_block_rio (rio, NULL, 64, RIO_FTS);
  if (ret < 0)
    return ret;
  
  if ( (cksum_hdr) && strstr(cksum_hdr, "CRIODATA") && (strstr((char *)rio->buffer, "SRIODATA") == NULL) ) {
    rio_log (rio, -EIO, "librioutil/rioio.c write_block_rio: second SRIODATA not found\n");
    return -EIO;
  }
  
  return URIO_SUCCESS;
}

/* all this command does is call control_msg but it allows to print debug without editing mutiple files */
int send_command_rio (rios_t *rio, int request, int value, int index) {
  static int cretry = 0;
  int ret = URIO_SUCCESS;

  if (!rio || !rio->dev)
    return -EINVAL;

  if (cretry > 3)
    return -ENODEV;
  
  if (rio->debug > 1)
    rio_log (rio, 0, "librioutil/rioio.c send_command_rio: sending command: len: 0x0c rt: 0x00 rq: 0x%02x va: 0x%04x id: 0x%04x\n", 
	     request, value, index);

  if (control_msg(rio, request, value, index, 0x0c, rio->cmd_buffer) < 0)
    return -ENODEV;
  
  rio_log_data (rio, "Command", rio->cmd_buffer, 0xc);

  if (rio->cmd_buffer[0] != 0x1 && request != 0x66 && request != 0x61) {
    cretry++;
    rio_log (rio, -1, "librioutil/rioio.c send_command_rio: device did not respond to command. retrying...");

    ret = send_command_rio (rio, request, value, index);

    cretry = 0;
  }

  return ret;
}

int abort_transfer_rio(rios_t *rio) {
  int ret;
  
  memset(rio->buffer, 0, 12);
  sprintf((char *)rio->buffer, "CRIOABRT");
  
  /* write an abort to the rio */
  ret = write_bulk (rio, rio->buffer, 64);
  if (ret < 0)
    return ret;

  rio_log_data (rio, "Out", rio->buffer, 64);
  
  return URIO_SUCCESS;
}
