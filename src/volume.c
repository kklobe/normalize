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
#if USE_AUDIOFILE
# include <audiofile.h>
#else
# include "wiener_af.h"
#endif

#ifdef ENABLE_NLS
# define _(msgid) gettext (msgid)
# include <libintl.h>
#else
# define _(msgid) (msgid)
#endif
#define N_(msgid) (msgid)

#include "common.h"

#undef DEBUG

#if USE_MAD
extern double signal_max_power_mp3(char *filename, struct signal_info *si);
extern int strncaseeq(const char *s1, const char *s2, size_t n);
#endif
extern void progress_callback(char *prefix, float fraction_completed);
extern char *basename(char *path);
extern void *xmalloc(size_t size);

extern char *progname;
extern int verbose;

static inline long
get_sample(unsigned char *pdata, int bytes_per_sample)
{
  long sample;

  switch(bytes_per_sample) {
  case 1:
    sample = *((int8_t *)pdata);
    break;
  case 2:
    /* audiofile returns frames with bytes in host order, so this is okay */
    sample = *((int16_t *)pdata);
    break;
  case 3:
    /* audiofile aligns 24-bit data on 32-bit boundaries,
     * so 3 and 4 are the same */
  case 4:
    /* audiofile returns frames with bytes in host order, so this is okay */
    sample = *((int32_t *)pdata);
    break;
  default:
    /* shouldn't happen */
    /* FIXME: could this happen now? */
    fprintf(stderr,
	    _("%s: I don't know what to do with %d bytes per sample\n"),
	    progname, bytes_per_sample);
    abort();
  }

  return sample;
}


typedef struct {
  double *buf;
  int buflen;  /* elements allocated to buffer */
  int start;   /* index of first element in buffer */
  int n;       /* num of elements in buffer */
} datasmooth_t;

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


/*
 * Get the maximum power level of the file
 * (and the peak sample info, if si is not NULL)
 */
