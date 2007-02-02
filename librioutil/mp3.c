/**
 *   (c) 2001-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 mp3.c 
 *
 *   MPEG file parser
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

#include <string.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "rioi.h"

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif


#define MP3_DEBUG 0

void mp3_debug (char *format, ...) {
#if MP3_DEBUG==1
  va_list arg;
  va_start (arg, format);
  vfprintf (stderr, format, arg);
  va_end (arg);
#endif
}

struct mp3_file {
  FILE *fh;

  int file_size;  /* Bytes */
  int tagv2_size; /* Bytes */
  int skippage;   /* Bytes */
  int data_size;  /* Bytes */

  int vbr;
  int bitrate;    /* bps */

  int initial_header;

  int frames;
  int xdata_size;

  int layer;
  int version;

  int samplerate; /* Hz */

  int length;     /* ms */
  int mtime, ctime;
};

/* [version][layer][bitrate] */
int bitrate_table[4][4][16] = {
  /* v2.5 */
  {{-1, -1, -1, -1, -1. -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160, -1},
   {-1,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160, -1},
   {-1, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, -1}},

  /* NOTUSED */
  {{-1, -1, -1, -1, -1. -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1, -1, -1, -1, -1. -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1, -1, -1, -1, -1. -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1, -1, -1, -1, -1. -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1}},

  /* v2 */
  {{-1, -1, -1, -1, -1. -1, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160, -1},
   {-1,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160, -1},
   {-1, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, -1}},

  /* v1 */
  {{-1, -1, -1, -1,  -1.  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1},
   {-1, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1},
   {-1, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1},
   {-1, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1}},

};

int samplerate_table[4][4] = {
  {11025, 12000,  8000, -1},
  {   -1,    -1,    -1, -1},
  {22050, 24000, 16000, -1},
  {44100, 48000, 32000, -1}
};

int xing_offsets[4][4] = {
  {32, 32, 32, 17},
  {-1, -1, -1, -1},
  {17, 17, 17,  9},
  {17, 17, 17,  9}
};

double version_table[] = {
  2.5, -1.0, 2.0, 1.0
};

size_t layer_table[] = {
  -1, 3, 2, 1
};

#define MP3_PROTECTION_BIT 0x00010000
#define MP3_PADDING_BIT    0x00000200
#define MPEG_SYNC          0xffe00000

#define MPEG_VERSION(header) ((header & 0x00180000) >> 19)
#define MPEG_LAYER(header) ((header & 0x00060000) >> 17)
#define MPEG_BITRATEI(header) ((header & 0x0000f000) >> 12)
#define MPEG_SAMPLERATEI(header) ((header & 0x00000c00) >> 10)
#define MPEG_PADDING(header) ((header & 0x00000200) >> 9)

/* 
   0 = stereo
   1 = joint stereo
   2 = dual channel
   3 = mono
*/
#define MPEG_CHANNELS(header) ((header & 0x000000c0) >> 7)

#define BITRATE(header) bitrate_table[MPEG_VERSION(header)][MPEG_LAYER(header)][MPEG_BITRATEI(header)]
#define SAMPLERATE(header) samplerate_table[MPEG_VERSION(header)][MPEG_SAMPLERATEI(header)]
#define PADDING(header) ((MPEG_LAYER(header) == 3) ? 4 : 1)

static size_t mpeg_frame_length (int header) {
  double bitrate = (double)BITRATE(header) * 1000.0;
  double samplerate = (double)SAMPLERATE(header);
  double padding = (double)MPEG_PADDING(header);
  char layer = MPEG_LAYER(header);
  size_t frame_length;

  if (layer == 0x11)
    frame_length = 144.0 * bitrate/samplerate + padding;
  else
    frame_length = (12 * bitrate/samplerate + padding) * 4.0;

  return (size_t)((layer != 0x11) ? (144.0 * bitrate/samplerate + padding)
	  : (12 * bitrate/samplerate + padding) * 4.0);
}

/* check_mp3_header: returns 0 on success */
static int check_mp3_header (int header) {
  if (((header & MPEG_SYNC) == MPEG_SYNC) && (MPEG_VERSION(header) > 0.0) && (BITRATE(header) > 0)
      && (SAMPLERATE(header) > 0))
    return 0;
  else if (header == 0x4d4c4c54) /* MLLT */
    return 2;
  else
    return 1;
}

