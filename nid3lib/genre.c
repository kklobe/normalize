/* Copyright (C) 2001--2005 Chris Vaill
   This file is part of nid3lib.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

/*
 * ID3v1 genre number <--> name mapping
 */

#define _POSIX_C_SOURCE 2

#include "config.h"

#if STDC_HEADERS
# include <stdlib.h>
#else
# define NULL (void *)0
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif

#define MAX_GENRE_NUM 147

static const char *_genre_map[] = {
  "Blues", /* 00 */
  "Classic Rock", /* 01 */
  "Country", /* 02 */
  "Dance", /* 03 */
  "Disco", /* 04 */
  "Funk", /* 05 */
  "Grunge", /* 06 */
  "Hip-Hop", /* 07 */
  "Jazz", /* 08 */
  "Metal", /* 09 */
  "New Age", /* 10 */
  "Oldies", /* 11 */
  "Other", /* 12 */
  "Pop", /* 13 */
  "R&B", /* 14 */
  "Rap", /* 15 */
  "Reggae", /* 16 */
  "Rock", /* 17 */
  "Techno", /* 18 */
  "Industrial", /* 19 */
  "Alternative", /* 20 */
  "Ska", /* 21 */
  "Death Metal", /* 22 */
  "Pranks", /* 23 */
  "Soundtrack", /* 24 */
  "Euro-Techno", /* 25 */
  "Ambient", /* 26 */
  "Trip-Hop", /* 27 */
  "Vocal", /* 28 */
  "Jazz+Funk", /* 29 */
  "Fusion", /* 30 */
  "Trance", /* 31 */
  "Classical", /* 32 */
  "Instrumental", /* 33 */
  "Acid", /* 34 */
  "House", /* 35 */
  "Game", /* 36 */
  "Sound Clip", /* 37 */
  "Gospel", /* 38 */
  "Noise", /* 39 */
  "Alternative Rock", /* 40 */
  "Bass", /* 41 */
  "Soul", /* 42 */
  "Punk", /* 43 */
  "Space", /* 44 */
  "Meditative", /* 45 */
  "Instrumental Pop", /* 46 */
  "Instrumental Rock", /* 47 */
  "Ethnic", /* 48 */
  "Gothic", /* 49 */
  "Darkwave", /* 50 */
  "Techno-Industrial", /* 51 */
  "Electronic", /* 52 */
  "Pop-Folk", /* 53 */
  "Eurodance", /* 54 */
  "Dream", /* 55 */
  "Southern Rock", /* 56 */
  "Comedy", /* 57 */
  "Cult", /* 58 */
  "Gangsta", /* 59 */
  "Top 40", /* 60 */
  "Christian Rap", /* 61 */
  "Pop/Funk", /* 62 */
  "Jungle", /* 63 */
  "Native US", /* 64 */
  "Cabaret", /* 65 */
  "New Wave", /* 66 */
  "Psychedelic", /* 67 */
  "Rave", /* 68 */
  "Showtunes", /* 69 */
  "Trailer", /* 70 */
  "Lo-Fi", /* 71 */
  "Tribal", /* 72 */
  "Acid Punk", /* 73 */
  "Acid Jazz", /* 74 */
  "Polka", /* 75 */
  "Retro", /* 76 */
  "Musical", /* 77 */
  "Rock & Roll", /* 78 */
  "Hard Rock", /* 79 */
  "Folk", /* 80 */
  "Folk-Rock", /* 81 */
  "National Folk", /* 82 */
  "Swing", /* 83 */
  "Fast Fusion", /* 84 */
  "Bebob", /* 85 */
  "Latin", /* 86 */
  "Revival", /* 87 */
  "Celtic", /* 88 */
  "Bluegrass", /* 89 */
  "Avant-garde", /* 90 */
  "Gothic Rock", /* 91 */
  "Progressive Rock", /* 92 */
  "Psychedelic Rock", /* 93 */
  "Symphonic Rock", /* 94 */
  "Slow Rock", /* 95 */
  "Big Band", /* 96 */
  "Chorus", /* 97 */
  "Easy Listening", /* 98 */
  "Acoustic ", /* 99 */
  "Humour", /* 100 */
  "Speech", /* 101 */
  "Chanson", /* 102 */
  "Opera", /* 103 */
  "Chamber Music", /* 104 */
  "Sonata", /* 105 */
  "Symphony", /* 106 */
  "Booty Bass", /* 107 */
  "Primus", /* 108 */
  "Porn Groove", /* 109 */
  "Satire", /* 110 */
  "Slow Jam", /* 111 */
  "Club", /* 112 */
  "Tango", /* 113 */
  "Samba", /* 114 */
  "Folklore", /* 115 */
  "Ballad", /* 116 */
  "Power Ballad", /* 117 */
  "Rhythmic Soul", /* 118 */
  "Freestyle", /* 119 */
  "Duet", /* 120 */
  "Punk Rock", /* 121 */
  "Drum Solo", /* 122 */
  "A Cappella", /* 123 */
  "Euro-House", /* 124 */
  "Dance Hall", /* 125 */
  "Goa", /* 126 */
  "Drum & Bass", /* 127 */
  "Club-House", /* 128 */
  "Hardcore", /* 129 */
  "Terror", /* 130 */
  "Indie", /* 131 */
  "BritPop", /* 132 */
  "Negerpunk", /* 133 */
  "Polsk Punk", /* 134 */
  "Beat", /* 135 */
  "Christian Gangsta", /* 136 */
  "Heavy Metal", /* 137 */
  "Black Metal", /* 138 */
  "Crossover", /* 139 */
  "Contemporary Christian", /* 140 */
  "Christian Rock", /* 141 */
  "Merengue", /* 142 */
  "Salsa", /* 143 */
  "Thrash Metal", /* 144 */
  "Anime", /* 145 */
  "JPop", /* 146 */
  "SynthPop", /* 147 */
  NULL
};

/* returns non-zero if a and b are equivalent strings, ignoring case */
static int
xstrcaseeq(const char *a, const char *b)
{
  while (*a && *b) {
    if (tolower(*a) != tolower(*b))
      return 0;
    a++; b++;
  }
  return 1;
}

/**
 * Gets a string description of a numerical ID3v1 genre.
 *
 * @param genre the numerical genre
 *
 * @return a pointer to static memory containing a string description
 *         of the genre.
 */
const char *
id3_genre_name(int genre)
{
  if (genre < 0 || genre > MAX_GENRE_NUM)
    return NULL;
  return _genre_map[genre];
}

/**
 * Finds the numerical ID3v1 genre associated with the given description.
 *
 * @param desc the string description of the genre, e.g. "Hardcore".
 *        Case is not significant.
 *
 * @return the numerical ID3v1 genre.  255 is returned if no match is
 *         found.
 */
int
id3_genre_number(const char *desc)
{
  int i;
  for (i = 0; _genre_map[i]; i++)
    if (xstrcaseeq(desc, _genre_map[i]))
      return i;
  return 255;
}
