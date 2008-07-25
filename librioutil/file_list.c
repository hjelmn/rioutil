/**
 *   (c) 2005-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 file_list.c
 *   
 *   c version of librioutil
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

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/time.h>

#include "rioi.h"

#if defined (HAVE_LIBGEN_H)
#include <libgen.h>
#endif

/*
  get_flist_riomc:
    Downloads the file list off of flash based players (Rio600, Rio800, S-Series, etc.).
*/
int generate_flist_riomc (rios_t *rio, u_int8_t memory_unit) {
  int i, ret;
  rio_file_t file;
  
  info_page_t info;

  info.data = &file;

  rio_log (rio, 0, "generate_flist_riomc: entering...\n");

  ret = URIO_SUCCESS;

  /*
    MAX_RIO_FILES is an arbitrary file limit. Rios can get into a state where
    the data in the file headers is garbage. This state can result in the termination
    condition (file number == 0) never being reached.
  */
  for (i = 0 ; i < MAX_RIO_FILES ; i++) {
    ret = get_file_info_rio(rio, &file, memory_unit, i);

    if (ret != URIO_SUCCESS) {
      if (ret == -ENOENT) 
	ret = URIO_SUCCESS;  /* not an error */
      break;
    }

    flist_add_rio (rio, memory_unit, info);
  }
  
  rio_log (rio, 0, "generate_flist_riomc: complete\n");

  return ret;
}

int hdfile_to_mcfile  (hd_file_t *hdf, rio_file_t *file, int file_no) {
  if (!hdf || !file)
    return -EINVAL;

  file->file_no = file_no;
  file->size = hdf->size;

  strncpy(file->artist, (char *)hdf->artist, 48);
  strncpy(file->title, (char *)hdf->title, 48);
  strncpy(file->album, (char *)hdf->album, 48);
  strncpy(file->name, (char *)hdf->file_name, 27);

  file->type = TYPE_MP3;

  return 0;
}

/*
  get_flist_riohd:
   Downloads the file list off of a hard drive based player (Rio Riot).
*/
int generate_flist_riohd (rios_t *rio) {
  int ret, i, block_count;
  rio_file_t file;
  info_page_t info;

  hd_file_t *hdf;
  u_int8_t read_buffer[RIO_FTS];

  /* the Riot only has a single memory unit */
  rio_log (rio, 0, "create_flist_riohd: entering...\n");

  ret = send_command_rio (rio, RIO_RIOTF, 0, 0);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "create_flist_riohd: device did not respond to playlist read command\n");

    return ret;
  }

  hdf = (hd_file_t *)read_buffer;
  info.data = &file;
  block_count = 0;

  read_block_rio (rio, read_buffer, 0x40, RIO_FTS);

  while (1) {
    sprintf ((char *)rio->buffer, "CRIODATA");

    ret = write_block_rio (rio, rio->buffer, 0x40, NULL);

    /* device returns SRIODONE when the transfer is complete */
    if (strstr ((char *)rio->buffer, "SRIODONE") != NULL)
      break;

    ret = read_block_rio (rio, read_buffer, RIO_FTS, RIO_FTS);

    /* 0x40 is RIO_FTS/hdr_size */
    for (i = 0 ; i < 0x40 ; i++) {
      if (hdf->unk0 == 0) /* blank song entry */
	continue;

      /* add to the file list */
      hdfile_to_mcfile (&hdf[i], &file, i + block_count);
      flist_add_rio (rio, 0, info);
    }

    block_count += i;
  }

  rio_log (rio, 0, "create_flist_riohd: complete\n");

  return ret;
}

int flist_first_free_rio (rios_t *rio, int memory_unit) {
  flist_rio_t *prev;
  uint file_incr, next_num;

  if (!rio || memory_unit >= MAX_MEM_UNITS)
    return -EINVAL;

  /* older devices increment the file id by 1 while newer devices increment the file id by 16 */
  file_incr = (return_generation_rio (rio) < 4) ? 0x01 : 0x10;

  for (prev = rio->info.memory[memory_unit].files, next_num = file_incr ; prev ; prev = prev->next, next_num += file_incr)
    if (next_num < prev->rio_num)
      break;

  return next_num;
}