static int find_first_frame (struct mp3_file *mp3) {
  int header, buffer, ret, xing_offset;

  mp3->skippage = 0;

  while (fread (&header, 4, 1, mp3->fh)) {
    header = big32_2_arch32(header);

    /* MPEG-1 Layer III */
    if ((ret = check_mp3_header (header)) == 0) {
      /* check for an Xing header in this frame */

      xing_offset = xing_offsets[MPEG_VERSION(header)][MPEG_CHANNELS(header)];

      fseek (mp3->fh, xing_offset, SEEK_CUR);
      fread (&buffer, 4, 1, mp3->fh);

      buffer = big32_2_arch32(buffer);

      if (buffer == ('X' << 24 | 'i' << 16 | 'n' << 8 | 'g') ||
	  buffer == ('I' << 24 | 'n' << 16 | 'f' << 8 | 'o') ){
	int xstart = ftell (mp3->fh);
	int xflags;

	/* an mp3 with an Xing header is ALWAYS vbr */
	mp3->vbr = 1;

	fread (&xflags, 4, 1, mp3->fh);

	mp3_debug ("Xing flags = %08x\n", xflags);

	if (xflags & 0x00000001) {
	  fread (&buffer, 4, 1, mp3->fh);
	  
	  /* the frame field does not include the Xing header
	     which is a valid frame */
	  mp3->frames = buffer + 1;

	  mp3_debug ("MPEG file has %i frames\n", mp3->frames);
	}

	if (xflags & 0x00000002) {
	  fread (&buffer, 4, 1, mp3->fh);
	  mp3->xdata_size = buffer;

	  mp3_debug ("MPEG file has %i bytes of data\n", mp3->xdata_size);
	}

	fseek (mp3->fh, xstart, SEEK_SET);
      }

      mp3->initial_header = header;

      mp3->samplerate = SAMPLERATE(header);

      mp3_debug ("Inital bitrate = %i\n", BITRATE(header));

      fseek (mp3->fh, -(xing_offset + 8), SEEK_CUR);
      return 0;
    } else if (ret == 2)
      return -2;
    
    fseek (mp3->fh, -3, SEEK_CUR);
    mp3->skippage++;
  }

  return -1;
}


static int mp3_open (char *file_name, struct mp3_file *mp3) {
  struct stat statinfo;

  char buffer[14];
  int has_v1 = 0;

  mp3_debug ("mp3_open: Entering...\n");

  memset (mp3, 0 , sizeof (struct mp3_file));

  if (stat (file_name, &statinfo) < 0)
    return -errno;

  mp3->file_size = mp3->data_size = statinfo.st_size;
  mp3->mtime  = statinfo.st_mtime;
  mp3->ctime  = statinfo.st_ctime;

  mp3->fh = fopen (file_name, "r");
  if (mp3->fh == NULL) 
    return -errno;

  /* Adjust total_size if an id3v1 tag exists */
  fseek (mp3->fh, -128, SEEK_END);
  memset (buffer, 0, 5);

  fread (buffer, 1, 3, mp3->fh);
  if (strncmp (buffer, "TAG", 3) == 0) {
    mp3->data_size -= 128;

    has_v1 = 1;

    mp3_debug ("mp3_open: Found id3v1 tag.\n");
  }
  /*                                          */

  /* Check for Lyrics v2.00 */
  fseek (mp3->fh, -9 - (has_v1 ? 128 : 0), SEEK_END);
  memset (buffer, 0, 10);
  fread (buffer, 1, 9, mp3->fh);

  if (strncmp (buffer, "LYRICS200", 9) == 0) {
    int lyrics_size;
    mp3_debug ("mp3_open: Found Lyrics v2.00\n");

    /* Get the size of the Lyrics */
    fseek (mp3->fh, -15, SEEK_CUR);
    memset (buffer, 0, 7);
    fread (buffer, 1, 6, mp3->fh);

    /* Include the size if LYRICS200 (9) and the size field (6) */
    lyrics_size = strtol (buffer, NULL, 10) + 15;
    mp3->data_size -= lyrics_size;

    mp3_debug ("mp3_open: Lyrics are 0x%x Bytes in length.\n", lyrics_size);
  }

  /* find and skip id3v2 tag if it exists */
  fseek (mp3->fh, 0, SEEK_SET);
  fread (buffer, 1, 14, mp3->fh);    
  mp3->tagv2_size = id3v2_size (buffer);

  fseek (mp3->fh, mp3->tagv2_size, SEEK_SET);

  mp3_debug ("mp3_open: id3v2 size: 0x%08x\n", mp3->tagv2_size);
  /****************************************/

  mp3->vbr = 0;

  mp3_debug ("mp3_open: Complete\n");

  return find_first_frame (mp3);
}

#define FRAME_COUNT 30

