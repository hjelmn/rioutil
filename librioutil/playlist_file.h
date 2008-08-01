/**
 *   (c) 2008 Jamie Faris <farisj@gmail.com>
 *
 *   playlist_file.h
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

#ifndef PLAYLIST_FILE_H
#define PLAYLIST_FILE_H

#include <sys/types.h> /* uint, u_int8_t */

/**
 * Read a playlist file that has been downloaded from a newer generation
 * Rio.
 *
 * filename: name of file to read
 * songs: pointer to pointer that will be set to a newly allocated array of
 *        song numbers (rio_num).  Must be freed by the caller.
 * nsongs: will be set to the number of songs
 *
 * Returns: -EINVAL if parameters are bad, errno if file reading fails.
 *          URIO_SUCCESS on success.
 */
int read_playlist_file ( const char *filename, unsigned int **songs, \
                         unsigned int *nsongs );

/**
 * Write a playlist file to disk.
 *
 * filename: filename to create
 * songs: array of rio_num
 * sflags: array of u_int8_t[3]
 * nsongs: number of songs in the playlist
 */
int write_playlist_file( char *filename, const unsigned int songs[], \
                         const u_int8_t *sflags[], unsigned int nsongs);


#endif /* PLAYLIST_FILE_H */
