/**
 *   (c) 2001-2019 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.4 main.c
 *
 *   Console based interface for Rios using librioutil
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

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

#include <signal.h>

#include <sys/stat.h>
#include <dirent.h>

#include <errno.h>

#if defined HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "rio.h"
#include "main.h"

#define MAX_DEPTH_RIO 3
#define TOTAL_MARKS  20

#define max(a, b) ((a > b) ? a : b)

/* a simple version of basename that returns a pointer into x where the basename
   begins or NULL if x has a trailing slash. */
static char *basename_simple (char *x){
  size_t i = strlen (x) - 1;
 
  if ('/' == x[i]) {
    return NULL;
  }

  while (--i > 0) {
    if (x[i] == '/') {
      return x + i + 1;
    }
  }

  return x;
}

static rios_t *current_rio = NULL;
static int is_a_tty;

static void usage (void);
static void print_version (void);

static void progress (int x, int X, void *ptr);
static void progress_no_tty (int x, int X, void *ptr);
static void new_printfiles (rios_t *rio);
static void print_info (rios_t *rio);
static int create_playlist (rios_t *rio, int argc, char *argv[]);
static int overwrite_file (rios_t *rio, int mem_unit, int argc, char *argv[]);
static int pipe_upload (rios_t *rio, int mem_unit, char *title, char *album, char *artist);
static int add_tracks (rios_t *rio);
static int download_tracks (rios_t *rio, char *copt, u_int32_t mem_unit);
static int delete_tracks (rios_t *rio, char *dopt, u_int32_t mem_unit);
static int print_playlists (rios_t *rio);

