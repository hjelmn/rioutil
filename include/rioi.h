/**
 *   (c) 2001-2012 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.3 rioi.h
 *
 *   header file for librioutil internal functions
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

/* KELLY: 08-21-03
 * The only thing in this file that seems to have changed from 1.3.3(riot) to 1.3.4
 * is the _rio_file structure.  Initial stab at getting the structure correct for the
 * Rio RIOT.  The changes have _not_ been checked to see if other players are effected.
 */

/* NOACK: 09-05-04
 * The riot needs an extra field in the _rio_file structure to sort the tracks
 * inside an album.
 * The riot specific part of the _rio_file structure seems to be identical to
 * the hd_file_t structure, so it's save to assume, that the trackno is stored
 * at the same offset as in hd_file_t, where it can be fount at 0xf9.
 */

#if !defined (_RIO_INTERNAL_H)
#define _RIO_INTERNAL_H

#include "rio.h"

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

/***

  Commands

***/
/*
command:                hex:   arg1:           arg2:              read (B):     write (B):
Device Description      0x62   N/A             N/A                256           0
Memory Description      0x68   Unit            N/A                256           0
File Description        0x69   File            Memory Unit        2048          0
Format Memory           0x6a   Unit            N/A                64            0
Update Fimware/Rom      0x6b   0x1             (Fim: 0, Rom: 1)   x             x
  seq:
   Read  64B
   Write 64B   (Firmware size in first 4)
   Read  64B

   Repeat: (until file is finished writing)
   Write 8192B (Maybe larger?, Data from firmware file, no chksum)
   Read  64B

   Delay

   Repeat: (three times w/ first 8192B of data and then until done w/ remaining data)
   Write 8192B 
   Read  64B

Upload File             0x6c   Unit            N/A                x             x
  seq:
   Read 64B

   Repeat: (until file is completly read/sent)
   Write 64B   (CRIODATA plus crc32 chksum of data)
   Write 16384B
   Read  64B

   Write 64B   (CRIOINFO plus crc32 chksum of header)
   Write 2048B (file header)

Download File           0x70   Unit            N/A                x            x
  seq:
   Read  64B
   Write 2048B (file header of file being downloaded)
   Read  64B   (if RIONOFL do not continue)

   Write 64B (CRIODATA)
   Read  64B

   Repeat: (until file is downloaded)
   Read  4096B (file data)
   Write 64B   (CRIODATA plus chksum)

   Keep reading past data until read bytes is a multiple of 16384.

Delete File             0x78   Unit            N/A                x             x
  seq:
   Read  64B
   Write 2048B (file header to file being deleted)
   Read  64B

Write Device Prefs      0x79   N/A             N/A                x             x
  seq:
   Read  64B
   Write 2048B (same format as is returned by Read Device Prefs)
   Read  64B

Read Device Prefs       0x7a   N/A             N/A                2048          0
Set Device Time         0x7b   Last 2B of time First 2B of time   0             0
Group ID                0x7c   ????            ????               64            ?
*/

#define UNKNOWN00  0x60
#define RIO_POLLD  0x61 /* not sure on this one */
#define RIO_DESCP  0x62 /* description of the device */
#define RIO_TYPEQ  0x63
#define NOCOMMAN1  0x64 /* this is not a command */
#define UNKNOWN01  0x65
#define UNKNOWN02  0x66
#define UNKNOWN03  0x67
#define RIO_MEMRI  0x68 /* what storage do we have */
#define RIO_FILEI  0x69 /* loop through files */
#define RIO_FORMT  0x6a /* this returns SRIOFMTD....Done if successful */
#define RIO_UPDAT  0x6b /* update firmware, and rom (don't know how to work with it yet) */
#define RIO_WRITE  0x6c
#define UNKNOWN04  0x6d
#define UNKNOWN05  0x6e
#define UNKNOWN06  0x6f
#define RIO_READF  0x70 /* doesnt work on S-Series */
/* 71 -- 76 seem to do nothing */
#define UNKNOWN07  0x77 /* clears the screen on the rio ??? */
#define RIO_DELET  0x78
#define RIO_PREFS  0x79 /* write prefs.   2kB */
#define RIO_PREFR  0x7a /* returns prefs. 2kB */
#define RIO_TIMES  0x7b /* set time */
#define RIO_GIDS   0x7c
#define UNKNOWN09  0x7d
#define UNKNOWN10  0x7e
#define UNKNOWN11  0x7f
#define UNKNOWN12  0x80 /* on S-Series, after command, can read 128B off the Rio */
#define RIO_RIOTF  0x82 /* on Riot, Rio sends playlist */
#define RIO_CHGIN  0x85 /* on S-Series and newer, change info about a file */
#define RIO_NINFO  0x87 /* on nitrus, send nitrus song info */
#define RIO_OVWRT  0x88 /* on S-Series and newer, replace a file */

