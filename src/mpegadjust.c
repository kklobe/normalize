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
#  ifndef HAVE_STRCHR
#   define strchr index
#   define strrchr rindex
#  endif
#  ifndef HAVE_MEMCPY
#   define memcpy(d,s,n) bcopy((s),(d),(n))
#   define memmove(d,s,n) bcopy((s),(d),(n))
#  endif
# endif
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef ENABLE_NLS
# define _(msgid) gettext (msgid)
# include <libintl.h>
#else
# define _(msgid) (msgid)
#endif
#define N_(msgid) (msgid)

#include "common.h"
#include "nid3.h"

extern void progress_callback(char *prefix, float fraction_completed);
extern char *basename(char *path);
extern void *xmalloc(size_t size);

extern char *progname;
extern int verbose;
extern int id3_compat;
extern int id3_unsync;
extern double adjust_thresh;
extern int batch_mode; /* FIXME: remove */


/* adjust_id3() takes gain in decibels */
static int
adjust_id3(char *fname, double gain)
{
  id3_t tag;
  id3_frame_t fr;
  int ret = 1;
  char prefix_buf[18];

  /* initialize progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    strncpy(prefix_buf, basename(fname), 17);
    prefix_buf[17] = '\0';
    progress_callback(prefix_buf, 0.0);
  }

  tag = id3_open(fname, ID3_RDWR);
  if (tag == NULL)
    return -1;

  if (fabs(gain) < adjust_thresh) {
    /* gain is below the threshold, so we should *remove* any RVA2 frame */
    fr = id3_get_frame_by_id(tag, "RVA2");
    if (fr)
      id3_frame_delete(fr);
    fr = id3_get_frame_by_id(tag, "XRVA");
    if (fr)
      id3_frame_delete(fr);
  } else if (id3_rva_set(tag, "normalize", ID3_CHANNEL_MASTER, gain) == -1) {
    fprintf(stderr, _("%s: error reading ID3 tag\n"), progname);
    ret = -1;
    goto error_close;
  }

  if (id3_compat) {
    if (id3_set_version(tag, ID3_VERSION_2_3) == -1) {
      fprintf(stderr, _("%s: error converting tag\n"), progname);
      ret = -1;
    }
  } else {
    if (id3_set_version(tag, ID3_VERSION_2_4) == -1) {
      fprintf(stderr, _("%s: error converting tag\n"), progname);
      ret = -1;
    }
  }
  if (id3_unsync)
    id3_set_unsync(tag, 1);
  if (id3_write(tag) == -1) {
    fprintf(stderr, _("%s: error writing ID3 tag\n"), progname);
    ret = -1;
  }

 error_close:
  id3_close(tag);

  /* update progress meter */
  if (verbose >= VERBOSE_PROGRESS)
    progress_callback(prefix_buf, 1.0);

  return ret;
}


/*
 * input is read from read_fd and output is written to write_fd:
 * filename is used only for messages.
 *
 * The si pointer gives the peaks so we know if limiting is needed
 * or not.  It may be specified as NULL if this information is not
 * available.
 */
int
apply_gain_mp3(char *filename, double gain, struct signal_info *si)
{
  int ret = 0;

  gain = FRACTODB(gain); /* we want the gain in dB */

  if (!batch_mode && verbose >= VERBOSE_PROGRESS)
    fprintf(stderr, _("Applying adjustment of %0.2fdB to %s...\n"),
	    gain, filename);

  /* either set RVA2 id3 tag or adjust scale factors */
#if 0
  ret = adjust_scalefactors(filename, gain);
#else
  ret = adjust_id3(filename, gain);
#endif

  return ret;
}
