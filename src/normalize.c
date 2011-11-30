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
#include <time.h>
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
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
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
#if HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#ifdef ENABLE_NLS
# define _(msgid) gettext (msgid)
# include <libintl.h>
# if HAVE_LOCALE_H
#  include <locale.h>
# endif
#else
# define _(msgid) (msgid)
#endif
#define N_(msgid) (msgid)

#include "getopt.h"
#include "common.h"

extern double signal_max_power(char *, struct signal_info *);
extern double signal_max_power_stream(FILE *, char *, struct signal_info *);
extern int apply_gain(char *fname, double, struct signal_info *);

void compute_levels(struct signal_info *sis, char **fnames, int nfiles);
double average_levels(struct signal_info *sis, int nfiles, double threshold);
int strncaseeq(const char *s1, const char *s2, size_t n);
char *basename(char *path);
void *xmalloc(size_t size);

extern char version[];
char *progname;
struct progress_struct progress_info;

void
usage_short()
{
  fprintf(stderr, _("Usage: %s [OPTION]... [FILE]...\n"), progname);
  fprintf(stderr, _("Try `%s --help' for more information.\n"), progname);
}

void
usage()
{
  printf(_("\
Usage: %s [OPTION]... [FILE]...\n\
Normalize volume of multiple audio files\n\
\n\
  -a, --amplitude=AMP          normalize the volume to the target amplitude\n\
                                 AMP [default -12dBFS]\n\
  -b, --batch                  batch mode: get average of all levels, and\n\
                                 use one adjustment, based on the average\n\
                                 level, for all files\n\
      --clipping               turn off limiter; do clipping instead\n\
      --fractions              display levels as fractions of maximum\n\
                                 amplitude instead of decibels\n\
  -g, --gain=ADJ               don't compute levels, just apply adjustment\n\
                                 ADJ to the files.  Use the suffix \"dB\"\n\
                                 to indicate a gain in decibels.\n\
  -l, --limiter=LEV            limit all samples above LEV [default -6dBFS]\n\
  -m, --mix                    mix mode: get average of all levels, and\n\
                                 normalize volume of each file to the\n\
                                 average\n\
  -n, --no-adjust              compute and display the volume adjustment,\n\
                                 but don't apply it to any of the files\n\
      --peak                   adjust by peak level instead of using\n\
                                 loudness analysis\n\
  -q, --quiet                  quiet (decrease verbosity to zero)\n\
  -t, --average-threshold=T    when computing average level, ignore any\n\
                                 levels more than T decibels from average\n\
  -T, --adjust-threshold=T     don't bother applying any adjustment smaller\n\
                                 than T decibels\n\
  -v, --verbose                increase verbosity\n\
  -w, --output-bitwidth=W      force adjusted files to have W-bit samples\n\
\n\
  -V, --version                display version information and exit\n\
  -h, --help                   display this help and exit\n\
\n\
Report bugs to <chrisvaill@gmail.com>.\n"), progname);
}

enum {
  OPT_CLIPPING     = 0x101,
  OPT_PEAK         = 0x102,
  OPT_FRACTIONS    = 0x103,
  OPT_ID3_COMPAT   = 0x104,
  OPT_ID3_UNSYNC   = 0x105,
  OPT_NO_PROGRESS  = 0x106,
  OPT_QUERY        = 0x107,
  OPT_FRONTEND     = 0x108,
};

/* options */
int verbose = VERBOSE_PROGRESS;
int do_print_only = FALSE;
int do_apply_gain = TRUE;
double target = 0.2511886431509580; /* -12dBFS */
double threshold = -1.0; /* in decibels */
int do_compute_levels = TRUE;
int output_bitwidth = 0;
int gain_in_decibels = FALSE;
int batch_mode = FALSE;
int mix_mode = FALSE;
int use_limiter = TRUE;
int use_peak = FALSE;
int use_fractions = FALSE;
int show_progress = TRUE;
int do_query = FALSE;
int frontend = FALSE;
double lmtr_lvl = 0.5;
double adjust_thresh = 0.125; /* don't adjust less than this many dB */
int id3_compat = FALSE;
int id3_unsync = FALSE;

