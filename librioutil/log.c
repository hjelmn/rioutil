/**
 *   (c) 2006-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
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

#include "riolog.h"

#include <ctype.h>  /* isprint() */
#include <stdio.h>
#include <stdarg.h>

static unsigned int debug_level = 0;
static FILE *debug_out = NULL;

void set_debug_level(unsigned int new_debug_level)
{
  debug_level = new_debug_level;
}

void set_debug_out(FILE* new_debug_out)
{
  debug_out = new_debug_out;
}

void riolog( unsigned int level, char *format, ...)
{
  va_list arg;

  if ( debug_level < level )
    return;

  va_start( arg, format );

  vfprintf( debug_out, format, arg);

  va_end( arg );

  fprintf( debug_out, "\n");

  fflush( debug_out );
}

static void pretty_print_block(unsigned char *b, int len, FILE *out) {
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
void rio_log_data (char *dir, unsigned char *data, size_t data_size)
{
  riolog (4, "dir: %s data size: 0x%08x", dir, data_size);

  if ((debug_level > 3 && data_size < 257) || (debug_level > 4))
    pretty_print_block (data, data_size, debug_out);
  else if (debug_level > 3)
    pretty_print_block (data, 256, debug_out);
}
