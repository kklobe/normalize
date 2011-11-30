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

#ifndef _COMMON_H_
#define _COMMON_H_

#define _POSIX_C_SOURCE 2

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

enum verbose_t {
  VERBOSE_QUIET    = 0,
  VERBOSE_PROGRESS = 1,
  VERBOSE_INFO     = 2,
  VERBOSE_DEBUG    = 3
};

struct signal_info {
  double level;      /* maximum sustained RMS amplitude */
  double peak;       /* peak amplitude */
  long max_sample;   /* maximum sample value */
  long min_sample;   /* minimum sample value */

  /* format info */
  int channels;
  int bits_per_sample;
  unsigned int samples_per_sec;

  off_t file_size;

  /* info for frontend mode */
  int orig_index;
};

struct progress_struct {
  time_t file_start;   /* what time we started processing the file */
  time_t batch_start;  /* what time we started processing the batch */
  off_t *file_sizes;   /* sizes of each file, in kb */
  off_t batch_size;    /* sum of all file sizes, in kb */
  off_t finished_size; /* sum of sizes of all completed files, in kb */
  int on_file;         /* the index of the file we're working on */
};

#ifndef MIN
# define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
# define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef ROUND
# define ROUND(x) floor((x) + 0.5)
#endif
#ifndef FALSE
# define FALSE  (0)
#endif
#ifndef TRUE
# define TRUE   (!FALSE)
#endif
#ifdef HAVE_DOS_FILE_NAMES
# define SLASH_CHAR '\\'
#else
# define SLASH_CHAR '/'
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

/* anything less than EPSILON is considered zero */
#ifndef EPSILON
# define EPSILON 0.00000000001
#endif

/* decibel-to-fraction conversion macros */
#define AMPTODBFS(x) (20 * log10(x))
#define FRACTODB(x) (20 * log10(x))
#define DBFSTOAMP(x) pow(10,(x)/20.0)
#define DBTOFRAC(x) pow(10,(x)/20.0)

#endif /* _COMMON_H_ */