int
main(int argc, char *argv[])
{
  int c, i, nfiles, ret;
  struct signal_info *sis, *psi;
  double level = 0.0, gain = 1.0, dBdiff = 0.0;
  char **fnames, *p;
  char cbuf[32];
  struct stat st;
  int file_needs_adjust = FALSE;

  struct option longopts[] = {
    {"help", 0, NULL, 'h'},
    {"version", 0, NULL, 'V'},
    {"no-adjust", 0, NULL, 'n'},
    {"quiet", 0, NULL, 'q'},
    {"verbose", 0, NULL, 'v'},
    {"batch", 0, NULL, 'b'},
    {"amplitude", 1, NULL, 'a'},
    {"average-threshold", 1, NULL, 't'},
    {"threshold", 1, NULL, 't'}, /* deprecate */
    {"gain", 1, NULL, 'g'},
    {"limiter", 1, NULL, 'l'},
    {"adjust-threshold", 1, NULL, 'T'},
    {"mix", 0, NULL, 'm'},
    {"compression", 0, NULL, 'c'}, /* deprecate */
    {"limit", 0, NULL, 'c'}, /* deprecate */
    {"output-bitwidth", 1, NULL, 'w'},
    {"clipping", 0, NULL, OPT_CLIPPING},
    {"peak", 0, NULL, OPT_PEAK},
    {"fractions", 0, NULL, OPT_FRACTIONS},
    {"id3-compat", 0, NULL, OPT_ID3_COMPAT},
    {"id3-unsync", 0, NULL, OPT_ID3_UNSYNC},
    {"no-progress", 0, NULL, OPT_NO_PROGRESS},
    {"query", 0, NULL, OPT_QUERY},
    {"frontend", 0, NULL, OPT_FRONTEND},
    {NULL, 0, NULL, 0}
  };

#ifdef __EMX__
  /* This gives wildcard expansion on Non-POSIX shells with OS/2 */
  _wildcard(&argc, &argv);
#endif

  /* get program name */
  progname = basename(argv[0]);
  if (strlen(progname) > 16)
    progname[16] = '\0';

#if ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  /* get args */
  while ((c = getopt_long(argc, argv, "hVnvqbmcT:l:g:a:t:w:", longopts, NULL)) != EOF) {
    switch(c) {
    case 'a':
      target = strtod(optarg, &p);

      /* check if "dB" or "dBFS" is given after number */
      while(isspace(*p))
	p++;
      if (strncaseeq(p, "db", 2)) {
	/* amplitude given as dBFS */

	if (target > 0) {
	  target = -target;
	  fprintf(stderr, _("%s: normalizing to %f dBFS\n"), progname, target);
	}

	/* translate to fraction */
	target = DBFSTOAMP(target);

      } else {

	/* amplitude given as fraction */
	if (target < 0 || target > 1.0) {
	  fprintf(stderr, _("%s: error: bad target amplitude %f\n"),
		  progname, target);
	  exit(1);
	}
      }
      break;
    case 't':
      /* a negative threshold means don't use threshold (use 2*stddev) */
      threshold = strtod(optarg, NULL);
      break;
    case 'g':
      gain = strtod(optarg, &p);

      /* check if "dB" is given after number */
      while(isspace(*p))
	p++;
      if (strncaseeq(p, "db", 2)) {
	dBdiff = gain;
	gain = DBTOFRAC(dBdiff);
	gain_in_decibels = TRUE;
      }

      do_compute_levels = FALSE;
      batch_mode = TRUE;
      if (gain < 0) {
	fprintf(stderr, _("%s: invalid argument to -g option\n"), progname);
	usage_short();
	exit(1);
      }
      break;
    case 'n':
      do_print_only = TRUE;
      do_apply_gain = FALSE;
      break;
    case 'b':
      batch_mode = TRUE;
      break;
    case 'm':
      mix_mode = TRUE;
      break;
    case 'c':
      fprintf(stderr, _("%s: Warning: the -c option is deprecated, and may be removed in v1.0\n"),
	      progname);
      break;
    case 'l':
      lmtr_lvl = strtod(optarg, &p);
      /* check if "dB" is given after number */
      while(isspace(*p))
	p++;
      /*fprintf(stderr, _("%s: limiting samples greater than "), progname);*/
      if (strncaseeq(p, "db", 2)) {
	if (lmtr_lvl > 0)
	  lmtr_lvl = -lmtr_lvl;
	fprintf(stderr, "%f dB\n", lmtr_lvl);
	lmtr_lvl = DBFSTOAMP(lmtr_lvl);
      } else {
	if (lmtr_lvl < 0)
	  lmtr_lvl = -lmtr_lvl;
	fprintf(stderr, "%f\n", lmtr_lvl);
      }

      use_limiter = TRUE;
      break;
    case 'T':
      adjust_thresh = strtod(optarg, &p);
      if (adjust_thresh < 0)
	adjust_thresh = -adjust_thresh;
      /*
      fprintf(stderr, _("%s: ignoring adjustments less than %fdB\n"),
	      progname, adjust_thresh);
      */
      break;
    case 'w':
      output_bitwidth = strtol(optarg, NULL, 0);
      break;
    case OPT_CLIPPING:
      use_limiter = FALSE;
      break;
    case OPT_PEAK:
      use_peak = TRUE;
      use_limiter = FALSE;
      break;
    case OPT_FRACTIONS:
      use_fractions = TRUE;
      break;
    case OPT_ID3_COMPAT:
      id3_compat = TRUE;
      break;
    case OPT_ID3_UNSYNC:
      id3_unsync = TRUE;
      break;
    case OPT_NO_PROGRESS:
      show_progress = FALSE;
      break;
    case OPT_QUERY:
      /*fprintf(stderr, _("%s: Warning: the --query option is deprecated, and may be removed in v1.0\n"),
	progname);*/
      do_query = TRUE;
      break;
    case OPT_FRONTEND:
      frontend = TRUE;
      verbose = VERBOSE_QUIET;
      break;
    case 'v':
      verbose++;
      break;
    case 'q':
      verbose = VERBOSE_QUIET;
      break;
    case 'V':
      printf("normalize %s\n", version);
      printf(_("\
Copyright (C) 2005 Chris Vaill\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"));
      printf(_("This copy of normalize is compiled with the following libraries:\n"));
#if USE_MAD
      printf("  MAD");
#endif
#if USE_AUDIOFILE
      printf("  audiofile");
#endif
      printf("\n");
      exit(0);
    case 'h':
      usage();
      exit(0);
    default:
      usage_short();
      exit(1);
    }
  }
  if (output_bitwidth < 0 || output_bitwidth > 32) {
    fprintf(stderr, _("%s: error: output bitwidth must be between 1 and 32\n"),
 	    progname);
    exit(1);
  }
  if (mix_mode && batch_mode) {
    fprintf(stderr,
	    _("%s: error: the -m and -b options are mutually exclusive\n"),
	    progname);
    exit(1);
  }
  if (use_peak && (mix_mode || batch_mode)) {
    fprintf(stderr,
	    _("%s: error: -m and -b can't be used with the --peak option\n"),
	    progname);
    exit(1);
  }
  if (optind >= argc) {
    usage_short();
    exit(1);
  }


  /*
   * get sizes of all files, for progress calculation
   */
  nfiles = 0;
  progress_info.batch_size = 0;
  fnames = (char **)xmalloc((argc - optind) * sizeof(char *));
  progress_info.file_sizes = (off_t *)xmalloc((argc - optind) * sizeof(off_t));
  for (i = optind; i < argc; i++) {
#if 0 /* FIXME: read from stdin currently not supported */
    if (strcmp(argv[i], "-") == 0) {
      if (do_apply_gain) {
	fprintf(stderr, _("%s: Warning: stdin specified on command line, not adjusting files\n"), progname);
	do_apply_gain = FALSE;
	do_print_only = TRUE;
      }
      fnames[nfiles++] = argv[i];
    } else
#endif
    if (stat(argv[i], &st) == -1) {
      fprintf(stderr, _("%s: file %s: %s\n"),
	      progname, argv[i], strerror(errno));
    } else {
      /* we want the size of the file in kilobytes, rounding up */
      progress_info.file_sizes[nfiles] = (st.st_size + 1023) / 1024;
      /* add the size of the file, in kb */
      progress_info.batch_size += progress_info.file_sizes[nfiles];
      fnames[nfiles] = argv[i];
      nfiles++;
    }
  }
  if (nfiles == 0) {
    fprintf(stderr, _("%s: no files!\n"), progname);
    return 1;
  }

  /* allocate space to store levels and peaks */
  sis = (struct signal_info *)xmalloc(nfiles * sizeof(struct signal_info));
  for (i = 0; i < nfiles; i++) {
    sis[i].file_size = progress_info.file_sizes[i];
    sis[i].orig_index = i;
  }

  if (frontend) {
    /* frontend mode: print "NUMFILES <number>" */
    printf("NUMFILES %d\n", nfiles);
    /* frontend mode: print "FILE <number> <filename>" for each file */
    for (i = 0; i < nfiles; i++)
      printf("FILE %d %s\n", i, fnames[i]);
  }

  /*
   * Compute the levels
   */
  if (do_compute_levels) {
    compute_levels(sis, fnames, nfiles);

    /* anything that came back with a level of -1 was bad, so remove it */
    for (i = 0; i < nfiles; i++) {
      if (sis[i].level < 0) {
	nfiles--;
	memmove(sis + i, sis + i + 1,
		(nfiles - i) * sizeof(struct signal_info));
	memmove(fnames + i, fnames + i + 1,
		(nfiles - i) * sizeof(char *));
	memmove(progress_info.file_sizes + i, progress_info.file_sizes + i + 1,
		(nfiles - i) * sizeof(off_t));
      }
    }

    if (batch_mode || mix_mode) {
      level = average_levels(sis, nfiles, threshold);

      /* For mix mode, we set the target to the average level */
      if (mix_mode)
	target = level;

      /* For batch mode, we use one gain for all files */
      if (batch_mode)
	gain = target / level;

      /* frontend mode: print "AVERAGE_LEVEL <level>" */
      if (frontend)
	printf("AVERAGE_LEVEL %f\n", AMPTODBFS(level));

      if (do_print_only) {
	if (!mix_mode) { /* in mix mode we print everything at the end */
	  if (use_fractions) {
	    printf(_("%-12.6f average level\n"), level);
	  } else {
	    sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(level));
	    printf(_("%-12s average level\n"), cbuf);
	  }
	}
      } else if (verbose >= VERBOSE_INFO) {
	if (use_fractions)
	  printf(_("Average level: %0.4f\n"), level);
	else
	  printf(_("Average level: %0.4fdBFS\n"), AMPTODBFS(level));
      }
    }

  } /* end of if (do_compute_levels) */


  /*
   * FIXME: this comment belongs somewhere else now...
   *
   * Check if we need to apply the gain --
   *
   *   If a file would be adjusted by an unnoticeable amount, we don't
   *   want to bother doing the adjustment.  The smallest noticeable
   *   difference varies with the absolute intensity and the pitch,
   *   but I don't think it's a stretch to say that a 0.25 dB
   *   difference is unnoticeable for most signals.
   *
   *   By default then, we allow amplitudes that are +/-0.125 dB from
   *   the target to pass without adjustment (mainly so that the
   *   normalize operation is idempotent, i.e. normalizing files for
   *   the second time has no effect).
   *
   *   Why 0.125 dB?  If we allow amplitudes that are 0.125 dB above
   *   and below the target, the total possible range is 0.25 dB,
   *   which shouldn't be noticeable.
   */

  if (batch_mode) {
    /* if gain_in_decibels, then dBdiff is the original specified gain */
    if (!gain_in_decibels)
      dBdiff = FRACTODB(gain);
  }


  /*
   * Apply the gain
   */
  if (do_apply_gain) {

    /* print adjust message for batch mode */
    if (batch_mode && verbose >= VERBOSE_PROGRESS) {
      /* if !do_compute_levels, -g was specified, so we force the adjust */
      if (do_compute_levels && fabs(dBdiff) < adjust_thresh) {
	fprintf(stderr, _("Files are already normalized, not adjusting..."));
      } else {
	if (!do_compute_levels) { /* if -g */
	  if (gain_in_decibels)
	    fprintf(stderr, _("Applying adjustment of %fdB...\n"), dBdiff);
	  else
	    fprintf(stderr, _("Applying adjustment of %f...\n"), gain);
	} else if (do_apply_gain) {
	  fprintf(stderr, _("Applying adjustment of %0.2fdB...\n"), dBdiff);
	}
      }
    }

    progress_info.batch_start = time(NULL);
    progress_info.finished_size = 0;

    for (i = 0; i < nfiles; i++) {

      if (!batch_mode) {
	if (use_peak)
	  gain = 1.0 / sis[i].peak;
	else
	  gain = target / sis[i].level;
      }

      /* frontend mode: print "ADJUSTING <number> <gain>" */
      if (frontend)
	printf("ADJUSTING %d %f\n", sis[i].orig_index, FRACTODB(gain));

      progress_info.file_start = time(NULL);
      progress_info.on_file = i;

      psi = do_compute_levels ? &sis[i] : NULL;
      ret = apply_gain(fnames[i], gain, psi);
      if (ret == -1) {
	fprintf(stderr, _("%s: error applying adjustment to %s: %s\n"),
		progname, fnames[i], strerror(errno));
      } else {
	if (ret == 0) {
	  /* gain was not applied */
	  if (!batch_mode) {
	    if (verbose >= VERBOSE_PROGRESS)
	      fprintf(stderr, _("%s already normalized, not adjusting..."),
		      fnames[i]);
	  }
	} else {
	  /* gain was applied */
	  file_needs_adjust = TRUE;
	}
	/* frontend mode: print "ADJUSTED <number> 1|0" */
	if (frontend)
	  printf("ADJUSTED %d %d\n", sis[i].orig_index, ret);
      }

      progress_info.finished_size += progress_info.file_sizes[i];

      if (verbose >= VERBOSE_PROGRESS && !batch_mode)
	fputc('\n', stderr);
    }

    /* we're done with the second progress meter, so go to next line */
    if (verbose >= VERBOSE_PROGRESS && batch_mode)
      fputc('\n', stderr);

  } else {

    if (batch_mode && do_print_only) {

      /* if we're not applying the gain, just print it out, and we're done */
      if (use_fractions) {
	printf(_("%-12f volume adjustment\n"), gain);
      } else {
	sprintf(cbuf, "%fdB", FRACTODB(gain));
	printf(_("%-12s volume adjustment\n"), cbuf);
      }

    } else if (mix_mode && do_print_only) {

      /*
       * In mix mode, we don't have all the information until the end,
       * so we have to print it all here.
       */
      /* clear the progress meter first */
      if (verbose >= VERBOSE_PROGRESS && show_progress)
	fprintf(stderr,
		"\r                                     "
		"                                     \r");
      for (i = 0; i < nfiles; i++) {
	if (use_fractions) {
	  sprintf(cbuf, "%0.6f", sis[i].level);
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.6f", sis[i].peak);
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.6f", target / sis[i].level);
	  printf("%-10s ", cbuf);
	} else {
	  sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(sis[i].level));
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(sis[i].peak));
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.4fdB", AMPTODBFS(target / sis[i].level));
	  printf("%-10s ", cbuf);
	}
	printf("%s\n", fnames[i]);
      }
      if (use_fractions) {
	printf(_("%-12.6f average level\n"), level);
      } else {
	sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(level));
	printf(_("%-12s average level\n"), cbuf);
      }

    } /* end of if (batch_mode && do_print_only) */

    /*
     * Since we're not applying any gain, we haven't computed
     * file_needs_adjust yet, so we do it now.
     */
    for (i = 0; i < nfiles; i++) {
      if (use_peak)
	gain = 1.0 / sis[i].peak;
      else
	gain = target / sis[i].level;
      dBdiff = FRACTODB(gain);
      
      if (fabs(dBdiff) >= adjust_thresh) {
	file_needs_adjust = TRUE;
	break;
      }
    }

  } /* end of if (do_apply_gain) */

  free(sis);
  free(progress_info.file_sizes);
  free(fnames);

  /*
   * frontend mode:
   *
   *   print "ADJUST_NEEDED 1" if a file was adjusted, or if the -n
   *   option was given and a file would need adjustment.
   *
   *   print "ADJUST_NEEDED 0" if the -n option was not given and no
   *   file was adjusted, or if -n was given and no file would need
   *   adjustment.
   */
  if (frontend)
    printf("ADJUST_NEEDED %d\n", file_needs_adjust ? 1 : 0);

  /* for --query option */
  /* NOTE: the --query option is broken and obsolete and will go away */
  if (do_query)
    return file_needs_adjust ? 0 : 2;

  return 0;
}

