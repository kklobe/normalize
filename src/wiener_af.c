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

/*
 * This is a wiener "WAV-only" substitute for the audiofile
 * library for those who don't have the real thing.
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
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
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

#ifndef O_BINARY
# define O_BINARY 0
#endif

#include "riff.h"
#include "wiener_af.h"

extern char *progname;

struct wavfmt {
  uint16_t format_tag;              /* Format category */
  uint16_t channels;                /* Number of channels */
  uint32_t samples_per_sec;         /* Sampling rate */
  uint32_t avg_bytes_per_sec;       /* For buffer estimation */
  uint16_t block_align;             /* Data block size */

  uint16_t bits_per_sample;         /* Sample size */
};

enum openmode {
  AF_RDONLY,
  AF_WRONLY
};

struct _AFfilesetup {
  int format;
  int byte_order;
  int nchannels;
  double rate;
  int sample_format;
  int sample_width;
};

struct _AFfilehandle {
  riff_t riff;
  riff_chunk_t top_chnk;
  riff_chunk_t fmt_chnk;
  riff_chunk_t data_chnk;
  struct wavfmt fmt;
  enum openmode mode;
};


AFfilehandle
afOpenFile(const char *filename, const char *mode, AFfilesetup setup)
{
  int fd, flags;
  if (mode[0] == 'w')
    flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
  else
    flags = O_RDONLY | O_BINARY;
  fd = open(filename, flags, 0666);
  if (fd == -1)
    return AF_NULL_FILEHANDLE;
  return afOpenFD(fd, mode, setup);
}


