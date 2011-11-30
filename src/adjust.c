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
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
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

/* warn about clipping if we clip more than this fraction of the samples */
#define CLIPPING_WARN_THRESH 0.001

/* Should we write to a temp file, which we then rename, rather than
 * just writing in place?  This must be 1 for the -w option to work.  */
#define USE_TEMPFILE 1

extern int apply_gain_mp3(char *, double, struct signal_info *);
extern int strncaseeq(const char *s1, const char *s2, size_t n);
extern void progress_callback(char *prefix, float fraction_completed);
extern char *basename(char *path);
extern void *xmalloc(size_t size);

extern char *progname;
extern int verbose;
extern int do_compute_levels;
extern int use_limiter;
extern int output_bitwidth;
extern double lmtr_lvl;
extern double adjust_thresh;
extern int batch_mode; /* FIXME: remove */

#if USE_TEMPFILE
int xmkstemp(char *template);
int xrename(const char *oldpath, const char *newpath);
#endif

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


static inline void
put_sample(long sample, unsigned char *pdata, int bytes_per_sample)
{
  switch(bytes_per_sample) {
  case 1:
    *((int8_t *)pdata) = (int8_t)sample;
    break;
  case 2:
    /* audiofile expects data in host byte order, so this is okay */
    *((int16_t *)pdata) = (int16_t)sample;
    break;
  case 3:
  case 4:
    /* audiofile expects data in host byte order, so this is okay */
    *((int32_t *)pdata) = (int32_t)sample;
    break;
  default:
    /* shouldn't happen */
    /* FIXME: could this happen now? */
    fprintf(stderr,
	    _("%s: I don't know what to do with %d bytes per sample\n"),
	    progname, bytes_per_sample);
    abort();
  }
}


/*
 * Limiter function:
 *
 *        / tanh((x + lev) / (1-lev)) * (1-lev) - lev        (for x < -lev)
 *        |
 *   x' = | x                                                (for |x| <= lev)
 *        |
 *        \ tanh((x - lev) / (1-lev)) * (1-lev) + lev        (for x > lev)
 *
 * With limiter level = 0, this is equivalent to a tanh() function;
 * with limiter level = 1, this is equivalent to clipping.
 */
static double
limiter(double x)
{
  double xp;

  if (x < -lmtr_lvl)
    xp = tanh((x + lmtr_lvl) / (1-lmtr_lvl)) * (1-lmtr_lvl) - lmtr_lvl;
  else if (x <= lmtr_lvl)
    xp = x;
  else
    xp = tanh((x - lmtr_lvl) / (1-lmtr_lvl)) * (1-lmtr_lvl) + lmtr_lvl;

  return xp;
}


/*
 * input is read from read_fd and output is written to write_fd:
 * filename is used only for messages.
 *
 * The si pointer gives the peaks so we know if limiting is needed
 * or not.  It may be specified as NULL if this information is not
 * available.
 */
static int
_do_apply_gain(int read_fd, int write_fd, char *filename, double gain,
	       struct signal_info *si)
{
  AFfilehandle fhin, fhout;
  AFframecount framecount;
  AFfilesetup setup;
  int i, c, af_fmt;
  int src_bytes_per_samp, dst_bytes_per_samp, src_framesz, dst_framesz;
  int channels, samp_fmt, src_samp_width, dst_samp_width, fmt_vers;
  unsigned int samp_rate, frames_done, nclippings;
  long sample, src_samplemax, src_samplemin, dst_samplemax, dst_samplemin;
  double sample_d;
  float clip_loss;

  float last_progress = 0, progress;
  char prefix_buf[18];

  unsigned char *src_buf = NULL, *dst_buf = NULL, *src_pos, *dst_pos;
  int frames_in_buf, frames_recvd;
  int use_limiter_this_file;
#if USE_LOOKUPTABLE
  int min_pos_clipped = 0; /* the minimum positive sample that gets clipped */
  int max_neg_clipped = 0; /* the maximum negative sample that gets clipped */
  int32_t *lut = NULL;
#endif

  /* FIXME: abort on any and all errors (in case using temp file) */

  setup = afNewFileSetup();
  if (setup == AF_NULL_FILESETUP) {
    fprintf(stderr, _("%s: afNewFileSetup failed\n"), progname);
    goto error1;
  }

  fhin = afOpenFD(read_fd, "r", NULL);
  if (fhin == AF_NULL_FILEHANDLE) {
    fprintf(stderr, _("%s: afOpenFD failed\n"), progname);
    goto error2;
  }