/***

  Other defines

***/
#define RIO_MTS   0x00000800
#define RIO_FTS   0x00004000

/*
  file types
*/
#define TYPE_MP3  0x4d504733
#define TYPE_WMA  0x574d4120
#define TYPE_WAV  0x41434c50
#define TYPE_WAVE 0x57415645
#define TYPE_PLS  0x504c5320

/* macros to get memory info in kilobytes */
#define FREE_SPACE(x) ((return_type_rio(rio) == RIORIOT) ? rio->info.memory[x].free : \
		       rio->info.memory[x].free / 1024)

#define MEMORY_SIZE(x) ((return_type_rio(rio) == RIORIOT) ? rio->info.memory[memory_unit].size : \
                        rio->info.memory[x].size / 1024)


#define UNLOCK(ret) do { unlock_rio (rio); return ret; } while (0);

/* byte-sex */
#if defined (linux) || defined(__GLIBC__)

#include <endian.h>
#include <byteswap.h>

#elif defined (__APPLE__)

#include <architecture/byte_order.h>

#define bswap_64(x) OSSwapInt64(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_16(x) OSSwapInt16(x)

#elif defined (__NetBSD__)

#define bswap_64(x) bswap64(x)
#define bswap_32(x) bswap32(x)
#define bswap_32(x) bswap16(x)

#elif defined (__FreeBSD__)

#include <machine/endian.h>

#define bswap_32(x) ((x >> 24) | (x & 0x00ff0000) >> 8 | (x & 0x0000ff00) << 8  | (x & 0x000000ff) << 24)
#define bswap_16(x) ((x >> 8) | (x & 0x00ff) << 8)

#else

#include <sys/endian.h>
#include <sys/bswap.h>

#endif

#if BYTE_ORDER==BIG_ENDIAN
#define arch32_2_little32(x) bswap_32(x)
#define little32_2_arch32(x) bswap_32(x)
#define little16_2_arch16(x) bswap_16(x)
#define big32_2_arch32(x) x
#else
#define arch32_2_little32(x) x
#define little32_2_arch32(x) x
#define little16_2_arch16(x) x
#define big32_2_arch32(x) bswap_32(x);
#endif


/***

  Internal Structures

***/
typedef struct _rio_mem {
  u_int32_t foo[4];
  u_int32_t size;
  u_int32_t used;
  u_int32_t free;
  u_int32_t system;
  
  u_int32_t foobar[8];
  
  char     name[64];
  u_int8_t unk0[32];
  u_int8_t unk1[32];
  u_int8_t model[64]; /* Riot: Hard drive model acked at the end with 0x20 */

  u_int8_t unk[16];
} rio_mem_t;

typedef struct _rio_desc {
  /* 0x00 - 0x0f */
  u_int32_t unk0;
  u_int32_t fw_version;
  u_int32_t unk1;
  u_int32_t unk2;

  /* 0x10 - 0x3f */
  u_int8_t  unk3[48];

  /* 0x40 - 0x5f */
  char      name[32];

  /* 0x60 - 0x6f */
  u_int8_t  serial_number[16];

  /* 0x70 - 0x7f */
  u_int8_t  unk4[16];

  /* 0x80 - 0x9f */
  u_int8_t  player_model[32];

  /* 0xa0 - 0xbf */
  u_int8_t  hd_model[32];

  /* 0xc0 - 0xdf */
  u_int8_t  vendor[32];

  /* 0xe0 - 0xff */
  u_int8_t  unk5[32];
} rio_desc_t;

