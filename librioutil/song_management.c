/**
 *   (c) 2001-2016 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v2.5.1 song_managment.c
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Library Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include <sys/stat.h>

#include "rioi.h"
#include "riolog.h"

#if defined(HAVE_LIBGEN_H)
#include <libgen.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 1024
#endif

static int init_new_upload_rio (rios_t *rio, u_int8_t memory_unit);
static int init_overwrite_rio (rios_t *rio, u_int8_t memory_unit);
static int complete_upload_rio (rios_t *rio, u_int8_t memory_unit, info_page_t info);
static int bulk_upload_rio (rios_t *rio, info_page_t info, int addpipe);
static int upload_dummy_hdr (rios_t *rio, u_int8_t memory_unit, rio_file_t *filexp);

/* the guts of any upload */
int do_upload (rios_t *rio, u_int8_t memory_unit, int addpipe, info_page_t info, int overwrite) {
  int error;

  debug("librioutil/song_management.c do_upload: entering");

  /* check if there the device has sufficient space for the file */
  if (overwrite == 0) {
    if (FREE_SPACE(memory_unit) < (info.data->size - info.skip)/1024) {
      free (info.data);
      
      return -ENOSPC;
    }
    
    if ((error = init_new_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
      error("librioutil/song_management.c do_upload: error in init_upload_rio");
      return error;
    }
  } else { 
    if ((error = init_overwrite_rio(rio, memory_unit)) != URIO_SUCCESS) {
      error("librioutil/song_management.c do_upload: error in init_overwrite_rio");
      return error;
    }
  }

  if ((error = bulk_upload_rio(rio, info, addpipe)) != URIO_SUCCESS) {
    error("librioutil/song_management.c do_upload: error in bulk_upload_rio");
    abort_transfer_rio(rio);
    return error;
  }

  if ((error = complete_upload_rio(rio, memory_unit, info))!= URIO_SUCCESS) {
    error("librioutil/song_management.c do_upload: error in complete_upload_rio");
    abort_transfer_rio(rio);
    return error;
  }

  /* rioutil keeps track of the rio's memory state */
  update_free_intrn_rio(rio, memory_unit);

  flist_add_rio (rio, memory_unit, info);

  if (info.data->type == TYPE_MP3)
    update_db_rio (rio);

  debug("librioutil/song_management.c do_upload: complete");

  return URIO_SUCCESS;
}

/*
  add_song_rio:
    Upload a music file to the rio.

  PreCondition:
      - An initiated rio instance.
      - A memory unit.
      - A filename.
    Optional:
      - Artist.
      - Title.
      - Album.

  PostCondition:
      - URIO_SUCCESS if the file was uploaded.
      - < 0 if an error occured.
*/
int add_song_rio (rios_t *rio, u_int8_t memory_unit, char *file_name,
                  const char *artist, const char *title, const char *album) {
  info_page_t song_info;
  int error;
  int addpipe;
  char *tmp, *tmp2;
  struct stat statinfo;

  if (!rio)
    return -EINVAL;
  
  if (memory_unit >= rio->info.total_memory_units)
    return -1;

  debug("add_song_rio: entering...");
  
  if (stat(file_name, &statinfo) < 0)
    return -ENOENT;

  /* common info */
  song_info.data = (rio_file_t *)calloc(1, sizeof(rio_file_t));
  song_info.data->size = statinfo.st_size;
  song_info.data->mod_date = statinfo.st_mtime;
  
  /* set the filename */
  tmp = strdup (file_name);
  tmp2 = basename(tmp);
  
  strncpy((char *)song_info.data->name, tmp2, 63);
  
  free (tmp);

  /* check for file types by extension */
  tmp = file_name + strlen(file_name) - 4;

  if (strcasecmp(tmp, ".mp3") == 0) {
    error = mp3_info(&song_info, file_name);
  
    /* just in case one of the info funcs failed */
    if (error != 0) {
      error("Error getting song info.");
    
      return error;
    }

    if ((error = try_lock_rio (rio)) != 0)
      return error;

    /* copy any user-suplied data*/
    if (artist)
      sprintf(song_info.data->artist, artist, 63);
  
    if (title)
      sprintf(song_info.data->title, title, 63);
  
    if (album)
      sprintf(song_info.data->album, album, 63);
  } else if (strcasecmp (file_name, ".lst") == 0 || strcasecmp (file_name, ".m3u") == 0) {
    if ((error = playlist_info(&song_info, file_name)) != 0)
      return error;
  } else {
    if ((error = downloadable_info(&song_info, file_name)) != 0)
      UNLOCK(error);
  }

  /* upload the file */
  addpipe = open(file_name, O_RDONLY);
  if (addpipe < 0)
    UNLOCK(-errno);

  debug("add_song_rio: file opened and ready to send to rio.");

  if ((error = do_upload (rio, memory_unit, addpipe, song_info, 0)) != URIO_SUCCESS) {
    free(song_info.data);
    
    close (addpipe);

    UNLOCK(error);
  }
  
  close (addpipe);

  free(song_info.data);
  
  debug("add_song_rio: complete");

  UNLOCK(URIO_SUCCESS);
}

int overwrite_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num, char *filename) {
  info_page_t song_info;  
  rio_file_t file;
  int ret, addpipe, file_id;
  
  struct stat statinfo;

  if (rio == NULL || memory_unit > 1 || filename == NULL)
    return -EINVAL;

  debug("overwrite_file_rio: entering");

  if (stat (filename, &statinfo) < 0) {
    error("overwrite_file_rio: could not stat %s", filename);

    return -errno;
  }

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;
  
  (void)wake_rio(rio);
  
  /* hopefully this list is up to date */
  file_id = flist_get_file_id_rio (rio, memory_unit, file_num);
  if (file_id < 0) {
    error("librioutility/song_management.c overwrite_file_rio: file not found.");

    UNLOCK(file_id);
  }

  if (get_file_info_rio(rio, &file, memory_unit, file_id) != URIO_SUCCESS)
    UNLOCK(-1);

  file.size = statinfo.st_size;
  song_info.data = &file;
    
  if ((addpipe = open(filename, O_RDONLY)) == -1) {
      error("overwrite_file_rio: open failed: %d", errno);
    UNLOCK(-1);
  }
  
  if ((ret = do_upload (rio, 0, addpipe, song_info, 1)) != URIO_SUCCESS) {
    error("overwrite_file_rio: do_upload failed");
    close (addpipe);
    
    UNLOCK(ret);
  }
  
  close (addpipe);
  
  debug("overwrite_file_rio: complete");
  
  UNLOCK(URIO_SUCCESS);
}

