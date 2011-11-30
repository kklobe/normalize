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

#ifndef _ID3_H_
#define _ID3_H_

typedef struct id3_struct *id3_t;
typedef struct id3_frame_struct *id3_frame_t;

enum {
  ID3_RDONLY,
  ID3_RDWR
};

/* text encoding codes */
enum id3_text_encoding {
  ID3_TEXT_ISO      = 0,
  ID3_TEXT_UTF16    = 1,
  ID3_TEXT_UTF16BE  = 2,
  ID3_TEXT_UTF8     = 3
};

/* tag version codes */
enum id3_version {
  ID3_VERSION_NONE,
  ID3_VERSION_1,   /* ID3v1 not implemented */
  ID3_VERSION_2_2,
  ID3_VERSION_2_3,
  ID3_VERSION_2_4
};

/* padding policy codes */
enum id3_pad_policy {
  /* use existing padding, double tag size if necessary */
  ID3_PADDING_DEFAULT,
  /* remove all padding */
  ID3_PADDING_NONE,
  /* user sets exact size of tag */
  ID3_PADDING_CUSTOM
};

/* frame flags */
enum id3_fflag {
  ID3_FFLAG_TAGALTER_PRESERVE  = 0,
  ID3_FFLAG_FILEALTER_PRESERVE = 1,
  ID3_FFLAG_READONLY           = 2,
  ID3_FFLAG_HAS_GROUPID        = 3,
  ID3_FFLAG_IS_COMPRESSED      = 4,
  ID3_FFLAG_IS_ENCRYPTED       = 5,
  ID3_FFLAG_IS_UNSYNCED        = 6,
  ID3_FFLAG_HAS_DATALEN        = 7
};

/*
 * frame-specific flags
 */

/* for RVA2 */
enum id3_rva_channel {
  ID3_CHANNEL_OTHER   = 0x00,
  ID3_CHANNEL_MASTER  = 0x01, /* master volume */
  ID3_CHANNEL_FRIGHT  = 0x02, /* front right */
  ID3_CHANNEL_FLEFT   = 0x03, /* front left */
  ID3_CHANNEL_BRIGHT  = 0x04, /* back right */
  ID3_CHANNEL_BLEFT   = 0x05, /* back left */
  ID3_CHANNEL_FCENTER = 0x06, /* front center */
  ID3_CHANNEL_BCENTER = 0x07, /* back center */
  ID3_CHANNEL_SUB     = 0x08, /* subwoofer */

  ID3_CHANNEL_ANY     = 0xFF
};

struct id3_rva_t {
  enum id3_rva_channel channel;
  float adjustment;
  unsigned char peak_bits;
  unsigned char peak[1];
};


/*
 * Function prototypes
 */
id3_t id3_open(const char *fname, int mode);
int id3_close(id3_t id3);
int id3_get_size(id3_t id3);
void id3_set_size(id3_t id3, int size);
int id3_set_version(id3_t tag, enum id3_version ver);
enum id3_version id3_get_version(id3_t tag);
void id3_set_pad_policy(id3_t tag, enum id3_pad_policy policy);
enum id3_pad_policy id3_get_pad_policy(id3_t tag);
int id3_frame_count(id3_t id3);
id3_frame_t id3_get_frame(id3_t id3, int index);
id3_frame_t id3_get_frame_by_id(id3_t tag, const char *id);
int id3_set_append(id3_t tag, int append);
int id3_set_unsync(id3_t tag, int unsync);

int id3_frame_get_size(id3_frame_t f);
char *id3_frame_get_id(id3_frame_t f);
int id3_frame_set_id(id3_frame_t f, const char *id);
void *id3_frame_get_raw(id3_frame_t f);
int id3_frame_set_raw(id3_frame_t f, void *buf, int size);
int id3_frame_get_flag(id3_frame_t f, enum id3_fflag flg);
void id3_frame_set_flag(id3_frame_t f, enum id3_fflag flg);
void id3_frame_clear_flag(id3_frame_t f, enum id3_fflag flg);
void id3_frame_delete(id3_frame_t f);
id3_frame_t id3_frame_add(id3_t id3, const char *id);

int id3_frame_text_enc(id3_frame_t f);
char *id3_frame_text(id3_frame_t f);
int id3_frame_save_image(id3_frame_t f, const char *fname);

int id3_write(id3_t id3);
void id3_strip(id3_t id3);

id3_frame_t id3_add_text_frame(id3_t id3, const char *id, const char *text, int encoding);

char *id3_title_get(id3_t tag);
int id3_title_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_artist_get(id3_t tag);
int id3_artist_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_album_get(id3_t tag);
int id3_album_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_genre_get(id3_t tag);
int id3_genre_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_date_get(id3_t tag);
int id3_date_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_tracknum_get(id3_t tag);
int id3_tracknum_set(id3_t tag, const char *s, enum id3_text_encoding enc);
char *id3_comment_get(id3_t tag, const char *desc, const char *lang);
int id3_comment_set(id3_t tag, const char *text, const char *desc,
		    const char *lang, enum id3_text_encoding enc);
float id3_rva_get(id3_t tag, const char *ident, enum id3_rva_channel channel);
int id3_rva_set(id3_t tag, const char *ident,
		enum id3_rva_channel channel, float adjust);

const char *id3_id_description(const char *id);
const char *id3_genre_name(int genre);
int id3_genre_number(const char *desc);

#endif
