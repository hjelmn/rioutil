/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.2.1 rio.c
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

#if defined (HAVE_LIBGEN_H)
#include <libgen.h>
#endif

#include "rioi.h"
#include "driver.h"

struct player_device_info player_devices[] = {
  /* Rio 600/800/900 and Nike psa[play Use bulk endpoint 2 for read/write */
  {VENDOR_DIAMOND01, PRODUCT_RIO600 , 0x2, 0x2, RIO600   , 3},
  {VENDOR_DIAMOND01, PRODUCT_RIO800 , 0x2, 0x2, RIO800   , 3},
  {VENDOR_DIAMOND01, PRODUCT_PSAPLAY, 0x2, 0x2, PSAPLAY  , 3},
  {VENDOR_DIAMOND01, PRODUCT_RIO900 , 0x2, 0x2, RIO900   , 3},
  /* Rio S-Series Uses bulk endpoint 1 for read and 2 for write */
  {VENDOR_DIAMOND01, PRODUCT_RIOS30 , 0x1, 0x2, RIOS30   , 4},
  {VENDOR_DIAMOND01, PRODUCT_RIOS35 , 0x1, 0x2, RIOS35   , 4},
  {VENDOR_DIAMOND01, PRODUCT_RIOS10 , 0x1, 0x2, RIOS10   , 4},
  {VENDOR_DIAMOND01, PRODUCT_RIOS50 , 0x1, 0x2, RIOS50   , 4},
  {VENDOR_DIAMOND01, PRODUCT_FUSE   , 0x1, 0x2, RIOFUSE  , 5},
  {VENDOR_DIAMOND01, PRODUCT_CHIBA  , 0x1, 0x2, RIOCHIBA , 5},
  {VENDOR_DIAMOND01, PRODUCT_CALI   , 0x1, 0x2, RIOCALI  , 5},
  {VENDOR_DIAMOND01, PRODUCT_CALI256, 0x1, 0x2, RIOCALI  , 5},
  {VENDOR_DIAMOND01, PRODUCT_RIOS11 , 0x1, 0x2, RIOS11   , 4},
  /* Rio Riot Uses bulk endpoint 2 for read and 1 for write */
  {VENDOR_DIAMOND01, PRODUCT_RIORIOT, 0x2, 0x1, RIORIOT  , 3},
  {VENDOR_DIAMOND01, PRODUCT_NITRUS,  0x1, 0x2, RIONITRUS, 5},
  {0}
};


/* statically defined functions */
static int set_time_rio (rios_t *rio);
static int return_intrn_info_rio(rios_t *rio);

/* read the supported file types from the rio */
static int read_ftypes_rio (rios_t *rio) {
  int i, ret;

  for (i = 0 ; i < 3 ; i++) {
    if ((ret = send_command_rio(rio, 0x60, 0, 0)) != URIO_SUCCESS)
      return ret;
    if ((ret = send_command_rio(rio, 0x63, i, 0)) != URIO_SUCCESS)
      return ret;

    read_block_rio (rio, NULL, 64, RIO_FTS);
    read_block_rio (rio, NULL, 64, RIO_FTS);
  }

  return 0;
}

