/* Copyright (C) 1999--2005 Chris Vaill
   This file is part of normalize.

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

#define _POSIX_C_SOURCE 2

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STRING_H
#  include <string.h>
# elif HAVE_STRERROR
char *strerror();
# else
#  define strerror(x) "Unknown error"
# endif
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
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

#ifdef ENABLE_NLS
# define _(msgid) gettext (msgid)
# include <libintl.h>
#else
# define _(msgid) (msgid)
#endif
#define N_(msgid) (msgid)

#include "riff.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef MIN
# define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if OLD
/* we use a stack to keep track of the nestedness of list chunks */
struct _riff_chunk_stack_t {
  long start_off;
  long end_off;
  struct _riff_chunk_stack_t *next;
};

typedef struct _riff_chunk_stack_t *riff_chunk_stack_t;

struct _riff_t {
  int ref;
  FILE *fp; /* needed for stream writing */
  long file_off; /* current file offset */
  int mode;

  riff_chunk_stack_t stack;

  /* stream to which messages are written */
  FILE *msg_stream;
};
#endif

struct _riff_t {
  FILE *fp; /* needed for stream writing */
  int mode;

  /* stream to which messages are written */
  FILE *msg_stream;
};

#if OLD
/*
 * converts the integer x to a 4-byte little-endian array
 */
static void
_int_to_buffer_lendian(unsigned char *buf, unsigned int x)
{
  buf[0] = x & 0xFF;
  buf[1] = (x >> 8) & 0xFF;
  buf[2] = (x >> 16) & 0xFF;
  buf[3] = (x >> 24) & 0xFF;  
}
#endif

#if 0
static void
_stack_print(riff_chunk_stack_t stack)
{
  fprintf(stderr, "STACK:");
  while(stack) {
    fprintf(stderr, " (%lX,%lX)", stack->start_off, stack->end_off);
    stack = stack->next;
  }
  fprintf(stderr, "\n");
}
#endif

#if OLD
static int
_stack_push(riff_chunk_stack_t *stack, long start, long end)
{
  riff_chunk_stack_t newelt;

  newelt = (riff_chunk_stack_t)malloc(sizeof(struct _riff_chunk_stack_t));
  if(newelt == NULL) {
    fprintf(stderr, "riff: unable to malloc!\n");
    return -1;
  }
  newelt->start_off = start;
  newelt->end_off = end;
  newelt->next = *stack;
  *stack = newelt;

  return 0;
}

static void
_stack_pop(riff_chunk_stack_t *stack)
{
  riff_chunk_stack_t tmp;
  if(*stack != NULL) {
    tmp = *stack;
    *stack = (*stack)->next;
    free(tmp);
  }
}
#endif

riff_t
riff_open(const char *fname, int mode)
{
  int fd, open_mode;

  switch (mode) {
  case RIFF_RDONLY: open_mode = O_RDONLY; break;
  case RIFF_WRONLY: open_mode = O_WRONLY | O_CREAT | O_TRUNC; break;
  case RIFF_RDWR: open_mode = O_RDWR | O_CREAT; break;
  default:
    errno = EACCES;
    return NULL;
  }

  open_mode |= O_BINARY;
  fd = open(fname, open_mode, 0666);
  if (fd == -1)
    return NULL;

  return riff_fdopen(fd, mode);
}

riff_t
riff_fdopen(int fd, int mode)
{
  riff_t riff;
  const char *fmode;
  int err = 0;

  riff = (riff_t)malloc(sizeof(struct _riff_t));
  if (riff == NULL) {
    err = ENOMEM;
    goto err1;
  }

  riff->mode = mode;
  riff->msg_stream = stderr;

  /* Open file stream */
  switch (riff->mode) {
  case RIFF_RDONLY: fmode = "rb";  break;
  case RIFF_WRONLY: fmode = "wb";  break;
  case RIFF_RDWR:   fmode = "r+b"; break;
  default:
    err = EINVAL;
    goto err2;
  }
  riff->fp = fdopen(fd, fmode);
  if (riff->fp == NULL)
    goto err2;

  return riff;

 err2:
  free(riff);
 err1:
  errno = err;
  return NULL;
}

int
riff_close(riff_t riff)
{
  FILE *fp;
#if 0
  while(riff->stack != NULL)
    _stack_pop(&riff->stack);
#endif
  fp = riff->fp;
  free(riff);
  return fclose(fp);
}