/*
  flist_add_rio:

  adds a file to the rio's internal file list
*/
int flist_add_rio (rios_t *rio, int memory_unit, info_page_t info) {
  flist_rio_t *flist;
  flist_rio_t *next = NULL, *prev = NULL;
  flist_rio_t *files;
  
  uint next_num, file_incr;

  if (!rio || !info.data || memory_unit >= MAX_MEM_UNITS)
    return -EINVAL;

  /* older devices increment the file id by 1 while newer devices increment the file id by 16 */
  file_incr = (return_generation_rio (rio) < 4) ? 0x01 : 0x10;

  rio_log (rio, 0, "librioutil/file_list.c flist_add_rio: entering...\n");

  /* allocate zeroed space for the new entry */
  flist = calloc (1, sizeof (flist_rio_t));
  if (flist == NULL) {
    rio_log (rio, -errno, "librioutil/file_list.c flist_add_rio: calloc returned an error (%s).\n", strerror (errno));

    return -errno;
  }

  files = rio->info.memory[memory_unit].files;

  if (files) {
    for (next = files, next_num = file_incr ; next ; next = next->next, next_num += file_incr) {
      if ((info.data->file_no == 0 && next_num < next->rio_num) || info.data->file_no == next_num)
	break;

      prev = next;
    }
    
    if (prev) {
      flist->num  = prev->inum + 1;
      flist->inum = prev->inum + 1;
    }
  } else
    memset (&rio->info.memory[memory_unit], 0, sizeof (mlist_rio_t));

  flist->rio_num = next_num;

  strncpy(flist->artist, info.data->artist, 64);
  strncpy(flist->title,  info.data->title, 64);
  strncpy(flist->album,  info.data->album, 64);
  strncpy(flist->name,   info.data->name, 64);
  strncpy(flist->genre,  (char *)info.data->genre2, 22);

  strncpy(flist->year,   (char *)info.data->year2, 4);
  
  flist->time       = info.data->time;  
  flist->bitrate    = info.data->bit_rate >> 7;
  flist->samplerate = info.data->sample_rate;
  flist->mod_date   = info.data->mod_date;
  flist->size       = info.data->size;
  flist->start      = info.data->start;
  flist->track_number = info.data->trackno2;
  flist->prev = prev;
  
  if (info.data->type == TYPE_MP3)
    flist->type = RIO_FILETYPE_MP3;
  else if (info.data->type == TYPE_WMA)
    flist->type = RIO_FILETYPE_WMA;
  else if (info.data->type == TYPE_WAV)
    flist->type = RIO_FILETYPE_WAV;
  else if (info.data->type == TYPE_WAVE)
    flist->type = RIO_FILETYPE_WAVE;
  else
    flist->type = RIO_FILETYPE_OTHER;
  
  if (return_generation_rio (rio) > 3)
    memcpy (flist->sflags, info.data->unk1, 3);
  
  if (prev) {
    prev->next = flist;
    flist->prev = prev;
  } else
    rio->info.memory[memory_unit].files = flist;

  if (next) {
    flist->next = next;
    next->prev = flist;
  
    for ( ; next ; next = next->next, next->inum++, next->num++);
  }

  rio->info.memory[memory_unit].num_files  += 1;
  rio->info.memory[memory_unit].total_time += flist->time;

  rio_log (rio, 0, "librioutil/file_list.c flist_add_rio: complete\n");

  return 0;
}

int flist_get_file_id_rio (rios_t *rio, int memory_unit, int file_no) {
  flist_rio_t *tmp;

  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next) {
    if (tmp->num == file_no)
      break;
  }

  if (tmp == NULL)
    return -1;

  if (return_type_rio (rio) != RIONITRUS)
    return tmp->inum;
  else
    return tmp->rio_num;
}