/*
  open_rio:
    Open rio.

  PostCondition:
      - An initiated rio instance.
      - NULL if an error occured.
*/
int open_rio (rios_t *rio, int number, int debug, int fill_structures) {
  int ret;

  if (rio == NULL)
    return -EINVAL;

  memset(rio, 0, sizeof(rios_t));
  
  rio->debug       = debug;
  rio->log         = stderr;
  
  rio_log (rio, 0,
	   "open_rio: creating new rio instance. device: 0x%08x\n", number);
  
  if (debug) {
    rio_log (rio, 0, "open_rio: setting usb driver verbosity level to %i\n",
	     debug);

    usb_setdebug(debug);
  }

  rio->abort = 0;
  
  /* open the USB device (this calls the underlying driver) */
  if ((ret = usb_open_rio (rio, number)) != 0) {
    rio_log (rio, ret, "open_rio: could not open a Rio device\n");

    return ret;
  }
  
  ret = set_time_rio (rio);
  if (ret != URIO_SUCCESS && fill_structures != 0) {
    close_rio (rio);

    return ret;
  }
  
  send_command_rio(rio, 0x61, 0, 0);
  send_command_rio(rio, 0x61, 0, 0);
  send_command_rio(rio, 0x65, 0, 0);

  read_ftypes_rio (rio);

  unlock_rio (rio);

  if (fill_structures != 0) {
    ret = return_intrn_info_rio (rio);
    if (ret != URIO_SUCCESS) {
      close_rio (rio);
      
      return ret;
    }
  }

  rio_log (rio, 0, "open_rio: new rio instance created.\n");

  return URIO_SUCCESS;
}

/*
  set_time_rio:
    Only sets the rio's time these days.
*/
static int set_time_rio (rios_t *rio) {
  long int curr_time;
  struct timeval tv;
  struct timezone tz;
  struct tm *tmp;
  int ret;
  
  /*
   * the rio has no concept of timezones so we need to take
   * the local time into account when setting the time.
   * now using the (non)obselete gettimeofday function
   * i am not sure if this is the best way
   *
   */
  gettimeofday (&tv, &tz);
  tmp = localtime ((const time_t *)&(tv.tv_sec));

  rio_log (rio, 0, "librioutil/rio.c set_time_rio: Setting device time from system clock: %s\n", asctime(tmp));

  curr_time = tv.tv_sec - 60 * tz.tz_minuteswest;
  
  if (tmp->tm_isdst != -1)
    curr_time += 3600 * tmp->tm_isdst;

  ret = send_command_rio (rio, 0x60, 0, 0);
  if (ret != URIO_SUCCESS)
    return ret;
  
  /* tell the rio what time it is, assuming your system clock is correct */
  ret = send_command_rio (rio, RIO_TIMES, curr_time >> 16, curr_time & 0xffff);
  if (ret != URIO_SUCCESS)
    return ret;
  
  return URIO_SUCCESS;
}

/*
  close_rio:
  Close connection with rio and free buffer.
*/
void close_rio (rios_t *rio) {
  if (try_lock_rio (rio) != 0)
    return;
  
  rio_log (rio, 0, "close_rio: entering...\n");

  (void)wake_rio (rio);
  
  /* close connection */
  usb_close_rio (rio);
  rio->dev = NULL;

  /* release the memory used by this instance */
  free_info_rio (rio);

  unlock_rio (rio);
  
  rio_log (rio, 0, "close_rio: complete\n");
}

int get_file_info_rio(rios_t *rio, rio_file_t *file, u_int8_t memory_unit, u_int16_t file_no) {
  int ret;
  
  if (rio == NULL || file == NULL)
    return -EINVAL;

  (void)wake_rio(rio);

  memset (file, 0, sizeof (rio_file_t));

  /* TODO -- Clean up code so it is easier to associate this with Riot
   * The RIOT doesn't need to do this to delete a file. */
  if (return_type_rio (rio) != RIORIOT) {
    /* send command to get file header */
    if ((ret = send_command_rio(rio, RIO_FILEI, memory_unit, file_no))
	!= URIO_SUCCESS)
      return ret;
    
    /* command was successful, read 2048 bytes of data from Rio */
    if ((ret = read_block_rio(rio, (unsigned char *)file, sizeof(rio_file_t), RIO_FTS))
	!= URIO_SUCCESS)
      return ret;

    /* library handles endianness */
    file_to_me(file);
    
    /* no file exists with number 0, they are listed from 1 */
    if (file->file_no == 0)
      return -ENOENT;
  } else {
    /* for the RIOT to delete files (does this also work with downloads?) */
    file->file_no      = file_no;
    file->riot_file_no = file_no;
  }

  return URIO_SUCCESS;
}

