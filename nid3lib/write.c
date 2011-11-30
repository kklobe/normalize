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
 * This file contains a single externally visible function,
 * id3_write(), and many helper functions.
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
#if HAVE_FTRUNCATE
/*
 * ftruncate() shouldn't be declared yet, since we turn on
 * _POSIX_C_SOURCE, but you never know, so we use an old style
 * declaration to lessen the chance of conflict.
 */
int ftruncate();
#endif

#include "nid3P.h"

#define USE_TEMPFILE 1

#ifdef HAVE_DOS_FILE_NAMES
# define SLASH_CHAR '\\'
#else
# define SLASH_CHAR '/'
#endif

static int xrename(const char *oldpath, const char *newpath);
static int xmkstemp(char *template);

/*
 * Frontend to fwrite that unsync's.
 * size and the return value are the number of de-unsynced bytes.
 * The number of bytes actually written to the stream (i.e. unsynced bytes)
 * is returned in written, unless written == NULL.
 *
 * state keeps track of the stream state between calls, and is
 * necessary for cases where the last byte of one write is 0xFF and
 * the first byte of the next write is 0x00 or 0xE0.
 */
static size_t
unsync_fwrite(void *buf, size_t size, FILE *stream,
	      int *state, size_t *written)
{
  unsigned char *p = (unsigned char *)buf;
  unsigned char *p_save;
  size_t writ;
  int c;

  writ = 0;
  p_save = p;

  while (size > 0) {
    c = *p;
    /* state means "was the last character written 0xFF?" */
    if (*state && (c == 0x00 || (c & 0xE0) == 0xE0)) {
      /* insert 0x00 to unsync */
      if (putc(0, stream) == EOF)
	break;
      writ++;
    }
    *state = (c == 0xFF);
    if (putc(c, stream) == EOF)
      break;
    p++; size--; writ++;
  }

  if (written)
    *written = writ;

  return p - p_save;
}

/*
 * Copy sz bytes of src to dest, unsyncing along the way.  src and
 * dest may not overlap.  Return value is number of bytes written to
 * dest.
 */
static int
encode_unsync(unsigned char *dest, unsigned char *src, int sz)
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
      if (*src == 0x00 || (*src & 0xE0) == 0xE0)
	*dest++ = 0x00; /* insert 0x00 to unsync */
      *dest++ = *src++;
      sz--;
    }
  }

  return dest - dest_save;
}

static inline void
put_be_int(unsigned char *buf, unsigned int x)
{
  buf[3] = x & 0xFF;
  x >>= 8;
  buf[2] = x & 0xFF;
  x >>= 8;
  buf[1] = x & 0xFF;
  x >>= 8;
  buf[0] = x & 0xFF;
}

/* place the syncsafe encoding of x in the four bytes starting at buf */
static void
syncsafe_int(unsigned char *buf, unsigned int x)
{
  buf[3] = x & 0x7F;
  x >>= 7;
  buf[2] = x & 0x7F;
  x >>= 7;
  buf[1] = x & 0x7F;
  x >>= 7;
  buf[0] = x & 0x7F;
}

/* return the sum of the data sizes of all frames */
static int
_sum_frame_data_sizes(id3_t tag)
{
  int sz = 0;
  id3_frame_t f;

  for (f = tag->frame_hd; f; f = f->next)
    sz += id3_frame_get_size(f);

  return sz;
}

/* return the sum of the total sizes of all frames, including headers */
static int
_sum_v4_frame_sizes(id3_t tag)
{
  int nframes = id3_frame_count(tag);
  return nframes * 10 + _sum_frame_data_sizes(tag);
}

/* return the sum of the total sizes of all frames, including headers */
static int
_sum_v3_frame_sizes(id3_t tag)
{
  int nframes = id3_frame_count(tag);
  return nframes * 10 + _sum_frame_data_sizes(tag);
}

#if 0 /* we would need this if we wrote ID3v2.2 tags, but we don't */
/* return the sum of the total sizes of all frames, including headers */
static int
_sum_v2_frame_sizes(id3_t tag)
{
  int nframes = id3_frame_count(tag);
  return nframes * 6 + _sum_frame_data_sizes(tag);
}
#endif


/*
 * ID3v2.4 writing routines
 */