/*
  init_upload_rio: sends write command to the device.
*/
static int init_upload_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t upload_command) {
  int ret;

  debug("librioutil/song_management.c init_upload_rio: entering");

  (void)wake_rio(rio);

  if ((ret = send_command_rio(rio, upload_command, memory_unit, 0)) != URIO_SUCCESS)
    return ret;
  
  read_block_rio(rio, NULL, 64, 64);
  if (strncmp((char *)rio->buffer, "SRIORDY", 7) != 0)
    return -EBUSY;
  
  read_block_rio(rio, NULL, 64, 64);
  if (strncmp((char *)rio->buffer, "SRIODATA", 8) != 0)
    return -EBUSY;
  
  debug("librioutil/song_management.c init_upload_rio: finished");

  return URIO_SUCCESS;
}

static int init_new_upload_rio (rios_t *rio, u_int8_t memory_unit) {
  return init_upload_rio (rio, memory_unit, RIO_WRITE);
}

static int init_overwrite_rio (rios_t *rio, u_int8_t memory_unit) {
  return init_upload_rio (rio, memory_unit, RIO_OVWRT);
}

/*
  bulk_upload_rio:
    function writes a file to the rio in blocks.
*/
static int bulk_upload_rio(rios_t *rio, info_page_t info, int addpipe) {
  unsigned char file_buffer[2 * RIO_FTS];
  size_t write_size;
  long int copied = 0, amount;
  int ret;

  debug("librioutil/song_management.c bulk_upload_rio: entering");
  debug("librioutil/song_management.c bulk_upload_rio: skipping %d bytes of input",
	   info.skip);

  write_size = (return_type_rio (rio) == RIONITRUS) ? (2 * RIO_FTS) : RIO_FTS;
  
  lseek(addpipe, info.skip, SEEK_SET);
  memset (file_buffer, 0, write_size);

  if (rio->progress != NULL)
    rio->progress(0, 1, rio->progress_ptr);

  while ((amount = read (addpipe, file_buffer, write_size)) != 0) {
    /* if we dont know the size we dont know how close we are to finishing */
    if (info.data->size && rio->progress != NULL)
      rio->progress(copied, info.data->size, rio->progress_ptr);

    if ((ret = write_block_rio(rio, file_buffer, write_size, "CRIODATA")) != URIO_SUCCESS)
      return ret;
    
    memset (file_buffer, 0, write_size);
    copied += amount;
  }

  if (info.data->size == 0) {
    info.data->size = copied;

    /* the time value should also be set, but there is no good way to do so */
    if (info.data->bit_rate)
      info.data->time = copied/(info.data->bit_rate * 1000);
  }
  
  debug("librioutil/song_management.c bulk_upload_rio: sent %d/%d bytes to player",
	   copied, info.data->size);

  if (rio->progress != NULL)
    rio->progress(1, 1, rio->progress_ptr);

  debug("librioutil/song_management.c bulk_upload_rio: finished");

  return URIO_SUCCESS;
}

