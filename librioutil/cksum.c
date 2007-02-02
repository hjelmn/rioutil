/*
 * $Id: cksum.c,v 1.8 2006/02/15 22:54:07 hjelmn Exp $
 */

/*
 * Mike Touloumtzis <miket@bluemug.com> dissassembled the spRio600.dll
 * and noticed the magic value 0x04c11db7, which is the polynomial for
 * CRC-32.
 *
 * So it does appear to be using CRC-32, but in some mangled way.
 *
 * info on the crc32 algo. was obtained from:
 *
 * http://chesworth.com/pv/technical/crc_error_detection_guide.htm
 * and
 * http://www.embedded.com/internet/0001/0001connect.htm
 *
 */

#include <stdlib.h>
#include <sys/types.h>

#include "rioi.h"

#define CRC32POLY 	0x04C11DB7

/*
 * 1024 byte look up table.
 */
static void crc32_init_table(void);

static u_int32_t crc32_table[256];

static int crc32_initialized = 0;

static void crc32_init_table(void) {
  u_int32_t i, j, r;

  crc32_initialized = 1;

  for (i = 0 ; i < 256 ; i++) {
    r = i;

    for (j = 0; j < 8; j++) {
      if (r & 1)
        r = (r >> 1) ^ CRC32POLY;
      else
        r >>= 1;
    }

    crc32_table[i] = r;
  }
}

u_int32_t crc32_rio (u_int8_t *buf, size_t length) {
  unsigned long crc = 0;
  int i;

  if (crc32_initialized == 0)
    crc32_init_table();

  for (i = 0 ; i < length ; i++)
    crc = (crc >> 8) ^ crc32_table[(crc ^ buf[i]) & 0xff];

  crc = big32_2_arch32 (crc);
  return crc;
}