/*
  generate_mem_list_rio:

  Generates the info->memory field of the rio structure.
*/
int generate_mem_list_rio (rios_t *rio) {
  int i, ret;
  rio_mem_t memory;
  int num_mem_units = MAX_MEM_UNITS;

  mlist_rio_t *list = rio->info.memory;

  rio_log (rio, 0, "create_mem_list_rio: entering...\n");

  memset(list, 0, sizeof(mlist_rio_t) * MAX_MEM_UNITS);

  if (return_type_rio(rio) == RIORIOT) {
    /* Riots have only one memory unit */
    ret = get_memory_info_rio (rio, &memory, 0);
    
    if (ret != URIO_SUCCESS)
      return ret;

    rio->info.total_memory_units = 1;

    list[0].size       = memory.size;
    list[0].free       = memory.free;
      
    ret = generate_flist_riohd (rio);

    if (ret != URIO_SUCCESS)
      return ret;
  } else {
    rio->info.total_memory_units = 0;

    for (i = 0 ; i < num_mem_units ; i++) {
      ret = get_memory_info_rio (rio, &memory, i);
      
      if (ret == ENOMEM)
	break; /* not an error */
      else if (ret != URIO_SUCCESS)
	return ret;

      rio->info.total_memory_units++;

      list[i].size       = memory.size;
      list[i].free       = memory.free;
      strncpy(list[i].name, memory.name, 32);
      
      ret = generate_flist_riomc (rio, i);
      
      if (ret != URIO_SUCCESS)
	return ret;
    }
  }

  rio_log (rio, 0, "create_mem_list_rio: complete\n");

  return URIO_SUCCESS;
}

int get_memory_info_rio(rios_t *rio, rio_mem_t *memory, u_int8_t memory_unit) {
  int ret;
  
  if (!rio)
    return -1;
  
  (void)wake_rio(rio);

  if ((ret = send_command_rio(rio, RIO_MEMRI, memory_unit, 0)) != URIO_SUCCESS)
    return ret;

  /* command was successful, read 256 bytes from Rio */
  if ((ret = read_block_rio(rio, (unsigned char *)memory, 256, RIO_FTS)) != URIO_SUCCESS) 
      return ret;

  /* swap to big endian if needed */
  mem_to_me(memory);
  
  /* if requested memory unit is out of range Rio returns 256 bytes of 0's */
  if (memory->size == 0)
    return ENOMEM; /* not an error */

  return URIO_SUCCESS;
}

/* this should work better that before */
void update_free_intrn_rio (rios_t *rio, u_int8_t memory_unit) {
  rio_mem_t memory;

  get_memory_info_rio(rio, &memory, memory_unit);

  rio->info.memory[memory_unit].free = memory.free;
}

int return_type_rio(rios_t *rio) {
  return ((struct rioutil_usbdevice *)rio->dev)->entry->type;
}

/*
  first generation : Rio300 (Unsupported)
  second generation: Rio500 (Unsupported)
  third generation : Rio600, Rio800, Rio900, psa[play, Riot
  fourth generation: S-Series
  fith generation  : Fuse, Chiba, Cali, Nitrus
                     Eigen, Karma (Unsupported)
*/
int return_generation_rio (rios_t *rio) {
  return ((struct rioutil_usbdevice *)rio->dev)->entry->gen;
}

float return_version_rio (rios_t *rio) {
  return rio->info.firmware_version;
}