double
signal_max_power(char *filename, struct signal_info *si)
{
  AFfilehandle fhin;
  AFframecount framecount;
  int samp_fmt, samp_width;

  int bytes_per_sample, framesz, frames_recvd;
  int last_window;
  unsigned int windowsz;
  unsigned int win_start, old_start, win_end, old_end;

  int i, c, end, offset;
  long sample, samplemax, samplemin;
  double *sums;
  double pow, maxpow;
  datasmooth_t *powsmooth;
  unsigned char *data_buf = NULL;

  float progress, last_progress = 0.0;
  char prefix_buf[18];
#if USE_MAD
  char *suffix;

  i = strlen(filename);
  if (i >= 4) {
    suffix = filename + i - 4;
    if (strncaseeq(suffix, ".mp3", 4))
      return signal_max_power_mp3(filename, si);
  }
#endif
#if DEBUG
  unsigned long total_frames_recvd = 0;
#endif

  fhin = afOpenFile(filename, "r", NULL);
  if (fhin == AF_NULL_FILEHANDLE)
    goto error1;

  /* pass back format info in *si */
  afGetSampleFormat(fhin, AF_DEFAULT_TRACK, &samp_fmt, &samp_width);
  si->channels = afGetChannels(fhin, AF_DEFAULT_TRACK);
  si->bits_per_sample = samp_width;
  si->samples_per_sec = (unsigned int)afGetRate(fhin, AF_DEFAULT_TRACK);

  /* set virtual format to be always 2's complement */
  afSetVirtualSampleFormat(fhin, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, samp_width);

  bytes_per_sample = (si->bits_per_sample - 1) / 8 + 1;
  samplemax = (1 << (bytes_per_sample * 8 - 1)) - 1;
  samplemin = -samplemax - 1;
  framecount = afGetFrameCount(fhin, AF_DEFAULT_TRACK);

  /* initialize peaks to effectively -inf and +inf */
  si->max_sample = samplemin;
  si->min_sample = samplemax;
#if DEBUG
  if (verbose >= VERBOSE_DEBUG) {
    fprintf(stderr,
	    "bytes_per_sample: %d framecount: %ld\n"
	    "samplemax: %ld samplemin: %ld\n",
	    bytes_per_sample, framecount, samplemax, samplemin);
  }
#endif

  sums = (double *)xmalloc(si->channels * sizeof(double));
  for (c = 0; c < si->channels; c++)
    sums[c] = 0;

  /* set up buffer to hold 1/100 of a second worth of frames */
  windowsz = (unsigned int)(si->samples_per_sec / 100);
  /* we care about the virtual frame size, not the format's particular
     frame size, which might be zero */
  /*framesz = afGetFrameSize(fhin, AF_DEFAULT_TRACK, 1);*/
  framesz = si->channels * bytes_per_sample;
  /* We use 4 bytes for 24-bit samples, adjust the frame size */
  if (bytes_per_sample == 3)
    framesz += si->channels;
  data_buf = (unsigned char *)xmalloc(windowsz * framesz);

  /* set up smoothing window buffer */
  powsmooth = (datasmooth_t *)xmalloc(si->channels * sizeof(datasmooth_t));
  for (c = 0; c < si->channels; c++) {
    powsmooth[c].buflen = 100; /* use a 100-element (1 second) window */
    powsmooth[c].buf = (double *)xmalloc(powsmooth[c].buflen * sizeof(double));
    powsmooth[c].start = powsmooth[c].n = 0;
  }

  /* initialize progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    strncpy(prefix_buf, basename(filename), 17);
    prefix_buf[17] = '\0';
    progress_callback(prefix_buf, 0.0);
    last_progress = 0.0;
  }


  /*
   * win_start, win_end, old_start, windowsz, interval, and i are in
   * units of frames.  c is in units of channels.
   *
   * The actual window extends from win_start to win_end - 1, inclusive.
   */
  old_start = win_start = 0;
  win_end = 0;
  last_window = FALSE;
  maxpow = 0.0;

  do {

    /* set up the window end */
    old_end = win_end;
    win_end = win_start + windowsz;
    if (win_end >= framecount) {
      win_end = framecount;
      last_window = TRUE;
    }

    /* read a windowsz sized chunk of frames */
    frames_recvd = afReadFrames(fhin, AF_DEFAULT_TRACK, data_buf, windowsz);
#if DEBUG
    printf("frames: %d\n", frames_recvd);
#endif
    if (frames_recvd == -1)
      goto error2;
    if (frames_recvd == 0)
      break;

#if DEBUG
    total_frames_recvd += frames_recvd;
#endif

    for (c = 0; c < si->channels; c++) {
      sums[c] = 0;
      offset = c * (framesz / si->channels);
      for (i = 0; i < (win_end - win_start); i++) {
	sample = get_sample(data_buf + offset, bytes_per_sample);
	offset += framesz;
	sums[c] += sample * (double)sample;
	/* track peak */
	if (sample > si->max_sample)
	  si->max_sample = sample;
	if (sample < si->min_sample)
	  si->min_sample = sample;
      }
    }

    /* compute power for each channel */
    for (c = 0; c < si->channels; c++) {
      pow = sums[c] / (double)(win_end - win_start);

      end = (powsmooth[c].start + powsmooth[c].n) % powsmooth[c].buflen;
      powsmooth[c].buf[end] = pow;
      if (powsmooth[c].n == powsmooth[c].buflen) {
	powsmooth[c].start = (powsmooth[c].start + 1) % powsmooth[c].buflen;
	pow = get_smoothed_data(&powsmooth[c]);
	if (pow > maxpow)
	  maxpow = pow;
      } else {
	powsmooth[c].n++;
      }
    }

    /* update progress meter */
    if (verbose >= VERBOSE_PROGRESS) {
      if (framecount - windowsz == 0)
	progress = 0;
      else
	progress = (win_end - windowsz) / (float)(framecount - windowsz);
      if (progress >= last_progress + 0.01) {
	progress_callback(prefix_buf, progress);
	last_progress += 0.01;
      }
    }

    /* slide the window ahead */
    old_start = win_start;
    win_start += windowsz;

  } while (!last_window);

#if DEBUG
    printf("%lu samples read\n", total_frames_recvd * si->channels);
#endif

  if (maxpow < EPSILON) {
    /*
     * Either this whole file has zero power, or was too short to ever
     * fill the smoothing buffer.  In the latter case, we need to just
     * get maxpow from whatever data we did collect.
     */
    for (c = 0; c < si->channels; c++) {
      pow = get_smoothed_data(&powsmooth[c]);
      if (pow > maxpow)
	maxpow = pow;
    }
  }

  for (c = 0; c < si->channels; c++)
    free(powsmooth[c].buf);
  free(powsmooth);
  free(data_buf);
  free(sums);

  /* scale the pow value to be in the range 0.0 -- 1.0 */
  maxpow = maxpow / (samplemin * (double)samplemin);

  /* fill in the signal_info struct */
  si->level = sqrt(maxpow);
  if (-si->min_sample > si->max_sample)
    si->peak = si->min_sample / (double)samplemin;
  else
    si->peak = si->max_sample / (double)samplemax;

  afCloseFile(fhin);

  return maxpow;

  /* error handling stuff */
 error2:
  for (c = 0; c < si->channels; c++)
    free(powsmooth[c].buf);
  free(powsmooth);
  free(data_buf);
  free(sums);
  afCloseFile(fhin);
 error1:
  return -1.0;
}

#if 0
/*
 * Get the maximum power level of the data read from a stream
 * (and the peak sample, if ppeak is not NULL)
 */
double
signal_max_power_stream(FILE *in, char *filename, struct signal_info *si)