AFfilehandle
afOpenFD(int fd, const char *mode, AFfilesetup setup)
{
  riff_t riff;
  struct wavfmt *fmt;
  int ret, bytes_per_sample;
  AFfilehandle newfh;
#ifdef WORDS_BIGENDIAN
  struct wavfmt fmt_le; /* struct wavfmt in little-endian format */
#endif

  newfh = (AFfilehandle)malloc(sizeof(struct _AFfilehandle));
  if (newfh == NULL) {
    fprintf(stderr, _("%s: unable to malloc\n"), progname);
    goto error1;
  }

  if (mode[0] == 'w') {

    /* check that we can do what the setup asks */
    if (setup->format != AF_FILE_WAVE
	|| (setup->sample_format != AF_SAMPFMT_TWOSCOMP
	    && setup->sample_format != AF_SAMPFMT_UNSIGNED)
	|| setup->byte_order != AF_BYTEORDER_LITTLEENDIAN
	|| setup->nchannels < 1) {
      /* bad setup error -- shouldn't happen in normalize */
      fprintf(stderr, "%s: internal error: bad file format\n", progname);
      goto error2;
    }
    if (setup->sample_format == AF_SAMPFMT_TWOSCOMP) {
      if (setup->sample_width <= 8) {
	/* bad setup error -- shouldn't happen in normalize */
	fprintf(stderr, "%s: Warning: forcing WAV data of <=8 bits to be unsigned\n", progname);
	setup->sample_format = AF_SAMPFMT_UNSIGNED;
      }
    } else { /* setup->sample_format == AF_SAMPFMT_UNSIGNED */
      if (setup->sample_width > 8) {
	/* bad setup error -- shouldn't happen in normalize */
	fprintf(stderr, "%s: Warning: forcing WAV data of >8 bits to be twos complement\n", progname);
	setup->sample_format = AF_SAMPFMT_TWOSCOMP;
      }
    }

    /* construct WAV fmt header from setup struct */
    fmt = &newfh->fmt;
    fmt->format_tag = 1;
    fmt->channels = setup->nchannels;
    fmt->samples_per_sec = setup->rate;
    fmt->bits_per_sample = setup->sample_width;
    bytes_per_sample = (fmt->bits_per_sample - 1) / 8 + 1;
    fmt->block_align = bytes_per_sample * fmt->channels;
    fmt->avg_bytes_per_sec = (bytes_per_sample * fmt->channels
			      * fmt->samples_per_sec);
#ifdef WORDS_BIGENDIAN
    /*
     * We need to write a little-endian wavfmt, but we want to keep
     * the wavfmt in the filehandle in big-endian so we can work with
     * it.  Therefore, we make a copy for writing and swap its bytes
     * around.
     */
    memcpy(&fmt_le, fmt, sizeof(struct wavfmt));
    fmt = &fmt_le;
    fmt->format_tag        = bswap_16(fmt->format_tag);
    fmt->channels          = bswap_16(fmt->channels);
    fmt->samples_per_sec   = bswap_32(fmt->samples_per_sec);
    fmt->avg_bytes_per_sec = bswap_32(fmt->avg_bytes_per_sec);
    fmt->block_align       = bswap_16(fmt->block_align);
    fmt->bits_per_sample   = bswap_16(fmt->bits_per_sample);
#endif

    /*
     * open for writing
     */

    riff = riff_fdopen(fd, RIFF_WRONLY);
    if (riff == NULL) {
      fprintf(stderr, _("%s: error opening WAV file: %s\n"),
	      progname, strerror(errno));
      goto error2;
    }

    newfh->top_chnk.id = RIFFID_RIFF;
    newfh->top_chnk.type = riff_string_to_fourcc("WAVE");
    newfh->top_chnk.size = 0;
    if (riff_create_chunk(riff, &newfh->top_chnk) == -1) {
      fprintf(stderr, _("%s: error writing: %s\n"), progname, strerror(errno));
      goto error3;
    }

    /* write WAV fmt header */
    newfh->fmt_chnk.id = riff_string_to_fourcc("fmt ");
    newfh->fmt_chnk.size = 0;
    riff_create_chunk(riff, &newfh->fmt_chnk);
    if (fwrite(fmt, sizeof(struct wavfmt), 1, riff_stream(riff)) < 1) {
      fprintf(stderr, "%s: unable to write WAV header: %s\n",
	      progname, strerror(errno));
      goto error3;
    }
    riff_ascend(riff, &newfh->fmt_chnk);

    /* start data chunk */
    newfh->data_chnk.id = riff_string_to_fourcc("data");
    newfh->data_chnk.size = 0;
    riff_create_chunk(riff, &newfh->data_chnk);

    newfh->riff = riff;
    newfh->mode = AF_WRONLY;

  } else {

    /*
     * open for reading
     */

    riff = riff_fdopen(fd, RIFF_RDONLY);
    if (riff == NULL) {
      fprintf(stderr, _("%s: error opening WAV file: %s\n"),
	      progname, strerror(errno));
      goto error2;
    }

    riff_descend(riff, &newfh->top_chnk, NULL, RIFF_SRCH_OFF);
    newfh->fmt_chnk.id = riff_string_to_fourcc("fmt ");
    ret = riff_descend(riff, &newfh->fmt_chnk, NULL, RIFF_SRCH_FLAT);
    if (ret == -1) {
      fprintf(stderr, _("%s: error searching for WAV header: %s\n"),
	      progname, strerror(errno));
      goto error3;
    } else if (ret == 0) {
      fprintf(stderr, _("%s: WAV header not found\n"), progname);
      goto error3;
    }

    /* WAV format info will be passed back */
    fmt = &newfh->fmt;

    fread(fmt, sizeof(struct wavfmt), 1, riff_stream(riff));
#ifdef WORDS_BIGENDIAN
    fmt->format_tag        = bswap_16(fmt->format_tag);
    fmt->channels          = bswap_16(fmt->channels);
    fmt->samples_per_sec   = bswap_32(fmt->samples_per_sec);
    fmt->avg_bytes_per_sec = bswap_32(fmt->avg_bytes_per_sec);
    fmt->block_align       = bswap_16(fmt->block_align);
    fmt->bits_per_sample   = bswap_16(fmt->bits_per_sample);
#endif

    /*
     * Make sure we can handle this type of wav
     */
    if (fmt->format_tag != 1) {
      fprintf(stderr, _("%s: this is not a PCM WAV file\n"), progname);
      goto error3;
    }
    if (fmt->bits_per_sample > 32) {
      fprintf(stderr, _("%s: more than 32 bits per sample not implemented\n"),
	      progname);
      goto error3;
    }

#if DEBUG
    if (verbose >= VERBOSE_DEBUG) {
      fprintf(stderr,
	      "fmt chunk for %s:\n"
	      "	 format_tag:	    %u\n"
	      "	 channels:	    %u\n"
	      "	 samples_per_sec:   %u\n"
	      "	 avg_bytes_per_sec: %u\n"
	      "	 block_align:	    %u\n"
	      "	 bits_per_sample:   %u\n",
	      filename, fmt->format_tag, fmt->channels, fmt->samples_per_sec,
	      fmt->avg_bytes_per_sec, fmt->block_align, fmt->bits_per_sample);
    }
#endif

    riff_ascend(riff, &newfh->fmt_chnk);
    newfh->data_chnk.id = riff_string_to_fourcc("data");
    ret = riff_descend(riff, &newfh->data_chnk, NULL, RIFF_SRCH_FLAT);
    if (ret == -1) {
      fprintf(stderr, _("%s: error searching for WAV data: %s\n"),
	      progname, strerror(errno));
      goto error3;
    } else if (ret == 0) {
      fprintf(stderr, _("%s: WAV data not found\n"), progname);
      goto error3;
    }

    newfh->riff = riff;
    newfh->mode = AF_RDONLY;
  }

  return newfh;

  /* error handling stuff */
 error3:
  riff_close(riff);
 error2:
  free(newfh);
 error1:
  return NULL;
}