/*
 * structure for a info header on the rio
 *
 * Kelly: 08-21-03 Modified the structure.  The
 * original code had 1600 bytes undefined after
 * the album.  For the RIOT, at a minimum we need
 * to set the 'size2' part.
 * Starting at: 
 */
typedef struct _rio_file {
  /* 0x0000 - 0x000f */
  u_int32_t file_no; /* set by rio ??? */
  /* 
     This appears to be a pointer to the start of the file.
     If it is left as 0x00000000 then the Rio seems to set it,
     but a value *can* be passed in with varying results.
  */
  u_int32_t start;


  u_int32_t size;
  u_int32_t time;

  
  /* 0x0010 - 0x001f */
  /* XXX time_t different sizes on different systems (64 bit on NetBSD/alpha) */
  u_int32_t mod_date; /* given in secs since epoch */

  /*
    So far I have found that this value contains this:
    0x00000001 0x00000010 0x00000100 must be set
    0x00000002 : has bookmark?
    0x00400000 : Name does not display
    0x00000080 : Downloadable
  */
  u_int32_t bits;

  /*
    This value can be set (currently) with the following values
    0x3347504d : MPG3 (MPEG Layer III)
    0x20414d57 : WMA  (Winbloze Media)
    0x504c4341 : ACLP (WAVE)
    0x45564157 : WAVE (Microshaft)
  */
  u_int32_t type; 

  u_int32_t foo3;

  
  /* 0x0020 - 0x002f */
  u_int32_t foo4; /* maybe channels? */
  u_int32_t sample_rate;
  u_int32_t bit_rate; /* it is multiplied by 128 so << 7 gives value :) */
  u_int32_t foo5;
  
  /* 0x0030 - 0x005f -- usually zeros */
  u_int8_t foobar[48];
  
  /* 0x0060 - 0x006f */
  u_int8_t info0[16];

  /* 0x0070 - 0x007f */
  u_int8_t  unk[8];
  u_int8_t  unk1[4]; /* Associated with S-Series playlists */
  u_int8_t  unk2[4];

  /* 0x0080 - 0x00b9 */
  u_int8_t info1[64];
  
  /* 0x00c0 - 0x00ff */
  char     name[64];
  
  /* 0x0100 - 0x013f */
  char     title[64];
  
  /* 0x0140 - 0x017f */
  char     artist[64];
  
  /* 0x0180 - 0x01bf */
  char     album[64];
    
  /* 0x01c0 - 0x0203 
  * Empty in the RIOT
  */
  u_int8_t temp1[68];
  
  /* 0x204 - 0x0207
   * Start of matching the short entry list 0x200-0x2ff
   * The RIOT needs to have this set.  Not sure what the
   * effect will be on the other types of players.  If it
   * is negative then we need to create a new structure
   * structure for the RIOT.
   */
  u_int32_t riot_file_no;
  
  /* 0x0208 - 0x020b 
   * space holder to push the file_number to the proper place. 
   * filled with 00's as far as I can tell
   */
  u_int8_t temp2[4];
  
  /* 0x020c - 0x020f 
   * This seems to always be set to:
   * 30 31 20 2d - I'm not sure if this is the same on all
   * RIOT's.  It's in every entry on mine.
   */
  u_int32_t file_prefix;
  
  /* 0x0210
   * Demarc for filename start always 0x20 
   */
  u_int8_t demarc;
  
  /* 0x0211 - 0x022b 
   * File name? - Seems to do odd things to the file name.
   * may effect the actual downloading of items from the RIOT
   */
  u_int8_t name2[27];
  
  /* 0x022c - 0x025b 
   * Used in the RIOT for the list of Titles
   */
  u_int8_t title2[48];
  
  /* 0x025c - 0x028b 
   * Uesd in the RIOT for selecting artists
   */
  u_int8_t artist2[48];
  
  /* 0x28c - 0x02bb 
   * Used in the RIOT for selecting Albums 
   */ 
  u_int8_t album2[48];
  
  /* 0x02bc - 0x02cb 
   * This area is used in the RIOT for the genre select. 
   */
  u_int8_t genre2[22];

  /* 0x02cc - 0x02d1 
   * 'zeros' may be part of the 'genre' field 
   */
  /*  u_int8_t temp3[6]; */

  /* 0x02d2 - 0x02d5 
   * Year song was produced.  This field is used by the RIOT for
   * one of the 'play' options called 'sounds of...' will play
   * songs from the 40's 50's 60's, etc.
   */
  u_int8_t year2[4];

  /* 0x02d6 - 0x02eb 
   * Not sure what goes here.  there is a single value at: 0x02d8 
   */
  u_int8_t temp4[22];
  
  /* 0x02ec - 0x02ef 
   * This is the time in seconds for the song 
   */
  u_int32_t time2;
  
  /* 0x02f0 - 0x02f3 
   * Seems to be empty 
   */
  u_int32_t temp5;
  
  /* 0x02f4 - 0x2f7 
   * This must be written for the
   * RIOT to be able to at least 'see' the song.
   */
  u_int32_t size2;
  
  /* 0x02f8 - 0x02f9 
   * This seems to be the trackno, that is used to sort the files of an
   * album.
   * Seemingly, the number is saved in 0x2f9, but if an album has more than
   * 255 tracks, there might be need for more.
   */
  u_int8_t unk8;
  u_int8_t trackno2;

  /* 0x02f8 - 0x07ff 
   * remaining space in the structure 
   */
  u_int8_t temp6[1286];  
} rio_file_t;