/*
 * Compute the RMS levels of the files.
 */
void
compute_levels(struct signal_info *sis, char **fnames, int nfiles)
{
  double power;
  int i;
  char cbuf[32];
  /*struct wavfmt fmt = { 1, 2, 44100, 176400, 0, 16 };*/

  if (verbose >= VERBOSE_PROGRESS) {
    fprintf(stderr, _("Computing levels...\n"));

    if (do_print_only) {
      if (batch_mode)
	fprintf(stderr, _("  level        peak\n"));
      else
	fprintf(stderr, _("  level        peak         gain\n"));
    }
  }

  progress_info.batch_start = time(NULL);
  progress_info.finished_size = 0;

  for (i = 0; i < nfiles; i++) {

    /* frontend mode: print "ANALYZING <number>" for each file index */
    if (frontend)
      printf("ANALYZING %d\n", i);

    sis[i].level = 0;

#if 0 /* FIXME: reinstate stdin reading */
    if (strcmp(fnames[i], "-") == 0) {
      progress_info.file_start = time(NULL);
      progress_info.on_file = i;
      errno = 0;

      /* for a stream, format info is passed through sis[i].fmt */
      sis[i].channels = 2;
      sis[i].bits_per_sample = 16;
      sis[i].samples_per_sec = 44100;
      power = signal_max_power_stream(stdin, NULL, &sis[i]);
      fnames[i] = "STDIN";

    } else {
#endif

      progress_info.file_start = time(NULL);
      progress_info.on_file = i;
      errno = 0;

      power = signal_max_power(fnames[i], &sis[i]);

#if 0 /* FIXME */
    }
#endif
    if (power < 0) {
      fprintf(stderr, _("%s: error reading %s"), progname, fnames[i]);
      if (errno)
	fprintf(stderr, ": %s\n", strerror(errno));
      else
	fprintf(stderr, "\n");
      sis[i].level = -1;
      goto error_update_progress;
    }
    /* frontend mode: print "LEVEL <number> <level>" for each file index */
    if (frontend)
      printf("LEVEL %d %f\n", i, AMPTODBFS(sis[i].level));
    if (power < EPSILON) {
      if (verbose >= VERBOSE_PROGRESS) {
	if (show_progress)
	  fprintf(stderr,
		  "\r                                     "
		  "                                     \r");
	fprintf(stderr,
		_("File %s has zero power, ignoring...\n"), fnames[i]);
      }
      sis[i].level = -1;
      goto error_update_progress;
    }

    if (do_print_only) {

      /* in mix mode we don't have enough info to print gain yet */
      if (!mix_mode) {

	/* clear the progress meter first */
	if (verbose >= VERBOSE_PROGRESS && show_progress)
	  fprintf(stderr,
		  "\r                                     "
		  "                                     \r");

	if (use_fractions) {
	  sprintf(cbuf, "%0.6f", sis[i].level);
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.6f", sis[i].peak);
	  printf("%-12s ", cbuf);
	} else {
	  sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(sis[i].level));
	  printf("%-12s ", cbuf);
	  sprintf(cbuf, "%0.4fdBFS", AMPTODBFS(sis[i].peak));
	  printf("%-12s ", cbuf);
	}
	if (!batch_mode) {
	  if (use_fractions)
	    sprintf(cbuf, "%0.6f", target / sis[i].level);
	  else
	    sprintf(cbuf, "%0.4fdB", AMPTODBFS(target / sis[i].level));
	  printf("%-10s ", cbuf);
	}
	printf("%s\n", fnames[i]);
      }

    } else if (verbose >= VERBOSE_INFO) {
      if (show_progress)
	fprintf(stderr,
		"\r                                     "
		"                                     \r");
      if (use_fractions)
	fprintf(stderr, _("Level for %s: %0.4f (%0.4f peak)\n"),
		fnames[i], sis[i].level, sis[i].peak);
      else
	fprintf(stderr, _("Level for %s: %0.4fdBFS (%0.4fdBFS peak)\n"),
		fnames[i], AMPTODBFS(sis[i].level), AMPTODBFS(sis[i].peak));
    }

  error_update_progress:
    progress_info.finished_size += progress_info.file_sizes[i];
  }

  /* we're done with the level calculation progress meter, so go to
     next line */
  if (verbose == VERBOSE_PROGRESS && !do_print_only)
    fputc('\n', stderr);
}

