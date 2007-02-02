/**
 *   (c) 2001-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 rio.h
 *
 *   header file for librioutil
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

#ifndef _RIO_H
#define _RIO_H

#include <sys/types.h>

/* errors */
#define URIO_SUCCESS 0

/* other defines */
/* no rio has more more than 2 memory units (internal + SD/MMC or "backpack") */
#define MAX_MEM_UNITS   2
#define MAX_RIO_FILES   3000 /* arbitrary */


/*
  Playlist structure for older players (.lst file format):

  first entry is always blank.
  name is copied from rio_file->name
  title is copied from the first 32 characters of rio_file->title
  unk[3] is the entry number (LSB first)

  Note: librioutil does not handle creation of playlists on older player.
*/
typedef struct _rio_lst {
    struct song_list {
	u_int8_t  name[64];
	u_int32_t unk[8];
	u_int8_t  title[32];
    } playlist[128];
} rio_plst_t;

typedef struct _rio_file_list {
  char artist[64];
  char title[64];
  char album[64];
  char name[64];
  
  int bitrate;
  int samplerate;
  int mod_date;
  int size;
  int time;
  
  /* pointer to start of file in rio's memory. do not change */
  int start;
  
  enum file_type {MP3 = 0, WMA, WAV, WAVE, OTHER} type;
  
  /* these values should not be set/changed outside the library */
  int num;
  int inum;
  
  struct _rio_file_list *prev;
  struct _rio_file_list *next;

  u_int8_t sflags[3];
  u_int32_t rio_num;
  /**************************************************************/

  char year[5];
  char genre[17];

  int track_number;
} file_list;

typedef file_list flist_rio_t;

typedef struct _rio_device_mem {
    u_int32_t size;
    u_int32_t free;
    char name[32];

    flist_rio_t *files;

    u_int32_t total_time;
    u_int32_t num_files;
} mem_list;

typedef mem_list mlist_rio_t;

typedef struct _rio_device_info {
  struct _rio_device_mem memory[MAX_MEM_UNITS];
  
  /* these values can be changed an sent to the rio */
  char name[16];
  
  u_int8_t  light_state;
  u_int8_t  repeat_state;
  u_int8_t  eq_state;
  u_int8_t  bass;
  u_int8_t  treble;
  u_int8_t  sleep_time;
  u_int8_t  contrast;
  u_int8_t  playlist;
  u_int8_t  volume;
  u_int8_t  random_state;
  u_int8_t  the_filter_state;
  
  /* these values can not be manipulated */
  u_int8_t  total_memory_units; /* 1 or 2 */
  
  float firmware_version;
  u_int8_t serial_number[16];
} rio_info_t;

typedef struct _rios {
  /* void here to avoid the user needing to define WITH_USBDEVFS and such */
  void *dev;

  rio_info_t info;

  int debug;
  void *log;
  int abort;
  
  unsigned char cmd_buffer[16];
  unsigned char buffer    [64];
  
  void (*progress)(int x, int X, void *ptr);
  void *progress_ptr;

  /* make rioutil thread-safe */
  int lock;
} rios_t;

typedef rios_t rio_instance_t;

/*
  rio funtions:
*/
int open_rio (rios_t *rio, int number, int debug, int fill_structures);
void close_rio (rios_t *rio);

int set_info_rio (rios_t *rio, rio_info_t *info);
int set_name_rio (rios_t *rio, char *name);
int add_song_rio (rios_t *rio, u_int8_t memory_unit, char *file_name, char *artist, char *title, char *album);
int download_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num, char *fileName);
int delete_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num);
int format_mem_rio (rios_t *rio, u_int8_t memory_unit);

/* upgrade the rio's firmware from a file */
int firmware_upgrade_rio (rios_t *rio, char *file_name);

/* update the rio structure's internal info structure */
int update_info_rio (rios_t *rio);
/* store a copy of the rio's internal info structure in info */
int get_info_rio (rios_t *rio, rio_info_t **info);

/* sets the progress callback function */
void set_progress_rio  (rios_t *rio, void (*f)(int x, int X, void *ptr), void *ptr);

/* These only work with S-Series or newer Rios */
int create_playlist_rio (rios_t *rio, char *name, int songs[], int memory_units[], int nsongs);
int overwrite_file_rio (rios_t *rio, u_int8_t memory_unit, u_int32_t file_num, char *filename);
int return_serial_number_rio (rios_t *rio, u_int8_t serial_number[16]);


/* Added to API 02-02-2005 */
/* Returns the file number that will be assigned to the next file uploaded. */
int first_free_file_rio (rios_t *rio, u_int8_t memory_unit);

/* library info */
char *return_conn_method_rio(void);

/*
  retrieve data on a rio's memory units

  memory_unit range : 0 -> MAX_MEM_UNITS - 1
*/
int return_mem_units_rio (rios_t *rio);
int return_free_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_used_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_total_mem_rio (rios_t *rio, u_int8_t memory_unit);
int return_num_files_rio (rios_t *rio, u_int8_t memory_unit);
int return_time_rio (rios_t *rio, u_int8_t memory_unit);

/* retrieve a copy of the file list and store it in flist */
typedef enum {RMP3=0x01, RWMA=0x02, RWAV=0x04, RDOW=0x08, RSYS=0x10, RLST=0x20, RALL=0x3f} list_flags_rio_t;
int return_flist_rio (rios_t *rio, u_int8_t memory_unit, u_int8_t list_flags, flist_rio_t **flist);

void free_flist_rio (flist_rio_t *flist);

char *return_file_name_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);
int return_file_size_rio (rios_t *rio, u_int32_t song_id, u_int8_t memory_unit);


/* device types */
enum rios { RIO600, RIO800, PSAPLAY, RIO900, RIOS10, RIOS50,
	    RIOS35, RIOS30, RIOFUSE, RIOCHIBA, RIOCALI,
	    RIORIOT, RIOS11, RIONITRUS, UNKNOWN };
int return_type_rio (rios_t *rio);

#endif /* _RIO_H */
