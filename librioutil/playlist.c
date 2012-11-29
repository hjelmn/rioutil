/**
 *   (c) 2001-2007 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.0 playlist.c 
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
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include "playlist_file.h"
#include "rioi.h"
#include "riolog.h"

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#if !defined (PATH_MAX)
#define PATH_MAX 255
#endif


int create_playlist_rio (rios_t *rio, char *name, uint songs[], uint memory_units[], uint nsongs) {
    info_page_t info;
    int error, addpipe;
    uint i; /* loop counter */
    char filename[PATH_MAX]; /* filename of temp file */
    file_list *tmp;
    uint *rio_num;
    u_int8_t **sflags;

    debug("create_playlist_rio()");

    if (!rio || !name || !songs || !memory_units)
	return -EINVAL;

    /* Current implementation only works for S-Series and newer. For
       older, upload a .lst file. */
    if (return_generation_rio (rio) < 4)
	return -EPERM;
  
    rio_num = calloc( nsongs, sizeof(uint) );
    sflags = calloc( nsongs, 3 * sizeof(u_int8_t) );
    if (rio_num == NULL || sflags == NULL)
    {
        error("calloc() failed!");
        return -ENOMEM;
    }

    if (try_lock_rio (rio) != 0)
	return -EBUSY;

    debug("create_playlist_rio: creating a new playlist %s.", name);

    /* Create a temporary file to store the new playlist */
    snprintf (filename, PATH_MAX, "/tmp/rioutil_%s.%08x.lst", name, (uint) time(NULL));
    for (i = 0 ; i < nsongs ; i++)
    {
	for (tmp = rio->info.memory[memory_units[i]].files ; tmp ; \
             tmp = tmp->next)
	    if (tmp->num == songs[i])
		break;

	if (tmp == NULL)
	    continue;

        rio_num[i] = tmp->rio_num;
        sflags[i] = tmp->sflags;
    }

    error = write_playlist_file( filename, rio_num, sflags, nsongs );
    if (error != URIO_SUCCESS)
        UNLOCK(error);


    error = new_playlist_info (&info, name);
    if (error != URIO_SUCCESS)
	UNLOCK(error);

    if ((addpipe = open(filename, O_RDONLY)) == -1)
	UNLOCK(-1);

    /* i moved the major functionality of both add_file and add_song down a layer */
    error = do_upload (rio, 0, addpipe, info, 0);
    if (error != URIO_SUCCESS)
    {
	free (info.data);

	close (addpipe);

	/* make sure no malicious user has messed with this variable */
	if (strstr (filename, "/tmp/rioutil_") == filename)
	    unlink (filename);

	UNLOCK(error);
    }

    close (addpipe);

    if (strstr (filename, "/tmp/rioutil_") == filename)
	unlink (filename);

    free (info.data);

    debug("create_playlist_rio(): success");

    UNLOCK(URIO_SUCCESS);
}

/* Public API function */
int get_playlist_rio( rios_t *rio, uint memory_unit, uint file_num, rio_playlist_t *playlist )
{
    int ret;
    char *filename;
    uint *songs;
    uint nsongs;
    flist_rio_t *flist;

    debug("get_playlist_rio(rio=%x,memory_unit=%d,file_num=%d,playlist=%x)",
	  rio, memory_unit, file_num, playlist);

    if (!rio || !playlist)
	return -EINVAL;

    filename = tempnam (NULL, "riopl");

    debug("Playlist temp file = %s", filename);

    ret = download_file_rio (rio, memory_unit, file_num, filename);
    if (ret != URIO_SUCCESS)
    {
	error("get_playlist_rio: downloading failed: %d", ret);
        return ret;
    }

    ret = read_playlist_file( filename, &songs, &nsongs );
    if (ret != URIO_SUCCESS)
    {
        error("get_playlist_rio: read_playlist_file failed: %s", strerror(-ret));
        /* TODO check for file & delete it */
        return ret;
    }

    unlink (filename);

    playlist->nsongs = nsongs;
    playlist->songs = songs;
    playlist->rio_num = file_num;

    flist = get_flist_rio( rio, memory_unit, file_num );

    strncpy( playlist->name, flist->title, sizeof(playlist->name) );

    debug("get_playlist_rio: success (%d songs in playlist).", nsongs);

    return URIO_SUCCESS;
}



/*
  playlist_info:
*/
int playlist_info (info_page_t *newInfo, char *file_name) {
    rio_file_t *playlist_file;
    int fnum;

    if (!newInfo || !file_name)
	return -EINVAL;

    playlist_file = malloc(sizeof(rio_file_t));
    if (playlist_file == NULL)
	return -ENOMEM;

    sscanf(file_name, "Playlist%02d.lst", &fnum);

    sprintf((char *)playlist_file->title, "Playlist %02d", fnum);

    playlist_file->bits = 0x21000590; /* playlist bits + file bits + download bit */

    newInfo->skip = 0;
    newInfo->data = playlist_file;

    return URIO_SUCCESS;
}

/* Playlists for S-Series and newer. */
int new_playlist_info (info_page_t *newInfo, char *name)
{
    rio_file_t *playlist_file;

    if (!newInfo || !name)
	return -EINVAL;

    playlist_file = malloc(sizeof(rio_file_t));
    if (playlist_file == NULL)
	return -ENOMEM;

    snprintf((char *)playlist_file->title, 64, "%s", name);

    playlist_file->bits = 0x11000110;
    playlist_file->type = TYPE_PLS;

    newInfo->skip = 0;
    newInfo->data = playlist_file;

    return URIO_SUCCESS;
}
