/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 byteorder.c
 * 
 *   Functions to handle big-endian machines.   
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

#include "rioi.h"

/*
  Swap rio_file_t to machine endianness (or back)
*/
void file_to_me (rio_file_t *data) {
  data->file_no     = little32_2_arch32(data->file_no);
  data->start       = little32_2_arch32(data->start);
  data->size        = little32_2_arch32(data->size);
  data->time        = little32_2_arch32(data->time);
  data->mod_date    = little32_2_arch32(data->mod_date);
  data->bits        = little32_2_arch32(data->bits);
  data->type        = little32_2_arch32(data->type);
  data->foo3        = little32_2_arch32(data->foo3);
  data->foo4        = little32_2_arch32(data->foo4);
  data->sample_rate = little32_2_arch32(data->sample_rate);
  data->bit_rate    = little32_2_arch32(data->bit_rate);
}

/*
  Swap rio_mem_t to machine endianness (or back)
*/
void mem_to_me (rio_mem_t *data) {
  data->size   = little32_2_arch32(data->size);
  data->used   = little32_2_arch32(data->used);
  data->free   = little32_2_arch32(data->free);
  data->system = little32_2_arch32(data->system);
}