typedef struct _hd_file {
  /*0x0000 to 0x000f */
  u_int32_t unk0; /* 01 indicates a listing is present */
  /* 0x0004 to 0x0007 */
  u_int32_t unk1;
  /* 0x0008 to 0x000B */
  u_int32_t ident;
  /* 0x000C to 0x000F */
  u_int32_t unk3;
  /* 0x0010 */
  u_int8_t  unk4;  /* Always 20 when there is a song entry */
  /* 0x0011 - 0x002B */
  u_int8_t  file_name[27];
  /* 0x002C - 0x005B */
  u_int8_t  title[48];
  /* 0x005C - 0x008B */
  u_int8_t  artist[48];
  /* 0x008C - 0x00BB */
  u_int8_t  album[48];
  /* 0x00BC - 0x00EB */
  u_int8_t  type[48];  /* not sure of if this is really 48 */
  /* 0x00EC - 0x00EF */
  u_int32_t time;
  /* 0x00F0 - 0x00F3 */
  u_int32_t unk6;
  /* 0x00F4 - 0x00F7 */
  u_int32_t size;
  /* 0x00F8 - 0x00F9 */
  u_int8_t unk8;
  u_int8_t trackno;
  /* 0x00FA - 0x00FB */
  u_int16_t unk9;
  /* 0x00FC - 0x00FF */
  u_int32_t unk10;  
} hd_file_t;

typedef struct _rio_prefs {
  /*
    0x00 - 0x0f Mostly known
  */
  u_int8_t unk0[4];
  u_int8_t eq_state;
  u_int8_t treble;
  u_int8_t bass;
  u_int8_t repeat_state;
  u_int8_t sleep_time;
  u_int8_t light_state;
  u_int8_t contrast;
  u_int8_t volume;
  u_int8_t unk1;
  u_int8_t unk2;
  u_int8_t unk3;
  u_int8_t playlist;

  u_int8_t unk4[48];

  char     name[16];

  /*
    i have only seen this set once
  */
  u_int8_t type[16];

  /*
    usually just 0's
  */
  u_int8_t unk5[1952];
} rio_prefs_t;

typedef struct _info_page {
    rio_file_t *data;

    int skip;
} info_page_t;

/*
 * RIOT Preferences Structure
 */