static int
_read_chunk_start(riff_t riff, riff_chunk_t *chnk)
{
  /* descend at the current offset */
  if (fread(&chnk->id, 4, 1, riff->fp) < 1) {
    if (ferror(riff->fp)) return -1;
    else return 0;
  }
  if (fread(&chnk->size, 4, 1, riff->fp) < 1) {
    if (ferror(riff->fp)) return -1;
    else return 0;
  }
#ifdef WORDS_BIGENDIAN
  chnk->size = bswap_32(chnk->size);
#endif
  chnk->offset = ftell(riff->fp);
  if (chnk->id == RIFFID_RIFF || chnk->id == RIFFID_LIST) {
    if (fread(&chnk->type, 4, 1, riff->fp) < 1) {
      if (ferror(riff->fp)) return -1;
      else return 0;
    }
  }

  return 1;
}

/*
 * return values: -1 means error
 *                 0 means chunk not found
 *                 1 means success
 */
int
riff_descend(riff_t riff, riff_chunk_t *chnk,
	     riff_chunk_t *par_chnk, int search)
{
  int ret;
  fourcc_t search_id;
  riff_chunk_t rec_chnk;
  long end_offset = 0, offset;

  /* make sure we're inside the parent chunk */
  if (par_chnk) {
    offset = ftell(riff->fp);
    /* +4 to skip the format field (assuming parent is LIST or RIFF) */
    if (offset < par_chnk->offset + 4
	|| offset >= par_chnk->offset + par_chnk->size) {
      return 0;
    }
  }

  chnk->riff = riff;
  chnk->write = 0;

  if (search) {
    search_id = chnk->id;

    if (par_chnk)
      end_offset = par_chnk->offset + par_chnk->size;

    ret = _read_chunk_start(riff, chnk);
    while (ret == 1 && chnk->id != search_id) {
      if (search & RIFF_SRCH_RECURSE
	  && (chnk->id == RIFFID_RIFF || chnk->id == RIFFID_LIST)) {

	/* recurse into the list */
	rec_chnk = *chnk;
	chnk->id = search_id;
	ret = riff_descend(riff, chnk, &rec_chnk, RIFF_SRCH_RECURSE);

      } else {

	/* flat search; skip this chunk and keep searching */
	offset = chnk->offset + chnk->size;
	if (par_chnk && offset >= end_offset) {
	  ret = 0;
	  break;
	}
	fseek(riff->fp, offset, SEEK_SET);
	ret = _read_chunk_start(riff, chnk);
      }
    }

  } else {
    /* don't search; just get the chunk that starts where we are */
    ret = _read_chunk_start(riff, chnk);
  }

  return ret;
}

int
riff_create_chunk(riff_t riff, riff_chunk_t *chnk)
{
  uint32_t size;

  size = chnk->size;
#ifdef WORDS_BIGENDIAN
  size = bswap_32(size);
#endif

  /* write chunk ID and size (considered estimated for now) */
  if (fwrite(&chnk->id, 4, 1, riff->fp) < 1
      || fwrite(&size, 4, 1, riff->fp) < 1)
    return -1;

  chnk->offset = ftell(riff->fp);
  chnk->riff = riff;
  chnk->write = 1;

  /* also write the type if chunk has id RIFF or LIST */
  if (chnk->id == RIFFID_RIFF || chnk->id == RIFFID_LIST)
    if (fwrite(&chnk->type, 4, 1, riff->fp) < 1)
      return -1;

  return 0;
}

/*
 * return values: -1 means error
 *                 0 means success
 */
int
riff_ascend(riff_t riff, riff_chunk_t *chnk)
{
  long offset;
  uint32_t size;

  if (chnk->write) {
    offset = ftell(riff->fp);
    if (offset == -1)
      return -1;
    size = offset - chnk->offset;
    if (size != chnk->size) {
      /* go back and fix up the size field in the chunk header */
      if (fseek(riff->fp, chnk->offset - 4, SEEK_SET) == -1)
	return -1;
      /* write the real size */
#ifdef WORDS_BIGENDIAN
      size = bswap_32(size);
#endif
      if (fwrite(&size, 4, 1, riff->fp) < 1)
	return -1;
      /* go back to where we were */
      if (fseek(riff->fp, offset, SEEK_SET) == -1)
	return -1;
    }
  } else {
    offset = chnk->offset + chnk->size;
    if (offset & 1)
      offset++;
    if (fseek(riff->fp, offset, SEEK_SET) == -1)
      return -1;
  }

  return 0;
}

FILE *
riff_stream(riff_t riff)
{
  return riff->fp;
}

/* seek to beginning of chunk, then return stream */
FILE *
riff_chunk_stream(riff_t riff, riff_chunk_t *chnk)
{
  fseek(riff->fp, chnk->offset, SEEK_SET);
  return riff->fp;
}

