/*
 *   (c) 2008 Jamie Faris <farisj@gmail.com>
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
/*
 *  riolog.h
 *
 *  Logging functions for librioutil
 */

#ifndef RIOLOG_H
#define RIOLOG_H

#include <stdio.h>

#define trace(...)   riolog( 2, __VA_ARGS__ )
#define debug(...)   riolog( 1, __VA_ARGS__ )
#define warning(...) riolog( 0, __VA_ARGS__ )
#define error(...)   riolog( 0, __VA_ARGS__ )

/**
 * Set the logging verbosity
 */
void set_debug_level(unsigned int new_debug_level);

/**
 * Set the file pointer to log to
 */
void set_debug_out(FILE *out);

/**
 * Log a message
 */
void riolog( unsigned int level, char *format, ... );


#endif /* RIOLOG_H */