static void
_form_v4_header(id3_t tag, unsigned char *hdr)
{
  memcpy(hdr, "ID3", 3);
  hdr[3] = tag->version; /* should always be 4 */
  hdr[4] = tag->revision;
  hdr[5] = 0x00;
  if (tag->unsync)
    hdr[5] |= 0x80;
  /* FIXME: implement extended header */
  if (tag->experimental)
    hdr[5] |= 0x20;
  /* if append is requested, set footer bit */
  if (tag->append_req)
    hdr[5] |= 0x10;
  syncsafe_int(hdr + 6, tag->tagsz);
}

static int
_write_v4_header(id3_t tag, FILE *out)
{
  unsigned char buf[10];

  _form_v4_header(tag, buf);

  if (fwrite(buf, 1, 10, out) < 10)
    return -1;
  tag->curr_off += 10;

  return 0;
}

static int
_write_v4_footer(id3_t tag, FILE *out)
{
  unsigned char buf[10];

  _form_v4_header(tag, buf);

  /* footer is the same as header, but starts with "3DI" instead */
  memcpy(buf, "3DI", 3);

  if (fwrite(buf, 1, 10, out) < 10)
    return -1;
  tag->curr_off += 10;

  return 0;
}

static int
_write_v4_frames(id3_t tag, FILE *out)
{
  id3_frame_t f;
  int sz, unsync_bufsz = 512;
  unsigned char hdr[10];
  unsigned char *buf, *unsync_buf = NULL;

  for (f = tag->frame_hd; f; f = f->next) {

    if (tag->unsync)
      id3_frame_set_flag(f, ID3_FFLAG_IS_UNSYNCED);

    /* form frame header */
    memcpy(hdr, f->id, 4);
    syncsafe_int(hdr + 4, f->sz);
    hdr[8] = (f->flags >> 8) & 0xFF;
    hdr[9] = f->flags & 0xFF;

    buf = id3_frame_get_raw(f);
    if (buf == NULL)
      continue;

    /* update frame offset */
    f->offset = tag->curr_off + 10;

    if (_frame_is_unsynced(f)) {
      if (unsync_buf == NULL || unsync_bufsz < f->sz * 3 / 2) {
	unsync_bufsz = f->sz * 3 / 2;
	if (unsync_buf)
	  free(unsync_buf);
	unsync_buf = (unsigned char *)malloc(unsync_bufsz);
	if (unsync_buf == NULL)
	  return -1;
      }
      sz = encode_unsync(unsync_buf, buf, f->sz);

      /* update frame size in header */
      syncsafe_int(hdr + 4, sz);

      /* unset the unsync bit if unsync didn't change the size */
      if (!tag->unsync && sz == f->sz)
	id3_frame_clear_flag(f, ID3_FFLAG_IS_UNSYNCED);

      /* write frame header */
      if (fwrite(hdr, 1, 10, out) < 10)
	return -1;
      /* write frame data */
      if (fwrite(unsync_buf, 1, sz, out) < sz)
	return -1;
      tag->curr_off += 10 + sz;

    } else {

      /* write frame header */
      if (fwrite(hdr, 1, 10, out) < 10)
	return -1;
      /* write frame data */
      if (fwrite(buf, 1, f->sz, out) < f->sz)
	return -1;
      tag->curr_off += 10 + f->sz;
    }

    /* keep as few frames in memory as possible */
    free(f->data);
    f->data = NULL;
  }

  if (unsync_buf)
    free(unsync_buf);

  return 0;
}


/*
 * ID3v2.3 writing routines
 */

static int
_write_v3_header(id3_t tag, FILE *out)
{
  unsigned char hdr[10];

  memcpy(hdr, "ID3", 3);
  hdr[3] = tag->version; /* should always be 3 */
  hdr[4] = tag->revision;
  hdr[5] = 0x00;
  if (tag->unsync)
    hdr[5] |= 0x80;
  /* FIXME: implement extended header */
  if (tag->experimental)
    hdr[5] |= 0x20;
  syncsafe_int(hdr + 6, tag->tagsz);
  if (fwrite(hdr, 1, 10, out) < 10)
    return -1;
  tag->curr_off += 10;

  return 0;
}