#ifdef BUILD_RIFFWALK

static char *progname;

static int
_riff_walk(riff_t riff, riff_chunk_t *par_chnk, int depth)
{
  riff_chunk_t chnk;
  char id_s[5], typ_s[5];
  int i, ret;

  while (1) {
    ret = riff_descend(riff, &chnk, par_chnk, RIFF_SRCH_OFF);
    if (ret == 0)
      break;
    if (ret < 0) {
      fprintf(stderr, "%s: error reading RIFF: %s\n",
	      progname, strerror(errno));
      return ret;
    }

    for (i = 0; i < depth; i++)
      printf("     ");
    riff_fourcc_to_string(id_s, chnk.id);
    if (chnk.id == RIFFID_LIST || chnk.id == RIFFID_RIFF) {
      riff_fourcc_to_string(typ_s, chnk.type);
      printf("%s('%s' [size %d]\n", id_s, typ_s, chnk.size);
      _riff_walk(riff, &chnk, depth + 1);
      for (i = 0; i < depth; i++)
	printf("     ");
      printf("    )\n");
    } else {
      printf("'%s' [size %d]\n", id_s, chnk.size);
    }

    ret = riff_ascend(riff, &chnk);
    /*fseek(riff->fp, chnk.offset + chnk.size, SEEK_SET);*/

  }

  return 0;
}

int
main(int argc, char *argv[])
{
  riff_t riff;
  int ret;

  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file>\n", progname);
    return 1;
  }

  riff = riff_open(argv[1], RIFF_RDONLY);
  if (riff == NULL) {
    fprintf(stderr, "%s: error opening file: %s\n",
	    progname, strerror(errno));
    return 1;
  }

  _riff_walk(riff, NULL, 0);

  ret = riff_close(riff);
  if (ret == -1) {
    fprintf(stderr, "%s: error closing file: %s\n",
	    progname, strerror(errno));
    return 1;
  }

  return 0;
}

#elif BUILD_WAVREAD

struct wavfmt {
  uint16_t format_tag;              /* Format category */
  uint16_t channels;                /* Number of channels */
  uint32_t samples_per_sec;         /* Sampling rate */
  uint32_t avg_bytes_per_sec;       /* For buffer estimation */
  uint16_t block_align;             /* Data block size */

  uint16_t bits_per_sample;         /* Sample size */
};

static char *progname;

int
main(int argc, char *argv[])
{
  riff_t riff;
  riff_chunk_t chnk;
  struct wavfmt fmt;
  int ret;

  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file>\n", progname);
    return 1;
  }

  riff = riff_open(argv[1], RIFF_RDONLY);
  if (riff == NULL) {
    fprintf(stderr, "%s: error opening file: %s\n",
	    progname, strerror(errno));
    return 1;
  }

  riff_descend(riff, &chnk, NULL, RIFF_SRCH_OFF);
  chnk.id = riff_string_to_fourcc("fmt ");
  ret = riff_descend(riff, &chnk, NULL, RIFF_SRCH_FLAT);
  if (ret == -1) {
    fprintf(stderr, "%s: error searching for WAV header: %s\n",
	    progname, strerror(errno));
    return 1;
  } else if (ret == 0) {
    printf("WAV header not found\n");
  } else {
    printf("WAV header found:\n");
    /*fread(&fmt, sizeof(struct wavfmt), 1, riff_chunk_get_stream(chnk));*/
    fread(&fmt, sizeof(struct wavfmt), 1, riff->fp);
#ifdef WORDS_BIGENDIAN
    fmt.format_tag        = bswap_16(fmt.format_tag);
    fmt.channels          = bswap_16(fmt.channels);
    fmt.samples_per_sec   = bswap_32(fmt.samples_per_sec);
    fmt.avg_bytes_per_sec = bswap_32(fmt.avg_bytes_per_sec);
    fmt.block_align       = bswap_16(fmt.block_align);
    fmt.bits_per_sample   = bswap_16(fmt.bits_per_sample);
#endif
    printf("\
  format tag=%d\n\
  channels=%d\n\
  sample rate=%d\n\
  avg bytes per sec=%d\n\
  block align=%d\n\
  sample size=%d\n\
",
	   fmt.format_tag, fmt.channels, fmt.samples_per_sec,
	   fmt.avg_bytes_per_sec, fmt.block_align, fmt.bits_per_sample);
  }

  ret = riff_close(riff);
  if (ret == -1) {
    fprintf(stderr, "%s: error closing file: %s\n",
	    progname, strerror(errno));
    return 1;
  }

  return 0;
}

#endif