static int get_device_prefs_rio (rios_t *rio, rio_info_t *infop) {
  int ret;
  unsigned char buffer[RIO_MTS];

  /* iTunes sends this set of commands before RIO_PREFR */
  ret = send_command_rio(rio, RIO_PREFR, 0, 0);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, -1, "get_device_prefs_rio: rio did not respond to read preferences command.\n");    

    return ret;
  }

  ret = read_block_rio (rio, buffer, RIO_MTS, RIO_FTS);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "return_info_rio: error reading preference data\n");

    return ret;
  }

  if (return_type_rio (rio) != RIORIOT) { /* All but the RIOT */
    rio_prefs_t prefs;

    /* copy into the prefs structure */
    memcpy ((void *)&prefs, buffer, RIO_MTS);

    /* Copy the prefs into the info structure */
    memcpy(infop->name, prefs.name, 17);
    infop->volume           = prefs.volume;
    infop->playlist         = prefs.playlist;
    infop->contrast         = prefs.contrast - 1;
    infop->sleep_time       = prefs.sleep_time % 5;
    infop->treble           = prefs.treble;
    infop->bass             = prefs.bass;
    infop->eq_state         = prefs.eq_state % 8;
    infop->repeat_state     = prefs.repeat_state % 4;
    infop->light_state      = prefs.light_state % 6;
    infop->random_state     = 0; /* RIOT Only */
    infop->the_filter_state = 0; /* RIOT Only */
  } else { /* This is a RIOT */
    riot_prefs_t riot_prefs;  

    /* copy into the prefs structure */
    memcpy ((void *)&riot_prefs, buffer, RIO_MTS);

    /* Copy the riot_prefs into the info structure */
    memcpy(infop->name, riot_prefs.name, 17);
    infop->volume           = riot_prefs.volume;
    infop->contrast         = riot_prefs.contrast - 1; /* do we really need the -1 */
    infop->sleep_time       = riot_prefs.sleep_time;
    infop->treble           = riot_prefs.treble;
    infop->bass             = riot_prefs.bass;
    infop->repeat_state     = riot_prefs.repeat_state % 4; /* Do we really need the mod 4? */
    infop->light_state      = riot_prefs.light_state;
    infop->random_state     = riot_prefs.random_state;
    infop->the_filter_state = riot_prefs.the_filter_state;
    infop->eq_state         = 0; /* Not on RIOT */
    infop->playlist         = 0; /* Not on RIOT */
  }

  return 0;
}

static int get_device_description_rio (rios_t *rio, rio_info_t *infop) {
  int ret;
  unsigned char desc[256];

  ret = send_command_rio(rio, RIO_DESCP, 0, 0);
  if (ret != URIO_SUCCESS)
    return ret;

  ret = read_block_rio(rio, desc, 256, RIO_FTS);
  if (ret != URIO_SUCCESS)
    return ret;

  infop->firmware_version = desc[5] + (0.1) * (desc[4] >> 4) + (0.01) * (desc[4] & 0xf);
  memmove (infop->serial_number, &desc[0x60], 16);

  return 0;
}

/*
  return_intrn_info_rio: fill the rio_info structure.
*/
static int return_intrn_info_rio(rios_t *rio) {
  int ret;
  
  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  (void)wake_rio (rio);

  memset (&rio->info, 0, sizeof (rio_info_t));
  
  /* retrieve serial number and firmware version */
  ret = get_device_description_rio (rio, &rio->info);
  if (ret < 0) {
    rio_log (rio, ret, "librioutil/rio.c return_intrn_info_rio: error reading device description\n");

    UNLOCK(ret);
  }

  /* retrieve user preferences */
  (void)get_device_prefs_rio (rio, &rio->info);
     
  /* generate internal file ane memory lists */
  ret = generate_mem_list_rio(rio);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "librioutil/rio.c return_intrn_info_rio: could not generate memory/file listing\n");

    UNLOCK(ret);
  }

  UNLOCK(URIO_SUCCESS);
}

static void sane_info_copy (rio_info_t *info, rio_prefs_t *prefs);