int
afCloseFile(AFfilehandle fh)
{
  if (fh) {
    riff_ascend(fh->riff, &fh->data_chnk);
    riff_ascend(fh->riff, &fh->top_chnk);
    riff_close(fh->riff);
    free(fh);
  }
  return 0;
}

AFfilesetup
afNewFileSetup(void)
{
  AFfilesetup retval;
  retval = (AFfilesetup)malloc(sizeof(struct _AFfilesetup));
  if (retval) {
    /* set defaults: WAV, 44.1k, mono, 16-bit LE */
    retval->format = AF_FILE_WAVE;
    retval->byte_order = AF_BYTEORDER_LITTLEENDIAN;
    retval->nchannels = 1;
    retval->rate = 44100;
    retval->sample_format = AF_SAMPFMT_TWOSCOMP;
    retval->sample_width = 16;
  }
  return retval;
}

void
afFreeFileSetup(AFfilesetup setup)
{
  free(setup);
}

static inline int
_afGetFrameSize(AFfilehandle fh, int track, int expand3to4)
{
  int bytes_per_sample;

  bytes_per_sample = (fh->fmt.bits_per_sample - 1) / 8 + 1;
  if (bytes_per_sample == 3 && expand3to4)
    bytes_per_sample = 4;

  return bytes_per_sample * fh->fmt.channels;
}

int
afReadFrames(AFfilehandle fh, int track, void *buffer, int frame_count)
{
  int framesize, frames_recvd, samples_recvd, i;
  int offset_current, bytes_remaining, frames_remaining;
  int8_t *psrc, *pdest;
  int8_t *p8;
#ifdef WORDS_BIGENDIAN
  int32_t *p32;
  int16_t *p16;
#endif

  framesize = _afGetFrameSize(fh, track, 0);

  /* FIXME: need to update this for large file support */
  offset_current = ftell(riff_stream(fh->riff));
  bytes_remaining = fh->data_chnk.offset + fh->data_chnk.size - offset_current;
  if (bytes_remaining < 0)
    bytes_remaining = 0;
  frames_remaining = bytes_remaining / framesize;

  if (frames_remaining < frame_count)
    frame_count = frames_remaining;

  frames_recvd = fread(buffer, framesize, frame_count, riff_stream(fh->riff));
  samples_recvd = frames_recvd * fh->fmt.channels;

  if (fh->fmt.bits_per_sample <= 8) {
    /* 8-bit WAV samples are unsigned (0-255), but normalize wants
     * twos complement, so we adjust.  See afSetVirtualSampleFormat() */
    p8 = (int8_t *)buffer;
    for (i = 0; i < samples_recvd; i++) {
      *p8 = *((uint8_t *)p8) - 128;
      p8++;
    }
  } else if (fh->fmt.bits_per_sample > 16 && fh->fmt.bits_per_sample <= 24) {
    /* align 24-bit samples on 32-bit boundaries */

    /* point to 3-byte values (source) */
    psrc = (int8_t *)buffer + (samples_recvd - 1) * 3;
    /* point to 4-byte values (dest) */
    pdest = (int8_t *)buffer + (samples_recvd - 1) * 4;

    for (i = 0; i < samples_recvd; i++) {
      pdest[3] = (psrc[2] & 0x80) ? 0xFF : 0x00;
      pdest[2] = psrc[2];
      pdest[1] = psrc[1];
      pdest[0] = psrc[0];
      psrc -= 3;
      pdest -= 4;
    }
  }

#ifdef WORDS_BIGENDIAN
  /* adjust for endianness */
  if (fh->fmt.bits_per_sample > 16) {
    p32 = (int32_t *)buffer;
    for (i = 0; i < samples_recvd; i++) {
      *p32 = bswap_32(*p32);
      p32++;
    }
  } else if (fh->fmt.bits_per_sample > 8) {
    p16 = (int16_t *)buffer;
    for (i = 0; i < samples_recvd; i++) {
      *p16 = bswap_16(*p16);

      p16++;
    }
  } /* else fh->fmt.bits_per_sample <= 8, so no swapping necessary */
#endif

  return frames_recvd;
}

/*
 * WARNING: afWriteFrames messes up the contents of buffer.  This is
 * inconsistent with the real audiofile, but normalize doesn't
 * care.
 */