struct sort_list {
  int seq_number;
  flist_rio_t *ptr;
};

static int str_cmp (char *str1, char *str2) {
  char *str1p, *str2p;
  int cmp = 0;
  int str1l, str2l;

  int i, shortest_string;

  str1l = strlen (str1);
  str2l = strlen (str2);

  if (str1l == 0 && str2l == 0)
    return 0;
  if (str1l == 0)
    return 1;
  if (str2l == 0)
    return -1;

  /* ignore leading "the " in strings */
  if (strlen (str1) > 4 && strncasecmp (str1, "the ", 4) == 0)
    str1p = &str1[4];
  else
    str1p = str1;

  if (strlen (str2) > 4 && strncasecmp (str2, "the ", 4) == 0)
    str2p = &str2[4];
  else
    str2p = str2;


  str1l = strlen (str1p);
  str2l = strlen (str2p);

  shortest_string = ((str1l < str2l) ? str1l : str2l) + 1;

  for (i = 0 ; !cmp && i < shortest_string ; i++)
    if (tolower (str1p[i]) > tolower (str2p[i]))
      cmp = 1;
    else if (tolower(str1p[i]) < tolower(str2p[i]))
      cmp = -1;

  if (cmp == 0) {
    if (str1l > str2l)
      cmp = -1;
    else if (str1l < str2l)
      cmp = 1;
  }

  return cmp;
}

static void dosort_flist_rio (int section, struct sort_list *x, struct sort_list *tmp, int size) {
  int i, j, k;

  char *str1, *str2;

  if (size < 2)
    return;
  
  dosort_flist_rio (section, x, tmp, size/2);
  dosort_flist_rio (section, &x[size/2], tmp, size - size/2);

  /* merge */
  for (i = 0, j = size/2, k = 0 ; (i < size/2) && (j < size) ; k++) {
    if (section == 0) {
      str1 = x[i].ptr->title;
      str2 = x[j].ptr->title;
    } else if (section == 1) {
      str1 = x[i].ptr->album;
      str2 = x[j].ptr->album;
    } else if (section == 2) {
      str1 = x[i].ptr->artist;
      str2 = x[j].ptr->artist;
    } else if (section == 3) {
      str1 = x[i].ptr->genre;
      str2 = x[j].ptr->genre;
    }
    
    if (str_cmp (str1, str2) <= 0) {
      tmp[k].seq_number = x[i].seq_number;
      tmp[k].ptr = x[i++].ptr;
    } else {
      tmp[k].seq_number = x[j].seq_number;
      tmp[k].ptr = x[j++].ptr;
    }
  }
  
  while (i < size/2) {
    tmp[k].seq_number = x[i].seq_number;
    tmp[k++].ptr = x[i++].ptr;
  }

  while (j < size) {
    tmp[k].seq_number = x[j].seq_number;
    tmp[k++].ptr = x[j++].ptr;
  }
  
  memcpy (x, tmp, size * sizeof (struct sort_list));
  
  return;
}

static void sort_flist_rio (rios_t *rio, int section, int num_tracks, struct sort_list **x) {
  flist_rio_t *flist;
  int i;
  struct sort_list *tmp;

  *x = calloc (num_tracks, sizeof (struct sort_list));
  tmp = calloc (num_tracks, sizeof (struct sort_list));

  for (i = 0, flist = rio->info.memory[0].files ; flist ; flist = flist->next, i++) {
    (*x)[i].seq_number = i;
    (*x)[i].ptr = flist;
  }

  dosort_flist_rio (section, *x, tmp, num_tracks);

  free (tmp);
}

