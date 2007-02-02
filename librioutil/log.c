/**
 *   (c) 2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.0 log.c
 *   
 *   debug routines for librioutil
 *   all sources are c style gnu (c-set-style in emacs)
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
#include <stdarg.h>

#include <string.h>

#include "rioi.h"

void rio_log (rios_t *rio, int error, char *format, ...) {
  FILE *debug_out;
  int debug_level;

  /* rio should not be null, but if it is the message should still print out  */
  if (rio == NULL) {
    debug_out   = stderr;
    debug_level = 5;
  } else {
    debug_out   = rio->log;
    debug_level = rio->debug;
  }

  if ( (debug_level > 0) && (debug_out != NULL) ) {
    va_list arg;
    
    va_start (arg, format);
    
    if (rio == NULL)
      fprintf (debug_out, "Warning: rio argument is NULL!\n");

    if (error != 0)
      fprintf(debug_out, "Error %i: ", error);

    if ( (error == 0 && debug_level > 1) || error != 0 )
      vfprintf (debug_out, format, arg);

    fflush (debug_out);

    va_end (arg);
  }
}

void pretty_print_block(unsigned char *b, int len, FILE *out) {
  int x, indent = 16, count;
    
  fputc('\n', out);
    
  for (count = 0 ; count < len ; count += indent) {
    fprintf (out, "%04x : ", count);

    for (x = 0 ; x < indent ; x++)
      fprintf (out, ((x + count + 1) >= len) ? "   " : "%02x ", b[x + count]);

    fprintf (out, ": ");
    
    for (x = 0 ; x < indent && (x + count + 1) < len ; x++)
      fputc ((isprint(b[x + count])) ? b[x + count]: '.', out);
    
    fputc ('\n', out);
  }
  
  fputc('\n', out);
}

/* writes out hex/ascii representation of a data buffer */
void rio_log_data (rios_t *rio, char *dir, unsigned char *data, int data_size) {
  int debug_level;
  FILE *debug_out;

  /* rio should not be null, but if it is the message should still print out  */
  if (rio == NULL) {
    debug_out   = stderr;
    debug_level = 5;
  } else {
    debug_out   = rio->log;
    debug_level = rio->debug;
  }

  rio_log (rio, 0, "dir: %s data size: 0x%08x\n", dir, data_size);

  if ((debug_level > 0 && data_size < 257) || (debug_level > 3))
    pretty_print_block (data, data_size, debug_out);
  else if (rio->debug > 0)
    pretty_print_block (data, 256, debug_out);
}