int
afWriteFrames(AFfilehandle fh, int track, void *buffer, int frame_count)
{
  int framesize, i, samp_count;
  int32_t samp;
  int8_t *pdest;
  int32_t *psrc;
  int8_t *p8;
#ifdef WORDS_BIGENDIAN
  int32_t *p32;
  int16_t *p16;
#endif

  framesize = _afGetFrameSize(fh, track, 0);
  samp_count = frame_count * fh->fmt.channels;

#ifdef WORDS_BIGENDIAN
  /* adjust for endianness */
  if (fh->fmt.bits_per_sample > 16) {
    p32 = (int32_t *)buffer;
    for (i = 0; i < samp_count; i++) {
      *p32 = bswap_32(*p32);
      p32++;
    }
  } else if (fh->fmt.bits_per_sample > 8) {
    p16 = (int16_t *)buffer;
    for (i = 0; i < samp_count; i++) {
      *p16 = bswap_16(*p16);
      p16++;
    }
  } /* else fh->fmt.bits_per_sample <= 8, so no swapping necessary */
#endif

  if (fh->fmt.bits_per_sample <= 8) {
    /* 8-bit WAV samples are unsigned (0-255), but normalize gives us
     * twos complement, so we adjust.  See afSetVirtualSampleFormat() */
    p8 = buffer;
    for (i = 0; i < samp_count; i++) {
      *p8 = *p8 + 128;
      p8++;
    }
  } else if (fh->fmt.bits_per_sample > 16 && fh->fmt.bits_per_sample <= 24) {
    /* 24-bit samples are aligned on 32-bit boundaries,
     * so we have to pack them together for writing */

    /* point to 4-byte values (source) */
    psrc = (int32_t *)buffer;
    /* point to 3-byte values (dest) */
    pdest = (int8_t *)buffer;

    for (i = 0; i < samp_count; i++) {
      samp = *psrc;
      /* samp is always in little-endian at this point */
#ifdef WORDS_BIGENDIAN
      samp >>= 8;
      pdest[2] = samp & 0xFF;
      samp >>= 8;
      pdest[1] = samp & 0xFF;
      samp >>= 8;
      pdest[0] = samp & 0xFF;
#else
      pdest[0] = samp & 0xFF;
      samp >>= 8;
      pdest[1] = samp & 0xFF;
      samp >>= 8;
      pdest[2] = samp & 0xFF;
#endif
      psrc++;
      pdest += 3;
    }
  }

  return fwrite(buffer, framesize, frame_count, riff_stream(fh->riff));
}

float
afGetFrameSize(AFfilehandle fh, int track, int expand3to4)
{
  return (float)(_afGetFrameSize(fh, track, expand3to4));
}

AFfileoffset
afGetTrackBytes(AFfilehandle fh, int track)
{
  return fh->data_chnk.size;
}

AFframecount
afGetFrameCount(AFfilehandle fh, int track)
{
  int framesize;
  uint32_t tracklen;

  framesize = _afGetFrameSize(fh, track, 0);
  tracklen = afGetTrackBytes(fh, track);

  return tracklen / framesize;
}

static AFerrfunc _af_err_func = NULL;

AFerrfunc
afSetErrorHandler(AFerrfunc errorFunction)
{
  AFerrfunc prev;
  prev = _af_err_func;
  _af_err_func = errorFunction;
  return prev;
}

void
afInitFileFormat(AFfilesetup setup, int format)
{
  setup->format = format;
}

void
afInitByteOrder(AFfilesetup setup, int track, int byte_order)
{
  setup->byte_order = byte_order;
}

void
afInitChannels(AFfilesetup setup, int track, int nchannels)
{
  setup->nchannels = nchannels;
}

void
afInitRate(AFfilesetup setup, int track, double rate)
{
  setup->rate = rate;
}

void
afInitSampleFormat(AFfilesetup setup, int track, int sample_format,
		   int sample_width)
{
  setup->sample_format = sample_format;
  setup->sample_width = sample_width;
}

int
afGetFileFormat(AFfilehandle fh, int *version)
{
  if (version)
    *version = 0;
  return AF_FILE_WAVE;
}

int
afGetByteOrder(AFfilehandle fh, int track)
{
  return AF_BYTEORDER_LITTLEENDIAN;
}

int
afGetChannels(AFfilehandle fh, int track)
{
  return fh->fmt.channels;
}

void
afGetSampleFormat(AFfilehandle fh, int track, int *sampfmt, int *sampwidth)
{
  if (sampfmt) {
    if (fh->fmt.bits_per_sample <= 8)
      *sampfmt = AF_SAMPFMT_UNSIGNED;
    else
      *sampfmt = AF_SAMPFMT_TWOSCOMP;
  }
  if (sampwidth) {
    *sampwidth = fh->fmt.bits_per_sample;
  }
}

double
afGetRate(AFfilehandle fh, int track)
{
  return (double)fh->fmt.samples_per_sec;
}

/*
 * normalize only uses this to ensure that the sample format is twos
 * complement, even for 8 bit.	We do this by default, so this is a
 * noop.
 */
int
afSetVirtualSampleFormat(AFfilehandle fh, int track,
			 int sampfmt, int sampwidth)
{
  return 0;
}

int
afSyncFile(AFfilehandle fh)
{
  if (fh->mode != AF_RDONLY)
    return fflush(riff_stream(fh->riff));
  return 0;
}
