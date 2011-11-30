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
 * Text frame (i.e. 'T???') manipulation routines
 */

#define _POSIX_C_SOURCE 2

#include "config.h"

#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STRING_H
#  include <string.h>
# else
#  ifndef HAVE_MEMCPY
#   define memcpy(d,s,n) bcopy((s),(d),(n))
#   define memmove(d,s,n) bcopy((s),(d),(n))
#  endif
# endif
#endif

#include "nid3P.h"


int
id3_frame_text_enc(id3_frame_t f)
{
  unsigned char *buf = id3_frame_get_raw(f);

  if (buf) {
    switch (buf[0]) {
    case ID3_TEXT_ISO:
    case ID3_TEXT_UTF16:
    case ID3_TEXT_UTF16BE:
    case ID3_TEXT_UTF8:
      return (int)buf[0];
    }
  }

  return -1;
}

/*
 * Returns each text string in turn, NULL when there are no more.
 */
char *
id3_frame_text(id3_frame_t f)
{
  unsigned char *buf = id3_frame_get_raw(f);
  char *retval = NULL;
  int i;

  if (buf == NULL)
    return NULL;

  return (char *)buf + 1;

  retval = f->curr_txt;

  /* advance the f->curr_txt pointer */
  if (f->curr_txt == NULL) {
    f->curr_txt = (char *)buf + 1; /* start over */
  } else {
    i = f->curr_txt - (char *)buf;
    switch (buf[0]) {
    case ID3_TEXT_UTF16:
    case ID3_TEXT_UTF16BE:
      while (i <= f->sz && !(buf[i] == '\0' && buf[i+1] == '\0'))
	i += 2;
      i += 2;
      break;
    case ID3_TEXT_UTF8:
    case ID3_TEXT_ISO:
      while (i <= f->sz && buf[i] != '\0')
	i++;
      i++;
      break;
    }
    if (i > f->sz)
      f->curr_txt = NULL;
    else
      f->curr_txt = (char *)buf + i;
  }

  return retval;
}

/* FIXME: add ability to manipulate individual fields */
id3_frame_t
id3_add_text_frame(id3_t tag, const char *id, const char *text, int encoding)
{
  id3_frame_t fr;
  int len;

  fr = id3_frame_add(tag, id);
  if (fr == NULL)
    return NULL;

  len = 0;
  switch (encoding) {
  case ID3_TEXT_UTF16:
  case ID3_TEXT_UTF16BE:
    while (!(text[len] == '\0' && text[len+1] == '\0'))
      len += 2;
    break;
  case ID3_TEXT_UTF8:
  case ID3_TEXT_ISO:
    while (text[len] != '\0')
      len++;
    break;
  default:
    _id3_frame_destroy(fr);
    return NULL;
  }

  if (fr->data)
    free(fr->data);
  fr->sz = len + 1; /* +1 for encoding byte */
  fr->data = (unsigned char *)calloc(fr->sz + 2, 1); /* +2 for nul chars */
  if (fr->data == NULL) {
    _id3_frame_destroy(fr);
    return NULL;
  }
  fr->data[0] = encoding;
  memcpy(fr->data + 1, text, len);

  return fr;
}
