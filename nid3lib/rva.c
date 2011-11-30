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
 * Relative volume adjust (RVA2) frame manipulation routines
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
_rva_id(id3_t tag)
{
  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    /* XRV is only an internal frame ID.  We don't write v2.2 tags, so
       we never write an XRV frame. */
    return "XRV";
  case ID3_VERSION_2_3:
    /* There's no RVA2 for v2.3, and RVAD is useless, so we use XRVA */
    return "XRVA";
  case ID3_VERSION_2_4:
    return "RVA2";
  default:
    ;
  }
  return NULL;
}


/*
 * Finds a relative volume adjust frame in the given tag.
 *
 * @param tag the tag
 *
 * @param ident the identification string associated with the volume
 *        adjust frame.  If this is NULL, the first relative volume
 *        adjust frame found will be returned.
 *
 * @return the relative volume adjust frame, or NULL if not found.
 */
/* FIXME: should be externally visible? */
static id3_frame_t
id3_rva_get_frame(id3_t tag, const char *ident)
{
  const char *id = _rva_id(tag);
  char *s;
  id3_frame_t f;
  int nframes;

  nframes = id3_frame_count(tag); /* make sure headers are read */
  if (nframes == -1)
    return NULL;
  for (f = tag->frame_hd; f; f = f->next) {
    if (strcmp(f->id, id) == 0) {
      /* Frame ID matches, now check the ident string */
      s = id3_frame_get_raw(f);
      if (s == NULL)
	continue;
      if (ident && strncmp(ident, s, f->sz) != 0)
	continue; /* content description doesn't match */
      return f;
    }
  }

  return NULL;
}


/*
 * Gets the relative volume adjust information from an rva frame.
 *
 * @param f the rva frame
 *
 * @param channel the channel the adjustment is associated with.
 *
 * @return the volume adjustment, in decibels
 */
/* FIXME: should be externally visible? */
static float
id3_rva_get_adjust(id3_frame_t f, enum id3_rva_channel channel)
{
  unsigned char *data;
  float adj = 0.0;
  int i, peakbytes, adj_fp;

  data = id3_frame_get_raw(f);

  /* skip identification string */
  for (i = 0; i < f->sz; i++)
    if (data[i] == '\0')
      break;
  if (data[i] != '\0')
    return 0.0; /* ident string not terminated; bad frame data */

  /* cycle through fields */
  i++;
  while (i + 3 < f->sz) {

    if (data[i] == (unsigned char)channel) {
      /* we found a field for the requested channel; decode it */
      adj_fp = *(signed char *)(data+i+1) << 8; /* first byte of adjustment */
      adj_fp |= *(unsigned char *)(data+i+2);   /* second byte of adjustment */
      adj = adj_fp / 512.0;
      break;
    }

    /* this field doesn't match the requested channel; skip it */
    i += 3;
    peakbytes = (data[i] + 7) / 8;
    i += 1 + peakbytes;
  }

  return adj;
}


/**
 * Gets the relative volume adjust information from an rva frame.
 *
 * @param tag the tag
 *
 * @param ident the identification string associated with the volume
 *        adjust frame.  If this is NULL, the first relative volume
 *        adjust frame found will be used.
 *
 * @param channel the channel the adjustment is associated with.
 *
 * @return the volume adjustment, in decibels.  If no matching
 *         adjustment is found, 0.0 (i.e. no adjustment) is returned.
 */
float
id3_rva_get(id3_t tag, const char *ident, enum id3_rva_channel channel)
{
  id3_frame_t f = id3_rva_get_frame(tag, ident);
  float adj = 0.0;

  if (f)
    adj = id3_rva_get_adjust(f, channel);

  return adj;
}


/**
 * Sets the relative volume adjust information in a tag.
 * Note that for ID3v2.2 and and ID3v2.3 tags, there is no useful
 * native volume adjust frame like the RVA2 frame in v2.4.  Therefore,
 * we use experimental frame ID's XRV and XRVA for v2.2 and v2.3 tags,
 * respectively.  These frames have exactly the same format as the
 * v2.4 RVA2 frame, and will be converted to RVA2 frames if the tag is
 * converted to version 2.4.
 *
 * @param tag the tag
 *
 * @param ident the identification string associated with the volume
 *        adjust frame.  If this is NULL, the first relative volume
 *        adjust frame found will be used.
 *
 * @param channel the channel the adjustment is associated with.
 *
 * @param adjust the volume adjustment, in decibels.
 *
 * @return 0, or -1 on error
 */
int
id3_rva_set(id3_t tag, const char *ident,
	    enum id3_rva_channel channel, float adjust)
{
  const char *id;
  unsigned char *data;
  id3_frame_t f = id3_rva_get_frame(tag, ident);
  int i, idlen, peakbytes, adjust_fp;

  idlen = strlen(ident);

  if (f == NULL) {

    /*
     * No relative volume adjust tag with the same ident string
     * exists, so we make a new one.
     */
    id = _rva_id(tag);
    if (ident == NULL)
      ident = "";
    f = _id3_frame_new();
    if (f == NULL)
      return -1;
    f->sz = idlen + 1 + 4; /* space for ident string + nul + one field */
    f->data = (unsigned char *)malloc(f->sz);
    if (f->data == NULL)
      return -1;
    _id3_frame_add(tag, f);

    f->id3 = tag;
    strncpy(f->id, id, 4);
    strcpy((char *)f->data, ident);
    /* we later write the field starting at f->data + i */
    i = idlen + 1;

  } else {

    /*
     * If a relative volume adjust tag with the same ident string
     * already exists, we use that frame, since two rva frames with
     * the same ident string are not allowed.
     */
    data = id3_frame_get_raw(f);

    /* find end of ident string */
    for (i = 0; i < f->sz; i++)
      if (data[i] == '\0')
	break;
    if (data[i] != '\0')
      return -1;

    /* find matching field */
    while (1) {
      i++;
      if (i >= f->sz || data[i] == (unsigned char)channel)
	break;

      /* skip peak info */
      i += 3;
      if (i >= f->sz) {
	/* frame is corrupt; overwrite this field */
	i -= 3;
	break;
      }
      peakbytes = data[i] / 8;
      i += peakbytes;
      if (i >= f->sz) {
	/* frame is corrupt; overwrite this field */
	i -= peakbytes + 3;
	break;
      }
    }

    if (i + 3 >= f->sz) {
      /* no matching field was found, so add one */
      f->data = (unsigned char *)realloc(f->data, i + 3); /* add 1 field */
      if (f->data == NULL) {
	f->data = data;
	return -1;
      }
      f->sz = i + 3;
    }
  }

  /*
   * write the new field at index i
   */
  /* set channel */
  f->data[i] = (unsigned char)channel;
  /* set adjustment bytes */
  if (adjust < 0.0) /* split by sign so we don't need floor() and libm */
    adjust_fp = (int)(adjust * 512.0 - 0.5);
  else
    adjust_fp = (int)(adjust * 512.0 + 0.5);
  f->data[i + 1] = (adjust_fp >> 8) & 0xFF; /* first byte of adjustment */
  f->data[i + 2] = adjust_fp & 0xFF;        /* second byte of adjustment */
  /* set (empty) peak field */
  f->data[i + 3] = 0x00;

  return 0;
}
