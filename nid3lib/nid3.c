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
 * General tag and frame manipulation routines
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
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_BYTESWAP_H
# include <byteswap.h>
#else
# define bswap_16(x) \
    ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
# define bswap_32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |       \
     (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
#endif /* HAVE_BYTESWAP_H */

#include "nid3P.h"

static unsigned short v3_fflag_masks[] = {
  0x8000, 0x4000, 0x2000, 0x0020, 0x0080, 0x0040, 0x0000, 0x0000
};

static unsigned short v4_fflag_masks[] = {
  0x4000, 0x2000, 0x1000, 0x0040, 0x0008, 0x0004, 0x0002, 0x0001
};

static inline unsigned int
get_be_int(unsigned char *buf)
{
  unsigned int retval;
  retval = buf[0];
  retval <<= 8;
  retval += buf[1];
  retval <<= 8;
  retval += buf[2];
  retval <<= 8;
  retval += buf[3];
  return retval;
}

/* return the integer corresponding to the syncsafe encoding starting at buf */
static unsigned int
unsyncsafe_int(unsigned char *buf)
{
  unsigned int retval = 0;
  retval = buf[0];
  retval <<= 7;
  retval |= buf[1];
  retval <<= 7;
  retval |= buf[2];
  retval <<= 7;
  retval |= buf[3];
  return retval;
}

/*
 * Copy sz bytes of src to dest, de-unsyncing along the way.
 * Return value is number of bytes written to dest.
 */
static int
decode_unsync(unsigned char *dest, unsigned char *src, int sz)
{
  unsigned char *dest_save = dest;

  while (sz > 0) {
    if (*src != 0xFF) {
      *dest++ = *src++;
      sz--;
    } else {
      *dest++ = *src++;
      sz--;
      if (sz <= 0)
	break;
      if (*src == 0x00)
	src++; /* remove 0x00 to de-unsync */
    }
  }

  return dest - dest_save;
}

id3_frame_t
_id3_frame_new(void)
{
  id3_frame_t f;

  f = (id3_frame_t)calloc(1, sizeof(struct id3_frame_struct));
  if (f == NULL)
    return NULL;
  f->offset = -1;

  return f;
}

void
_id3_frame_destroy(id3_frame_t f)
{
  if (f->data)
    free(f->data);
  free(f);
}

void
_id3_frame_add(id3_t tag, id3_frame_t f)
{
  f->next = NULL;
  if (tag->frame_tl == NULL) {
    tag->frame_hd = tag->frame_tl = f;
  } else {
    tag->frame_tl->next = f;
    tag->frame_tl = f;
  }
  tag->nframes++;
}


/**
 * Creates a new tag, and associates it with a file.
 *
 * @param fname the name of the file
 *
 * @param mode one of ID3_RDONLY or ID3_RDWR
 *
 * @return a handle to a new tag
 */
id3_t
id3_open(const char *fname, int mode)
{
  int fd, omode, err;
  char *fmode;
  id3_t newtag = NULL;

  /*
   * We want to create the file if it doesn't exist, and start at the
   * beginning of the file, but we don't want to truncate, hence all
   * the open/fdopen rigamarole.
   */
  switch (mode) {
  case ID3_RDONLY:
    omode = O_RDONLY | O_BINARY;
    fmode = "rb";
    break;
  case ID3_RDWR:
    omode = O_RDWR | O_CREAT | O_BINARY;
    fmode = "rb+";
    break;
  default:
    errno = EINVAL;
    return NULL;
  }
  fd = open(fname, omode, 0666);
  if (fd == -1)
    return NULL;

  newtag = (id3_t)calloc(1, sizeof(struct id3_struct));
  if (newtag == NULL)
    goto error_close;

  newtag->fp = fdopen(fd, fmode);
  if (newtag->fp == NULL)
    goto error_free;

  newtag->fname = (char *)malloc(strlen(fname) + 1);
  if (newtag->fname == NULL)
    goto error_fclose;
  strcpy(newtag->fname, fname);
  newtag->mode = mode;
  newtag->pad_policy = ID3_PADDING_DEFAULT;
  newtag->nframes = -1;
  newtag->tagsz = -1;
  newtag->seekable = 1;
  newtag->version = 3; /* write 2.3 tags by default */
  newtag->v1.requested = 1; /* also write ID3v1 tags by default */
  newtag->v1.genre = 255; /* initialize to "unknown" */

  return newtag;

 error_fclose:
  err = errno;
  fclose(newtag->fp);
  free(newtag);
  errno = err;
  return NULL;

 error_free:
  err = errno;
  free(newtag);
  errno = err;
 error_close:
  err = errno;
  close(fd);
  errno = err;
  return NULL;
}

/*
 * Frontend to fread that de-unsync's.
 * size and the return value are the number of de-unsynced bytes.
 * The number of bytes actually read from the stream (i.e. unsynced bytes)
 * is returned in consumed, unless consumed == NULL.
 * No more than consume_limit bytes will actually be read from the stream.
 */
static size_t
unsync_fread(void *buf, size_t size, FILE *stream,
	     size_t consume_limit, size_t *consumed)
{
  unsigned char *p = (unsigned char *)buf;
  unsigned char *p_save;
  size_t cons;
  int c;

  cons = 0;
  p_save = p;

  while (size > 0 && cons < consume_limit) {
    c = getc(stream);
    if (c == EOF)
      break;
    *p++ = c; size--; cons++;
    if (c == 0xFF) {
      c = getc(stream);
      if (c == EOF)
	break;
      if (c == 0x00 && cons < consume_limit) {
	/* drop 0x00 to de-unsync, but count the consumed byte */
	cons++;
      } else if (size > 0 && cons < consume_limit) {
	*p++ = c; size--; cons++;
      } else {
	/*
	 * We had to check that it wasn't a dropped byte, but it
	 * wasn't, and we can't hold another byte, so we put it back.
	 */
	if (ungetc(c, stream) == EOF)
	  break;
      }
    }
  }

  if (consumed)
    *consumed = cons;

  return p - p_save;
}

/*
 * warning: fseek'ing backwards in an unsynced stream is probably slow!
 * warning: this won't seek past the end of a file!
 */
static int
unsync_fseek(FILE *stream, long offset, int whence, long *real_offset)
{
  long roff = 0;
  int c;

  if (whence != SEEK_CUR)
    if (fseek(stream, 0, whence) == -1)
      return -1;

  if (offset > 0) {

    /* seek forward */
    while (offset > 0) {
      c = getc(stream);
      if (c == EOF)
	break;
      offset--;
      roff++;
      if (c == 0xFF) {
	c = getc(stream);
	if (c == EOF)
	  break;
	roff++;
	if (c == 0x00) {
	  ; /* drop 0x00 to de-unsync */
	} else if (offset > 0) {
	  offset--;
	} else {
	  if (ungetc(c, stream) == EOF) /* put it back */
	    break;
	}
      }
    }

  } else if (offset < 0) {

    /* seek backward */
    getc(stream);
    while (offset < 0) {
      /* one step forward, two steps back... */
      if (fseek(stream, -2, SEEK_CUR) == -1)
	return -1;
      c = getc(stream);
      if (c == EOF)
	break;
      roff--;
      if (c == 0x00 && ftell(stream) >= 2) {
	if (fseek(stream, -2, SEEK_CUR) == -1)
	  return -1;
	c = getc(stream);
	if (c == EOF)
	  break;
	roff--;
	if (c == 0xFF)
	  ; /* drop 0x00 to de-unsync */
	else if (offset < 0)
	  offset++;
	else
	  getc(stream); /* seek ahead 1 byte */
      } else {
	offset++;
      }
    }
    if (fseek(stream, -1, SEEK_CUR) == -1)
      return -1;

  }

  if (real_offset)
    *real_offset = roff;

  if (ferror(stream))
    return -1;

  return 0;
}

static int
_read_v2_header(id3_t tag, unsigned char *hdr)
{
  tag->unsync = (hdr[5] >> 7) & 1;
  tag->tagsz = unsyncsafe_int(hdr + 6);
  return 0;
}

static int
_read_v3_header(id3_t tag, unsigned char *hdr)
{
  unsigned char xhdr[10];
  size_t consumed;
  long offset;
  int sz;

  tag->unsync       = (hdr[5] >> 7) & 1;
  tag->has_ext_hdr  = (hdr[5] >> 6) & 1;
  tag->experimental = (hdr[5] >> 5) & 1;
  tag->tagsz = unsyncsafe_int(hdr + 6);

  /* read the extended header, if it exists */
  if (tag->has_ext_hdr) {
    if (tag->unsync) {
      if (unsync_fread(xhdr, 10, tag->fp, 20, &consumed) < 10)
	return -1;
    } else {
      if (fread(xhdr, 1, 10, tag->fp) < 10)
	return -1;
    }
    sz = get_be_int(xhdr);
    tag->has_crc = (xhdr[4] & 0x80) ? 1 : 0;

    /* we could read the CRC here, but does anyone really care? */

    if (tag->unsync) {
      if (unsync_fseek(tag->fp, sz - 6, SEEK_CUR, &offset) == -1)
	return -1;
      tag->curr_off += consumed;
      tag->curr_off += offset;
    } else {
      if (tag->seekable) {
	if (fseek(tag->fp, sz - 6, SEEK_CUR) == -1)
	  tag->seekable = 0;
      }
      if (!tag->seekable) {
	sz -= 6;
	while (sz-- > 0)
	  if (getc(tag->fp) == EOF)
	    return -1;
      }
      tag->curr_off += sz;
    }
  }

  return 0;
}

static int
_read_v4_header(id3_t tag, unsigned char *hdr)
{
  unsigned char xhdr[6];
  int sz;

  tag->unsync       = (hdr[5] >> 7) & 1;
  tag->has_ext_hdr  = (hdr[5] >> 6) & 1;
  tag->experimental = (hdr[5] >> 5) & 1;
  tag->has_footer   = (hdr[5] >> 4) & 1;
  tag->tagsz = unsyncsafe_int(hdr + 6);

  /* read the extended header, if it exists */
  if (tag->has_ext_hdr) {
    if (fread(xhdr, 1, 6, tag->fp) < 6)
      return -1;
    sz = unsyncsafe_int(xhdr);
    tag->is_update = (xhdr[5] & 0x40) ? 1 : 0;
    tag->has_crc = (xhdr[5] & 0x20) ? 1 : 0;
    tag->has_restrict = (xhdr[5] & 0x10) ? 1 : 0;

    /* FIXME: read CRC and restrictions here */

    tag->curr_off += sz;
    if (tag->seekable) {
      if (fseek(tag->fp, sz - 6, SEEK_CUR) == -1)
	tag->seekable = 0;
    }
    if (!tag->seekable) {
      sz -= 6;
      while (sz-- > 0)
	if (getc(tag->fp) == EOF)
	  return -1;
    }
  }

  return 0;
}

static int
_look_for_v1tag(id3_t tag)
{
  int sz;
  char buf[128];
  if (fseek(tag->fp, -128, SEEK_END) == -1)
    return 0;
  sz = fread(buf, 1, 128, tag->fp);
  if (sz == 128 && memcmp(buf, "TAG", 3) == 0) {
    tag->v1.exists = 1;
    strncpy(tag->v1.title, buf + 3, 30);
    strncpy(tag->v1.artist, buf + 33, 30);
    strncpy(tag->v1.album, buf + 63, 30);
    strncpy(tag->v1.year, buf + 93, 4);
    strncpy(tag->v1.comment, buf + 97, 30);
    /* make sure track field is not part of a v1.0 comment */
    tag->v1.track = (buf[125] == 0) ? buf[126] : 0;
    tag->v1.genre = buf[127];
  }
  return tag->v1.exists;
}

static int
_is_id3_header(unsigned char *buf)
{
  if (strncmp((char *)buf, "ID3", 3) != 0
      || buf[3] == 0xFF || buf[4] == 0xFF
      || buf[6] >= 0x80 || buf[7] >= 0x80
      || buf[8] >= 0x80 || buf[9] >= 0x80)
    /* first 10 bytes do not match id3 tag header pattern */
    return 0;
  return 1;
}

static int
_is_id3_footer(unsigned char *buf)
{
  if (strncmp((char *)buf, "3DI", 3) != 0
      || buf[3] == 0xFF || buf[4] == 0xFF
      || buf[6] >= 0x80 || buf[7] >= 0x80
      || buf[8] >= 0x80 || buf[9] >= 0x80)
    /* first 10 bytes do not match id3 tag footer pattern */
    return 0;
  return 1;
}

static int
_look_for_footer(id3_t tag, unsigned char *hdr, long offset_from_end)
{
  int sz, tag_found = 0;
  if (fseek(tag->fp, offset_from_end, SEEK_END) == -1)
    return 0;
  sz = fread(hdr, 1, 10, tag->fp);
  if (sz == 10 && _is_id3_footer(hdr)) {
    /* seek to beginning of tag */
    tag->tagsz = unsyncsafe_int(hdr + 6);
    if (fseek(tag->fp, -tag->tagsz - 10, SEEK_CUR) == -1)
      return -1;
    tag->curr_off = ftell(tag->fp);
    tag->offset = tag->curr_off - 10;
    tag->version = hdr[3];
    tag->revision = hdr[4];
    tag->append_req = tag->append = tag->has_footer = 1;
    tag_found = 1;
  }
  return tag_found;
}

/**
 * Gets the data size of the tag.
 *
 * @param tag the tag
 *
 * @return the size of the tag, not including header and footer.  The
 *         first time this is called on a file with an unsupported tag
 *         version, it sets errno to ENOSYS and returns -1.  The user
 *         can ignore the existing tag, by calling this again, in
 *         which case it will return zero, as if no tag exists on the
 *         file.
 */
int
id3_get_size(id3_t tag)
{
  unsigned char buf[10];
  int tag_found = 0;
  int sz;

  if (tag->tagsz == -1) {

    /*
     * We haven't read the header yet -- do it now
     */
    if (tag->seekable) {
      if (fseek(tag->fp, 0, SEEK_SET) == -1)
	tag->seekable = 0;
    }
    tag->offset = 0;
    tag->curr_off = 0; /* if not seekable, we just start here */

    tag->tagsz = 0;

    sz = fread(buf, 1, 10, tag->fp);
    tag->curr_off += sz;
    if (sz < 10) {
      if (ferror(tag->fp))
	return -1;
      /* there is no tag, so just leave it at size zero */
      ;
    } else if (!_is_id3_header(buf)) {
      /* first 10 bytes do not match id3 tag pattern, so no tag */
      ;
    } else {
      tag->version = buf[3];
      tag->revision = buf[4];
      tag_found = 1;
    }

    /* check for an appended tag */
    if (tag->seekable) {
      /* check for ID3v1 tag */
      if (_look_for_v1tag(tag) == -1)
	return -1;

      if (!tag_found) {
	if (tag->v1.exists) {
	  /* check just before id3v1 tag */
	  tag_found = _look_for_footer(tag, buf, -138);
	  if (tag_found == -1)
	    return -1;
	} else {
	  /* check at the very end */
	  tag_found = _look_for_footer(tag, buf, -10);
	  if (tag_found == -1)
	    return -1;
	}
      }

      fseek(tag->fp, tag->offset + 10, SEEK_SET);
    }

    if (tag_found) {
      switch (tag->version) {
      case 2:
	if (_read_v2_header(tag, buf) == -1) {
	  errno = EINVAL;
	  return -1;
	}
	break;
      case 3:
	if (_read_v3_header(tag, buf) == -1) {
	  errno = EINVAL;
	  return -1;
	}
	break;
      case 4:
	if (_read_v4_header(tag, buf) == -1) {
	  errno = EINVAL;
	  return -1;
	}
	break;
      default:
	/* unsupported id3v2 version */
	tag->tagsz = 0;
	errno = ENOSYS;
	return -1;
      }
#if DEBUG
      fprintf(stderr, "DEBUG: v2.%d.%d tag, size %d, offset=%ld\n",
	      tag->version, tag->revision, tag->tagsz, tag->offset);
#endif
    }
  }

  return tag->tagsz;
}


/*
 * Stream must be positioned at beginning of frame data on entering.
 * Returns number of bytes consumed.
 */
static int
_read_v4_frame_data(id3_frame_t f)
{
  int err;
  id3_t tag = f->id3;

  if (tag->seekable)
    if (fseek(tag->fp, f->offset, SEEK_SET) == -1)
      tag->seekable = 0;

  /* calloc instead of malloc to ensure termination of text data */
  f->data = (unsigned char *)calloc(f->sz + 2, 1);
  if (f->data == NULL)
    goto error;
  if (fread(f->data, 1, f->sz, tag->fp) < f->sz)
    goto error_free;
  if (_frame_is_unsynced(f))
    f->sz = decode_unsync(f->data, f->data, f->sz);

  if (f->id[0] == 'T') {
    /* for text frames, set the curr_txt pointer */
    f->curr_txt = (char *)f->data + 1;
  }

  return f->sz;
 error_free:
  err = errno;
  free(f->data);
  f->data = NULL;
  errno = err;
 error:
  return -1;
}

static int
_read_v4_frame_headers(id3_t tag)
{
  unsigned char buf[10];
  id3_frame_t newframe;
  int tagsz;

  tag->nframes = 0;
  tagsz = id3_get_size(tag); /* make sure we've read the tag header */
  if (tagsz < 1)
    return tagsz;

  /* scan frames */
  while (1) {
    if (tag->curr_off + 10 > tag->offset + tagsz + 10)
      break;
    /* read the frame header */
    if (fread(buf, 1, 10, tag->fp) < 10)
      goto error;
    tag->curr_off += 10;

    /* if the frame id is all zeroes, we've reached padding */
    if (memcmp(buf, "\0\0\0\0", 4) == 0)
      break;

    newframe = _id3_frame_new();
    if (newframe == NULL)
      goto error;
    memcpy(newframe->id, buf, 4);
    newframe->sz = unsyncsafe_int(buf + 4);
    newframe->flags = ((unsigned int)buf[8] << 8) | (unsigned int)buf[9];
    newframe->offset = tag->curr_off;
    /* check for bad size (frame would exceed end of tag) */
    if (newframe->offset + newframe->sz > tag->offset + 10 + tag->tagsz) {
      _id3_frame_destroy(newframe);
      break;
    }
    newframe->id3 = tag;
    _id3_frame_add(tag, newframe);

#if 0
      fprintf(stderr, "DEBUG: found frame %s, size %d\n",
	      newframe->id, newframe->sz);
#endif

    /*
     * seek through or read frame data
     */
    if (tag->seekable) {
      if (fseek(tag->fp, newframe->sz, SEEK_CUR) == -1)
	tag->seekable = 0;
    }
    if (!tag->seekable) {
      /* if we can't seek, we need to read everything now */
      if (_read_v4_frame_data(newframe) == -1)
	goto error;
    }
    tag->curr_off += newframe->sz;
  }

  return tag->nframes;

 error:
  return -1;
}


/*
 * Stream must be positioned at beginning of frame data on entering.
 * Returns number of bytes consumed.
 */
static int
_read_v3_frame_data(id3_frame_t f)
{
  int err;
  size_t consumed;
  id3_t tag = f->id3;

  if (tag->seekable)
    if (fseek(tag->fp, f->offset, SEEK_SET) == -1)
      tag->seekable = 0;

  /* calloc instead of malloc to ensure termination of text data */
  f->data = (unsigned char *)calloc(f->sz + 2, 1);
  if (f->data == NULL)
    goto error;
  if (tag->unsync) {
    if (unsync_fread(f->data, f->sz, tag->fp, f->offset - tag->tagsz,
		     &consumed) < f->sz)
      goto error_free;
  } else {
    if (fread(f->data, 1, f->sz, tag->fp) < f->sz)
      goto error_free;
    consumed = f->sz;
  }

  if (f->id[0] == 'T') {
    /* for text frames, set the curr_txt pointer */
    f->curr_txt = (char *)f->data + 1;
  }

  return (int)consumed;
 error_free:
  err = errno;
  free(f->data);
  f->data = NULL;
  errno = err;
 error:
  return -1;
}

static int
_read_v3_frame_headers(id3_t tag)
{
  unsigned char buf[10];
  id3_frame_t newframe;
  size_t consumed, limit;
  long offset;
  int tagsz;

  tag->nframes = 0;
  tagsz = id3_get_size(tag); /* make sure we've read the tag header */
  if (tagsz < 1)
    return tagsz;

  /* scan frames */
  while (1) {
    if (tag->unsync) {

      /*
       * Read the unsynced frame header.  We have to be careful not to
       * go past the end of the tag here, so we set the consume limit
       * on unsync_fread().
       */
      limit = tag->offset + tagsz + 10 - tag->curr_off;
      if (unsync_fread(buf, 10, tag->fp, limit, &consumed) < 10)
	break;
      tag->curr_off += consumed;

    } else {

      /*
       * Read the non-unsynced frame header normally.
       */
      /* The next line, if written for readability, would be
       *     if (tag->curr_off + 10 > tagsz + 10)
       * because:
       *     +10 to offset because we want to read that many,
       *     +10 to tagsz because it doesn't include the header */
      if (tag->curr_off > tagsz)
	break;
      if (fread(buf, 1, 10, tag->fp) < 10)
	goto error;
      tag->curr_off += 10;

    }
    /* if the frame id is all zeroes, we've reached padding */
    if (memcmp(buf, "\0\0\0\0", 4) == 0)
      break;

    newframe = _id3_frame_new();
    if (newframe == NULL)
      goto error;
    memcpy(newframe->id, buf, 4);
    newframe->sz = get_be_int(buf + 4);
    newframe->flags = ((unsigned int)buf[8] << 8) | (unsigned int)buf[9];
    newframe->offset = tag->curr_off;
    /* check for bad size (frame would exceed end of tag) */
    if (newframe->offset + newframe->sz > tag->offset + 10 + tag->tagsz) {
      _id3_frame_destroy(newframe);
      break;
    }
    newframe->id3 = tag;
    _id3_frame_add(tag, newframe);

#if 0
    fprintf(stderr, "DEBUG: found frame %s, size %d\n",
	    newframe->id, newframe->sz);
#endif

    /*
     * seek through or read frame data
     */
    if (tag->seekable) {
      if (tag->unsync) {
	if (unsync_fseek(tag->fp, newframe->sz, SEEK_CUR, &offset) == -1)
	  return -1;
	tag->curr_off += offset;
      } else {
	if (fseek(tag->fp, newframe->sz, SEEK_CUR) == -1)
	  tag->seekable = 0;
	else
	  tag->curr_off += newframe->sz;
      }
    }
    if (!tag->seekable) {
      /* if we can't seek, we need to read everything now */
      offset = _read_v3_frame_data(newframe);
      if (offset == -1)
	goto error;
      tag->curr_off += offset;
    }
  }

  return tag->nframes;

 error:
  return -1;
}


/*
 * Stream must be positioned at beginning of frame data on entering.
 * Returns number of bytes consumed.
 */
static int
_read_v2_frame_data(id3_frame_t f)
{
  /* the v3 routine happens to work for v2 also */
  return _read_v3_frame_data(f);
}

static int
_read_v2_frame_headers(id3_t tag)
{
  unsigned char buf[6];
  id3_frame_t newframe;
  size_t consumed;
  long offset;
  int tagsz;

  tag->nframes = 0;
  tagsz = id3_get_size(tag); /* make sure we've read the tag header */
  if (tagsz < 1)
    return tagsz;

  /* scan frames */
  while (1) {
    if (tag->unsync) {

      /*
       * Read the unsynced frame header.  We have to be careful not to
       * go past the end of the tag here, so we set the consume limit
       * on unsync_fread().
       */
      if (unsync_fread(buf, 6, tag->fp,
		       tagsz+10 - tag->curr_off, &consumed) < 6)
	break;
      tag->curr_off += consumed;

    } else {

      /*
       * Read the non-unsynced frame header normally.
       */
      if (tag->curr_off + 6 > tagsz + 10)
	break;
      if (fread(buf, 1, 6, tag->fp) < 6)
	goto error;
      tag->curr_off += 6;

    }
    /* if the frame id is all zeroes, we've reached padding */
    if (memcmp(buf, "\0\0\0", 3) == 0)
      break;

    newframe = _id3_frame_new();
    if (newframe == NULL)
      goto error;
    memcpy(newframe->id, buf, 3);
    newframe->sz = get_be_int(buf + 2) & 0xFFFFFF; /* read 4 bytes & drop 1 */
    newframe->offset = tag->curr_off;
    /* check for bad size (frame would exceed end of tag) */
    if (newframe->offset + newframe->sz > tag->offset + 10 + tag->tagsz) {
      _id3_frame_destroy(newframe);
      break;
    }
    newframe->id3 = tag;
    _id3_frame_add(tag, newframe);

#if 0
    fprintf(stderr, "DEBUG: found frame %s, size %d\n",
	    newframe->id, newframe->sz);
#endif

    /*
     * seek through or read frame data
     */
    if (tag->seekable) {
      if (tag->unsync) {
	if (unsync_fseek(tag->fp, newframe->sz, SEEK_CUR, &offset) == -1)
	  return -1;
	tag->curr_off += offset;
      } else {
	if (fseek(tag->fp, newframe->sz, SEEK_CUR) == -1)
	  tag->seekable = 0;
	else
	  tag->curr_off += newframe->sz;
      }
    }
    if (!tag->seekable) {
      /* if we can't seek, we need to read everything now */
      offset = _read_v2_frame_data(newframe);
      if (offset == -1)
	goto error;
      tag->curr_off += offset;
    }
  }

  return tag->nframes;

 error:
  return -1;
}


/**
 * Gets the number of frames in the tag.
 *
 * @param tag the tag
 *
 * @return the number of frames in the tag. The first time this is
 *         called on a file with an unsupported tag version, it sets
 *         errno to ENOSYS and returns -1.  The user can ignore the
 *         existing tag, by calling this again, in which case it will
 *         return zero, as if no tag exists on the file.
 */
int
id3_frame_count(id3_t tag)
{
  if (id3_get_size(tag) == -1)
    return -1;

  if (tag->nframes == -1) {
    /*
     * At this point, we should already be at the end of the header
     * (and extended header, if any), at the beginning of the first
     * frame.  We now read the headers of all the frames.
     */
    switch (tag->version) {
    case 4:
      if (_read_v4_frame_headers(tag) == -1)
	return -1;
      break;
    case 3:
      if (_read_v3_frame_headers(tag) == -1)
	return -1;
      break;
    case 2:
      if (_read_v2_frame_headers(tag) == -1)
	return -1;
      break;
    default:
      tag->nframes = 0;
    }
  }

  return tag->nframes;
}

/**
 * Deallocates the tag.
 *
 * @param tag the tag
 *
 * @return 0, or -1 on error.
 */
/* FIXME: should we flush to disk here? */
int
id3_close(id3_t tag)
{
  id3_frame_t f, tmp;

  f = tag->frame_hd;
  while (f) {
    tmp = f;
    f = f->next;
    _id3_frame_destroy(tmp);
  }

  if (tag->fname)
    free(tag->fname);
  fclose(tag->fp);
  free(tag);

  return 0;
}


/**
 * Sets the final size of the tag when written.
 * Padding policy is set to ID3_PADDING_CUSTOM if this is called.
 * @see id3_set_pad_policy()
 * @param tag the tag
 * @param size the size of the tag on disk, including header, padding,
 *        and footer.
 */
void
id3_set_size(id3_t tag, int size)
{
  tag->requested_sz = size;
  tag->pad_policy = ID3_PADDING_CUSTOM;
}


/**
 * Sets padding policy of the tag when it is written.
 * <ul>
 * <li> ID3_PADDING_NONE: no padding will be added to the end of the tag.
 * <li> ID3_PADDING_CUSTOM: the total size of the tag, with header and
 *      footer, will be at least the size set by id3_set_size().
 * <li> ID3_PADDING_DEFAULT: padding will be added according to the
 *      following rules:
 *    <ol>
 *    <li> We always add at least 32 bytes of padding.
 *    <li> For tags less than 256 bytes, we pad up to 256 bytes.
 *    <li> For tags larger than 256 bytes but smaller than 32k, we pad
 *         up to the next largest power of 2.
 *    <li> For tags larger than 32k, we pad up to the nearest 16k.
 *    </ol>
 * </ul>
 * @see id3_set_size()
 * @see id3_get_pad_policy()
 * @param tag the tag
 * @param policy the padding policy
 */
void
id3_set_pad_policy(id3_t tag, enum id3_pad_policy policy)
{
  switch (policy) {
  case ID3_PADDING_NONE:
  case ID3_PADDING_CUSTOM:
  case ID3_PADDING_DEFAULT:
    tag->pad_policy = policy;
    break;
  }
}

/**
 * Gets padding policy of the tag.
 * @see id3_set_pad_policy()
 * @param tag the tag
 * @return the padding policy
 */
enum id3_pad_policy
id3_get_pad_policy(id3_t tag)
{
  return tag->pad_policy;
}

/**
 * Set whether to prepend or append the tag when it is written.
 * Only ID3v2.4 and higher tags may be appended, so turning append on
 * will cause the tag to be converted to version 2.4.0.
 * @param tag the tag
 * @param append non-zero to turn append on, zero to turn append off
 *        (i.e. prepend)
 * @return 0, or -1 on error.  Errors can occur during the v2.4
 *         conversion process
 */
int
id3_set_append(id3_t tag, int append)
{
  /* only v2.4 tags can be appended */
  if (append)
    if (id3_set_version(tag, ID3_VERSION_2_4) == -1)
      return -1;
  tag->append_req = append;
  return 0;
}


/**
 * Set whether to unsync the tag when it is written.
 * @param tag the tag
 * @param unsync non-zero to turn unsync on, zero to turn unsync off
 * @return 0, or -1 on error.
 */
int
id3_set_unsync(id3_t tag, int unsync)
{
  tag->unsync = unsync;
  return 0;
}


/**
 * Gets the nth frame in the tag.
 * @param tag the tag
 * @param n the index of the frame to return
 * @return the nth frame, or NULL on error.
 */
/* FIXME: this is O(n) right now */
id3_frame_t
id3_get_frame(id3_t tag, int n)
{
  id3_frame_t f;

  /* make sure we've read the headers */
  id3_frame_count(tag);

  for (f = tag->frame_hd; f && n > 0; n--)
    f = f->next;

  return f;
}

/*
 * returns the first frame with the given id
 */
/* FIXME: this is O(n) */
/* FIXME: need some way to get matching frames other than the first */
id3_frame_t
id3_get_frame_by_id(id3_t tag, const char *id)
{
  id3_frame_t f;

  /* make sure we've read the headers */
  id3_frame_count(tag);

  for (f = tag->frame_hd; f; f = f->next)
    if (strcmp(id, f->id) == 0)
      break;

  return f;
}

/* FIXME: O(n) */
void
id3_frame_delete(id3_frame_t f)
{
  id3_t tag = f->id3;
  id3_frame_t prev;

  /* make sure we've read the headers */
  id3_frame_count(tag);

  if (tag->frame_hd == f) {
    tag->frame_hd = f->next;
    if (tag->frame_hd == NULL)
      tag->frame_tl = NULL;
  } else {
    /* find the previous frame */
    for (prev = tag->frame_hd; prev; prev = prev->next)
      if (prev->next == f)
	break;
    if (prev)
      prev->next = f->next;
  }
  _id3_frame_destroy(f);

  tag->nframes--;
}


/**
 * Gets the string identifier of a frame.
 * @param f the frame
 * @return a NUL-terminated string containing the frame ID.  This is
 *         part of another structure and should not be free()ed.
 */
char *
id3_frame_get_id(id3_frame_t f)
{
  return f->id;
}

int
id3_frame_set_id(id3_frame_t f, const char *id)
{
  if (strlen(id) > 4)
    return -1;
  strcpy(f->id, id);
  return 0;
}


/**
 * Gets the size of a frame.
 * @param f the frame
 * @return the size of the frame, in bytes, not including the header
 */
int
id3_frame_get_size(id3_frame_t f)
{
  return f->sz;
}


/**
 * Gets the raw data of a frame.
 * @param f the frame
 * @return a pointer to a buffer containing the raw data of the frame.
 *         This pointer is deallocated automatically and should not be
 *         free()ed.
 */
void *
id3_frame_get_raw(id3_frame_t f)
{
  id3_t tag = f->id3;

  if (f->data == NULL && !_frame_is_compressed(f) && !_frame_is_encrypted(f)) {
    /* if the tag is not seekable, we should already have the data */
    if (tag->seekable) {
      switch (tag->version) {
      case 4:
	_read_v4_frame_data(f);
	break;
      case 3:
	_read_v3_frame_data(f);
	break;
      case 2:
	_read_v2_frame_data(f);
      }
    }
  }

  return (void *)f->data;
}

int
id3_frame_set_raw(id3_frame_t f, void *buf, int size)
{
  if (f->data)
    free(f->data);
  f->data = (unsigned char *)malloc(size);
  if (f->data == NULL)
    return -1;
  memcpy(f->data, buf, size);
  f->sz = size;
  return 0;
}

/* FIXME: make static? */
id3_frame_t
id3_frame_add(id3_t tag, const char *id)
{
  id3_frame_t fr;

  fr = id3_get_frame_by_id(tag, id);

  if (fr == NULL) {
    fr = _id3_frame_new();
    if (fr == NULL)
      return NULL;

    strncpy(fr->id, id, 4);

    fr->id3 = tag;
    _id3_frame_add(tag, fr);
  }

  return fr;
}

int
id3_frame_get_flag(id3_frame_t f, enum id3_fflag flg)
{
  switch (f->id3->version) {
  case 4:
    return (f->flags & v4_fflag_masks[flg]) ? 1 : 0;
  case 3:
    return (f->flags & v3_fflag_masks[flg]) ? 1 : 0;
  }
  return 0;
}

void
id3_frame_set_flag(id3_frame_t f, enum id3_fflag flg)
{
  switch (f->id3->version) {
  case 4:
    f->flags |= v4_fflag_masks[flg];
    break;
  case 3:
    f->flags |= v3_fflag_masks[flg];
    break;
  case 2:
  default:
    return;
  }
}

void
id3_frame_clear_flag(id3_frame_t f, enum id3_fflag flg)
{
  switch (f->id3->version) {
  case 4:
    f->flags &= ~v4_fflag_masks[flg];
    break;
  case 3:
    f->flags &= ~v3_fflag_masks[flg];
    break;
  case 2:
  default:
    return;
  }
}

void
id3_strip(id3_t tag)
{
  id3_frame_t f, tmp;

  f = tag->frame_hd;
  while (f) {
    tmp = f;
    f = f->next;
    _id3_frame_destroy(tmp);
  }

  tag->frame_hd = tag->frame_tl = NULL;
  tag->nframes = 0;
  tag->v1.requested = 0;
}
