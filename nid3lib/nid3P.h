/* Copyright (C) 2002--2005 Chris Vaill
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

/* config.h must be included before this */

#ifndef _ID3P_H_
#define _ID3P_H_

#include <stdio.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "nid3.h"

struct id3v1_struct {
  unsigned int exists:1;    /* does it exist on disk? */
  unsigned int requested:1; /* should we render v1 tag when we write? */
  char title[31];
  char artist[31];
  char album[31];
  char year[5];
  char comment[31];
  unsigned char track;
  char trackstr[4]; /* track number, as a string */
  unsigned char genre;
  char genrestr[4]; /* genre number, as a string */
};

struct id3_struct {
  FILE *fp;
  off_t offset;   /* offset of beginning of tag header in file */
  off_t curr_off; /* track current offset, for unseekable streams */
  char *fname;
  unsigned char version;
  unsigned char revision;
  int tagsz;
  int nframes;
  int mode;
  enum id3_pad_policy pad_policy;
  int requested_sz;     /* for custom padding */
  id3_frame_t frame_hd;
  id3_frame_t frame_tl;

  /* flags */
  unsigned int unsync:1;
  unsigned int has_ext_hdr:1;
  unsigned int experimental:1;
  unsigned int has_footer:1;

  /* extended header fields */
  unsigned int is_update:1;
  unsigned int has_crc:1;
  unsigned int has_restrict:1;

  unsigned int seekable:1; /* does fseek() work on the fp field? */
  unsigned int append:1;   /* does the tag go at the end of the file? */
  unsigned int append_req:1; /* should we append when we write? */

  /* ID3v1 tag */
  struct id3v1_struct v1;
};

struct id3_frame_struct {
  char id[5]; /* NUL terminated string */
  int sz; /* size of the field, as reported in the header */
  unsigned short flags;
  unsigned char groupid; /* group identifier, if present */
  int datalen; /* size of data, from the data length indicator, if present */

  unsigned char *data;
  char *curr_txt; /* pointer to the current text field, or NULL */
  off_t offset; /* file offset of first byte after header */
  id3_t id3;
  struct id3_frame_struct *next;
};

#define _frame_tagalter_preserve(f) \
  id3_frame_get_flag((f), ID3_FFLAG_TAGALTER_PRESERVE)
#define _frame_filealter_preserve(f) \
  id3_frame_get_flag((f), ID3_FFLAG_FILEALTER_PRESERVE)
#define _frame_is_readonly(f) \
  id3_frame_get_flag((f), ID3_FFLAG_IS_READONLY)
#define _frame_has_groupid(f) \
  id3_frame_get_flag((f), ID3_FFLAG_HAS_GROUPID)
#define _frame_is_compressed(f) \
  id3_frame_get_flag((f), ID3_FFLAG_IS_COMPRESSED)
#define _frame_is_encrypted(f) \
  id3_frame_get_flag((f), ID3_FFLAG_IS_ENCRYPTED)
#define _frame_is_unsynced(f) \
  id3_frame_get_flag((f), ID3_FFLAG_IS_UNSYNCED)
#define _frame_has_datalen(f) \
  id3_frame_get_flag((f), ID3_FFLAG_HAS_DATALEN)

#ifndef O_BINARY
# define O_BINARY 0
#endif

/*
 * prototypes for internal functions
 */
id3_frame_t _id3_frame_new(void);
void _id3_frame_destroy(id3_frame_t f);
void _id3_frame_add(id3_t id3, id3_frame_t f);

#endif