int flist_get_file_name_rio (rios_t *rio, int memory_unit, int file_no, char *file_namep, int file_name_len) {
  flist_rio_t *tmp;

  if (file_namep == NULL)
    return -1;

  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next) {
    if (tmp->num == file_no)
      break;
  }

  if (tmp == NULL)
    return -1;

  strncpy (file_namep, tmp->name, file_name_len);

  return 0;
}

/*
  flist_remove_rio:

  removes a file from the rio's internal file list
*/
int flist_remove_rio (rios_t *rio, int memory_unit, int file_no) {
  flist_rio_t *flist, *tmp;

  if (rio == NULL || memory_unit >= MAX_MEM_UNITS)
    return -EINVAL;

  for (flist = rio->info.memory[memory_unit].files ; flist ; flist = flist->next)
    if (flist->num == file_no)
      break;

  if (flist == NULL)
    return -EINVAL;

  if (flist->prev)
    flist->prev->next = flist->next;
  if (flist->next)
    flist->next->prev = flist->prev;

  /* The file number used to access the file is reduced when a file is deleted */
  for (tmp = flist->next ; tmp ; tmp = tmp->next)
    tmp->inum--;

  rio->info.memory[memory_unit].num_files  -= 1;
  rio->info.memory[memory_unit].total_time -= flist->time;

  if (flist == rio->info.memory[memory_unit].files)
    rio->info.memory[memory_unit].files = flist->next;

  free (flist);
 
  return 0;
}

/*
  return_list_rio:

  returns a flist_rio_t contained on a memory unit.
*/
flist_rio_t *return_list_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t list_flags) {
  flist_rio_t *tmp;

  rio_log (rio, 0, "return_list_rio: depricated function. use return_flist_rio instead.\n");

  if (return_flist_rio (rio, memory_unit, list_flags, &tmp) < 0)
    return NULL;

  return tmp;
}

/*
 return_flist_rio:

 copies the internal file list from a rio and stores it in flist.
*/
int return_flist_rio (rios_t *rio, u_int8_t memory_unit, rio_filetype list_flags, flist_rio_t **flist) {
  flist_rio_t *tmp;
  flist_rio_t *bflist;
  flist_rio_t *prev = NULL;
  flist_rio_t *head = NULL;
  int first = 1, ret;

  rio_log (rio, 0, "return_flist_rio: entering...\n");
  
  if (rio == NULL || memory_unit >= MAX_MEM_UNITS || flist == NULL) {
    rio_log (rio, -EINVAL, "return_flist_rio: invalid argument.\n");

    return -EINVAL;
  }

  /* build file list if needed */
  if (rio->info.memory[0].size == 0) 
    if ((ret = generate_mem_list_rio(rio)) != URIO_SUCCESS)
      return ret;

  /* make a copy of the file list with only what we want in it */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next) {
    if (list_flags & tmp->type)
    {
      if ((bflist = malloc(sizeof(flist_rio_t))) == NULL) {
	rio_log (rio, errno, "return_flist_rio: malloc returned an error (%s).\n", strerror (errno));

	return -errno;
      }
      
      *(bflist) = *(tmp);
      
      bflist->prev = prev;
      bflist->next = NULL;
      
      if (bflist->prev != NULL)
	bflist->prev->next = bflist;
      
      if (first != 0) {
	first = 0;
	head = bflist;
      }
      
      prev = bflist;
    }
  }
  
  *flist = head;

  rio_log (rio, 0, "return_flist_rio: complete\n");

  return 0;
}

int size_flist_rio (rios_t *rio, int memory_unit) {
  int i;
  flist_rio_t *flist;
  
  for (i = 0, flist = rio->info.memory[memory_unit].files ; flist ; flist = flist->next, i++);

  return i;
}


void free_flist_rio (flist_rio_t *flist) {
  flist_rio_t *tmp, *ntmp;

  for (tmp = flist ; tmp ; tmp = ntmp) {
    ntmp = tmp->next;
    free(tmp);
  }
}

