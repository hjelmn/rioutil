/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 downloadable.c 
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

#include <stdlib.h>
#include <string.h>

#include "rioi.h"

/*
  downloadable_info:
      Create an info page for the file pointed to file_name. The resulting
    info page will enable to file to be downloaded from the device but the
    file will not be playable on the device.
*/
int downloadable_info (info_page_t *newInfo, char *file_name) {
  rio_file_t *misc_file = newInfo->data;
  
  newInfo->skip = 0;
  
  if (strstr(file_name, ".bin") == NULL) {
    strncpy((char *)misc_file->title, (char *)misc_file->title, 63);
    
    misc_file->bits     = 0x11000110; /* this matches rio taxi file bits */
    misc_file->type     = 0x54415849; /* TAXI. matches rio taxi file type */
  } else {
    /* probably a special file */
    misc_file->bits     = 0x20800590;
    misc_file->type     = 0x46455250;
    misc_file->mod_date = 0x00000000;
    
    strncpy((char *)misc_file->info1, "system", 6);
  }
  
  return URIO_SUCCESS;
}
