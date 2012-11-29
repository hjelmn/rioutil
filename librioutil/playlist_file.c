/**
 *   (c) 2008 Jamie Faris <farisj@gmail.com>
 *
 *   playlist_file.c
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

#include "playlist_file.h"
#include "riolog.h"
#include "rioi.h" /* for arch32_2_little32 */

#include <errno.h>
#include <stdio.h>
#include <string.h> /* strerror() */

/*
  Playlist file format:

  header is 12 bytes
  0-3:  FIDL
  4-7:  ST10
  8:    0
  9-11: number of songs (Little-Endian 3 byte int)

  each file entry is 6 bytes
  0-2:  rio_num of song (Little-Endian)
  3-5:  sflags
*/

struct rio_playlist_file_header
{
    char unk[9];
    uint nsongs:24;
};

struct rio_playlist_file_entry
{
    /* We have to declare each byte separately to ensure proper alignment */
    u_int8_t rio_num_0; /* lsb */
    u_int8_t rio_num_1;
    u_int8_t rio_num_2; /* msb */
    u_int8_t sflags_0;
    u_int8_t sflags_1;
    u_int8_t sflags_2;
};

static struct rio_playlist_file_header rio_playlist_file_header_create( const uint nsongs )
{
    struct rio_playlist_file_header h;

    h.unk[0] = 'F';
    h.unk[1] = 'I';
    h.unk[2] = 'D';
    h.unk[3] = 'L';
    h.unk[4] = 'S';
    h.unk[5] = 'T';
    h.unk[6] = 1;
    h.unk[7] = 0;
    h.unk[8] = 0;

    h.nsongs = arch32_2_little32(nsongs);

    return h;
}

static struct rio_playlist_file_entry rio_playlist_file_entry_create( const uint rio_num, const u_int8_t sflags[] )
{
   struct rio_playlist_file_entry e;

   e.rio_num_0 = rio_num;
   e.rio_num_1 = rio_num >> 8;
   e.rio_num_2 = rio_num >> 16;

   e.sflags_0 = sflags[0];
   e.sflags_1 = sflags[1];
   e.sflags_2 = sflags[2];

   return e;
}

static uint rio_num( struct rio_playlist_file_entry entry )
{
    return entry.rio_num_2 << 16 | entry.rio_num_1 << 8 | entry.rio_num_0;
}


int read_playlist_file ( const char *filename, uint **songs, uint *nsongs )
{
    FILE *file;
    struct rio_playlist_file_header header;
    struct rio_playlist_file_entry entry;
    int read;
    uint i;

    debug("read_playlist_file(filename=%s,songs=%x,nsongs=%x)", \
          filename, songs, nsongs);

    if (!filename || !songs || !nsongs)
	return -EINVAL;

    file = fopen( filename, "ro" );
    if (file == 0)
    {
        error("read_playlist_file: Failed to open file \"%s\": %s", \
              filename, strerror(errno));
        return -errno;
    }

    read = fread(&header, sizeof(header), 1, file);
    if (read != 1)
    {
        int err = errno; /* fclose resets errno */
        error("read_playlist_file: Couldn't read playlist file: %d %s", \
              read, strerror(err));
        fclose(file);
        return -err;
    }

    *nsongs = header.nsongs;
    debug("read_playlist_file: nsongs=%d", *nsongs);

    *songs = malloc(sizeof(uint) * *nsongs);
    if (!*songs)
    {
	fclose(file);
        error("malloc() failed!");
	return -ENOMEM;
    }

    i = 0;
    while ( fread(&entry, sizeof(entry), 1, file) && i < *nsongs )
    {
        (*songs)[i] = rio_num( entry );
        debug("Read playlist entry: rio_num=%d", (*songs)[i]);
        i++;
    }

    if (ferror(file))
    {
        int err = errno;
        error("read_playlist_file: Error reading playlist file: %s", \
              strerror(err));
        fclose(file);
        return -err;
    }


    if (i < *nsongs)
    {
        warning("read_playlist_file: playlist file shorter than expected");
    }


    fclose( file );

    debug("read_playlist_file(): Success");

    return URIO_SUCCESS;
}

/* songs: array of rio_num
 * sflags: array of u_int8_t[3]
 */
int write_playlist_file( char *filename, const uint songs[], u_int8_t * const *sflags, uint nsongs)
{
    struct rio_playlist_file_header header;
    struct rio_playlist_file_entry entry;
    FILE *f;
    uint i;
    int ret;

    debug("write_playlist_file(filename=%s,songs=%x,nsongs=%d)", \
          filename, songs, nsongs);

    if (!filename || !songs || !nsongs)
        return -EINVAL;

    f = fopen(filename, "w");
    if (f == 0)
        return -errno;

    header = rio_playlist_file_header_create( nsongs );

    ret = fwrite( &header, sizeof(header), 1, f );

    if (ret != 1)
    {
       error("Failed to write header to file: %s", strerror(errno));
       ret = errno; /* save errno because fclose() resets it */
       fclose(f);
       return -ret;
    }

    for (i = 0; i < nsongs; i++)
    {
       entry = rio_playlist_file_entry_create( songs[i], sflags[i] );

       ret = fwrite( &entry, sizeof(entry), 1, f );

       if (ret != 1)
       {
          error("Failed writing playlist entry: %s", strerror(errno));
          ret = errno; /* save errno because fclose() resets it */
          fclose(f);
          return -ret;
       }
    }

    fclose(f);

    debug("write_playlist_file(): Success");

    return URIO_SUCCESS;
}