/*
  set_info_rio:
  Set preferences on rio.

  PreCondition:
  - An initiated rio instance (Rio S-Series does not support this command).
  - A pointer to a filled info structure.

  PostCondition:
  - URIO_SUCCESS if the preferences get set.
  - < 0 if an error occured.
*/
int set_info_rio(rios_t *rio, rio_info_t *info) {
  rio_prefs_t pref_buf;
  int ret;
  unsigned char cmd;

  if (rio == NULL)
    return -EINVAL;

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  /* noting to write */
  if (info == NULL)
    return -1;

  (void)wake_rio (rio);
  
  ret = send_command_rio(rio, RIO_PREFR, 0, 0);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "set_info_rio: error sending read preferences command\n");

    UNLOCK(ret);
  }
  
  ret = read_block_rio(rio, (unsigned char *)&pref_buf, RIO_MTS, RIO_FTS);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "set_info_rio: error reading preference data\n");

    UNLOCK(ret);
  }
  
  sane_info_copy (info, &pref_buf);
  
  ret = send_command_rio(rio, RIO_PREFS, 0, 0);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "set_info_rio: error sending set preferences command\n");

    UNLOCK(ret);
  }

  ret = read_block_rio(rio, NULL, 64, RIO_FTS);
  if (ret != URIO_SUCCESS) {
    rio_log (rio, ret, "set_info_rio: error reading from device\n");
    
    UNLOCK(ret);
  }

  if ((ret = write_block_rio(rio, (unsigned char *)&pref_buf, RIO_MTS, NULL)) != URIO_SUCCESS)
    rio_log (rio, ret, "set_info_rio: error writing preferences\n");

  UNLOCK(ret);
}

int set_name_rio (rios_t *rio, char *name) {
  int ret;
  rio_info_t *info;

  if (rio == NULL || name == NULL)
    return -EINVAL;

  if (rio->info.memory[0].size == 0 && (ret = return_intrn_info_rio (rio)) != URIO_SUCCESS)
    return ret;

  strncpy (rio->info.name, name, 16);

  return set_info_rio (rio, info);
}

/*
  sane_info_copy:
  Make sure all values of info are sane and put them into prefs.
*/
static void sane_info_copy (rio_info_t *info, rio_prefs_t *prefs) {
  prefs->eq_state     = ((info->eq_state < 7)     ? info->eq_state     : 7);
  prefs->treble       = ((info->treble < 9)       ? info->treble       : 9);
  prefs->bass         = ((info->bass < 9)         ? info->bass         : 9);
  prefs->repeat_state = ((info->repeat_state < 2) ? info->repeat_state : 2);
  prefs->sleep_time   = ((info->sleep_time < 9)   ? info->sleep_time   : 9);
  prefs->light_state  = ((info->light_state < 5)  ? info->light_state  : 5);
  prefs->contrast     = ((info->contrast < 9)     ? info->contrast + 1 : 10);
  prefs->volume       = ((info->volume < 20)      ? info->volume       : 20);

  /* i don't think it would be a good idea to set this */
  /* prefs->playlist */

  if (strlen(info->name) > 0)
    strncpy(prefs->name, info->name, 16);
}

/*
  format_mem_rio:
  
  Erase a memory unit.
*/
int format_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  u_int32_t ret, pd;

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  rio_log (rio, 0, "librioutil/rio.c format_mem_rio: erasing memory unit %i\n", memory_unit);

  /* don't need to call wake_rio here */

  if (rio->progress)
    rio->progress (0, 1, rio->progress_ptr);

  if ((ret = send_command_rio(rio, RIO_FORMT, memory_unit, 0)) != URIO_SUCCESS)
    UNLOCK(ret);

  while (1) {
    if ((ret = read_block_rio(rio, NULL, 64, RIO_FTS)) != URIO_SUCCESS)
      UNLOCK(ret);

    /* newer players (Fuse, Chiba, Cali) return their progress */
    if (strstr((char *)rio->buffer, "SRIOPR") != NULL) {
      sscanf ((char *)rio->buffer, "SRIOPR%02d", &pd);

      if (rio->progress)
	rio->progress (pd, 100, rio->progress_ptr);
    } else if (strstr((char *)rio->buffer, "SRIOFMTD") != NULL) {
      /* format operation completed successfully */
      break;
    } else {
      rio_log (rio, -1, "librioutil/rio.c format_mem_rio: erase failed\n");

      UNLOCK(-1);
    }
  }

  if (rio->progress)
    rio->progress (1, 1, rio->progress_ptr);

  rio_log (rio, 0, "librioutil/rio.c format_mem_rio: erase complete\n");

  UNLOCK(URIO_SUCCESS);
}