static int
_write_v3_frames(id3_t tag, FILE *out)
{
  id3_frame_t f;
  int fwrite_state = 0;
  size_t consumed;
  unsigned char hdr[10];
  unsigned char *buf;

  for (f = tag->frame_hd; f; f = f->next) {

    /* form frame header */
    memcpy(hdr, f->id, 4);
    put_be_int(hdr + 4, f->sz);
    hdr[8] = (f->flags >> 8) & 0xFF;
    hdr[9] = f->flags & 0xFF;

    buf = id3_frame_get_raw(f);
    if (buf == NULL)
      continue;

    /* update frame offset */
    f->offset = tag->curr_off + 10;

    if (tag->unsync) {
      /* write frame header */
      if (unsync_fwrite(hdr, 10, out, &fwrite_state, &consumed) < 10)
	return -1;
      tag->curr_off += consumed;
      /* write frame data */
      if (unsync_fwrite(buf, f->sz, out, &fwrite_state, &consumed) < f->sz)
	return -1;
      tag->curr_off += consumed;
    } else {
      /* write frame header */
      if (fwrite(hdr, 1, 10, out) < 10)
	return -1;
      /* write frame data */
      if (fwrite(buf, 1, f->sz, out) < f->sz)
	return -1;
      tag->curr_off += 10 + f->sz;
    }

    /* keep as few frames in memory as possible */
    free(f->data);
    f->data = NULL;
  }

  return 0;
}


/* build the 128-byte ID3v1 disk structure */
static void
_build_v1_tag(char *buf, id3_t tag)
{
  /* FIXME: copy appropriate existing v2 tags to their v1 counterparts? */
  memset(buf, 0, 128);
  strcpy(buf, "TAG");
  strncpy(buf + 3, tag->v1.title, 30);
  strncpy(buf + 33, tag->v1.artist, 30);
  strncpy(buf + 63, tag->v1.album, 30);
  strncpy(buf + 93, tag->v1.year, 4);
  strncpy(buf + 97, tag->v1.comment, 30);
  if (buf[125] == 0) /* check if tag is v1.1 */
    buf[126] = tag->v1.track;
  buf[127] = tag->v1.genre;
}


/* Figure out size of old tag and new tag, based on the current pad policy */
static void
_calculate_tag_sizes(id3_t tag, int *pold_sz, int *pnew_sz)
{
  /* old_ and new_tagsz are size of tag, including header + footer + padding */
  int old_tagsz, new_tagsz;
  /* frames_sz is the sum of the sizes of the frames */
  int frames_sz;
  int padded_sz;

  old_tagsz = id3_get_size(tag);
  if (old_tagsz > 0)
    old_tagsz += 10; /* +10 for header */
  if (tag->has_footer)
    old_tagsz += 10; /* +10 for footer */

  switch (tag->version) {
  case 4:
    frames_sz = _sum_v4_frame_sizes(tag);
    break;
  case 3:
    frames_sz = _sum_v3_frame_sizes(tag);
    break;
  default:
    abort();
  }

  /* figure out how much padding to add */
  switch (tag->pad_policy) {
  default:
  case ID3_PADDING_DEFAULT:
    new_tagsz = frames_sz + 10;
    if (old_tagsz >= new_tagsz) {
      new_tagsz = old_tagsz;
    } else {
      /* always pad at least 32 bytes */
      new_tagsz += 32;
      if (new_tagsz <= 256) {
	/* for new_tagsz <= 256, pad up to 256 bytes */
	new_tagsz = 256;
      } else if (new_tagsz <= 32768) {
	/* for 256 < new_tagsz <= 32768, pad up to nearest power of 2 */
	padded_sz = 1;
	new_tagsz--;
	while (new_tagsz > 0) {
	  padded_sz <<= 1;
	  new_tagsz >>= 1;
	}
	new_tagsz = padded_sz;
      } else {
	/* for new_tagsz > 32768, pad up to nearest 16k */
	new_tagsz--; /* want e.g. 65536 to pad to 65536 */
	new_tagsz = ((new_tagsz >> 14) + 1) << 14;
      }
    }
    break;
  case ID3_PADDING_NONE:
    new_tagsz = frames_sz + 10;
    break;
  case ID3_PADDING_CUSTOM:
    new_tagsz = frames_sz + 10;
    if (tag->requested_sz > new_tagsz)
      new_tagsz = tag->requested_sz;
    break;
  }

  /* If we're supposed to write the tag appended, we have to write a
     footer, and padding is not allowed with a footer. */
  if (tag->append_req)
    new_tagsz = frames_sz + 20; /* +10 for header, +10 for footer */

  /* if there are no frames, we're stripping the tag */
  if (frames_sz == 0)
    new_tagsz = 0;

  /* set the tag size field (doesn't include header and footer) */
  tag->tagsz = new_tagsz;
  if (tag->tagsz)
    tag->tagsz -= 10;
  if (tag->append_req)
    tag->tagsz -= 10;

  /* pass back tag sizes */
  *pold_sz = old_tagsz;
  *pnew_sz = new_tagsz;
}