static void set_uint24 (unsigned char *buf, int block, unsigned int value) {
  buf [block * 3 + 0] = value & 0x000000ff;
  buf [block * 3 + 1] = (value & 0x0000ff00) >> 8;
  buf [block * 3 + 2] = (value & 0x00ff0000) >> 16;
}

static char *db_sections[] = {"title", "source", "artist", "genre", "year", "date", "playlist"};

static int build_db_sec_rio (rios_t *rio, unsigned char *buf, int num_tracks, unsigned char *taxi_buf, int section, int start_block) {
  int padding = 0xffffff;
  int cblock = start_block;
  struct sort_list *tmp_list;
  int i;

  int next_block = 0;
  int next_letter_block = 0;
  int prev_letter_block = 0;

  int prev_block = 0;
  int count_block = 0;
  int xblock;

  int taxi1, taxi2;

  char *str, *str2;

  char last_letter = 0;

  set_uint24 (buf, 2 * (section + 2) + 1, start_block);
    
  memset (&buf[cblock * 3], 0xff, 6);
  cblock += 2;

  strcpy ((char *)&buf[3 * cblock], db_sections[section]);
  cblock += (strlen(db_sections[section]) + 2)/3;

  /* these sections are not yet implemented. (year, date, playlist) */
  if (section > 3)
    return cblock;

  if (section != 3)
    set_uint24 (buf, 2 * (section + 2), cblock + 3);
  else
    set_uint24 (buf, 2 * (section + 2), cblock);

  if (section != 3)
    set_uint24 (buf, start_block, cblock);

  sort_flist_rio (rio, section, num_tracks, &tmp_list);

  for (i = 0 ; i < num_tracks ; i++) {
    if (section == 0)
      str = tmp_list[i].ptr->title;
    else if (section == 1)
      str = tmp_list[i].ptr->album;
    else if (section == 2)
      str = tmp_list[i].ptr->artist;
    else
      str = tmp_list[i].ptr->genre;

    taxi1 = cblock;

    if (tolower (str[0]) != last_letter && section != 3) {
      last_letter = tolower (str[0]);

      if (next_letter_block)
	set_uint24 (buf, next_letter_block, cblock);

      next_letter_block = xblock = cblock;
      
      set_uint24 (buf, cblock++, padding);
      set_uint24 (buf, cblock++, (prev_letter_block) ? prev_letter_block : start_block);
      
      prev_letter_block = xblock;

      buf[3 * cblock++ + 0] = str[0];
    }

    taxi2 = cblock;    
    xblock = cblock;
    
    if (next_block)
      set_uint24 (buf, next_block, cblock);
    
    next_block = cblock;

    set_uint24 (buf, cblock++, padding);
    set_uint24 (buf, cblock++, (prev_block) ? prev_block : padding);

    count_block = cblock++;

    for ( ; i < num_tracks ; i++) {
      set_uint24 (buf, cblock++, tmp_list[i].ptr->rio_num);

      set_uint24 (taxi_buf, tmp_list[i].seq_number * 0x1b + 2 * (section) + 1, taxi1);
      set_uint24 (taxi_buf, tmp_list[i].seq_number * 0x1b + 2 * (section), taxi2);
      
      if ((i + 1) == num_tracks)
	break;

      if (section == 0)
	str2 = tmp_list[i+1].ptr->title;
      else if (section == 1)
	str2 = tmp_list[i+1].ptr->album;
      else if (section == 2)
	str2 = tmp_list[i+1].ptr->artist;
      else
	str2 = tmp_list[i+1].ptr->genre;

      if (strcmp (str, str2) != 0)
	break;
    }
    
    set_uint24 (buf, count_block, cblock - count_block - 1);
    
    strcpy ((char *)&buf[3 * cblock], str);
    cblock += (strlen(str) + 3 - strlen(str) % 3)/3;
    
    prev_block = xblock;
  }

  free (tmp_list);
  return cblock;
}