/*
  update_rio:

  Update the firmware on a Rio. Function supports all rioutil supported players.
*/
int update_rio (rios_t *rio, char *file_name) {
  rio_log (rio, 0, "update_rio: function depricated. use firmware_upgrade_rio instead.\n");

  return firmware_upgrade_rio (rio, file_name);
}

int firmware_upgrade_rio (rios_t *rio, char *file_name) {
  /* a block size of 0x2000 bytes is used by the original software */
  size_t blocksize = 0x2000;
  unsigned char fileBuffer[blocksize];

  struct stat statinfo;
  int size, blocks, x;

  u_int32_t *intp;
  int firm_fd;

  int ret, pg;
  int player_generation;

  if (rio == NULL || file_name == NULL || stat(file_name, &statinfo) < 0)
    return -EINVAL;

  size = statinfo.st_size;

  rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: updating firmware of generation %d rio...\n",
	   player_generation);
  
  (void)wake_rio(rio);

  /* some upgrades require that the memory unit be erased */
  rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: formatting internal memory\n");
  if ((ret = format_mem_rio (rio, 0)) != URIO_SUCCESS)
    UNLOCK(ret);

  if ((ret = try_lock_rio (rio)) != 0)
    return ret;

  player_generation = return_generation_rio (rio);


  /* try to open the firmware file */
  if ((firm_fd = open(file_name, O_RDONLY)) < 0)
    UNLOCK(errno);

  /* it is not necessary to check the .lok file as the player will reject bad input */
  rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: sending firmware update device command...\n");

  if ((ret = send_command_rio(rio, RIO_UPDAT, 0x1, 0)) != URIO_SUCCESS) {
    rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: device did not respond to command.\n");

    close (firm_fd);
    UNLOCK(ret);
  }

  if ((ret = read_block_rio(rio, rio->buffer, 64, RIO_FTS)) != URIO_SUCCESS) {
    rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: device did not respond as expected.\n");
    
    close (firm_fd);
    UNLOCK(ret);
  }
  
  rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: device acknowleged command.\n");

  if (player_generation > 3)
    rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: erasing...\n");
  else
    rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: writing firmware...\n");

  /* send the size of the firmware data */
  memset(rio->buffer, 0, 64);
  intp = (u_int32_t *)rio->buffer;

  intp[0] = arch32_2_little32(size);

  if ((ret = write_block_rio(rio, rio->buffer, 64, NULL)) != URIO_SUCCESS)
    UNLOCK(ret);

  /* initialize progress callback */
  if (rio->progress != NULL)
    rio->progress (0, 1, rio->progress_ptr);

  /* on newer players the first write apparently erases the device (data is still sent) */
  for (blocks = size / blocksize, x = 0 ; x < blocks; x++) {
    /* read in a chunk of file */
    read(firm_fd, fileBuffer, blocksize);

    if (player_generation == 5) {
      /* newer players return the progress of the erase */
      if (strstr ((char *)rio->buffer, "SRIOPR") != NULL) {
	sscanf ((char *)rio->buffer, "SRIOPR%02d", &pg);
	
	if (rio->progress != NULL)
	  rio->progress (pg, 200, rio->progress_ptr);
      } else if (strstr ((char *)rio->buffer, "SRIODONE") != NULL) {
	if (rio->progress != NULL)
	  rio->progress (1, 1, rio->progress_ptr);

	close (firm_fd);
	return URIO_SUCCESS;
      }
    } else if (rio->buffer[1] == 2) {
      /* on older rios (third generation) it appears a 2 is returned to indicate the update
	 was successful */
      break;
    } if (rio->progress != NULL)
      /* assume the block was received ok */
      rio->progress ((player_generation == 4) ? x : x/2, blocks, rio->progress_ptr);

    if (player_generation > 3)
      write_block_rio (rio, fileBuffer, blocksize, NULL);
  }

  if (player_generation > 3) {
    /* if this is a newer player the update is not quite done */
    rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: writing firmware...\n");

    /* it takes a moment before the rio is ready to continue */
    usleep (1000);
  
    /* half-way mark on the progress bar */
    if (rio->progress != NULL)
      rio->progress (blocks/2, blocks, rio->progress_ptr);
    
    lseek(firm_fd, 0, SEEK_SET);

    /* write firmware */
    for (x = 0 ; x < blocks ; x++) {
      /* read in a chunk of file */
      read (firm_fd, fileBuffer, blocksize);
    
      write_block_rio (rio, fileBuffer, blocksize, NULL);
    
      /* the rio expects the first block to be sent three times */
      if (x == 0) {
	write_block_rio (rio, fileBuffer, blocksize, NULL);
	write_block_rio (rio, fileBuffer, blocksize, NULL);
      }
      
      if (rio->progress != NULL)
	rio->progress (x/2 + blocks/2, blocks, rio->progress_ptr);
    }
  }

  /* make sure the progress bar reaches 100% */
  if (rio->progress != NULL)
    rio->progress (1, 1, rio->progress_ptr);

  close(firm_fd);

  rio_log (rio, 0, "librioutil/rio.c firmware_upgrade_rio: firmware update complete\n");

  UNLOCK(URIO_SUCCESS);
}