/*
 * For batch mode, we take the levels for all the input files, throw
 * out any that appear to be statistical aberrations, and average the
 * rest together to get one level and one gain for the whole batch.
 */
double
average_levels(struct signal_info *sis, int nlevels, double threshold)
{
  int i, files_to_avg;
  double sum, level_difference, std_dev, variance;
  double level, mean_level;
  char *badlevels;

  /* badlevels is a boolean array marking the level values to be thrown out */
  badlevels = (char *)xmalloc(nlevels * sizeof(char));
  memset(badlevels, 0, nlevels * sizeof(char));

  /* get mean level */
  sum = 0;
  for (i = 0; i < nlevels; i++)
    sum += sis[i].level;
  mean_level = sum / nlevels;

  /* if no threshold is specified, use 2 * standard dev */
  if (threshold < 0.0) {

    /*
     * We want the standard dev of the levels, but we need it in decibels.
     * Therefore, if u is the mean, the variance is
     *                  (1/N)summation((20*log10(x/u))^2)
     *       instead of (1/N)summation((x-u)^2),
     * which it would be if we needed straight variance "by the numbers".
     */

    /* get variance */
    sum = 0;
    for (i = 0; i < nlevels; i++) {
      double tmp = FRACTODB(sis[i].level / mean_level);
      sum += tmp * tmp;
    }
    variance = sum / nlevels;

    /* get standard deviation */
    if (variance < EPSILON)
      std_dev = 0.0;
    else
      std_dev = sqrt(variance);
    if (verbose >= VERBOSE_INFO)
      printf(_("Standard deviation is %0.2f dB\n"), std_dev);

    threshold = 2 * std_dev;
  }

  /*
   * Throw out level values that seem to be aberrations
   * (so that one "quiet song" doesn't throw off the average)
   * We define an aberration as a level that is > 2*stddev dB from the mean.
   */
  if (threshold > EPSILON && nlevels > 1) {
    for (i = 0; i < nlevels; i++) {

      /* Find how different from average the i'th file's level is.
       * The "level" here is the signal's maximum sustained amplitude,
       * from which we can compute the difference in decibels. */
      level_difference = fabs(FRACTODB(mean_level / sis[i].level));

      /* mark as bad any level that is > threshold different than the mean */
      if (level_difference > threshold) {

	/* frontend mode: print "AVERAGE_EXCLUDES <number> <difference>" */
	if (frontend)
	  printf("AVERAGE_EXCLUDES %d %f\n",
		 sis[i].orig_index, level_difference);

	if (verbose >= VERBOSE_INFO) {
	  if (use_fractions) {
	    printf(_("Throwing out level of %0.4f (different by %0.2fdB)\n"),
		   sis[i].level, level_difference);
	  } else {
	    printf(_("Throwing out level of %0.4fdBFS (different by %0.2fdB)\n"),
		   AMPTODBFS(sis[i].level), level_difference);
	  }
	}
	badlevels[i] = TRUE;
      }
    }
  }

  /* throw out the levels marked as bad */
  files_to_avg = 0;
  sum = 0;
  for (i = 0; i < nlevels; i++)
    if (!badlevels[i]) {
      sum += sis[i].level;
      files_to_avg++;
    }

  if (files_to_avg == 0) {
    fprintf(stderr, _("%s: all files ignored, try using -t 100\n"), progname);
    exit(1);
  }

  free(badlevels);

  level = sum / files_to_avg;

  return level;
}


