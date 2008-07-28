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


#define trace(msg, args...) riolog( 2, msg, ## args )
#define debug(msg, args...) riolog( 1, msg, ## args )
#define error(msg, args...) riolog( 0, msg, ## args )

/**
 * Set the logging verbosity
 */
void set_debug_level(uint new_debug_level);

/**
 * Set the file pointer to log to
 */
void set_debug_out(FILE *out);

/**
 * Log a message
 */
void riolog( uint level, char *format, ... );


#endif /* RIOLOG_H */