/*
  wake_rio:

  internal function to send a common set of commands
*/
int wake_rio (rios_t *rio) {
  int ret;
  
  if (!rio || !rio->dev)
    return -EINVAL;

  if ((ret = send_command_rio(rio, 0x66, 0, 0)) != URIO_SUCCESS)
    return ret;

  send_command_rio(rio, 0x61, 0, 0);
  send_command_rio(rio, 0x65, 0, 0);
  send_command_rio(rio, 0x60, 0, 0);
  
  return URIO_SUCCESS;
}

/* frees the info ptr in rios_t structure */
void free_info_rio (rios_t *rio) {
  int i;
  flist_rio_t *tmp, *ntmp;
  
  for (i = 0 ; i < MAX_MEM_UNITS ; i++)
    for (tmp = rio->info.memory[i].files ; tmp ; tmp = ntmp) {
      ntmp = tmp->next;
      free(tmp);
    }
}

/*
  update_info_rio:

  funtion updates the info portion of the rio_instance structure
*/
int update_info_rio (rios_t *rio) {
  if (rio == NULL)
    return -EINVAL;

  free_info_rio (rio);
  
  return return_intrn_info_rio (rio);
}


/*
  return_mem_units_rio:

  returns to total number of memory units an instance has.
*/
int return_mem_units_rio (rios_t *rio) {
  if (rio == NULL)
    return -EINVAL;
  
  return rio->info.total_memory_units;
}

/*
  return_free_mem_rio:

  returns the free space on a memory unit (in kiB)
*/
int return_free_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return -EINVAL;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_free_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }

  return FREE_SPACE(memory_unit);
}

/*
  return_used_mem_rio:

  returns the used space on a memory unit (in kiB)
*/
int return_used_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return -EINVAL;

  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_used_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return (MEMORY_SIZE(memory_unit) - FREE_SPACE(memory_unit));
}

