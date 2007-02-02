/**
 *   (c) 2001-2006 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.3 main.h
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

#ifndef MAIN_H
#define MAIN_H

static const char *equilizerStates[8] = {
  "Normal", "Jazz", "Rock", "Classic",
  "Book", "Rap", "Flat", "Custom"
};

static const char *lightStates[6] = {
  "Always off", "1 sec", "2 secs",
  "5 secs", "10 secs", "Always on"
};
  
static const char *repeatStates[4] = {
  "Off", "All", "Track", "Playlist"
};

static const char *randomStates[4] = {
  "Off", "On"
};

static const char *theFilterStates[2] = {
  "Off", "On"
};

static const char *sleepStates[5] = {
  "Never", "1 Min", "2 Mins", "5 Mins", "15 Mins"
};

/* minimal stack structure for uploading songs */
struct _song {
  int mem_unit;
  
  char *title;
  char *artist;
  char *album;
    
  char *filename;

  int recursive_depth;
};

struct stack_item {
  struct _song *data;

  struct stack_item *next;
};

struct upload_stack {
  struct stack_item *head, *tail;
};

#endif
