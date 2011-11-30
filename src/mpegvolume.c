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

/* interface to the mad decoding library */

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

#include <mad.h>

#include "common.h"

extern void progress_callback(char *prefix, float fraction_completed);
extern char *basename(char *path);
extern void *xmalloc(size_t size);

extern char *progname;
extern int verbose;

#define MPEG_BUFSZ 40000
#define samplemax 32767
#define samplemin -32768
#define bytes_per_sample 2

typedef struct {
  double *buf;
  int buflen;  /* elements allocated to buffer */
  int start;   /* index of first element in buffer */
  int n;       /* num of elements in buffer */
} datasmooth_t;

struct decode_struct {
  FILE *in;
  unsigned char buffer[MPEG_BUFSZ + MAD_BUFFER_GUARD];
  int buflen;
  struct signal_info *si;
  char *filename;
  off_t file_offset;
  int eof;

  double sums[2];
  double maxpow;
  datasmooth_t powsmooth[2];

  unsigned int windowsz;
  unsigned int samples_so_far;

  float last_progress;
  char prefix_buf[18];
};

static enum mad_flow decode_input(void *, struct mad_stream *);
static enum mad_flow decode_output(void *, struct mad_header const *, struct mad_pcm *);
static enum mad_flow decode_error(void *, struct mad_stream *, struct mad_frame *);

/*
 * Takes a full smoothing window, and returns the value of the center
 * element, smoothed.  Currently, just does a mean filter, but we could
 * do a median or gaussian filter here instead.
 */
static inline double
get_smoothed_data(datasmooth_t *s)
{
  int i;
  /*int center = (s->n + 1) / 2;*/
  double smoothed;

  smoothed = 0;
  for (i = 0; i < s->n; i++)
    smoothed += s->buf[i];
  smoothed = smoothed / s->n;

  return smoothed;
}


static void
get_window_power(struct decode_struct *ds)
{
  double pow;
  int c, end;

  /* compute the power of the current window */
  for (c = 0; c < ds->si->channels; c++) {
    pow = ds->sums[c] / (double)ds->samples_so_far;
    ds->sums[c] = 0;
    end = ((ds->powsmooth[c].start + ds->powsmooth[c].n)
	   % ds->powsmooth[c].buflen);
    ds->powsmooth[c].buf[end] = pow;
    if (ds->powsmooth[c].n == ds->powsmooth[c].buflen) {
      ds->powsmooth[c].start = ((ds->powsmooth[c].start + 1)
				% ds->powsmooth[c].buflen);
      pow = get_smoothed_data(&ds->powsmooth[c]);
      if (pow > ds->maxpow)
	ds->maxpow = pow;
    } else {
      ds->powsmooth[c].n++;
    }
  }

  ds->samples_so_far = 0;
}


/*
 * Get the maximum power level of the mp3 file
 * (and the peak sample, if si is not NULL)
 */
double
signal_max_power_mp3(char *filename, struct signal_info *si)
{
  int c, result;
  struct decode_struct ds;
  struct mad_decoder decoder;

  ds.in = fopen(filename, "rb");
  if (ds.in == NULL)
    return -1.0;

  ds.si = si;
  ds.filename = filename;
  ds.buflen = 0;
  ds.file_offset = 0;
  ds.eof = 0;
  ds.windowsz = 0;
  ds.samples_so_far = 0;
  /* initialize peaks to effectively -inf and +inf */
  si->max_sample = samplemin;
  si->min_sample = samplemax;


  /* initialize progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    strncpy(ds.prefix_buf, basename(filename), 17);
    ds.prefix_buf[17] = '\0';
    progress_callback(ds.prefix_buf, 0.0);
    ds.last_progress = 0.0;
  }

  /* initialize and start decoder */
  mad_decoder_init(&decoder, &ds,
		   decode_input /* input */,
		   NULL /* header */,
		   NULL /* filter */,
		   decode_output /* output */,
		   decode_error /* error */,
		   NULL /* message */);

  mad_decoder_options(&decoder, MAD_OPTION_IGNORECRC);

  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  mad_decoder_finish(&decoder);

  fclose(ds.in);

  if (ds.samples_so_far > 0) {
    /* compute the power of the remaining partial window */
    get_window_power(&ds);
  }

  /* cleanup */
  for (c = 0; c < ds.si->channels; c++)
    if (ds.powsmooth[c].buf)
      free(ds.powsmooth[c].buf);

  /* scale the pow value to be in the range 0.0 -- 1.0 */
  ds.maxpow = ds.maxpow / (samplemin * (double)samplemin);

  /* fill in the signal_info struct */
  ds.si->level = sqrt(ds.maxpow);
  if (-ds.si->min_sample > ds.si->max_sample)
    ds.si->peak = ds.si->min_sample / (double)samplemin;
  else
    ds.si->peak = ds.si->max_sample / (double)samplemax;

  if (result == -1)
    return -1.0;

  return ds.maxpow;
}

