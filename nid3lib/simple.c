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
 * Some simple, high-level tag manipulation routines for common frames
 */

#define _POSIX_C_SOURCE 2

#include "config.h"

#include <stdio.h>
#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STRING_H
#  include <string.h>
# endif
#endif

#include "nid3P.h"

static const char *_title_id(id3_t);
static const char *_artist_id(id3_t);
static const char *_album_id(id3_t);
static const char *_genre_id(id3_t);
static const char *_date_id(id3_t);
static const char *_tracknum_id(id3_t);
static char *_do_get(id3_t, const char *);
static int _do_set(id3_t, const char *, const char *, enum id3_text_encoding);


/*
 * version-independent title, artist, album, genre, and tracknumber
 * get/set routines
 */

char *
id3_title_get(id3_t tag)
{
  char *s = _do_get(tag, _title_id(tag));
  if (s == NULL && tag->v1.exists && tag->v1.title[0] != '\0')
    s = tag->v1.title;
  return s;
}

int
id3_title_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _title_id(tag), s, enc);
  if (enc == ID3_TEXT_ISO)
    strncpy(tag->v1.title, s, 30);
  return ret;
}

char *
id3_artist_get(id3_t tag)
{
  char *s = _do_get(tag, _artist_id(tag));
  if (s == NULL && tag->v1.exists && tag->v1.artist[0] != '\0')
    s = tag->v1.artist;
  return s;
}

int
id3_artist_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _artist_id(tag), s, enc);
  if (enc == ID3_TEXT_ISO)
    strncpy(tag->v1.artist, s, 30);
  return ret;
}

char *
id3_album_get(id3_t tag)
{
  char *s = _do_get(tag, _album_id(tag));
  if (s == NULL && tag->v1.exists && tag->v1.album[0] != '\0')
    s = tag->v1.album;
  return s;
}

int
id3_album_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _album_id(tag), s, enc);
  if (enc == ID3_TEXT_ISO)
    strncpy(tag->v1.album, s, 30);
  return ret;
}

char *
id3_genre_get(id3_t tag)
{
  char *s = _do_get(tag, _genre_id(tag));
  if (s == NULL && tag->v1.exists && tag->v1.genre != 0xFF) {
    sprintf(tag->v1.genrestr, "%d", (int)tag->v1.genre);
    s = tag->v1.genrestr;
  }
  return s;
}

int
id3_genre_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _genre_id(tag), s, enc);
  int is_numeric = 0;
  const char *p;
  if (enc == ID3_TEXT_ISO) {
    /* if desc is "number" or "(number)", set to number */
    if (s[0] >= '0' && s[0] <= '9') {
      is_numeric = 1;
      tag->v1.genre = atoi(s) & 0xFF;
    } else if (s[0] == '(') {
      for (p = s; *p && *p != ')'; p++)
	  if (*p < '0' || *p > '9')
	    break;
      if (*p == ')') {
	is_numeric = 1;
	tag->v1.genre = atoi(s + 1) & 0xFF;
      }
    }
    /* otherwise, check if it matches a known v1 genre */
    if (!is_numeric)
      tag->v1.genre = id3_genre_number(s);
  } else {
    tag->v1.genre = 255;
  }
  return ret;
}

char *
id3_date_get(id3_t tag)
{
  char *s = _do_get(tag, _date_id(tag));
  if (s == NULL && tag->v1.exists && tag->v1.year[0] != '\0')
    s = tag->v1.year;
  return s;
}

int
id3_date_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _date_id(tag), s, enc);
  if (enc == ID3_TEXT_ISO)
    strncpy(tag->v1.year, s, 4);
  return ret;
}

char *
id3_tracknum_get(id3_t tag)
{
  char *s = _do_get(tag, _tracknum_id(tag));
  if (s == NULL && tag->v1.exists) {
    if (tag->v1.track != 0) {
      sprintf(tag->v1.trackstr, "%d", (int)tag->v1.track);
      s = tag->v1.trackstr;
    }
  }
  return s;
}

int
id3_tracknum_set(id3_t tag, const char *s, enum id3_text_encoding enc)
{
  int ret = _do_set(tag, _tracknum_id(tag), s, enc);
  tag->v1.track = atoi(s) & 0xFF;
  return ret;
}


/*
 * Utility functions
 */

static const char *
_title_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TT2";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "TIT2";
  default:
    ;
  }
  return NULL;
}

static const char *
_artist_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TP1";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "TPE1";
  default:
    ;
  }
  return NULL;
}

static const char *
_album_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TAL";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "TALB";
  default:
    ;
  }
  return NULL;
}

static const char *
_genre_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TCO";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "TCON";
  default:
    ;
  }
  return NULL;
}

static const char *
_date_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TYE";
  case ID3_VERSION_2_3:
    return "TYER";
  case ID3_VERSION_2_4:
    return "TDRC";
  default:
    ;
  }
  return NULL;
}

static const char *
_tracknum_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "TRK";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "TRCK";
  default:
    ;
  }
  return NULL;
}

static char *
_do_get(id3_t tag, const char *id)
{
  id3_frame_t f;
  char *s;

  /* make sure we've read the headers */
  id3_frame_count(tag);

  if (id) {
    f = id3_get_frame_by_id(tag, id);
    if (f && (s = id3_frame_get_raw(f)))
      return s + 1;
  }
  return NULL;
}

static int
_do_set(id3_t tag, const char *id, const char *s, enum id3_text_encoding enc)
{
  if (id == NULL)
    return -1;
  if (id3_add_text_frame(tag, id, s, enc) == NULL)
    return -1;
  return 0;
}