/* write the tag to the stream out, dispatching on the tag's version */
static int
_write_tag(id3_t tag, FILE *out)
{
  int nframes = id3_frame_count(tag);

  if (nframes > 0) {
    switch (tag->version) {
    case 4:
      tag->has_footer = tag->append_req; /* just to make sure */
      if (_write_v4_header(tag, out) == -1)
	return -1;
      if (_write_v4_frames(tag, out) == -1)
	return -1;
      if (tag->append_req)
	if (_write_v4_footer(tag, out) == -1)
	  return -1;
      break;
    case 3:
      if (_write_v3_header(tag, out) == -1)
	return -1;
      if (_write_v3_frames(tag, out) == -1)
	return -1;
      break;
    default:
      abort();
    }
  }

  return 0;
}

/**
 * Updates the tag on disk
 * @param tag the tag to update
 */
int
id3_write(id3_t tag)
{
  /* old_ and new_tagsz are size of tag, including header + footer + padding */
  int old_tagsz, new_tagsz;
  int sz, written, nframes;
  char copybuf[4096];
  char v1buf[128];
  off_t new_offset;
#if USE_TEMPFILE
  char *tmpfname = NULL, *p;
  int fd, err, write_in_place;
  struct stat stbuf;
  fpos_t pos_save;
  FILE *out = NULL;
#endif

  if (!tag->seekable) {
    errno = EINVAL;
    return -1;
  }
  if (tag->mode == ID3_RDONLY) {
    errno = EACCES;
    return -1;
  }

  /* make sure we've read the tag and frame headers */
  nframes = id3_frame_count(tag);

  /* make sure we know the version */
  if (tag->version < 2 || tag->version > 4) {
    errno = EINVAL;
    return -1;
  }

  /* if the version is 2.2, convert to v2.3 */
  if (tag->version == 2) {
    if (id3_set_version(tag, ID3_VERSION_2_3) == -1) {
      errno = EINVAL;
      return -1;
    }
  }

  /* if we are to write a v1 tag, build the 128-byte buffer now */
  if (tag->v1.requested)
    _build_v1_tag(v1buf, tag);

  /*
   * figure old and new sizes, with padding and headers, and set the
   * tag->tagsz field
   */
  _calculate_tag_sizes(tag, &old_tagsz, &new_tagsz);

#if DEBUG
  fprintf(stderr, "DEBUG: total tag size will be %d bytes\n", new_tagsz);
#endif

  write_in_place = (tag->append == tag->append_req
		    && (tag->append || new_tagsz == old_tagsz));
#ifndef HAVE_FTRUNCATE
  /* If we're appending and the new tag is smaller than before, we
     need to truncate.  If we can't truncate, we can't write the tag
     in place. */
  if (tag->append_req && new_tagsz < old_tagsz)
    write_in_place = 0;
  if (tag->v1.exists && !tag->v1.requested)
    write_in_place = 0;
#endif

  if (write_in_place) {
    /*
     * We can write the tag in place, and don't need to copy the whole file
     */

    /* We can't write the tag while reading it from the same place, so
       we write it (just the tag) to a temporary file, then copy it back. */
    out = tmpfile();
    if (out == NULL)
      goto error_free;

    /*
     * write tag to temp file
     */
    _write_tag(tag, out);

    /* do padding, if necessary */
    if (ftell(out) < new_tagsz) {
      if (tag->append_req) {
	abort(); /* should never pad appended tags */
      } else {
	sz = new_tagsz - ftell(out);
	while (sz > 0) {
	  if (putc(0, out) == EOF)
	    goto error_free;
	  sz--;
	}
      }
    }

    /* write ID3v1 tag to temp file, if appending */
    if (tag->v1.requested && tag->append_req) {
      if (fwrite(v1buf, 1, 128, out) < 128)
	goto error_free;
    }

    /* copy it back */
    rewind(out);
    if (fseek(tag->fp, tag->offset, SEEK_SET) == -1)
      goto error_free;
    while ((sz = fread(copybuf, 1, 4096, out)) > 0) {
      if (fwrite(copybuf, 1, sz, tag->fp) < sz)
	goto error_free;
    }

    /* temp file is deleted automatically */
    fclose(out); out = NULL;

    /* If we're not appending, and we have an ID3v1 tag,
     * fseek and write the v1 tag. */
    if (tag->v1.requested && !tag->append_req) {
      if (fseek(tag->fp, tag->v1.exists ? -128 : 0, SEEK_END) == -1)
	goto error_free;
      if (fwrite(v1buf, 1, 128, tag->fp) < 128)
	goto error_free;
    }

#if HAVE_FTRUNCATE
    /* truncate the file to the proper length */
    if (new_tagsz < old_tagsz) {
      /* (note: this case only happens when we're appending) */
      if (ftruncate(fileno(tag->fp), tag->offset + new_tagsz
		    + (tag->v1.requested ? 128 : 0)) == -1)
	goto error_free;
    } else if (tag->v1.exists && !tag->v1.requested) {
      /* strip the v1 tag */
      if (fseek(tag->fp, -128, SEEK_END) == -1)
	goto error_free;
      if (ftruncate(fileno(tag->fp), (size_t)ftell(tag->fp)) == -1)
	goto error_free;
    }
#endif

  } else {
    /*
     * We can't write the tag in place, so we need to copy the whole file
     */

#if USE_TEMPFILE
    /*
     * Create temporary file name, and open it for writing.  We want
     * it to be in the same directory (and therefore, in the same
     * filesystem, for a fast rename).
     */
    if (tag->fname) {
      tmpfname = (char *)malloc(strlen(tag->fname) + 16);
      if (tmpfname == NULL)
	return -1;
      strcpy(tmpfname, tag->fname);
      if ((p = strrchr(tmpfname, SLASH_CHAR)) == NULL)
	p = tmpfname;
      else
	p++;
      strcpy(p, "_id3XXXXXX");
    } else {
      tmpfname = (char *)malloc(4 + 16);
      if (tmpfname == NULL)
	return -1;
      /* use /tmp on real machines, just put file wherever on DOS */
#ifdef HAVE_DOS_FILE_NAMES
      strcpy(tmpfname, "_id3XXXXXX");
#else
      strcpy(tmpfname, "/tmp/_id3XXXXXX");
#endif
    }

    fd = xmkstemp(tmpfname);
    if (fd == -1) {
      fprintf(stderr, "id3: error opening temp file: %s\n", strerror(errno));
      goto error_free;
    }
    out = fdopen(fd, "wb");
    if (out == NULL) {
      fprintf(stderr, "id3: error in fdopen: %s\n", strerror(errno));
      goto error_free;
    }

    /* preserve original permissions */
    fstat(fileno(tag->fp), &stbuf);
    chmod(tmpfname, stbuf.st_mode); /* fchmod() not posix */
#endif /* USE_TEMPFILE */

    tag->curr_off = 0;

    /*
     * If we're appending, copy rest of file first
     */
    if (tag->append_req) {
      if (tag->append) {
	/* old tag was appended */
	if (fseek(tag->fp, 0, SEEK_SET) == -1)
	  goto error_free;
	tag->curr_off = 0;
	while (1) {
	  if (tag->curr_off >= tag->offset)
	    break;
	  sz = tag->offset - tag->curr_off;
	  if (sz > 4096)
	    sz = 4096;
	  written = fread(copybuf, 1, sz, tag->fp);
	  if (written < sz)
	    goto error_free;
	  tag->curr_off += written;
	}
      } else {
	/* old tag was prepended */
	if (fseek(tag->fp, old_tagsz, SEEK_SET) == -1)
	  goto error_free;
	tag->curr_off = old_tagsz;
	while ((sz = fread(copybuf, 1, 4096, tag->fp)) > 0) {
	  if (fwrite(copybuf, 1, sz, out) < sz)
	    goto error_free;
	  tag->curr_off += sz;
	}
      }

      if (tag->v1.exists) {
	/* we have to put the appended v2 tag before the v1 tag, so back up */
	if (fseek(tag->fp, -128, SEEK_CUR) == -1)
	  goto error_free;
	tag->curr_off -= 128;
      }
    }

    new_offset = tag->curr_off;

    /*
     * write tag
     */
    _write_tag(tag, out);

    /*
     * do padding
     */
    if (!tag->append_req) {
#if USE_TEMPFILE
      if (tag->curr_off != new_tagsz) {
	if (new_tagsz > 0) {
	  /* this lets the padding be a file hole */
	  fseek(out, new_tagsz - 1, SEEK_SET);
	  putc(0, out);
	} else {
	  rewind(out);
	}
      }
#else
      while (tag->curr_off < new_tagsz) {
	putc(0, out);
	tag->curr_off++;
      }
#endif
    }

    if (!tag->append_req) {
      /*
       * if we're not appending, copy rest of file last
       */
      fseek(tag->fp, tag->offset + old_tagsz, SEEK_SET);
      while ((sz = fread(copybuf, 1, 4096, tag->fp)) > 0) {
	if (fwrite(copybuf, 1, sz, out) < sz)
	  goto error_free;
      }

      /* we're not appending, so if we have an ID3v1 tag,
       * fseek and write the v1 tag. */
      if (tag->v1.requested) {
	if (fseek(out, tag->v1.exists ? -128 : 0, SEEK_END) == -1)
	  goto error_free;
	if (fwrite(v1buf, 1, 128, out) < 128)
	  goto error_free;
      }

    } else {
      /*
       * if we *are* appending, copy the v1 tag last
       */
      if (tag->v1.requested)
	if (fwrite(v1buf, 1, 128, out) < 128)
	  goto error_free;
    }

    tag->append = tag->append_req;
    tag->offset = new_offset;

#if USE_TEMPFILE
    fclose(out); out = NULL;

    /*
     * move the temporary file back to the original file
     */
    if (tag->fname == NULL) {
      fprintf(stderr, "id3: no filename, leaving output in %s\n", tmpfname);
      goto error_free;
    }

    fgetpos(tag->fp, &pos_save);
    fclose(tag->fp); tag->fp = NULL;
    if (xrename(tmpfname, tag->fname) == -1) {
      fprintf(stderr, "id3: error moving %s to %s: %s\n",
	      tmpfname, tag->fname, strerror(errno));
      goto error_free;
    }
    free(tmpfname); tmpfname = NULL;

    /* it's actually a different file now, so we must reopen it */
    tag->fp = fopen(tag->fname, "rb+");
    if (tag->fp == NULL)
      goto error_free;
    fsetpos(tag->fp, &pos_save);
#endif /* USE_TEMPFILE */
  }

  return 0;

 error_free:
  err = errno;
#if USE_TEMPFILE
  if (tmpfname)
    free(tmpfname);
  if (out)
    fclose(out);
#endif
  errno = err;
  return -1;
}