static int mp3_scan (struct mp3_file *mp3) {
  int header;
  int ret;
  int frames = 0;
  int last_bitrate = -1;
  int total_framesize = 0;

  size_t bitrate;
  int frame_size;

  mp3_debug ("mp3_scan: Entering...\n");

  if (mp3->frames == 0 || mp3->xdata_size == 0) {
    while (ftell (mp3->fh) < mp3->data_size && (frames < FRAME_COUNT || mp3->vbr)) {
      fread (&header, 4, 1, mp3->fh);

      header = big32_2_arch32 (header);
      
      if (check_mp3_header (header) != 0) {
	fseek (mp3->fh, -4, SEEK_CUR);

	mp3_debug ("mp3_scan: Invalid header %08x %08x Bytes into the file.\n", header, ftell(mp3->fh));
	
	if ((ret = find_first_frame (mp3)) == -1) {
	  mp3_debug ("mp3_scan: An error occured at line: %i\n", __LINE__);
	  
	  /* This is hack-ish, but there might be junk at the end of the file. */
	  
	  break;
	} else if (ret == -2) {
	  mp3_debug ("mp3_scan: Ran into MLLT frame.\n");
	  
	  mp3->data_size -= (mp3->file_size) - ftell (mp3->fh);
	  
	  break;
	}
	
	continue;
      }
      
      bitrate = BITRATE(header);
      
      if (!mp3->vbr && (last_bitrate != -1) && (bitrate != last_bitrate))
	mp3->vbr = 1;
      else
	last_bitrate = bitrate;
      
      frame_size = mpeg_frame_length (header);
      total_framesize += frame_size;
      fseek (mp3->fh, frame_size - 4, SEEK_CUR);      
      frames++;
    }

    if (frames == FRAME_COUNT) {
      frames = (int)((double)((mp3->data_size - mp3->tagv2_size) * FRAME_COUNT) / (double)total_framesize);
      total_framesize = mp3->data_size - mp3->tagv2_size;
    }

    if (mp3->frames == 0)
      mp3->frames = frames;

    if (mp3->xdata_size == 0)
      mp3->xdata_size = total_framesize;
  }

  mp3->length     = (int)((double)mp3->frames * 26.12245); /* each mpeg frame represents 26.12245ms */
  mp3->bitrate    = (int)(((float)mp3->xdata_size * 8.0)/(float)mp3->length);

  mp3_debug ("mp3_scan: Finished scan. SampleRate: %i, BitRate: %i, Length: %i, Frames: %i.\n",
	     mp3->samplerate, mp3->bitrate, mp3->length, mp3->frames);

  if (mp3->samplerate <= 0 || mp3->bitrate <= 0 || mp3->length <= 0)
    return -1;

  return 0;
}

static void mp3_close (struct mp3_file *mp3) {
  fclose (mp3->fh);
}

static int get_mp3_info (char *file_name, rio_file_t *mp3_file) {
  struct mp3_file mp3;

  if (mp3_open (file_name, &mp3) < 0)
    return -1;

  mp3_scan (&mp3);
  mp3_close (&mp3);

  mp3_file->bit_rate    = mp3.bitrate << 7;
  mp3_file->sample_rate = mp3.samplerate;
  mp3_file->time        = mp3.length/1000;
  mp3_file->size        = mp3.file_size;

  return mp3.skippage;
}


/*
  mp3_info:
    Function takes in a file name (MP3) and returns a
  Info structure containing the amount of junk (in bytes)
  and a compete Rio header struct.
*/
int mp3_info (info_page_t *newInfo, char *file_name){
  rio_file_t *mp3_file = newInfo->data;

  int id3_version;
  int mp3_header_offset;

  if ((mp3_header_offset = get_mp3_info(file_name, mp3_file)) < 0) {
    free(mp3_file);
    newInfo->data = NULL;
    return -1;
  }

  if ((id3_version = get_id3_info(file_name, mp3_file)) < 0) {
    free(mp3_file);
    newInfo->data = NULL;
    return -1;
  }
  
  /* the file that will be uploaded is smaller if there is junk */
  if (mp3_header_offset > 0 && !(id3_version >= 2)) {
      mp3_file->size -= mp3_header_offset;
      newInfo->skip = mp3_header_offset;
  } else
    /* dont want to not copy the id3v2 tags */
    newInfo->skip = 0;

  /* it is an mp3 all right, finish up the INFO structure */
  mp3_file->bits     = 0x10000b11;
  mp3_file->type     = TYPE_MP3;
  mp3_file->foo4     = 0x00020000;

  return URIO_SUCCESS;
}