typedef struct _riot_rio_prefs {
  u_int8_t	ferp[4];	/* shows FERP at the beginning */
  u_int8_t	unk0;
  u_int8_t	unk1;
  u_int8_t	unk2;
  u_int8_t	unk3;
  u_int8_t	repeat_state;	/* 0,1 or 2 */
  u_int8_t	random_state;	/* 0 or 1 */
  u_int8_t	contrast;	/* 9 to 0 */
  u_int8_t	light_state;	/* 0,1,2,3,4,5 off, 1 sec,
				   2 sec, 5 sec, 10 sec, on */
  u_int8_t	sleep_time;	/* 0,1,2,5,15 mins */
  u_int8_t	unk4;
  u_int8_t	treble;		/* 0 to 12 default 6 */
  u_int8_t	bass;		/* 0 to 12 default 6 */
  u_int8_t	volume;		/* 0 to 20 */
  u_int8_t	unk5;
  u_int8_t	unk6;
  u_int8_t	unk7;
  u_int8_t	unk8;
  u_int8_t	unk9;
  u_int8_t	unk10;
  u_int8_t	unk11;
  u_int8_t	the_filter_state;	/* 0 or 1 */
  
  u_int8_t	unk12[39];
  
  char  	name[16];
  
  u_int8_t	type[16];
  
  u_int8_t	unk13[1952];
} riot_prefs_t;

/*                                                                                                                                                     Playlist format used on older players (.lst file format):

  Notes:
   - The first entry is always blank.
   - The name is the same as the rio_file->name of the song.
   - The title is the first 32 characters of rio_file->title.
*/
typedef struct _rio_lst {
  struct song_list {
    u_int8_t  name[64];
    u_int32_t unk0[3];
    u_int32_t entry;
    u_int32_t unk1[4];
    u_int8_t  title[32];
  } playlist[128];
} rio_plst_t;


/***

  Internal Functions

***/

int  wake_rio     (rios_t *rio);
int  try_lock_rio (rios_t *rio);
void unlock_rio   (rios_t *rio);

/* rio.c : used to build a rios_t */
int get_file_info_rio (rios_t *rio, rio_file_t *file, u_int8_t memory_unit, u_int16_t file_no);
int get_memory_info_rio (rios_t *rio, rio_mem_t *memory, u_int8_t memory_unit);
int generate_mem_list_rio (rios_t *rio);

int return_generation_rio (rios_t *rio);
int return_type_rio(rios_t *rio);
void update_free_intrn_rio (rios_t *rio, u_int8_t memory_unit);
float return_version_rio (rios_t *rio);

/* rioio.c */
int read_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, u_int32_t block_size);
int write_cksum_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr);
int write_block_rio (rios_t *rio, unsigned char *ptr, u_int32_t size, char *cksum_hdr);
int abort_transfer_rio (rios_t *rio);
int send_command_rio (rios_t *rio, int request, int value, int index);

/* id3.c */
int get_id3_info (char *file_name, rio_file_t *mp3_file);
int id3v2_size (unsigned char data[14]);

/* mp3.c, downloadable.c */
int mp3_info (info_page_t *newInfo, char *file_name);
int downloadable_info (info_page_t *newInfo, char *file_name);

/* file_list.c */
int generate_flist_riomc (rios_t *rio, u_int8_t memory_unit);
int generate_flist_riohd (rios_t *rio);
int flist_add_rio (rios_t *rio, int memory_unit, info_page_t info);
int flist_remove_rio (rios_t *rio, uint memory_unit, uint file_no);
int flist_get_file_name_rio (rios_t *rio, uint memory_unit, uint file_no, char *file_namep, int file_name_len);
int flist_get_file_id_rio (rios_t *rio, uint memory_unit, uint file_no);
int size_flist_rio (rios_t *rio, int memory_unit);
int flist_first_free_rio (rios_t *rio, int memory_unit);
flist_rio_t *get_flist_rio (rios_t *rio, uint memory_unit, uint file_no);

/* song_management.c */
int do_upload (rios_t *rio, u_int8_t memory_unit, int addpipe, info_page_t info, int overwrite);
int update_db_rio (rios_t *rio);

/* cksum.c */
u_int32_t crc32_rio (u_int8_t *, size_t);

/* playlist.c */
int new_playlist_info (info_page_t *newInfo, char *name);
int playlist_info (info_page_t *newInfo, char *file_name);


#ifndef HAVE_BASENAME
char *basename(char *x);
#endif

void file_to_arch (rio_file_t *);
void mem_to_arch  (rio_mem_t *);

#endif /* _RIO_INTERNAL_H */
