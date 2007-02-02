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

#include "rioi.h"

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#if !defined (PATH_MAX)
#define PATH_MAX 255
#endif

int new_playlist_info (info_page_t *newInfo, char *name);
int playlist_info (info_page_t *newInfo, char *file_name);

int create_playlist_rio (rios_t *rio, char *name, int songs[], int memory_units[], int nsongs) {
  info_page_t info;
  int error, addpipe, i, tmpi;
  char filename[PATH_MAX];
  file_list *tmp;
  FILE *fh;

  if (!rio || !name || !songs || !memory_units)
    return -EINVAL;

  /* Current implementation only works for S-Series and newer. For
     older, upload a .lst file. */
  if (return_generation_rio (rio) < 4)
    return -EPERM;
  
  if (try_lock_rio (rio) != 0)
    return -EBUSY;

  rio_log (rio, 0, "create_playlist_rio: creating a new playlist %s.\n", name);

  /* Create a temporary file to store the new playlist */
  snprintf (filename, PATH_MAX, "/tmp/rioutil_%s.%08x.lst", name, time (NULL));
  fh = fopen (filename, "w");
  if (fh == 0)
    UNLOCK(-errno);
  fprintf (fh, "FIDLST%c%c%c", 1, 0, 0);
  tmpi = arch32_2_little32(nsongs);
  fwrite (&tmpi, 1, 3, fh);

  for (i = 0 ; i < nsongs ; i++) {
    rio_log (rio, 0, "Adding for song %i to playlist %s...\n", songs[i], name);
    for (tmp = rio->info.memory[memory_units[i]].files ; tmp ; tmp = tmp->next)
      if (tmp->num == songs[i])
        break;

    if (tmp == NULL)
      continue;

    tmpi = arch32_2_little32(tmp->rio_num);
    fwrite (&tmpi, 1, 3, fh);
    fwrite (tmp->sflags, 3, 1, fh);
  }

  fclose (fh);

  error = new_playlist_info (&info, name);
  if (error != URIO_SUCCESS)
    UNLOCK(error);

  if ((addpipe = open(filename, O_RDONLY)) == -1)
    UNLOCK(-1);

  /* i moved the major functionality of both add_file and add_song down a layer */
  if ((error = do_upload (rio, 0, addpipe, info, 0)) != URIO_SUCCESS) {
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

  rio_log (rio, 0, "add_file_rio: copy complete.\n");

  UNLOCK(URIO_SUCCESS);
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
