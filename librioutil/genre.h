/**
 *   (c) 2001-2016 Nathan Hjelm <hjelmn@users.sourceforge.net>
 *   v1.5.3 rio.c
 *
 *   c version of librioutil
 *   all sources are c style gnu (c-set-style in emacs)
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

char *genre_table[] = {
  /* stardard codes */
  "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
  "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
  "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
  "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
  "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
  "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
  "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
  "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
  "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
  "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
  "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
  "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
  "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
  "Hard Rock",
	
  /* expanded codes */
  "Folk", "Folk/Rock", "National folk", "Swing", "Fast-fusion", "Bebob",
  "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
  "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
  "Big Band", "Chorus","Easy Listening", "Acoustic", "Humour", "Speech",
  "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
  "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
  "Folklore", "Ballad", "Powder Ballad", "Rhythmic Soul", "Freestyle", "Duet",
  "Punk Rock", "Drum Solo", "A Capella", "Euro-House", "Dance Hall",

  /* additional codes */
  "Goa", "Drum & Bass", "Club House", "Hardcore", "Terror", "Indie",
  "BritPop", "NegerPunk", "Polsk Punk", "Beat", "Christian Gangsta",
  "Heavy Metal", "Black Metal", "Crossover", "Contemporary Crap",
  "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
  "SynthPop",

  "Unknown"
};

const int genre_count = ((int)(sizeof(genre_table)/sizeof(char*))-1);