{
  struct wavfmt *fmt;
  int bytes_per_sample;
  int last_window;
  unsigned int windowsz;
  unsigned int win_start, old_start, win_end, old_end;

  int i, c;
  long sample, samplemax, samplemin;
  double *sums;
  double pow, maxpow;
  datasmooth_t *powsmooth;

  char prefix_buf[18];

  unsigned char *data_buf = NULL;
  int filled_sz;

  if (filename == NULL || strcmp(filename, "-") == 0)
    filename = "STDIN";

  /* WAV format info must be passed to us in si->fmt */
  fmt = &si->fmt;

  windowsz = (unsigned int)(fmt->samples_per_sec / 100);

  bytes_per_sample = (fmt->bits_per_sample - 1) / 8 + 1;
  samplemax = (1 << (bytes_per_sample * 8 - 1)) - 1;
  samplemin = -samplemax - 1;
  /* initialize peaks to effectively -inf and +inf */
  si->max_sample = samplemin;
  si->min_sample = samplemax;

  sums = (double *)xmalloc(fmt->channels * sizeof(double));
  for (c = 0; c < fmt->channels; c++)
    sums[c] = 0;

  data_buf = (unsigned char *)xmalloc(windowsz
				      * fmt->channels * bytes_per_sample);

  /* set up smoothing window buffer */
  powsmooth = (datasmooth_t *)xmalloc(fmt->channels * sizeof(datasmooth_t));
  for (c = 0; c < fmt->channels; c++) {
    powsmooth[c].buflen = 100; /* use a 100-element (1 second) window */
    powsmooth[c].buf = (double *)xmalloc(powsmooth[c].buflen * sizeof(double));
    powsmooth[c].start = powsmooth[c].n = 0;
  }

  /* initialize progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    strncpy(prefix_buf, basename(filename), 17);
    prefix_buf[17] = 0;
    progress_callback(prefix_buf, 0.0);
  }


  /*
   * win_start, win_end, old_start, windowsz, interval, and i are in
   * units of samples.  c is in units of channels.
   *
   * The actual window extends from win_start to win_end - 1, inclusive.
   */
  old_start = win_start = 0;
  win_end = 0;
  last_window = FALSE;
  maxpow = 0.0;

  do {

    /* set up the window end */
    old_end = win_end;
    win_end = win_start + windowsz;

    /* read a windowsz sized chunk */
    filled_sz = fread(data_buf, bytes_per_sample,
		      windowsz * fmt->channels, in);

    /* if we couldn't read a complete chunk, then this is the last chunk */
    if (filled_sz < windowsz * fmt->channels) {
      win_end = win_start + (filled_sz / fmt->channels);
      last_window = TRUE;
    }

    for (c = 0; c < fmt->channels; c++) {
      sums[c] = 0;
      for (i = 0; i < (win_end - win_start); i++) {
	sample = get_sample(data_buf + (i * fmt->channels * bytes_per_sample)
			    + (c * bytes_per_sample), bytes_per_sample);
	sums[c] += sample * (double)sample;
	/* track peak */
	if (sample > si->max_sample)
	  si->max_sample = sample;
	if (sample < si->min_sample)
	  si->min_sample = sample;
      }
    }

    /* compute power for each channel */
    for (c = 0; c < fmt->channels; c++) {
      int end;
      pow = sums[c] / (double)(win_end - win_start);

      end = (powsmooth[c].start + powsmooth[c].n) % powsmooth[c].buflen;
      powsmooth[c].buf[end] = pow;
      if (powsmooth[c].n == powsmooth[c].buflen) {
	powsmooth[c].start = (powsmooth[c].start + 1) % powsmooth[c].buflen;
	pow = get_smoothed_data(&powsmooth[c]);
	if (pow > maxpow)
	  maxpow = pow;
      } else {
	powsmooth[c].n++;
      }
    }

    /* slide the window ahead */
    old_start = win_start;
    win_start += windowsz;

  } while (!last_window);

  if (maxpow < EPSILON) {
    /*
     * Either this whole file has zero power, or was too short to ever
     * fill the smoothing buffer.  In the latter case, we need to just
     * get maxpow from whatever data we did collect.
     */
    for (c = 0; c < fmt->channels; c++) {
      pow = get_smoothed_data(&powsmooth[c]);
      if (pow > maxpow)
	maxpow = pow;
    }
  }

  for (c = 0; c < fmt->channels; c++)
    free(powsmooth[c].buf);
  free(powsmooth);
  free(data_buf);
  free(sums);

  /* scale the pow value to be in the range 0.0 -- 1.0 */
  maxpow = maxpow / (samplemin * (double)samplemin);

  /* fill in the signal_info struct */
  si->level = sqrt(maxpow);
  if (-si->min_sample > si->max_sample)
    si->peak = si->min_sample / (double)samplemin;
  else
    si->peak = si->max_sample / (double)samplemax;

  return maxpow;
}
#endif
