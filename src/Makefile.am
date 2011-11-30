## Makefile for the program src directory of normalize
## Copyright (C) 1999--2005 Chris Vaill
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

## Process this file with automake to produce Makefile.in

bin_PROGRAMS = normalize
bin_SCRIPTS = normalize-mp3

if AUDIOFILE
AUDIOFILESOURCES =
else
AUDIOFILESOURCES = wiener_af.c wiener_af.h riff.c riff.h
endif
if MAD
MADSOURCES = mpegvolume.c
else
MADSOURCES =
endif

normalize_SOURCES = normalize.c volume.c adjust.c mpegadjust.c \
	common.h version.c getopt.c getopt1.c getopt.h \
	$(AUDIOFILESOURCES) $(MADSOURCES)

EXTRA_normalize_SOURCES = wiener_af.c wiener_af.h riff.c riff.h mpegvolume.c

normalize_LDADD = -L$(top_builddir)/nid3lib -lnid3 \
	@MADLIBS@ @AUDIOFILE_LIBS@ @LIBINTL@

normalize_DEPENDENCIES = $(top_builddir)/nid3lib/libnid3.a

normalize_CFLAGS = @AUDIOFILE_CFLAGS@ $(AM_CFLAGS)

localedir = $(datadir)/locale

INCLUDES = -I$(top_srcdir)/nid3lib \
	-I$(top_builddir)/intl -DLOCALEDIR=\"$(localedir)\"

EXTRA_DIST = normalize-mp3.in mktestwav.c

CLEANFILES = mktestwav riffwalk wavread test-wiener-af test-real-af mp3adjust

install-exec-hook:
	(cd $(DESTDIR)$(bindir); \
	$(LN_S) `echo normalize-mp3 | sed @program_transform_name@` \
	        `echo normalize-ogg | sed @program_transform_name@`; \
	mv -f `echo normalize | sed @program_transform_name@` \
	      `echo @NORMALIZE_BIN@ | sed @program_transform_name@`; \
	exit 0)

# test program targets follow

mktestwav: mktestwav.o riff.o version.o getopt.o getopt1.o
	$(LINK) $^ -lm

mktestwav.o: mktestwav.c riff.h

riffwalk: riffwalk.o

riffwalk.o: riff.c
	$(COMPILE) -DBUILD_RIFFWALK -c -o $@ $^

wavread: wavread.o

wavread.o: riff.c
	$(COMPILE) -DBUILD_WAVREAD -c -o $@ $^

test-wiener-af: test-wiener-af.o wav.o riff.o version.o
	$(LINK) $^ -lm

test-wiener-af.o: testaf.c
	$(COMPILE) -DFORCE_NOAUDIOFILE -c -o $@ $^

test-real-af: test-real-af.o
	$(LINK) $^ -L/usr/local/lib -lm -laudiofile

test-real-af.o: testaf.c
	$(COMPILE) -DFORCE_AUDIOFILE -c -o $@ $^