  /* construct audiofile setup object */
  af_fmt = afGetFileFormat(fhin, &fmt_vers);
  afInitFileFormat(setup, af_fmt);
  afInitByteOrder(setup, AF_DEFAULT_TRACK, afGetByteOrder(fhin, AF_DEFAULT_TRACK));
  channels = afGetChannels(fhin, AF_DEFAULT_TRACK);
  afInitChannels(setup, AF_DEFAULT_TRACK, channels);
  afGetSampleFormat(fhin, AF_DEFAULT_TRACK, &samp_fmt, &src_samp_width);
  dst_samp_width = src_samp_width;
  if (output_bitwidth) {
    dst_samp_width = output_bitwidth;
    if (af_fmt == AF_FILE_WAVE)
      samp_fmt = (dst_samp_width > 8) ? AF_SAMPFMT_TWOSCOMP : AF_SAMPFMT_UNSIGNED;
  }
  afInitSampleFormat(setup, AF_DEFAULT_TRACK, samp_fmt, dst_samp_width);
  afInitRate(setup, AF_DEFAULT_TRACK, afGetRate(fhin, AF_DEFAULT_TRACK));
  samp_rate = (unsigned int)afGetRate(fhin, AF_DEFAULT_TRACK);

  fhout = afOpenFD(write_fd, "w", setup);
  if (fhout == AF_NULL_FILEHANDLE) {
    fprintf(stderr, _("%s: afOpenFD failed\n"), progname);
    goto error3;
  }

  /* set virtual format to be always 2's complement */
  afSetVirtualSampleFormat(fhin, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, src_samp_width);
  afSetVirtualSampleFormat(fhout, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, dst_samp_width);

  src_bytes_per_samp = (src_samp_width - 1) / 8 + 1;
  src_samplemax = (1 << (src_bytes_per_samp * 8 - 1)) - 1;
  src_samplemin = -src_samplemax - 1;
  dst_bytes_per_samp = (dst_samp_width - 1) / 8 + 1;
  dst_samplemax = (1 << (dst_bytes_per_samp * 8 - 1)) - 1;
  dst_samplemin = -dst_samplemax - 1;

  /* ignore different channels, apply gain to all samples */
  framecount = afGetFrameCount(fhin, AF_DEFAULT_TRACK);

  /* set up buffer to hold 1/100 of a second worth of frames */
  frames_in_buf = samp_rate / 100;
  src_framesz = afGetFrameSize(fhin, AF_DEFAULT_TRACK, 1);
  dst_framesz = afGetFrameSize(fhout, AF_DEFAULT_TRACK, 1);
  src_buf = (unsigned char *)xmalloc(frames_in_buf * src_framesz);
  dst_buf = (unsigned char *)xmalloc(frames_in_buf * dst_framesz);


  /*
   * Check if we actually need to do limiting on this file:
   * we don't if gain <= 1 or if the peaks wouldn't clip anyway.
   */
  use_limiter_this_file = use_limiter && gain > 1.0;
  if (use_limiter_this_file && si) {
    if (si->max_sample * gain <= src_samplemax
	&& si->min_sample * gain >= src_samplemin)
      use_limiter_this_file = FALSE;
  }

  /*
   * Change gain to account for different output sample width
   */
  if (dst_bytes_per_samp != src_bytes_per_samp)
    gain *= pow(256.0, dst_bytes_per_samp - src_bytes_per_samp);


#if USE_LOOKUPTABLE
  /*
   * If input samples are 16 bits or less, build a lookup table for
   * fast adjustment.  This table is 256k, look out!
   */
  if (src_bytes_per_samp <= 2) {
    lut = (int32_t *)xmalloc((src_samplemax - src_samplemin + 1) * sizeof(int32_t));
    lut -= src_samplemin; /* so indices don't have to be offset */
    min_pos_clipped = src_samplemax + 1;
    max_neg_clipped = src_samplemin - 1;
    if (gain > 1.0) {
      if (use_limiter_this_file) {
	/* apply gain, and apply limiter to avoid clipping */
	for (i = src_samplemin; i < 0; i++)
	  lut[i] = ROUND(-dst_samplemin * limiter(i * gain / (double)-dst_samplemin));
	for (; i <= src_samplemax; i++)
	  lut[i] = ROUND(dst_samplemax * limiter(i * gain / (double)dst_samplemax));
      } else {
	/* apply gain, and do clipping */
	for (i = src_samplemin; i <= src_samplemax; i++) {
	  sample = ROUND(i * gain);
	  if (sample > dst_samplemax) {
	    sample = dst_samplemax;
	    if (i < min_pos_clipped)
	      min_pos_clipped = i;
	  } else if (sample < dst_samplemin) {
	    sample = dst_samplemin;
	    if (i > max_neg_clipped)
	      max_neg_clipped = i;
	  }
	  lut[i] = sample; /* negative indices are okay, see above */
	}
      }
    } else {
      /* just apply gain if it's less than 1 */
      for (i = src_samplemin; i <= src_samplemax; i++)
	lut[i] = ROUND(i * gain);
    }
#if 0
    {
      /* write the lookup table function for display in gnuplot */
      FILE *tblout = fopen("lut.dat", "w");
      for (i = src_samplemin; i <= src_samplemax; i++)
	fprintf(tblout, "%d %d\n", i, lut[i]);
      fclose(tblout);
    }
#endif
  }
#endif

