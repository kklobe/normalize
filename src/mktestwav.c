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
# include <math.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(); char *strrchr();
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
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

#ifndef M_PI
# define M_PI 3.14159265358979323846  /* pi */
#endif

/* sqrt(2)/2 is the RMS amplitude of a 1-amplitude sine wave */
#define SQRT2_2 0.70710678118654752440 /* sqrt(2) / 2 */

#include "getopt.h"

#include "common.h"
#include "riff.h"

struct wavfmt {
  uint16_t format_tag;              /* Format category */
  uint16_t channels;                /* Number of channels */
  uint32_t samples_per_sec;         /* Sampling rate */
  uint32_t avg_bytes_per_sec;       /* For buffer estimation */
  uint16_t block_align;             /* Data block size */

  uint16_t bits_per_sample;         /* Sample size */
};

extern char version[];
char *progname;

void
usage()
{
  fprintf(stderr, "\
Usage: %s [OPTION]... [FILE]\n\
Create a WAV file containing a sine wave of a given\n\
amplitude and frequency\n\
\n\
  -a, --amplitude=AMP       make sine wave with RMS amplitude AMP\n\
                              [default 0.25]\n\
  -b, --bytes-per-sample=B  write WAV with B bytes per sample [default 2]\n\
  -c, --channels=C          write WAV with C channels [default 1]\n\
  -f, --frequency=F         make a sine wave with frequency F [default 1000]\n\
  -r, --sample-rate=R       write WAV with sample rate R [default 44100]\n\
  -s, --samples=S           write S samples [default 1 second worth]\n\
  -V, --version             display version information and exit\n\
  -h, --help                display this help and exit\n\
\n\
Report bugs to <chrisvaill@gmail.com>.\n", progname);
}

static inline void
put_sample(long sample, FILE *outf, int bytes_per_sample)
{
  unsigned char pdata[8];
  switch(bytes_per_sample) {
  case 1:
    *pdata = sample + 128;
    break;
  case 2:
#ifdef WORDS_BIGENDIAN
    sample = bswap_16(sample);
#endif
    *((int16_t *)pdata) = (int16_t)sample;
    break;
  case 3:
    *pdata = (unsigned char)(sample & 0xFF);
    *(pdata + 1) = (unsigned char)((sample >> 8) & 0xFF);
    *(pdata + 2) = (unsigned char)((sample >> 16) & 0xFF);
    break;
  case 4:
#ifdef WORDS_BIGENDIAN
    sample = bswap_32(sample);
#endif
    *((int32_t *)pdata) = (int32_t)sample;
    break;
  default:
    /* shouldn't happen */
    fprintf(stderr, "%s: I don't know what to do with %d bytes per sample\n",
	    progname, bytes_per_sample);
  }
  if (fwrite(pdata, bytes_per_sample, 1, outf) < 1)
    fprintf(stderr, "%s: error writing: %s\n",
	    progname, strerror(errno));
}

/*
 * Return nonzero if the two strings are equal, ignoring case, up to
 * the first n characters
 */
int
strncaseeq(const char *s1, const char *s2, size_t n)
{
  for ( ; n > 0; n--) {
    if (tolower(*s1++) != tolower(*s2++))
      return 0;
  }

  return 1;
}