static int build_database_rio (rios_t *rio, unsigned char *buf, size_t buf_size) {
  int i;
  uint j;

  unsigned char *taxi_buf;
  
  int next_sec;
  int num_tracks;
  int taxi_offset;

  flist_rio_t *flist;

  unsigned char db_magic[] = { 0x55, 0x9a, 0x81, 0x03, 0x00, 0x00 };
  uint prev_num = 0;
  int num_tracks_block;

  /* buf_size is not used (though it should be checked). remove compiler warning */
  (void) buf_size;

  memset (buf, 0xff, 0x3c);

  memcpy (buf, db_magic, 6);

  num_tracks = size_flist_rio (rio, 0);

  taxi_buf = calloc (1, num_tracks * 0x51);

  for (i = 0, flist = rio->info.memory[0].files ; flist ; flist = flist->next, i++) {
    int track_offset = 0x51 * i;

    memset (&taxi_buf[track_offset], 0xff, 3 * 12);
    memset (&taxi_buf[track_offset + 0x4e], 0xff, 3);

    set_uint24 (taxi_buf, track_offset/3 + 12, i + 1);

    /* file size */
    taxi_buf[track_offset + 3 * 14 + 0] = (flist->size & 0x0ff00000) >> 20;
    taxi_buf[track_offset + 3 * 14 + 1] = (flist->size & 0xf0000000) >> 28;
    taxi_buf[track_offset + 3 * 14 + 2] = 0; /* not needed */

    taxi_buf[track_offset + 3 * 15 + 0] = (bswap_32 (flist->size) & 0xff000000) >> 24;
    taxi_buf[track_offset + 3 * 15 + 1] = (bswap_32 (flist->size) & 0x00ff0000) >> 16;
    taxi_buf[track_offset + 3 * 15 + 2] = (bswap_32 (flist->size) & 0x00000f00) >> 8;

    /* time */
    set_uint24 (taxi_buf, track_offset/3 + 16, flist->time);

    set_uint24 (taxi_buf, track_offset/3 + 18, flist->track_number);
  }

  next_sec = 0x14;
  for (i = 0 ; i < 7 ; i++)
    next_sec = build_db_sec_rio (rio, buf, num_tracks, taxi_buf, i, next_sec);

  memset (&buf[next_sec * 3], 0xff, 6);
  next_sec += 2;

  strcpy ((char *)&buf[next_sec * 3], "taxi");
  next_sec += 2;
  
  set_uint24 (buf, next_sec, num_tracks);
  set_uint24 (buf, 3, next_sec++);

  taxi_offset = next_sec;

  memcpy (&buf[next_sec * 3], taxi_buf, num_tracks * 0x51);

  next_sec += (num_tracks * 0x51) / 3 + 2;

  set_uint24 (buf, 2, next_sec);
  num_tracks_block = next_sec++;
  set_uint24 (buf, next_sec++, 0xffffff);

  for (flist = rio->info.memory[0].files, i = 0 ; flist ; flist = flist->next, i++) {
    if (flist->rio_num != prev_num + 0x10)
      /* if there is a gap between the files we need to set those pointers to -1 */
      for (j = prev_num + 0x10 ; j < flist->rio_num ; j += 0x10)
	set_uint24 (buf, next_sec++, 0xffffff);

    set_uint24 (buf, next_sec++, taxi_offset + (0x1b * i));
    prev_num = flist->rio_num;
  }

  set_uint24 (buf, num_tracks_block, prev_num/0x10 + 2);

  free (taxi_buf);

  return next_sec;
}