  /* initialize progress meter */
  if (verbose >= VERBOSE_PROGRESS) {
    strncpy(prefix_buf, basename(filename), 17);
    prefix_buf[17] = 0;
    progress_callback(prefix_buf, 0.0);
    last_progress = 0.0;
  }

  /* read, apply gain, and write, one chunk at time */
  nclippings = frames_done = 0;
  while ((frames_recvd = afReadFrames(fhin, AF_DEFAULT_TRACK, src_buf, frames_in_buf)) > 0) {
#if USE_LOOKUPTABLE
    if (lut) {
      /* use the lookup table if we built one */

      /* FIXME: is the loop order here causing bad cache behavior? */
      for (c = 0; c < channels; c++) {
	src_pos = src_buf + c * (src_framesz / channels);
	dst_pos = dst_buf + c * (dst_framesz / channels);
	for (i = 0; i < frames_recvd; i++) {

	  sample = get_sample(src_pos, src_bytes_per_samp);

	  if (!use_limiter
	      && (sample >= min_pos_clipped || sample <= max_neg_clipped))
	    nclippings++;

	  sample = lut[sample];

	  put_sample(sample, dst_pos, dst_bytes_per_samp);

	  src_pos += src_framesz;
	  dst_pos += dst_framesz;
	}
      }

    } else {
#endif
      /* no lookup table, do it by hand */

      if (use_limiter_this_file && verbose >= VERBOSE_INFO)
	fprintf(stderr,
	_("%s: Warning: no lookup table available; this may be slow...\n"),
		progname);

      for (c = 0; c < channels; c++) {
	src_pos = src_buf + c * (src_framesz / channels);
	dst_pos = dst_buf + c * (dst_framesz / channels);
	for (i = 0; i < frames_recvd; i++) {

	  sample = get_sample(src_pos, src_bytes_per_samp);

	  /* apply the gain to the sample */
	  sample_d = sample * gain;

	  if (gain > 1.0) {
	    if (use_limiter_this_file) {
	      /* use limiter function instead of clipping */
	      sample = ROUND(dst_samplemax * limiter(sample_d/(double)dst_samplemax));
	    } else {
	      sample = ROUND(sample_d);
	      /* perform clipping */
	      if (sample_d > dst_samplemax) {
		sample = dst_samplemax;
		nclippings++;
	      } else if (sample_d < dst_samplemin) {
		sample = dst_samplemin;
		nclippings++;
	      }
	    }
	  } else { /* gain <= 1.0 */
	    sample = ROUND(sample_d);
	  }

	  put_sample(sample, dst_pos, dst_bytes_per_samp);

	  src_pos += src_framesz;
	  dst_pos += dst_framesz;
	}
      }
#if USE_LOOKUPTABLE
    }
#endif

    if (afWriteFrames(fhout, AF_DEFAULT_TRACK, dst_buf, frames_recvd) == -1) {
      fprintf(stderr, _("%s: afWriteFrames failed\n"), progname);
      goto error4;
    }

    frames_done += frames_recvd;

    /* update progress meter */
    if (verbose >= VERBOSE_PROGRESS) {
      progress = frames_done / (float)framecount;
      if (progress >= last_progress + 0.01) {
	progress_callback(prefix_buf, progress);
	last_progress += 0.01;
      }
    }
  }

  /* make sure progress meter is finished */
  if (verbose >= VERBOSE_PROGRESS)
    progress_callback(prefix_buf, 1.0);

  if (!use_limiter_this_file) {
    clip_loss = (float)nclippings / (framecount * (float)channels);

    if (verbose >= VERBOSE_INFO) {
      if (nclippings) {
	fprintf(stderr, "\n");
	fprintf(stderr, _("%s: %d clippings performed, %.4f%% loss\n"),
		progname, nclippings, clip_loss * 100);
      }
    } else if (verbose >= VERBOSE_PROGRESS) {
      if (clip_loss > CLIPPING_WARN_THRESH)
	fprintf(stderr,
        _("%s: Warning: lost %0.2f%% of data due to clipping              \n"),
		progname, clip_loss * 100);
    }
  }

#if USE_LOOKUPTABLE
  if (lut) {
    /* readjust the pointer to the beginning of the array */
    lut += src_samplemin;
    free(lut);
  }
#endif
  if (afSyncFile(fhout) < 0)
    fprintf(stderr, _("%s: afSyncFile failed\n"), progname);
  afCloseFile(fhin);
  afCloseFile(fhout);
  afFreeFileSetup(setup);
  free(src_buf);
  free(dst_buf);