/*
 * like the BSD mkstemp
 */
static int
xmkstemp(char *template)
{
  static char sfx[7] = "AAAAAA";
  char *p;
  int fd, i, done;

  p = template + strlen(template) - 6;
  if (strcmp(p, "XXXXXX") != 0) {
    errno = EINVAL;
    return -1;
  }

  do {
    strcpy(p, sfx);

    /* increment the suffix */
    done = 0; i = 5;
    while (!done && i >= 0) {
      sfx[i]++;
      if (sfx[i] > 'Z') {
	sfx[i] = 'A';
	i--;
      } else {
	done = 1;
      }
    }
    if (!done) {
      errno = EEXIST;
      return -1;
    }

    /* attempt to open the file */
    fd = open(template, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);

  } while (fd == -1 && errno == EEXIST);

  return fd;
}


/*
 * Move the file "oldpath" to "newpath", or copy and delete if they
 * are on different filesystems.
 */
static int
xrename(const char *oldpath, const char *newpath)
{
  FILE *in, *out;
  char buf[4096];
  size_t sz;

  if (strcmp(oldpath, newpath) == 0)
    return 0;

#ifdef __EMX__
  if (unlink(newpath) == -1 && errno != ENOENT)
    return -1;
#endif

  if (rename(oldpath, newpath) == -1) {
    if (errno == EXDEV) {
      /* files are on different filesystems, so we have to copy */
      if (unlink(newpath) == -1 && errno != ENOENT)
	return -1;

      in = fopen(oldpath, "rb");
      if (in == NULL)
	return -1;
      out = fopen(newpath, "wb");
      if (out == NULL) {
	fclose(in);
	return -1;
      }

      while ((sz = fread(buf, 1, 4096, in)) > 0)
	fwrite(buf, 1, sz, out);

      if (ferror(in) || ferror(out)) {
	fclose(in);
	fclose(out);
	return -1;
      }
      if (fclose(in) == EOF) {
	fclose(out);
	return -1;
      }
      if (fclose(out) == EOF)
	return -1;

      if (unlink(oldpath) == -1)
	return -1;
    } else {
      return -1;
    }
  }

  return 0;
}