/*
  return_total_mem_rio:

  returns the size of a memory unit (in kiB)
*/
int return_total_mem_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return -EINVAL;

  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_total_mem_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return MEMORY_SIZE(memory_unit);
}

/*
  return_file_name_rio:

  returns the file name associated with a song_id/mem_id
*/
char *return_file_name_rio(rios_t *rio, u_int32_t song_id,
			   u_int8_t memory_unit) {
  flist_rio_t *tmp;
  char *ntmp;
  
  if (rio == NULL)
    return NULL;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_file_name_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return NULL;
  }
  
  /* find the file */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == song_id)
      break;
  
  if (tmp == NULL)
    return NULL;
  
  ntmp = (char *)calloc(strlen(tmp->name) + 1, 1);
  strncpy(ntmp, tmp->name, strlen(tmp->name));
  
  return ntmp;
}

int return_file_size_rio(rios_t *rio, u_int32_t song_id, u_int8_t memory_unit) {
  flist_rio_t *tmp;
  
  if (rio == NULL)
    return -1;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2,
	     "return_file_size_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  /* find the file */
  for (tmp = rio->info.memory[memory_unit].files ; tmp ; tmp = tmp->next)
    if (tmp->num == song_id)
      break;
  
  if (tmp == NULL)
    return -1;
  
  return tmp->size;
}

/*
  return_num_files_rio:

  returns the number of files on a memory unit.
*/
int return_num_files_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return -EINVAL;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2,
	     "return_num_files_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return rio->info.memory[memory_unit].num_files;
}

/*
  return_time_rio:

  returns the sum of the duration of all tracks on a memory unit
*/
int return_time_rio (rios_t *rio, u_int8_t memory_unit) {
  if (rio == NULL)
    return -EINVAL;
  
  if (memory_unit >= MAX_MEM_UNITS) {
    rio_log (rio, -2, "return_time_rio: memory unit %02x out of range.\n",
	     memory_unit);
    return -2;
  }
  
  return rio->info.memory[memory_unit].total_time;
}

/*
  Other Info -- This does not return file list
*/
rio_info_t *return_info_rio (rios_t *rio) {
  rio_info_t *new_info = NULL;

  rio_log (rio, 0, "return_info_rio: function depricated. use get_info_rio instead.\n");

  get_info_rio (rio, &new_info);
  
  return new_info;
}

int get_info_rio (rios_t *rio, rio_info_t **info) {
  int i;

  if (rio == NULL || info == NULL)
    return -EINVAL;
  
  if (rio->info.memory[0].size == 0)
    return_intrn_info_rio (rio);
  
  *info = calloc(1, sizeof (rio_info_t));
  
  /* make a duplicate of rio's info */
  memcpy(*info, &rio->info, sizeof(rio_info_t));

  for (i = 0 ; i < 2 ; i++)
    (*info)->memory[i].files = NULL;
  
  return 0;  
}

/*
  set_progress_rio:
*/
void set_progress_rio (rios_t *rio, void (*f)(int x, int X, void *ptr), void *ptr) {
  if (rio != NULL) {
    rio->progress_ptr = ptr;
    rio->progress = f;
  }
}

/*
  return_conn_method_rio: return the driver librioutil is using (soon to be deprecated)
*/
char *return_conn_method_rio (void) {
  return driver_method;
}

int return_serial_number_rio (rios_t *rio, u_int8_t serial_number[16]) {
  if (rio == NULL)
    return -EINVAL;

  memmove (serial_number, rio->info.serial_number, 16);

  return 0;
}

/* locking/unlocking routines */
int try_lock_rio (rios_t *rio) {
  if (rio == NULL)
    return -EINVAL;

  if (rio->lock != 0) {
    rio_log (rio, -EBUSY, "librioutil/rio.c try_lock_rio: rio is being used by another thread.\n");

    return -EBUSY;
  }

  rio->lock = 1;

  return 0;
}

void unlock_rio (rios_t *rio) {
  rio->lock = 0;
}