  return 0;


  /* error handling stuff */
 error4:
  free(src_buf);
  free(dst_buf);
  afCloseFile(fhout);
 error3:
  afCloseFile(fhin);
 error2:
  afFreeFileSetup(setup);
 error1:
  return -1;
}

/*
 * Apply the gain to the given file.
 *
 * The si pointer gives the peaks so we know if limiting is needed
 * or not.  It may be specified as NULL if this information is not
 * available.
 *
 * returns: 1 if the gain is actually applied
 *          0 if the gain is not applied, but there were no errors
 *         -1 if there was an error
 */
int
apply_gain(char *filename, double gain, struct signal_info *si)
{
  int i;
  int read_fd, write_fd;
  char *suffix;
  double dBdiff;
#if USE_TEMPFILE
  struct stat stbuf;
  char *tmpfile, *p;
#endif

  /* defer to specialized function for mp3 files */
  i = strlen(filename);
  if (i >= 4) {
    suffix = filename + i - 4;
    if (strncaseeq(suffix, ".mp3", 4)
	|| strncaseeq(suffix, ".mp2", 4))
      return apply_gain_mp3(filename, gain, si);
  }

  dBdiff = FRACTODB(gain);

  /* if !do_compute_levels, -g was specified, so we force the adjust */
  if (do_compute_levels) {
    if (fabs(dBdiff) < adjust_thresh) {
      /* gain is below the threshold, so don't apply */
      return 0;
    }
  }

  if (!batch_mode && verbose >= VERBOSE_PROGRESS)
    fprintf(stderr, _("Applying adjustment of %0.2fdB to %s...\n"),
	    dBdiff, filename);

  /* open a descriptor for reading */
  read_fd = open(filename, O_RDONLY | O_BINARY);
  if (read_fd == -1) {
    fprintf(stderr, _("%s: error opening %s: %s\n"), progname, filename,
	    strerror(errno));
    return -1;
  }

#if USE_TEMPFILE
  /* Create temporary file name, and open it for writing.  We want it
   * to be in the same directory (and therefore, in the same
   * filesystem) for a fast rename later. */
  tmpfile = (char *)xmalloc(strlen(filename) + 16);
  strcpy(tmpfile, filename);
  p = basename(tmpfile);
  strcpy(p, "_normXXXXXX");
  write_fd = xmkstemp(tmpfile);
  if (write_fd == -1) {
    fprintf(stderr, _("%s: error opening temp file: %s\n"), progname,
	    strerror(errno));
    close(read_fd);
    free(tmpfile);
    return -1;
  }

  /* preserve original permissions */
  fstat(read_fd, &stbuf);
  chmod(tmpfile, stbuf.st_mode); /* fchmod() not posix */
#else
  /* open a write descriptor on the same file */
  write_fd = open(filename, O_WRONLY | O_BINARY);
  if (write_fd == -1) {
    fprintf(stderr, _("%s: error opening %s: %s\n"), progname, filename,
	    strerror(errno));
    close(read_fd);
    return -1;
  }
#endif

  /*
   * We have opened the descriptors; call _do_apply_gain() to do the
   * real work.  _do_apply_gain() also closes the descriptors.
   */
  if (_do_apply_gain(read_fd, write_fd, filename, gain, si) == -1) {
#if USE_TEMPFILE
    free(tmpfile);
#endif
    return -1;
  }

#if USE_TEMPFILE
  /* move the temporary file back to the original file */
  if (xrename(tmpfile, filename) == -1) {
    fprintf(stderr, _("%s: error moving %s to %s: %s\n"), progname,
	    tmpfile, filename, strerror(errno));
    free(tmpfile);
    return -1;
  }
  free(tmpfile);
#endif

  return 1;
}


#if USE_TEMPFILE
/*
 * This works like the BSD mkstemp, except that we don't unlink the
 * file, since we end up renaming it to something else.
 */
int
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
int
xrename(const char *oldpath, const char *newpath)
{
  FILE *in, *out;
  char buf[4096];
  size_t sz;

  if (strcmp(oldpath, newpath) == 0)
    return 0;

#if defined(__EMX__) || defined(HAVE_DOS_FILE_NAMES)
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

#endif /* USE_TEMPFILE */