int
main(int argc, char *argv[])
{
  riff_t riff;
  riff_chunk_t top_chnk, fmt_chnk, data_chnk;
  FILE *outf;
  int c;
  long sample, i;
  struct wavfmt wf;
  double mconst, gconst, f_samp;
  char *outfile = NULL, *p;
  double amp = 0.25;
  long s = -1, freq = 1000;
  int rate = 44100, channels = 1, bytes_per_samp = 2;

  struct option longopts[] = {
    {"help", 0, NULL, 'h'},
    {"version", 0, NULL, 'V'},
    {"amplitude", 1, NULL, 'a'},
    {"bytes-per-sample", 1, NULL, 'b'},
    {"channels", 1, NULL, 'c'},
    {"frequency", 1, NULL, 'f'},
    {"sample-rate", 1, NULL, 'r'},
    {"samples", 1, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  while ((c = getopt_long(argc,argv,"hVa:b:c:f:r:s:",longopts,NULL)) != EOF) {
    switch(c) {
    case 'a':
      amp = strtod(optarg, &p);

      /* check if "dB" or "dBFS" is given after number */
      while(isspace(*p))
	p++;
      if (strncaseeq(p, "db", 2)) {
	/* amplitude given as dBFS */

	if (amp > 0) {
	  amp = -amp;
	  fprintf(stderr, "%s: amplitude %f dBFS\n", progname, amp);
	}

	/* translate to fraction */
	amp = DBFSTOAMP(amp);

      } else {

	/* amplitude given as fraction */
	if (amp < 0 || amp > 1.0) {
	  fprintf(stderr, "%s: error: bad amplitude %f\n", progname, amp);
	  exit(1);
	}
      }
      break;
    case 'b':
      bytes_per_samp = strtol(optarg, NULL, 0);
      break;
    case 'c':
      channels = strtol(optarg, NULL, 0);
      break;
    case 'f':
      freq = strtol(optarg, NULL, 0);
      break;
    case 'r':
      rate = strtol(optarg, NULL, 0);
      break;
    case 's':
      s = strtol(optarg, NULL, 0);
      break;
    case 'V':
      printf("mktestwav (normalize) %s\n", version);
      exit(0);
    case 'h':
      usage();
      exit(0);
    default:
      usage();
      exit(1);
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Usage: %s [OPTION]... [FILE]\n", progname);
    fprintf(stderr, "Try `%s --help' for more information.\n", progname);
    exit(1);
  }

  outfile = argv[optind];

  /*if (bytes_per_samp != 1 && bytes_per_samp != 2 && bytes_per_samp != 4) {*/
  if (bytes_per_samp < 1 || bytes_per_samp > 4) {
    fprintf(stderr, "%s: %d bytes per sample not supported\n",
	    progname, bytes_per_samp);
    exit(1);
  }
  if (channels < 1) {
    fprintf(stderr, "%s: bad number of channels\n", progname);
    exit(1);
  }

  if (s == -1)
    s = rate;

  /* set up WAV fmt header */
  wf.format_tag = 1; /* for PCM wav format */
  wf.channels = channels;
  wf.samples_per_sec = rate;
  wf.avg_bytes_per_sec = rate * bytes_per_samp * channels;
  wf.block_align = bytes_per_samp * channels;
  wf.bits_per_sample = bytes_per_samp * 8;
#ifdef WORDS_BIGENDIAN
  wf.format_tag        = bswap_16(wf.format_tag);
  wf.channels          = bswap_16(wf.channels);
  wf.samples_per_sec   = bswap_32(wf.samples_per_sec);
  wf.avg_bytes_per_sec = bswap_32(wf.avg_bytes_per_sec);
  wf.block_align       = bswap_16(wf.block_align);
  wf.bits_per_sample   = bswap_16(wf.bits_per_sample);
#endif

  riff = riff_open(outfile, RIFF_WRONLY);
  if (riff == NULL) {
    fprintf(stderr, "%s: unable to open %s: %s\n",
	    progname, outfile, strerror(errno));
    exit(1);
  }
  outf = riff_stream(riff);

  top_chnk.id = RIFFID_RIFF;
  top_chnk.type = riff_string_to_fourcc("WAVE");
  top_chnk.size = 0;
  riff_create_chunk(riff, &top_chnk);

  fmt_chnk.id = riff_string_to_fourcc("fmt ");
  fmt_chnk.size = 0;
  riff_create_chunk(riff, &fmt_chnk);
  /* write header */
  if (fwrite(&wf, sizeof(struct wavfmt), 1, outf) < 1) {
    fprintf(stderr, "%s: unable to write WAV header: %s\n",
	    progname, strerror(errno));
    exit(1);
  }
  riff_ascend(riff, &fmt_chnk);

  data_chnk.id = riff_string_to_fourcc("data");
  data_chnk.size = 0;
  riff_create_chunk(riff, &data_chnk);
  /* write data */
  mconst = freq * 2.0 * M_PI / rate;
  gconst = amp / SQRT2_2;
  for (i = 0; i < s; i++) {
    f_samp = sin(mconst * i) * gconst;
    if (f_samp > 1.0)
      f_samp = 1.0;
    if (f_samp < -1.0)
      f_samp = -1.0;
    sample = f_samp * (0x7FFFFFFF >> (8 * (4 - bytes_per_samp)));
    /*printf("%ld %ld\n", i, sample);*/
    for (c = 0; c < channels; c++)
      put_sample(sample, outf, bytes_per_samp);
  }

  riff_ascend(riff, &data_chnk);

  riff_ascend(riff, &top_chnk);
  riff_close(riff);


  return 0;
}
