/* Copyright (C) 2001--2005 Chris Vaill
   This file is part of nid3lib.

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
 * Frame ID description strings
 */

#define _POSIX_C_SOURCE 2

#include "config.h"

#include <stdio.h>
#if STDC_HEADERS
# include <string.h>
#else
int strcmp();
#endif

#include "nid3P.h"

struct fid_desc {
  const char *id;
  const char *desc;
};

static const struct fid_desc fid_desc_map[] = {

  /* v2.2 frame id's */
  { "BUF", "Recommended buffer size" },
  { "CNT", "Play counter" },
  { "COM", "Comments" },
  { "CRA", "Audio encryption" },
  { "CRM", "Encrypted meta frame" },
  { "ETC", "Event timing codes" },
  { "EQU", "Equalization" },
  { "GEO", "General encapsulated object" },
  { "IPL", "Involved people list" },
  { "LNK", "Linked information" },
  { "MCI", "Music CD Identifier" },
  { "MLL", "MPEG location lookup table" },
  { "PIC", "Attached picture" },
  { "POP", "Popularimeter" },
  { "REV", "Reverb" },
  { "RVA", "Relative volume adjustment" },
  { "SLT", "Synchronized lyric/text" },
  { "STC", "Synced tempo codes" },
  { "TAL", "Album/Movie/Show title" },
  { "TBP", "BPM (Beats Per Minute)" },
  { "TCM", "Composer" },
  { "TCO", "Content type" },
  { "TCR", "Copyright message" },
  { "TDA", "Date" },
  { "TDY", "Playlist delay" },
  { "TEN", "Encoded by" },
  { "TFT", "File type" },
  { "TIM", "Time" },
  { "TKE", "Initial key" },
  { "TLA", "Language(s)" },
  { "TLE", "Length" },
  { "TMT", "Media type" },
  { "TOA", "Original artist(s)/performer(s)" },
  { "TOF", "Original filename" },
  { "TOL", "Original Lyricist(s)/text writer(s)" },
  { "TOR", "Original release year" },
  { "TOT", "Original album/Movie/Show title" },
  { "TP1", "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group" },
  { "TP2", "Band/Orchestra/Accompaniment" },
  { "TP3", "Conductor/Performer refinement" },
  { "TP4", "Interpreted, remixed, or otherwise modified by" },
  { "TPA", "Part of a set" },
  { "TPB", "Publisher" },
  { "TRC", "ISRC (International Standard Recording Code)" },
  { "TRD", "Recording dates" },
  { "TRK", "Track number/Position in set" },
  { "TSI", "Size" },
  { "TSS", "Software/hardware and settings used for encoding" },
  { "TT1", "Content group description" },
  { "TT2", "Title/Songname/Content description" },
  { "TT3", "Subtitle/Description refinement" },
  { "TXT", "Lyricist/text writer" },
  { "TXX", "User defined text information frame" },
  { "TYE", "Year" },
  { "UFI", "Unique file identifier" },
  { "ULT", "Unsychronized lyric/text transcription" },
  { "WAF", "Official audio file webpage" },
  { "WAR", "Official artist/performer webpage" },
  { "WAS", "Official audio source webpage" },
  { "WCM", "Commercial information" },
  { "WCP", "Copyright/Legal information" },
  { "WPB", "Publishers official webpage" },
  { "WXX", "User defined URL link frame" },

  /* v2.3 frame id's */
  { "AENC", "Audio encryption" },
  { "APIC", "Attached picture" },
  { "COMM", "Comments" },
  { "COMR", "Commercial frame" },
  { "ENCR", "Encryption method registration" },
  { "EQUA", "Equalisation" },
  { "ETCO", "Event timing codes" },
  { "GEOB", "General encapsulated object" },
  { "GRID", "Group identification registration" },
  { "IPLS", "Involved people list" },
  { "LINK", "Linked information" },
  { "MCDI", "Music CD identifier" },
  { "MLLT", "MPEG location lookup table" },
  { "OWNE", "Ownership frame" },
  { "PRIV", "Private frame" },
  { "PCNT", "Play counter" },
  { "POPM", "Popularimeter" },
  { "POSS", "Position synchronisation frame" },
  { "RBUF", "Recommended buffer size" },
  { "RVAD", "Relative volume adjustment" },
  { "RVRB", "Reverb" },
  { "SYLT", "Synchronised lyric/text" },
  { "SYTC", "Synchronised tempo codes" },
  { "TALB", "Album/Movie/Show title" },
  { "TBPM", "BPM (beats per minute)" },
  { "TCOM", "Composer" },
  { "TCON", "Content type" },
  { "TCOP", "Copyright message" },
  { "TDAT", "Date" },
  { "TDLY", "Playlist delay" },
  { "TENC", "Encoded by" },
  { "TEXT", "Lyricist/Text writer" },
  { "TFLT", "File type" },
  { "TIME", "Time" },
  { "TIT1", "Content group description" },
  { "TIT2", "Title/songname/content description" },
  { "TIT3", "Subtitle/Description refinement" },
  { "TKEY", "Initial key" },
  { "TLAN", "Language(s)" },
  { "TLEN", "Length" },
  { "TMED", "Media type" },
  { "TOAL", "Original album/movie/show title" },
  { "TOFN", "Original filename" },
  { "TOLY", "Original lyricist(s)/text writer(s)" },
  { "TOPE", "Original artist(s)/performer(s)" },
  { "TORY", "Original release year" },
  { "TOWN", "File owner/licensee" },
  { "TPE1", "Lead performer(s)/Soloist(s)" },
  { "TPE2", "Band/orchestra/accompaniment" },
  { "TPE3", "Conductor/performer refinement" },
  { "TPE4", "Interpreted, remixed, or otherwise modified by" },
  { "TPOS", "Part of a set" },
  { "TPUB", "Publisher" },
  { "TRCK", "Track number/Position in set" },
  { "TRDA", "Recording dates" },
  { "TRSN", "Internet radio station name" },
  { "TRSO", "Internet radio station owner" },
  { "TSIZ", "Size" },
  { "TSRC", "ISRC (international standard recording code)" },
  { "TSSE", "Software/Hardware and settings used for encoding" },
  { "TYER", "Year" },
  { "TXXX", "User defined text information frame" },
  { "UFID", "Unique file identifier" },
  { "USER", "Terms of use" },
  { "USLT", "Unsynchronised lyric/text transcription" },
  { "WCOM", "Commercial information" },
  { "WCOP", "Copyright/Legal information" },
  { "WOAF", "Official audio file webpage" },
  { "WOAR", "Official artist/performer webpage" },
  { "WOAS", "Official audio source webpage" },
  { "WORS", "Official Internet radio station homepage" },
  { "WPAY", "Payment" },
  { "WPUB", "Publishers official webpage" },
  { "WXXX", "User defined URL link frame" },

    /* new v2.4 frame id's */
  { "ASPI", "Audio seek point index" },
  { "EQU2", "Equalisation (2)" },
  { "RVA2", "Relative volume adjustment (2)" },
  { "SEEK", "Seek frame" },
  { "SIGN", "Signature frame" },
  { "TDEN", "Encoding time" },
  { "TDOR", "Original release time" },
  { "TDRC", "Recording time" },
  { "TDRL", "Release time" },
  { "TDTG", "Tagging time" },
  { "TIPL", "Involved people list" },
  { "TMCL", "Musician credits list" },
  { "TMOO", "Mood" },
  { "TPRO", "Produced notice" },
  { "TSOA", "Album sort order" },
  { "TSOP", "Performer sort order" },
  { "TSOT", "Title sort order" },
  { "TSST", "Set subtitle" },

  { NULL, NULL }
};

/* FIXME: hash me! */
const char *
id3_id_description(const char *id)
{
  const struct fid_desc *d;
  
  for (d = fid_desc_map; d->id; d++)
    if (strcmp(id, d->id) == 0)
      break;

  return d->desc;
}