/* The Nitrus expects extra information in a buffer made up of 3 byte chunks */
int update_db_rio (rios_t *rio) {
  unsigned char *buf;
  int i, ret;
  int blocks, db_size;

  if (return_type_rio (rio) != RIONITRUS)
    return URIO_SUCCESS;

  debug("librioutil/song_management.c update_db_rio: entering...");

  /* 8 * RIO_FTS is an arbitrary size for the buffer picked because it should provide enough storage
     for building the song database. */
  buf = calloc (1, 8 * RIO_FTS);
  if (buf == NULL) {
    error("librioutil/song_management.c update_db_rio: could not allocate a buffer in which to build the new database.");

    return -errno;
  }

  blocks = build_database_rio (rio, buf, 8 * RIO_FTS);
  db_size = blocks * 3;

  wake_rio (rio);
  if ((ret = send_command_rio (rio, RIO_NINFO, 0, 0)) != URIO_SUCCESS) {
    error("librioutil/song_management.c update_db_rio: rio did not respond to command.");

    free (buf);

    return ret;
  }
  
  /* RDY. */
  ret = read_block_rio (rio, NULL, 64, 64);

  if (ret != URIO_SUCCESS || strncmp ((char *)rio->buffer, "SRIORDY.", 8) != 0) {
    error("librioutil/song_management.c update_db_rio: device is not ready to receive the nitrus database: %d", ret);

    free (buf);

    return -EIO;
  }

  /* DATA */
  ret = read_block_rio (rio, NULL, 64, 64);

  if (ret != URIO_SUCCESS || strncmp ((char *)rio->buffer, "SRIODATA", 8) != 0) {
    error("librioutil/song_management.c update_db_rio: device did not respond as expected: %d", ret);

    free (buf);

    return -EIO;
  }

  for (i = 0 ; i < 8 * RIO_FTS ; i += RIO_FTS) {
    if (i > db_size)
      break;

    write_block_rio (rio, &buf[i], RIO_FTS, "CRIODATA");
  }

  write_cksum_rio (rio, buf, 0, "CRIOINFO");
  ret = read_block_rio (rio, NULL, 64, 64);

  if (ret != URIO_SUCCESS || strncmp ((char *)rio->buffer, "SRIODONE", 8) != 0) {
    error("librioutil/song_management.c update_db_rio: an unknown error occured while trying to write the nitrus database: %d", ret);

    free (buf);

    return -EIO;
  }

  free (buf);

  debug("librioutil/song_management.c update_db_rio: complete.");

  return URIO_SUCCESS;
}

/*
  complete_upload_rio:
    function uploads the final info page to tell the rio the transfer is complete

  * This function cannot be called before start_upload_rio.
*/
static int complete_upload_rio (rios_t *rio, u_int8_t memory_unit, info_page_t info) {
  int ret;

  debug("complete_upload_rio: entering...");

  /* memory_unit is unused. remove compiler warning */
  (void) memory_unit;

  /* Kelly: 08-23-03
     TODO: Set some the data in the RIOT's portion of the
     data structure.  Exclude other players until it
     can be confirmed that they use it or that it
     doesn't matter.
     
     All the variables ending in 2 may be
     specific to the RIOT.
     
     The size2 and possibly riot_file_no _MUST_ be
     set for the RIOT to successfully accept the data
     file. 
  */
  if (return_type_rio(rio) == RIORIOT || return_type_rio (rio) == RIONITRUS) {
    info.data->size2 = info.data->size;
    info.data->riot_file_no = info.data->file_no;
    info.data->time2 = info.data->time;
    info.data->demarc = 0x20;
    info.data->file_prefix = 0x2d203130;
    
    strncpy((char *)info.data->name2, info.data->name, 27);
    strncpy((char *)info.data->title2, info.data->title, 48);
    strncpy((char *)info.data->artist2, info.data->artist, 48);
    strncpy((char *)info.data->album2, info.data->album, 48);
  }

  file_to_arch (info.data);

  /* upload the info page */
  debug("complete_upload_rio: writing file header");
  
  write_block_rio (rio, (unsigned char *)info.data, sizeof (rio_file_t), "CRIOINFO");

  file_to_arch (info.data);
  
  
  /* 0x60 command is send by both iTunes and windows softare */
  if ((ret = send_command_rio(rio, 0x60, 0, 0)) != URIO_SUCCESS)
    return ret;
  
  debug("complete_upload_rio: complete.");

  return URIO_SUCCESS;
}

static int execute_delete_rio (rios_t *rio, u_int8_t memory_unit, rio_file_t *filep) {
  int ret;

  (void)wake_rio (rio);

  if ((ret = send_command_rio(rio, RIO_DELET, memory_unit, 0)) != URIO_SUCCESS)
    return ret;

  read_block_rio(rio, NULL, 64, RIO_FTS);
    
  if (strncmp((char *)rio->buffer, "SRIODELS", 8) != 0)
    return -EIO;

  /* correct the endianness of data */
  file_to_arch(filep);

  if ((ret = write_block_rio(rio, (unsigned char *)filep, RIO_MTS, NULL)) != URIO_SUCCESS)
    return ret;

  file_to_arch(filep);

  if (strncmp((char *)rio->buffer, "SRIODELD", 8) != 0)
    return ret;

  return 0;
}