static enum mad_flow
decode_input(void *dat, struct mad_stream *ms)
{
  struct decode_struct *ds = (struct decode_struct *)dat;
  size_t ret;
  float progress;

  if (ds->eof)
    return MAD_FLOW_STOP;

  if (ms->next_frame) {
    ds->buflen = &ds->buffer[ds->buflen] - ms->next_frame;
    memmove(ds->buffer, ms->next_frame, ds->buflen);
  }

  ret = fread(ds->buffer + ds->buflen, 1, MPEG_BUFSZ - ds->buflen, ds->in);
  if (ret == 0) {
    if (ferror(ds->in))
      return MAD_FLOW_BREAK;
    ds->eof = 1;
    ret = MAD_BUFFER_GUARD;
    while (ret--)
      ds->buffer[ds->buflen++] = 0;
  }
  ds->file_offset += ret;
  ds->buflen += ret;

  mad_stream_buffer(ms, ds->buffer, ds->buflen);

  /* update progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    if (ds->si->file_size == 0)
      progress = 0;
    else
      progress = (ds->file_offset / 1024) / (float)ds->si->file_size;
    if (progress >= ds->last_progress + 0.01) {
      progress_callback(ds->prefix_buf, progress);
      ds->last_progress = progress;
    }
  }

  return MAD_FLOW_CONTINUE;
}

/* utility to scale and round samples to 16 bits */
static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static enum mad_flow
decode_output(void *dat, struct mad_header const *mh, struct mad_pcm *pcm)
{
  struct decode_struct *ds = (struct decode_struct *)dat;
  mad_fixed_t *lchan, *rchan;
  unsigned int nsamples;
  int sample, c;

  if (ds->windowsz == 0) {
    /*
     * first time through for this file, so do initialization
     */
    ds->si->bits_per_sample = 16;
    ds->sums[0] = ds->sums[1] = 0;

    /* set up smoothing window buffer */
    for (c = 0; c < 2; c++) {
      ds->powsmooth[c].buflen = 100; /* use a 100-element (1 second) window */
      ds->powsmooth[c].buf = (double *)xmalloc(ds->powsmooth[c].buflen * sizeof(double));
      ds->powsmooth[c].start = ds->powsmooth[c].n = 0;
    }
  }

  /* these fields can change in the middle of a file! */
  ds->si->channels = pcm->channels;
  ds->si->samples_per_sec = pcm->samplerate;

  /* set up buffer to hold 1/100 of a second worth of frames */
  ds->windowsz = (unsigned int)(pcm->samplerate / 100);

  nsamples = pcm->length;
  lchan = pcm->samples[0];
  rchan = pcm->samples[1];

  while (nsamples--) {

    /*
     * compute sums
     */

    /* left channel */
    sample = scale(*lchan++);
    ds->sums[0] += sample * (double)sample;
    /* track peak */
    if (sample > ds->si->max_sample)
      ds->si->max_sample = sample;
    if (sample < ds->si->min_sample)
      ds->si->min_sample = sample;

    /* right channel */
    if (pcm->channels > 1) {
      sample = scale(*rchan++);
      ds->sums[1] += sample * (double)sample;
      /* track peak */
      if (sample > ds->si->max_sample)
	ds->si->max_sample = sample;
      if (sample < ds->si->min_sample)
	ds->si->min_sample = sample;
    }

    ds->samples_so_far++;

    if (ds->samples_so_far >= ds->windowsz) {
      /* we've got a window worth of samples, so compute the power */
      get_window_power(ds);
    }
  }

  return MAD_FLOW_CONTINUE;
}

static enum mad_flow
decode_error(void *dat, struct mad_stream *ms, struct mad_frame *mf)
{
  if (MAD_RECOVERABLE(ms->error)) {
    if (verbose >= VERBOSE_DEBUG)
      fprintf(stderr, _("%s: mad error 0x%04x\n"), progname, ms->error);
    return MAD_FLOW_CONTINUE;
  }
  if (verbose >= VERBOSE_PROGRESS)
    fprintf(stderr, _("%s: unrecoverable mad error 0x%04x\n"),
	    progname, ms->error);
  errno = EINVAL;
  return MAD_FLOW_BREAK;
}
