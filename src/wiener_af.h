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
 * We provide a mini-version of the audiofile interface, this will
 * only be included for systems lacking the real audiofile library.
 */

#ifndef _WIENER_AF_H_
#define _WIENER_AF_H_

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define AF_NULL_FILESETUP  ((struct _AFfilesetup *) 0)
#define AF_NULL_FILEHANDLE ((struct _AFfilehandle *) 0)

enum {
  AF_DEFAULT_TRACK = 1001
};

enum {
  AF_BYTEORDER_BIGENDIAN = 501,
  AF_BYTEORDER_LITTLEENDIAN = 502
};

enum {
  AF_FILE_UNKNOWN = -1,
  AF_FILE_RAWDATA = 0,
  AF_FILE_AIFFC = 1,
  AF_FILE_AIFF = 2,
  AF_FILE_NEXTSND = 3,
  AF_FILE_WAVE = 4
};

enum {
  AF_SAMPFMT_TWOSCOMP = 401, /* linear two's complement */
  AF_SAMPFMT_UNSIGNED = 402, /* unsigned integer */
  AF_SAMPFMT_FLOAT = 403, /* 32-bit IEEE floating-point */
  AF_SAMPFMT_DOUBLE = 404 /* 64-bit IEEE double-precision floating-point */
};

enum {
  AF_BAD_OPEN,
  AF_BAD_READ,
  AF_BAD_WRITE,
  AF_BAD_LSEEK,
  AF_BAD_MALLOC,
  AF_BAD_FILEFMT
};

typedef struct _AFfilesetup *AFfilesetup;
typedef struct _AFfilehandle *AFfilehandle;
typedef off_t AFframecount;
typedef off_t AFfileoffset;
typedef void (*AFerrfunc)(long, const char *);

AFfilehandle afOpenFile(const char *filename, const char *mode,
			AFfilesetup setup);
AFfilehandle afOpenFD(int fd, const char *mode, AFfilesetup setup);
int afCloseFile(AFfilehandle file);
AFfilesetup afNewFileSetup(void);
void afFreeFileSetup(AFfilesetup);
int afReadFrames(AFfilehandle, int track, void *buffer, int frameCount);
int afWriteFrames(AFfilehandle, int track, void *buffer, int frameCount);
int afSyncFile(AFfilehandle);
float afGetFrameSize(AFfilehandle, int track, int expand3to4);
AFfileoffset afGetTrackBytes(AFfilehandle, int track);
AFframecount afGetFrameCount(AFfilehandle, int track);
AFerrfunc afSetErrorHandler(AFerrfunc errorFunction);

void afInitFileFormat(AFfilesetup, int format);
void afInitByteOrder(AFfilesetup, int track, int byteOrder);
void afInitChannels(AFfilesetup, int track, int nchannels);
void afInitRate(AFfilesetup, int track, double rate);
void afInitSampleFormat(AFfilesetup, int track, int sampleFormat,
			int sampleWidth);

int afGetFileFormat(AFfilehandle, int *version);
int afGetByteOrder(AFfilehandle, int track);
int afGetChannels(AFfilehandle, int track);
void afGetSampleFormat(AFfilehandle, int track, int *sampfmt, int *sampwidth);
double afGetRate(AFfilehandle, int track);
int afSetVirtualSampleFormat(AFfilehandle, int track,
			     int sampleFormat, int sampleWidth);


#ifdef __cplusplus
}
#endif

#endif /* _WIENER_AF_H_ */