/* prototypes for modifying this driver's upload stack */
static struct upload_stack upstack = {NULL, NULL};
static void upstack_push (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
static void upstack_push_top (int mem_unit, char *title, char *artist, char *album, char *filename, int recursive_depth);
static struct _song *upstack_pop (void);
static void free__song (struct _song *);

/* signal handler */

static void aborttransfer (int sigraised) {
  /* quiet compiler warning */
  (void) sigraised;
  current_rio->abort = 1;
}

/******************/


int main (int argc, char *argv[]) {
  int c, ret;
  unsigned int i;

  unsigned char flags[27];
  char *flag_args[26];
  int command_flags[] = {0, 2, 3, 5, 6, 8, 9, 11, 13, 15, 20, 26, -1};

  uint num_command_flags = 0;
  unsigned int mem_unit = -1;
  long int dev;

  rios_t rio;

  struct option long_options[] = {
    {"upload",    required_argument, 0,    'a'},
    {"bulk",      no_argument,       0,    'b'},
    {"download",  required_argument, 0,    'c'},
    {"delete",    required_argument, 0,    'd'},
    {"debug",     no_argument,       0,    'e'},
    {"format",    no_argument,       0,    'f'},
    {"get-playlist", no_argument,    0,    'g'},
    {"help",      no_argument,       0,    'h'},
    {"info",      no_argument,       0,    'i'},
    {"playlist",  no_argument,       0,    'j'},
    {"nocolor",   no_argument,       0,    'k'},
    {"list",      no_argument,       0,    'l'},
    {"memory",    required_argument, 0,    'm'},
    {"name",      required_argument, 0,    'n'},
    {"device",    required_argument, 0,    'o'},
    {"overwrite", no_argument,       0,    'O'},
    {"pipe",      no_argument,       0,    'p'}, 
    {"album" ,    required_argument, 0,    'r'},
    {"artist",    required_argument, 0,    's'},
    {"title" ,    required_argument, 0,    't'},
    {"update",    required_argument, 0,    'u'},
    {"version",   no_argument,       0,    'v'},
    {"recovery",  no_argument,       0,    'z'},
    {NULL,        0,                 NULL,  0 },
  };
      
  /*
    find out if rioutil is running on a tty
    if you do not want coloring, replace this line with:
    is_a_tty = 0
  */
  is_a_tty = isatty(1);

  memset (flags, 0, 27);
  memset (flag_args, 0, 26 * sizeof (char *));

  while((c = getopt_long(argc, argv, "W;a:bld:ec:u:s:t:r:m:po:n:fh?ivgzjkO",
			 long_options, NULL)) != -1){
    switch(c){
    case 'm':
      /* memory units are not relevant when upgrading */
      mem_unit = strtol (optarg, NULL, 10);
      if (errno != 0) {
	fprintf (stderr, "Invalid argument for --memory option: %s\n", optarg);
	exit (EXIT_FAILURE);
      }

      break;
    case 'a':
      for (i = (unsigned) optind ; argv[i] && argv[i][0] != '-' ; ++i) {
	upstack_push (mem_unit, flag_args[19], flag_args[18], flag_args[17], argv[i], 0);
      }
    case 'c':
    case 'd':
    case 'n':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'o':
      flag_args[c - 'a'] = optarg;
    case 'b':
    case 'f':
    case 'i':
    case 'j':
    case 'l':
    case 'p':
    case 'e':
    case 'g':
      if (flags[c - 'a'] < 255)
	flags[c - 'a'] ++;
      
      break;
    case 'z':
      printf ("Warning: use of -z is deprecated\n");

      break;
    case 'k':
      is_a_tty = 0;

      break;
    case 'v':
      print_version ();

      return 0;
    case 'O':
      flags[26] = 1;

      break;
    case 0:
      break;
    case 'h':
    case '?':
    default:
      if (c != 'h' && c != '?')
	printf ("Unrecognized option -%c.\n\n", c);

      usage ();
    }
  }

  if (flags[1]) {
    flags[0] = 1;

    for (i = optind ; i < (uint) argc ; i++)
      upstack_push (mem_unit, flag_args[19], flag_args[18], flag_args[17], argv[i], 0);
  }

  for (i = 0, num_command_flags = 0 ; command_flags[i] > -1 ; i++)
    if (flags[command_flags[i]])
      num_command_flags ++;

  /* print usage and exit if no commands are specified */
  if (num_command_flags == 0)
      usage();

  if ((flags[9] || flags[25]) && num_command_flags > 1) {
    fprintf (stderr, "Playlist and overwrite commands can not be used with any other commands.\n");
    exit (EXIT_FAILURE);
  }

  /* open the player */
  if (flags[5] || flags[20]) {
    flags[25] = 1;
    printf ("Attempting to open Rio for format/upgrade.... ");
  } else
    printf ("Attempting to open Rio and retrieve song list.... ");

  dev = (flags[14]) ? strtol (flag_args[14], NULL, 10) : 0;

  ret = open_rio (&rio, dev, flags[4], (flags[25]) ? 0 : 1);
  if (ret != URIO_SUCCESS) {
      printf ("failed!\n");

      fprintf (stderr, "Reason: %s.\n", strerror (-ret));
      
      exit (EXIT_FAILURE);
  } else
    printf ("complete\n");

  current_rio = &rio;


  /* setup progress bar callback */
  set_progress_rio (&rio, ((is_a_tty) ? progress : progress_no_tty), NULL);

  /* print device/file information */
  if (flags[25] == 0) {
    /* neither device nor file info is available when formating or upgrading */
    if (flags[8]) {
      print_info (&rio);
      num_command_flags--;
    }

    if (flags[11]) {
      new_printfiles (&rio);
      num_command_flags--;
    }

    if (flags[6])
    {
      print_playlists (&rio);
      num_command_flags--;
    }
  }

  if (num_command_flags > 0) {
    if (flags[20]) {
      printf ("Updating firmware, this will take about 30 seconds to complete.\n");

      ret = firmware_upgrade_rio (&rio, flag_args[20]);
    } else if (flags[13]) {
      printf ("Seting device name to %s...\n", flag_args[13]);
      ret = set_name_rio (&rio, flag_args[13]);
    } else if (flags[5])
      ret = format_mem_rio (&rio, mem_unit);
    else if (flags[9])
      ret = create_playlist (&rio, argc, argv);
    else if (flags[26])
      ret = overwrite_file (&rio, mem_unit, argc, argv);
    else if (flags[2])
      ret = download_tracks (&rio, flag_args[2], mem_unit);
    else if (flags[3])
      ret = delete_tracks (&rio, flag_args[3], mem_unit);
    else if (flags[15])
      ret = pipe_upload (&rio, mem_unit, flag_args[19], flag_args[18], flag_args[17]);
    else if (flags[0]) {
      for (i = 0 ; i < return_mem_units_rio (&rio) ; i++)
	printf ("Free space on %s: %03.01f MiB.\n", rio.info.memory[i].name,
		(float)return_free_mem_rio (&rio, i) / 1024.0);
    
      ret = add_tracks (&rio);
    }

    printf (" Command %ssuccessful\n", (ret) ? "un" : "");
  }

  close_rio (&rio);
  
  return ret;
}

static void dir_add_songs (char *filename, int depth, int mem_unit) {
  struct stat statinfo;
  DIR *dir_fd;
  struct dirent *entry;
  char *path_temp = NULL;

  if (depth > MAX_DEPTH_RIO)
    return;

  dir_fd = opendir (filename);

  while ((entry = readdir (dir_fd)) != NULL) {
    if (path_temp) {
      free (path_temp);
      path_temp = NULL;
    }
      
    path_temp = calloc (strlen(filename) + strlen(entry->d_name) + 1, 1);
    sprintf (path_temp, "%s/%s", filename, entry->d_name);

    if (entry->d_name[0] == '.')
      continue;

    if (stat (path_temp, &statinfo) < 0)
      continue;
    
    if (S_ISDIR (statinfo.st_mode)) {
      dir_add_songs (path_temp, depth + 1, mem_unit);
      continue;
    }

    upstack_push_top (mem_unit, NULL, NULL, NULL, path_temp, depth + 1);
  }

  closedir (dir_fd);
}

static int pipe_upload (rios_t *rio, int mem_unit, char *title, char *album, char *artist) {
  int error, ret, fd;
  char file_name[] = "/tmp/rioutil_XXXXXX.mp3";
  char file_buffer[1024];

  if (NULL == mktemp (file_name)) {
    fprintf (stderr, "rioutil/pipe_upload: mktemp failed: %s\n", strerror (errno));

    return -errno;
  }

  fd = open (file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0) {
    fprintf (stderr, "rioutil/pipe_upload: could not create a temporary file: %s\n", strerror (errno));

    return -errno;
  }

  while ((ret = read (0, file_buffer, 1024)) > 0)
    write (fd, file_buffer, ret);

  close (fd);

  error = add_song_rio (rio, mem_unit, file_name, artist, title, album);

  /* delete the temporary file */
  ret = unlink (file_name);
  if (ret < 0)
    fprintf (stderr, "rioutil/pipe_upload: could not destroy temporary file: %s\n", strerror (errno));

  return error;
}

static void process_song (rios_t *rio, struct _song *p) {
  int ret, mem_unit, mem_units;
  char display_name[32];
  struct stat statinfo;
  size_t file_namel;
  char *file_name;
  long free_size;

  mem_unit = p->mem_unit;
  mem_units = return_mem_units_rio (rio);

  if (mem_unit > -1) {
    if (mem_unit > mem_units) {
      fprintf (stderr, "Memory unit identifier %d is outside the valid range of 0-%d for this device.\n",
	       mem_unit, mem_units - 1);
      return;
    }

    free_size = (long) return_free_mem_rio (rio, p->mem_unit) * 1024;
  }

  if (stat(p->filename, &statinfo) < 0) {
    printf("rioutil/src/main.c add_track: could not stat file %s (%s)\n", p->filename, strerror (errno));
    return;
  }

  if (S_ISDIR(statinfo.st_mode)) {
    /* add files from directory */
    dir_add_songs (p->filename, p->recursive_depth, p->mem_unit);
    return;
  }

  if (!S_ISREG(statinfo.st_mode)) {
    printf("rioutil/src/main.c add_track: %s is not a regular file!\n", p->filename);
    return;
  }

  file_name = basename_simple (p->filename);
  file_namel = strlen (file_name);

  strncpy (display_name, file_name, 31);

  if (file_namel > 32)
    /* truncate long filenames */
    sprintf (&display_name[14], "...%s", &file_name[file_namel - 14]);

  printf("%32s [%03.1f MiB]: ", display_name, (double)statinfo.st_size / 1048576.0);
    
  if (-1 == p->mem_unit) {
    for (int i = 0 ; i < mem_units ; i++) {
      free_size = return_free_mem_rio (rio, i) * 1024;

      if (free_size >= statinfo.st_size) {
	p->mem_unit = i;
	break;
      }

      /* insufficient space on this memory unit, try another */
      if (i == mem_units - 1) {
	ret = -ENOSPC;
      }
    }
  } else if (free_size < statinfo.st_size) {
    ret = -ENOSPC;
    p->mem_unit = -1;
  }

  if (p->mem_unit >= 0) {
    ret = add_song_rio (rio, p->mem_unit, p->filename, p->artist, p->title, p->album);
  }

  if (ret == URIO_SUCCESS) 
    printf(" Complete [memory %i]\n", p->mem_unit);
  else
    printf(" Incomplete: %s\n", strerror (-ret));
}

static int add_tracks (rios_t *rio){
  struct _song *p;
  
  /* set up a signal handler for ^C and kill -15 */
  signal (SIGINT,  aborttransfer);
  signal (SIGTERM, aborttransfer);
  
  while ((p = upstack_pop()) != NULL) {
    process_song (rio, p);
    free__song (p);
  }
  
  return 0;
}

static int download_single_file (rios_t *rio, int file, int mem_unit) {
  int file_size;
  char *file_name;

  int ret;

  file_size = return_file_size_rio (rio, file, mem_unit);
  file_name = return_file_name_rio (rio, file, mem_unit);

  if (file_name != NULL) {
    printf ("%32s [%03.01f MiB]:", file_name, (float)file_size/1048576.0);
    free (file_name);
  } else {
    printf ("No file name associated with file number: %i. Aborting...\n", file);
    return -1;
  }

  if ((ret = download_file_rio (rio, mem_unit, file, NULL)) == URIO_SUCCESS)
    printf(" Download complete.\n");
  else
    printf(" Download failed. Reason: %s.\n", strerror (ret));

  return ret;
}

static int delete_single_file (rios_t *rio, int file, int mem_unit) {
  int ret;
  
  if ((ret = delete_file_rio (rio, mem_unit, file)) == URIO_SUCCESS)
    printf("File %i successfully deleted.\n", file);
  else
    printf("File %i could not be deleted.\n", file);

  return ret;
}

static int parse_input (rios_t *rio, char *copt, u_int32_t mem_unit, int (*fp)(rios_t *, int, int)) {
  int dtl;
  char *breaker;
  
  fprintf(stderr, "Setting up signal handler\n");
  signal (SIGINT, aborttransfer);
  signal (SIGKILL, aborttransfer);

  /*
    "a-b"
    
     runs fp with integers a through b
  */
  if ((breaker = strstr(copt, "-")) != NULL) {
    int low, high;
    
    low  = strtol (copt, NULL, 10);
    high = strtol (breaker + 1, NULL, 10);

    for (dtl = low ; dtl <= high ; dtl++)
      fp (rio, dtl, mem_unit);
    
  } else {
    /*
      "a b c d"
      
      runs fp with integers a, b, c, and d
    */

    char *startp, *endp;
    int file_number = -1;

    startp = copt;

    do {
      endp = startp;

      while ((*startp != '\0') && (endp == startp)) {
	file_number = strtol (startp, &endp, 10);

	if (endp == startp) {
	  /* recognize only strings that are made up of numbers and spaces */
	  if (*startp != ' ') 
	    *startp = '\0';
	  else
	    endp = startp = startp + 1;
	}
      }

      if (*startp != '\0') {
	fp (rio, file_number, mem_unit);
      
	startp = endp;
      }
    } while (*startp != '\0');
  }

  return URIO_SUCCESS;
}

static int download_tracks (rios_t *rio, char *copt, u_int32_t mem_unit) {
  return parse_input (rio, copt, mem_unit, download_single_file);
}

static int delete_tracks (rios_t *rio, char *dopt, u_int32_t mem_unit) {
  return parse_input (rio, dopt, mem_unit, delete_single_file);
}

  
static int intwidth(int i) {
  int j = 1;

  while (i /= 10)
    j++;

  return ((j > 10) ? 10 : j);
}

static void new_printfiles(rios_t *rio) {
  flist_rio_t *tmpf;
  int j;
  int id_width;
  int size_width;
  int minutes_width;
  int header_width;
  int max_title_width = strlen("Title");
  int max_name_width = strlen("Name");
  uint max_id = 0;
  int max_size = 0;
  unsigned int max_time = 0;
  int num_mem_units;
  
  flist_rio_t **flst;
  
  num_mem_units = return_mem_units_rio (rio);
  
  flst = (flist_rio_t**) malloc (sizeof(flist_rio_t *) * num_mem_units);
  
  for (j = 0 ; j < num_mem_units ; j++) {
    if (return_flist_rio (rio, j, RIO_FILETYPE_ALL, &flst[j]) < 0) {
      printf ("Could not retrieve the file list from memory unit %i\n", j);

      continue;
    }
    
    for (tmpf = flst[j]; tmpf ; tmpf = tmpf->next) {
      max_title_width = max(max_title_width,(int)strlen(tmpf->title));
      max_name_width = max(max_name_width,(int)strlen(tmpf->name));
      max_id = max(max_id, tmpf->num);
      max_size = max(max_size,tmpf->size);
      max_time = max(max_time,tmpf->time);
    }
  }
  
  id_width = intwidth(max_id);
  id_width++;
  
  size_width = intwidth(max_size/1024);
  
  minutes_width = intwidth(max_time / 60);
  minutes_width++;
  
  header_width = printf("%*s | %*s |  %*s   %*s:%2s %*s %s %s %s\n",
			id_width,
			"id",
			max_title_width, "Title",
			max_name_width, "Name",
			minutes_width, "mm", "ss",
			size_width, "Size", "Bitrate", "rio_num", "filn");
  
  for (j = 0; j < header_width; ++j)
    putchar('-');
  
  putchar('\n');
  
  for (j = 0 ; j < num_mem_units ; j++) {
    if (is_a_tty)
      printf("[%im", 33 + j);

    printf("%s:\n", rio->info.memory[j].name);

    if (is_a_tty)
      printf("[m");

    
    for (tmpf = flst[j]; tmpf ; tmpf = tmpf->next) {
      printf("%*i | %*s |  %*s | %*i:%02i %*i %*i 0x%02x %i\n",
	     id_width, tmpf->num,
	     max_title_width, tmpf->title,
	     max_name_width, tmpf->name,
	     minutes_width, (tmpf->time / 60),
	     (tmpf->time % 60),
	     size_width, tmpf->size / 1024, 7, tmpf->bitrate, tmpf->rio_num, tmpf->inum);
    }
    
    free_flist_rio (flst[j]);
  }
  
  free (flst);
  
  printf ("\n");
}

static void print_info(rios_t *rio) {
  int i, j, ticks, type, ret;
  uint ttime;
  int num_mem_units;
  int nfiles = 0;
  uint ptt = 0, ptf = 0;

  float ptu = 0.0, free_mem;
  float used = 0.;
  float size_div, total;
  rio_info_t *info;

  u_int8_t serial_number[16];

  type = return_type_rio (rio);

  ret = get_info_rio (rio, &info);
  if (ret < 0) {
    fprintf (stderr, "rioutil/main.c print_info: could not retrieve device info\n");

    return;
  }

  num_mem_units = return_mem_units_rio (rio);

  size_div = 1024.0;

  if (is_a_tty)
    printf("[37;40mName[m: %s\n", info->name);
  else
    printf("Name: %s\n", info->name);

  ret = return_serial_number_rio (rio, serial_number);
  if (ret == URIO_SUCCESS) {
    /* the serial number is a 16-byte hexidecimal number */
    printf ("Serial Number: ");

    for (i = 0 ; i < 16 ; i++)
      printf ("%02x", serial_number[i]);

    printf ("\n");
  }

  /* these values cannot be read/changed with the current S-Series firmware, so
     dont bother printing anything */
  if (type == RIO800 || type == RIO600 || type == RIORIOT ||
      type == PSAPLAY) {
    printf("Volume: %i\n", info->volume);
    printf("Repeat: %s\n", repeatStates[info->repeat_state]);
    if (type == RIORIOT) {
      printf("Random: %s\n", randomStates[info->random_state]);
      printf("'The' Filter: %s\n", theFilterStates[info->the_filter_state]);
    }
    printf("Sleep Time: %s\n",sleepStates[info->sleep_time]);
    printf("Bass: %i\n",-(info->bass - 6));
    printf("Treble: %i\n",-(info->treble - 6));
    
    if (type != RIORIOT) {
      printf("Equilizer: %s\n", equilizerStates[info->eq_state]);
      printf("Programmed to Playlist: %i\n", info->playlist);
    }
    
    if (type == RIO800 || type == RIORIOT)
      printf("Contrast: %i\n", info->contrast);

    printf("Backlight: %s\n", lightStates[info->light_state]);

  }

  if (is_a_tty) {
    printf("\n[37:30mFirmware Version[m: %01.02f\n", info->firmware_version);

    printf("[37:30mMemory units[m: %i\n\n", return_mem_units_rio (rio));
  } else {
    printf("\nFirmware Version: %01.02f\n", info->firmware_version);

    printf("Memory units: %i\n\n", return_mem_units_rio (rio));
  }

  for (j = 0 ; j < num_mem_units ; j++) {
    printf("Memory unit %i: %s\n", j, rio->info.memory[j].name);
    
    total    = (float) return_total_mem_rio (rio, j) / size_div;
    used     = (float) return_used_mem_rio (rio, j)  / size_div;
    free_mem = (float) return_free_mem_rio (rio, j)  / size_div;
    nfiles   = return_num_files_rio (rio, j);
    ttime    = return_time_rio (rio, j);
      
    ticks = 50 * (used / total);
    
    if (is_a_tty)
      printf("[%im", 33 + j);

    printf("Free: %03.01f MiB (", free_mem);
    printf("%03.01f MiB Total)\n", total);
    printf("Used: %03.01f MiB in %i files\n", used, nfiles);
    
    printf("[");
    for(i = 1; i <= 50 ; i++) {
      if (i <= ticks)
	putchar('#');
      else
	putchar(' ');
    }

    printf("] %03.01f %%\n\n", 100.0 * (used / total));
    
    printf("Total Time: %02u:%02u:%02u\n", (uint)ttime / 3600, 
	   (uint)(ttime % 3600) / 60, (uint)ttime % 60);
    
    ptt += ttime;
    ptu += used;
    ptf += nfiles;

    if (is_a_tty)
      printf("[m\n");
  }

  printf("Player Space Used: %03.01f MiB in %i files\n", used, nfiles);
  printf("Total Player Time: %02u:%02u:%02u\n\n", (uint)ptt / 3600, 
	 (uint)(ptt % 3600) / 60, (uint)ptt % 60);

  free (info);
}

static int create_playlist (rios_t *rio, int argc, char *argv[]) {
  int nsongs = argc - optind - 1;
  int i, ret;
  uint *mems, *songs;

  if (nsongs < 1) {
    fprintf (stderr, "No songs to add!\n");
    return 1;
  }

  fprintf (stderr, "Creating playlist %s on Rio.\n", argv[optind]);

  mems = calloc (sizeof (int), nsongs);
  songs = calloc (sizeof (int), nsongs);
  
  optind++;

  for (i = 0 ; i < nsongs ; i++) {
    sscanf (argv[optind+i], "%u,%u", &mems[i], &songs[i]);
  }

  ret = create_playlist_rio (rio, argv[optind-1], songs, mems, nsongs);

  if (ret < 0)
    fprintf (stderr, " Could not create playlist!\n");
  else
    fprintf (stderr, " Playlist creation successfull.\n");

  free (mems);
  free (songs);

  return 0;
}

static int overwrite_file (rios_t *rio, int mem_unit, int argc, char *argv[]) {
  int song;
  int ret;

  if (optind >= argc) {
    return -1;
  }

  sscanf (argv[optind], "%d", &song);
  printf ("Overwriting %s with contents of %s\n", argv[optind], argv[optind+1]);
  ret = overwrite_file_rio (rio, mem_unit, song, argv[optind+1]);

  if (ret < 0)
    printf (" Error %i\n", ret);
  else
    printf (" Complete\n");

  return ret;
}

static int print_playlists (rios_t *rio)
{
    flist_rio_t *flist, *f;
    rio_playlist_t playlist;
    unsigned int i, playlist_count = 0;
    int ret;

    if (!rio)
        return -EINVAL;

    ret = return_flist_rio (rio, 0, RIO_FILETYPE_PLAYLIST, &flist);
    if (ret != URIO_SUCCESS)
        return ret;

    printf("Playlists:\n\n");

    for (f = flist; f; f = f->next)
    {
        playlist_count++;

        ret = get_playlist_rio (rio, 0, f->num, &playlist);
        if (ret != URIO_SUCCESS)
            continue;

        printf("  %s\n", playlist.name);
        for (i = 0; i < playlist.nsongs; i++)
            printf("    %d\n", playlist.songs[i]); /* TODO print song name */
    }

    free_flist_rio (flist);
    flist = NULL;

    printf ("Total playlists: %d\n", playlist_count);

    return URIO_SUCCESS;
}

static void print_version (void) {
  printf("%s %s\n", PACKAGE, VERSION);
  printf("Copyright (C) 2003-2016 Nathan Hjelm\n\n");
  
  printf("%s comes with NO WARRANTY.\n", PACKAGE);
  printf("You may redistribute copies of %s under the terms\n", PACKAGE);
  printf("of the GNU Lesser Public License.\n");
  printf("For more information about these issues\n");
  printf("see the file named COPYING in the %s distribution.\n", PACKAGE);
  
  exit (0);
}

static void usage (void) {
  printf("Usage: rioutil <OPTIONS>\n\n");
  printf("Interface with Diamond MM/Sonic Blue/DNNA MP3 players.\n");
  printf("A command switch is required, and if you use either the -t or\n");

  printf(" uploading:\n");
  printf("  -a, --upload=<file>    upload an track\n");
  printf("  -b, --bulk=<filelist>  upload mutiple tracks\n\n");

  printf(" uploading from pipe:\n");
  printf("  -p, --pipe <mp3?(1 or 0)> <filename> <bitrate> <samplerate>\n\n");

  printf(" upload options:\n");
  printf("  -r, --album=<string>   album.  MAX:63 chars\n");
  printf("  -s, --artist=<string>  artist. MAX:63 chars\n");
  printf("  -t, --title=<string>   title.  MAX:63 chars\n\n");

  printf(" uploading new firmware:\n");
  printf("  -u, --update=<file>    update with a new firmware\n");

  printf(" other commands:\n");
  printf("  -j, --playlist <name> <list of mem_unit,song pairs>   create a playlist (S-Series and newer)\n");
  printf("       i.e. rioutil -j fubar 0,0 1,0     (song 0 on mem_unit 0, song 0 on mem_unit 1)\n");
  printf("  -i, --info             device info\n");
  printf("  -l, --list             list tracks\n");
  printf("  -f, --format           format rio memory (default is internal)\n");
  printf("  -n, --name=<string>    change the name. MAX:15 chars\n");
  printf("  -c, --download=<int>   download a track(s)\n");
  printf("  -d, --delete=<int>     delete a track(s)\n\n");

  printf(" general options:\n");
#if !defined(__FreeBSD__) || !defined(__NetBSD__)
  printf("  -o, --device=<int>     minor number of rio (assigned by driver), /dev/usb/rio?\n");
#else
  printf("  -o, --device=<int>     minor number of rio (assigned by driver), /dev/urio?\n");
#endif
  printf("  -k, --nocolor          supress ansi color\n");
  printf("  -m, --memory=<int>     memory unit to upload/download/delete/format to/from\n");
  printf("  -e, --debug            increase verbosity level.\n");

  printf(" rioutil info: librioutil driver: %s\n", return_conn_method_rio ());
  printf("  -v, --version          print version\n");
  printf("  -?, --help             print this screen\n\n");

  exit (EXIT_FAILURE);
}

static void progress (int x, int X, void *ptr) {
  int nummarks = (x * TOTAL_MARKS) / X;
  int percent = (x * 100) / X;
  char m[] = "-\\|/";
  char HASH_MARK;
  int i;
  char HASH_BARRIER = '>';
  char NO_HASH      = ' ';

  /* quiet compiler warning */
  (void) ptr;

  if (percent != 100)
    HASH_MARK  = '-';
  else
    HASH_MARK  = '*';

  printf("%c [[34m", m[percent%4]);
  
  for (i = 0 ; i < TOTAL_MARKS ; i++){
    if (i < nummarks)
      putchar(HASH_MARK);
    else if (i == nummarks)
      putchar(HASH_BARRIER);
    else
      putchar(NO_HASH);
  }

  printf("[m] [37;40m%3i[m%%", percent);

  if (x != X)
    for (i = 0 ; i < (TOTAL_MARKS + 9) ; i++) putchar('\b');

  fflush(stdout);
}

static void progress_no_tty(int x, int X, void *ptr) {
  static int last_nummarks = 0;
  int i, nummarks;

  /* quiet compiler warning */
  (void) ptr;

  if (x == 0) {
    putchar ('*');
    last_nummarks = 0;
  }

  nummarks =  (x * TOTAL_MARKS) / X - last_nummarks;
  for (i = 0 ; i < nummarks; i++)
      putchar('.');
    
  last_nummarks += nummarks;

  if (x == X)
    putchar (']');

  fflush(stdout);
}

static struct stack_item *new_stack_item (int mem_unit, char *title, char *artist, char *album,
					  char *filename, int recursive_depth) {
  struct stack_item *p;

  if (filename == NULL) {
    fprintf (stderr, "main.c/new_stack_item: error! called with no path.\n");
    
    exit (EXIT_FAILURE);
  }

  p = (struct stack_item *) calloc (1, sizeof (struct stack_item));
  if (p == NULL) {
    perror ("main.c/new_stack_item: calloc failed");

    exit (EXIT_FAILURE);
  }

  p->data = (struct _song *) calloc (1, sizeof (struct _song));
  if (p->data == NULL) {
    perror ("main.c/new_stack_item: calloc failed");

    exit (EXIT_FAILURE);
  }

  p->data->mem_unit = mem_unit;
  p->data->title    = (title) ? strdup (title) : NULL;
  p->data->artist   = (artist) ? strdup (artist) : NULL;
  p->data->album    = (album) ? strdup (album) : NULL;
  p->data->filename = strdup (filename);
  p->data->recursive_depth = recursive_depth;

  return p;
}

/* upload stack routines */
static void upstack_push (int mem_unit, char *title, char *artist, char *album,
			  char *filename, int recursive_depth) {
  struct stack_item *p;
  
  p = new_stack_item (mem_unit, title, artist, album, filename, recursive_depth);
  p->next = NULL;

  if (upstack.tail != NULL) {
    upstack.tail->next = p;
    upstack.tail = p;
  } else
    upstack.tail = upstack.head = p;
}

static void upstack_push_top (int mem_unit, char *title, char *artist, char *album,
	       char *filename, int recursive_depth) {
  struct stack_item *p;
  
  p = new_stack_item (mem_unit, title, artist, album, filename, recursive_depth);
  p->next = upstack.head;

  if (upstack.head == NULL)
    upstack.tail = p;

  upstack.head = p;
}

static struct _song *upstack_pop(void) {
  struct stack_item *p;
  struct _song *dp;

  if (!upstack.head)
    return NULL;
  
  p = upstack.head;
  upstack.head = p->next;

  if (upstack.head == NULL)
    upstack.tail = NULL;

  dp = p->data;
  free (p);

  return dp;
}

static void free__song (struct _song *p) {
  if (!p)
    return;

  if (p->title)
    free (p->title);
  if (p->artist)
    free (p->artist);
  if (p->album)
    free (p->album);

  free (p->filename);
  free (p);
}