/*
  delete_file_rio:

  delete a file off the rio. if the file has two info pages then delete
  both.

  PreCondition:
      - An initiated rio instance.
      - A memory unit.
      - A file on the unit.

  PostCondition:
      - URIO_SUCCESS if the file was delete successfully.
      - < 0 if some error occured.
*/
int delete_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num) {
  rio_file_t file;
  int ret, file_id;

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  debug("delete_file_rio: entering...");

  file_id = flist_get_file_id_rio (rio, memory_unit, file_num);
  if (file_id < 0) {
    error("librioutil/delete_file_rio: file not found.");

    UNLOCK (file_id);
  }

  ret = get_file_info_rio(rio, &file, memory_unit, file_id);
  if (ret != URIO_SUCCESS) {
    error("librioutil/delete_file_rio: could not get file info");

    UNLOCK(ret);
  }

  ret = execute_delete_rio (rio, memory_unit, &file);
  if (ret != 0) {
    error("librioutil/delete_file_rio: file deletion failed.");

    UNLOCK(ret);
  }

  /* file deletion successful */

  flist_remove_rio (rio, memory_unit, file_num);
  update_free_intrn_rio (rio, memory_unit);
    
  /* update nitrus database */
  update_db_rio (rio);
    
  debug("delete_file_rio: complete.");

  UNLOCK(URIO_SUCCESS);
}

static int upload_dummy_hdr (rios_t *rio, u_int8_t memory_unit, rio_file_t *filep) {
  info_page_t info;
  int error;
  int file_num = flist_first_free_rio (rio, memory_unit);

  debug("upload_dummy_hdr: entering...");

  /* uploading a duplicate file header with this value in the file bits makes
     the data downloadable on older diamond rios (600, 800, 900) */
  filep->bits = 0x10000591;

  /* these fields need to be clear to prevent the rio from removing the
     downloadable bit */
  filep->type        = 0;
  filep->file_no     = 0;
  filep->bit_rate    = 0;
  filep->sample_rate = 0;

  if ((error = init_new_upload_rio(rio, memory_unit)) != URIO_SUCCESS) {
    abort_transfer_rio(rio);
    return error;
  }

  info.data = filep;
  info.skip = 0;

  if ((error = complete_upload_rio(rio, memory_unit, info))!= URIO_SUCCESS) {
    abort_transfer_rio(rio);
    return error;
  }

  debug("upload_dummy_hdr: complete.");

  return file_num;
}