void
progress_callback(char *prefix, float fraction_completed)
{
  /* the field lengths used by the sprintf() calls below are carefully
   * chosen so that buf will never overflow */
  char buf[128];
  time_t now, time_spent;
  unsigned int file_eta_hr, file_eta_min, file_eta_sec;
  off_t kb_done;
  float batch_fraction = 0;
  unsigned int batch_eta_hr, batch_eta_min, batch_eta_sec;

  if (!show_progress)
    return;

  now = time(NULL);

  if (fraction_completed > 1.0)
    fraction_completed = 1.0;

  /* figure out the ETA for this file */
  file_eta_hr = file_eta_sec = file_eta_min = 0;
  if (fraction_completed > 0.0) {
    time_spent = now - progress_info.file_start;
    if (fraction_completed == 0.0)
      file_eta_sec = 0;
    else
      file_eta_sec = (unsigned int)((float)time_spent / fraction_completed
				    - (float)time_spent + 0.5);

    file_eta_min = file_eta_sec / 60;
    file_eta_sec = file_eta_sec % 60;
    file_eta_hr = file_eta_min / 60;
    file_eta_min = file_eta_min % 60;
    if (file_eta_hr > 99)
      file_eta_hr = 99;
  }


  /* figure out the ETA for the whole batch */
  batch_eta_hr = batch_eta_min = batch_eta_sec = 0;
  if (progress_info.batch_size != 0) {
    kb_done = progress_info.finished_size
      + fraction_completed * progress_info.file_sizes[progress_info.on_file];
    batch_fraction = (float)kb_done / (float)progress_info.batch_size;
    time_spent = now - progress_info.batch_start;
    if (kb_done == 0)
      batch_eta_sec = 0;
    else
      batch_eta_sec = (unsigned int)((float)time_spent / batch_fraction
				     - (float)time_spent + 0.5);

    batch_eta_min = batch_eta_sec / 60;
    batch_eta_sec = batch_eta_sec % 60;
    batch_eta_hr = batch_eta_min / 60;
    batch_eta_min = batch_eta_min % 60;
    if (batch_eta_hr > 99) {
      batch_eta_hr = 99;
      batch_eta_min = 59;
      batch_eta_sec = 59;
    }
  }


  /* if progress on current file is zero, don't do file ETA */
  if (fraction_completed <= 0.0) {
    if (progress_info.batch_size == 0) {
      /* if we don't have batch info, don't compute batch ETA either */
      sprintf(buf, _(" %-17s  --%% done, ETA --:--:-- (batch  --%% done, ETA --:--:--)"),
	      prefix);
    } else {
      sprintf(buf, _(" %-17s  --%% done, ETA --:--:-- (batch %3.0f%% done, ETA %02d:%02d:%02d)"),
	      prefix, batch_fraction * 100,
	      batch_eta_hr, batch_eta_min, batch_eta_sec);
    }

  } else {

    sprintf(buf, _(" %-17s %3.0f%% done, ETA %02d:%02d:%02d (batch %3.0f%% done, ETA %02d:%02d:%02d)"),
	    prefix, fraction_completed * 100,
	    file_eta_hr, file_eta_min, file_eta_sec,
	    batch_fraction * 100,
	    batch_eta_hr, batch_eta_min, batch_eta_sec);

  }

  fprintf(stderr, "%s \r", buf);
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

/*
 * Like the SUSv2 basename, except guaranteed non-destructive, and
 * doesn't correctly handle pathnames ending in '/'.
 */
char *
basename(char *path)
{
  char *p;
  p = strrchr(path, SLASH_CHAR);
  if (p)
    p++;
  else
    p = path;
  return p;
}

void *
xmalloc(size_t size)
{
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, _("%s: unable to malloc\n"), progname);
    exit(1);
  }
  return ptr;
}
