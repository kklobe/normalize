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
 * comment manipulation routines
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
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#include "nid3P.h"

static const char *
_comment_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    return "COM";
  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    return "COMM";
  default:
    ;
  }
  return NULL;
}

/* returns nonzero iff a and b hold equivalent strings in encoding enc. */
static int
_desc_eq(const char *a, const char *b, int size, enum id3_text_encoding enc)
{
  int i = 0;
  switch (enc) {
    case ID3_TEXT_UTF16:
    case ID3_TEXT_UTF16BE:
      while (i < size) {
	if (a[i] != b[i] || a[i+1] != b[i+1])
	  return 0;
	if (a[i] == '\0' && a[i+1] == '\0')
	  break;
	i += 2;
      }
      break;
    case ID3_TEXT_UTF8:
    case ID3_TEXT_ISO:
      while (i < size) {
	if (a[i] != b[i])
	  return 0;
	if (a[i] == '\0')
	  break;
	i++;
      }
      break;
  }
  return 1;
}

/* extracts the comment text from the comment frame's data block */
static char *
_get_comment_text(id3_frame_t f)
{
  unsigned char *s = id3_frame_get_raw(f);

  if (s == NULL)
    return NULL;
  switch (s[0]) {
    case ID3_TEXT_UTF16:
    case ID3_TEXT_UTF16BE:
      s += 4;
      while ((s - f->data) <= f->sz && !(s[0] == '\0' && s[1] == '\0'))
	s += 2;
      if (!(s[0] == '\0' && s[1] == '\0'))
	return NULL;
      s += 2;
      break;
    case ID3_TEXT_UTF8:
    case ID3_TEXT_ISO:
      s += 4;
      while ((s - f->data) <= f->sz && s[0] != '\0')
	s++;
      if (s[0] != '\0')
	return NULL;
      s++;
      break;
  }
  return (char *)s;
}

/*
 * Find a comment frame with the given description string and
 * language.  If desc or lang is NULL, it is ignored as a matching
 * criterion.
 */
static id3_frame_t
_get_comment_frame(id3_t tag, const char *desc, const char *lang)
{
  const char *id = _comment_id(tag);
  char *s;
  id3_frame_t f;
  int nframes;

  nframes = id3_frame_count(tag); /* make sure headers are read */
  if (nframes == -1)
    return NULL;
  for (f = tag->frame_hd; f; f = f->next) {
    if (strcmp(f->id, id) == 0) {
      s = id3_frame_get_raw(f);
      if (s == NULL)
	continue;
      if (desc && !_desc_eq(desc, s + 4, f->sz - 4, s[0]))
	continue; /* content description doesn't match */
      if (lang && memcmp(lang, s + 1, 3) != 0)
	continue; /* language doesn't match */
      return f;
    }
  }

  return NULL;
}

char *
id3_comment_get(id3_t tag, const char *desc, const char *lang)
{
  char *s = NULL;
  id3_frame_t f = _get_comment_frame(tag, desc, lang);

  if (f)
    s = _get_comment_text(f);
  else if (tag->v1.exists && tag->v1.comment[0] != '\0')
    s = tag->v1.comment;

  return s;
}

int
id3_comment_set(id3_t tag, const char *text, const char *desc,
		const char *lang, enum id3_text_encoding enc)
{
  id3_frame_t f;
  unsigned char *data;
  const char *id;
  int desclen, textoffset, textlen, sz;

  if (desc == NULL)
    desc = "\0\0";
  if (lang == NULL)
    lang = "XXX";

  /* calculate size of new frame */
  desclen = textlen = 0;
  switch (enc) {
  case ID3_TEXT_UTF16:
  case ID3_TEXT_UTF16BE:
    while (!(desc[desclen] == '\0' && desc[desclen+1] == '\0'))
      desclen += 2;
    while (!(text[textlen] == '\0' && text[textlen+1] == '\0'))
      textlen += 2;
    textoffset = 4 + desclen + 2;
    sz = 4 + desclen + 2 + textlen + 2;
    break;
  case ID3_TEXT_UTF8:
  case ID3_TEXT_ISO:
    while (desc[desclen] != '\0')
      desclen++;
    while (text[textlen] != '\0')
      textlen++;
    textoffset = 4 + desclen + 1;
    sz = 4 + desclen + 1 + textlen + 2;
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  /* set up the frame */
  f = _get_comment_frame(tag, desc, lang);
  if (f) {
    if (f->data) {
      if (f->sz < sz - 2) {
	data = f->data;
	f->data = (unsigned char *)calloc(sz, 1);
	if (f->data == NULL) {
	  f->data = data;
	  return -1;
	}
      } else {
	memset(f->data, 0, f->sz);
      }
    } else {
      f->data = (unsigned char *)calloc(sz, 1);
      if (f->data == NULL)
	return -1;
    }
  } else {
    id = _comment_id(tag);
    f = id3_frame_add(tag, id);
    if (f == NULL)
      return -1;
    f->data = (unsigned char *)calloc(sz, 1);
    if (f->data == NULL)
      return -1;
  }

  f->sz = sz - 2;
  f->data[0] = (unsigned char)enc;
  memcpy(f->data + 1, lang, 3);
  memcpy(f->data + 4, desc, desclen);
  memcpy(f->data + textoffset, text, textlen);

  if (enc == ID3_TEXT_ISO)
    strncpy(tag->v1.comment, text, 30);

  return 0;
}