/*
  download_file_rio:
  Function takes in the number of the file
  and attemt to download it from the Rio.
 
  Note: This only works with the following files:
  - Recorded WAVE files on the Rio 800
  - preferences.bin file
  - bookmarks.bin file
  - non-music files uploaded by rioutil
  - any file from an S-Series** or newer player 
  
  ** In order to support downloading an S-Series player should be
  updated with the latest firmware.

  -- Note --
  Any file on a third generation player that has the 0x80 bit
  set can be downloaded.
  
  It seems that, probably due to the riaa, diamond has
  made it so that wma and mp3 files CAN NOT
  be downloaded. mp3s that are downloaded will be deleted.
  (rio600/800/900 only).

  All of the newer players from Rio support the download of any
  file on the player!
*/
int download_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num, char *file_name) {
  int i, blocks, size, ret, file_id, player_generation, read_size;
  int downfd;
  
  int download_complete, cr_dummy = -1;
  int mode = S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
  int block_size;

  unsigned char dload_buffer[RIO_FTS];

  char tmp_np[PATH_MAX];
  rio_file_t file;

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  debug("librioutil/song_management.c download_file_rio: entering...");

  player_generation = return_generation_rio (rio);
  
  /* get file header data */
  file_id = flist_get_file_id_rio (rio, memory_unit, file_num);
  if (file_id < 0) {
    error("librioutil/song_management.c download_file_rio: file not found: %d", file_id);

    UNLOCK(file_id);
  }

  if ((ret = get_file_info_rio(rio, &file, memory_unit, file_id)) != URIO_SUCCESS) {
    error("librioutil/song_management.c download_file_rio: error getting file info: %d", ret);

    UNLOCK(ret);
  }

  /* generate a filename if one was not supplied */
  if (file_name == NULL) {
    (void)flist_get_file_name_rio (rio, memory_unit, file_num, tmp_np, PATH_MAX);

    for ( i = strlen (tmp_np) - 1 ; i > 0 && tmp_np[i] != '\\' && tmp_np[i] != '/' ; i--);

    file_name = &tmp_np[i];
  }
  


  if (player_generation < 5 && return_version_rio(rio) < 2.0 && return_type_rio (rio) != RIORIOT) {
    /*
      This code is only relevant to older players
      
      A dummy header is not needed with newer players/firmwares and the RIOT as
      they do not have the same restrictions on downloading from the device.
    */
    if (file.start == 0)
      UNLOCK(-EPERM);

    if (player_generation == 3 && !(file.bits & 0x00000080)) {
      /* Older players will only allow non-music files to be downloaded. A fake
	 file header is used to download these files. Such a download will cause
	 the deletion of the file off of the device. */
      file_id = upload_dummy_hdr (rio, memory_unit, &file);
      if (file_id < 0) {
	error("librioutil/song_management.c download_file_rio: error uploading dummy file header.");
	UNLOCK(file_id);
      }
  
      if ((ret = get_file_info_rio(rio, &file, memory_unit, file_id)) != URIO_SUCCESS) {
        error("librioutil/song_management.c download_file_rio: could not fetch song info: %d", ret);
	UNLOCK(ret);
      }
    }
  }


  /* send the send file command */
  (void)wake_rio (rio);
  
  if ((ret = send_command_rio(rio, RIO_READF, memory_unit, 0)) != URIO_SUCCESS)
    UNLOCK(ret);
  
  if ((ret = read_block_rio(rio, NULL, 64, RIO_FTS)) != URIO_SUCCESS)
    UNLOCK(ret);
    
  /* send file header data */
  file_to_arch (&file);
  write_block_rio(rio, (unsigned char *)&file, sizeof(rio_file_t), NULL);
  file_to_arch (&file);
  
  if (memcmp(rio->buffer, "SRIONOFL", 8) == 0) {
    /* file does not exist */
    error("librioutil/song_management.c download_file_rio: (device) no such file");

    UNLOCK(-1);
  }


  /* create local file */
  debug("librioutil/song_management.c download_file_rio: downloading to file %s", file_name);

  downfd = creat (file_name, mode);
  if (downfd < 0) {
    error("librioutil/song_management.c download_file_rio: could not create file %s: %s", file_name, strerror (errno));

    abort_transfer_rio (rio);

    UNLOCK(-1);
  }


  size = file.size;
  /* older rios (rio600, rio800, etc) send file data in smaller (4096 byte) chunks. */
  block_size = (player_generation >= 4) ? RIO_FTS : 4096;
  blocks = size/block_size + ((size % block_size) ? 1 : 0);

  if (rio->progress)
    rio->progress(0, 1, rio->progress_ptr);

  /* retrieve file data from the device */
  for (i = 0, download_complete = 0 ; i < blocks ; i++) {
    memset (dload_buffer, 0, block_size);

    if (rio->abort) {
      abort_transfer_rio (rio);
      rio->abort = 0;
      
      if (rio->progress)
	rio->progress(1, 1, rio->progress_ptr);
      
      close(downfd);
      UNLOCK(URIO_SUCCESS);
    }
    
    /* the rio appears to expect a checksum in the CRIODATA packet */
    write_cksum_rio (rio, dload_buffer, block_size, "CRIODATA");
    
    read_block_rio(rio, NULL, 64, 64);
    
    /* check for completion */
    if (memcmp(rio->buffer, "SRIODONE", 8) == 0){
      download_complete = 1;

      break;
    }
    
    if (size >= block_size)
      read_size = block_size;
    else
      read_size = size;
    
    read_block_rio (rio, dload_buffer, RIO_FTS, block_size);
    
    if (rio->progress)
      rio->progress(i, blocks, rio->progress_ptr);
    
    write(downfd, dload_buffer, read_size);
    
    size -= read_size;
  }
  
  if (!download_complete) {
    write_cksum_rio (rio, dload_buffer, block_size, "CRIODATA");
  
    if (player_generation < 4)
      read_block_rio(rio, NULL, 64, RIO_FTS);

    if (rio->progress)
      rio->progress(1, 1, rio->progress_ptr);
  }

  close(downfd);
  
  if (cr_dummy != -1) {
    /* If a dummy header was uploaded, delete it. An unfortunate side-effect of this
       deletion is the file's data will no longer be available as the device appears
       to free the memory. */
    execute_delete_rio (rio, memory_unit, &file);

    /* avoid a bad device state by deleting the original file */
    delete_file_rio (rio, memory_unit, cr_dummy);
  }
  
  debug("librioutil/song_management.c download_file_rio: complete.");
  UNLOCK(URIO_SUCCESS);
}
