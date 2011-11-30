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

#define _POSIX_C_SOURCE 2

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include "nid3P.h"

/*
 * Version conversion data
 */

/*
 * struct frame_convert holds all information for converting a frame
 * between different versions of tags.
 *
 * The from field holds the frame ID as used in the earlier version;
 * the to field has the equivalent frame ID for the newer version.
 *
 * If the converter field is non-NULL, it contains a pointer to a
 * function which will convert the frame data.
 *
 * The converter's arguments are the frame, the two frame ID's from
 * and to, and an int which, if non-zero, indicates that the
 * conversion should be done backwards.  If the converter field for a
 * frame convert is NULL, the data left unchanged (only the frame ID
 * is changed).
 *
 * A converter function should return 0 normally, 1 if the frame
 * should be deleted instead of converted, or -1 on error.
 */

struct frame_convert {
  const char *from;
  const char *to;
  int (*converter)(id3_frame_t, const char *, const char *, int);
};

static int convert_link(id3_frame_t, const char *, const char *, int);
static int convert_apic(id3_frame_t, const char *, const char *, int);
static int convert_time(id3_frame_t, const char *, const char *, int);
static int convert_tcon(id3_frame_t, const char *, const char *, int);
static int convert_rva(id3_frame_t, const char *, const char *, int);

static const struct frame_convert _convert_map_v2to3[] = {
  { "BUF", "RBUF", NULL },
  { "CNT", "PCNT", NULL },
  { "COM", "COMM", NULL },
  { "CRA", "AENC", NULL },
  { "CRM", NULL,   NULL },
  { "ETC", "ETCO", NULL },
  { "EQU", "EQUA", NULL },
  { "GEO", "GEOB", NULL },
  { "IPL", "IPLS", NULL },
  { "LNK", "LINK", convert_link },
  { "MCI", "MCDI", NULL },
  { "MLL", "MLLT", NULL },
  { "PIC", "APIC", convert_apic },
  { "POP", "POPM", NULL },
  { "REV", "RVRB", NULL },
  { "RVA", "RVAD", NULL },
  { "SLT", "SYLT", NULL },
  { "STC", "SYTC", NULL },
  { "TAL", "TALB", NULL },
  { "TBP", "TBPM", NULL },
  { "TCM", "TCOM", NULL },
  { "TCO", "TCON", NULL },
  { "TCR", "TCOP", NULL },
  { "TDA", "TDAT", NULL },
  { "TDY", "TDLY", NULL },
  { "TEN", "TENC", NULL },
  { "TFT", "TFLT", NULL },
  { "TIM", "TIME", NULL },
  { "TKE", "TKEY", NULL },
  { "TLA", "TLAN", NULL },
  { "TLE", "TLEN", NULL },
  { "TMT", "TMED", NULL },
  { "TOA", "TOPE", NULL },
  { "TOF", "TOFN", NULL },
  { "TOL", "TOLY", NULL },
  { "TOR", "TORY", NULL },
  { "TOT", "TOAL", NULL },
  { "TP1", "TPE1", NULL },
  { "TP2", "TPE2", NULL },
  { "TP3", "TPE3", NULL },
  { "TP4", "TPE4", NULL },
  { "TPA", "TPOS", NULL },
  { "TPB", "TPUB", NULL },
  { "TRC", "TSRC", NULL },
  { "TRD", "TRDA", NULL },
  { "TRK", "TRCK", NULL },
  { "TSI", "TSIZ", NULL },
  { "TSS", "TSSE", NULL },
  { "TT1", "TIT1", NULL },
  { "TT2", "TIT2", NULL },
  { "TT3", "TIT3", NULL },
  { "TXT", "TEXT", NULL },
  { "TXX", "TXXX", NULL },
  { "TYE", "TYER", NULL },
  { "UFI", "UFID", NULL },
  { "ULT", "USLT", NULL },
  { "WAF", "WOAF", NULL },
  { "WAR", "WOAR", NULL },
  { "WAS", "WOAS", NULL },
  { "WCM", "WCOM", NULL },
  { "WCP", "WCOP", NULL },
  { "WPB", "WPUB", NULL },
  { "WXX", "WXXX", NULL },

  { "XRV", "XRVA", convert_rva },

  { NULL, NULL, NULL }
};

static const struct frame_convert _convert_map_v3to4[] = {

  /* removed and changed frames in 2.4 */
  { "EQUA", NULL, NULL },
  { "IPLS", "TIPL", NULL },
  { "RVAD", NULL, NULL },
  { "TDAT", "TDRC", convert_time },
  { "TIME", "TDRC", convert_time },
  { "TORY", "TDOR", NULL },
  /* TRDA is free-form, so we can't convert it to TDRC automatically */
  { "TRDA", NULL, NULL },
  { "TSIZ", NULL, NULL },
  { "TYER", "TDRC", convert_time },

  { "XRVA", "RVA2", convert_rva },

  /* new frames in 2.4 */
  { NULL, "ASPI", NULL },
  { NULL, "EQU2", NULL },
  { NULL, "RVA2", NULL },
  { NULL, "SEEK", NULL },
  { NULL, "SIGN", NULL },
  { NULL, "TDEN", NULL },
  { NULL, "TDOR", NULL },
  { NULL, "TDRC", NULL },
  { NULL, "TDRL", NULL },
  { NULL, "TDTG", NULL },
  { NULL, "TIPL", NULL },
  { NULL, "TMCL", NULL },
  { NULL, "TMOO", NULL },
  { NULL, "TPRO", NULL },
  { NULL, "TSOA", NULL },
  { NULL, "TSOP", NULL },
  { NULL, "TSOT", NULL },
  { NULL, "TSST", NULL },

  /* frames whose format changed */
  { "TCON", "TCON", convert_tcon },

  { NULL, NULL, NULL }
};

/*** Start of converter routines ***/

static int
convert_link(id3_frame_t f, const char *from, const char *to, int backward)
{
  /* FIXME: implement, maybe */
  return 0;
}

struct imgtype_mimetype_struct {
  const char *imgtype;
  const char *mimetype;
};

static const struct imgtype_mimetype_struct imgtype_mimetype_map[] = {
  { "jpg", "jpeg" },
  { "tif", "tiff" },
  { "xbm", "x-xbitmap" },
  { "xpm", "x-xpixmap" },
  { "xwd", "x-xwindowdump" },
  { "ras", "x-cmu-raster" },
  { "pnm", "x-portable-anymap" },
  { "pbm", "x-portable-bitmap" },
  { "pgm", "x-portable-graymap" },
  { "rgb", "x-rgb" },

  { NULL, NULL }
};

/* convert v2.2 PIC to v2.3 APIC */
static int
_convert_apic_forward(id3_frame_t f)
{
  char imgtype[4];
  char mimetype[32]; /* large enough for "image/" + largest string in map */
  const struct imgtype_mimetype_struct *im_map;
  unsigned char *data;
  int i, newsz;

  data = id3_frame_get_raw(f);
  if (data == NULL)
    return 0;
  memcpy(imgtype, data + 1, 3);
  imgtype[3] = '\0';
  for (i = 0; i < 3; i++)
    imgtype[i] = tolower(imgtype[i]);

  /* use the map to get the corresponding mime type */
  strcpy(mimetype, "image/");
  im_map = imgtype_mimetype_map;
  while (im_map->imgtype) {
    if (strcmp(imgtype, im_map->imgtype) == 0) {
      strcat(mimetype, im_map->mimetype);
      break;
    }
    im_map++;
  }

  /* just use image/<imgtype> if we didn't find it in the map */
  if (im_map->imgtype == NULL)
    strcat(mimetype, imgtype);

  i = strlen(mimetype);
  newsz = f->sz - 3 + i + 1;
  f->data = (unsigned char *)malloc(newsz);
  if (f->data == NULL) {
    f->data = data;
    return -1;
  }
  f->data[0] = data[0];
  strcpy((char *)f->data + 1, mimetype);
  memcpy(f->data + 1 + i + 1, data + 4, f->sz - 4);
  f->sz = newsz;
  free(data);

  return 0;
}

/* convert v2.3 APIC to v2.2 PIC */
static int
_convert_apic_backward(id3_frame_t f)
{
  char imgtype[4], *mimetype = NULL;
  const struct imgtype_mimetype_struct *im_map;
  unsigned char *data;
  int i, newsz, len, bad_mimetype = 0;

  data = id3_frame_get_raw(f);
  if (data == NULL)
    return 0;

  /* make sure the mime type is terminated */
  for (i = 1; i < f->sz; i++)
    if (data[i] == '\0')
      break;

  if (data[i] != '\0' || i < 8)
    bad_mimetype = 1;

  if (!bad_mimetype) {
    mimetype = (char *)data + 1;
    len = strlen(mimetype);
    for (i = 0; i < len; i++)
      mimetype[i] = tolower(mimetype[i]);
    if (strncmp(mimetype, "image/", 6) != 0)
      bad_mimetype = 1;
  }

  if (!bad_mimetype) {
    mimetype += 6; /* skip "image/" */
    /* use the map to get the corresponding image type */
    im_map = imgtype_mimetype_map;
    while (im_map->mimetype) {
      if (strcmp(mimetype, im_map->mimetype) == 0) {
	strcpy(imgtype, im_map->imgtype);
	break;
      }
      im_map++;
    }

    /* just use the first three chars after "image/"
       if we didn't find it in the map */
    if (im_map->mimetype == NULL)
      strncpy(imgtype, mimetype, 3);
    imgtype[3] = '\0';

  } else {
    /* can't make head or tail of the mime type */
    return 0;
  }

  newsz = f->sz - len - 1 + 3;
  memcpy(f->data + 1, imgtype, 3);
  memmove(f->data + 4, f->data + 4 + len + 1, f->sz - 4 - len - 1);
  f->sz = newsz;

  return 0;
}

static int
convert_apic(id3_frame_t f, const char *from, const char *to, int backward)
{
  if (backward)
    return _convert_apic_backward(f);
  else
    return _convert_apic_forward(f);
}

/* used by the following convert_time() converter */
static int
_convert_time_backward(id3_frame_t f)
{
  id3_t tag = f->id3;
  id3_frame_t f2;
  unsigned char *old_data;
  unsigned char buf[32];
  int len;

  /* split TDRC frame into TDAT, TIME, and TYER frames */
  old_data = id3_frame_get_raw(f);
  if (old_data == NULL)
    return 0;
  len = strlen((char *)old_data + 1);

  if (len >= 4) {
    /* form TYER frame */
    f2 = id3_frame_add(tag, "TYER");
    if (f2 == NULL)
      return -1;
    if (id3_frame_set_raw(f2, old_data, 5) == -1)
      return -1;
  }

  if (len >= 10) {
    /* form TDAT frame */
    f2 = id3_frame_add(tag, "TDAT");
    if (f2 == NULL)
      return -1;
    buf[0] = '\0';
    buf[1] = old_data[9];
    buf[2] = old_data[10];
    buf[3] = old_data[6];
    buf[4] = old_data[7];
    if (id3_frame_set_raw(f2, buf, 5) == -1)
      return -1;
  }

  if (len >= 16) {
    /* form TIME frame */
    f2 = id3_frame_add(tag, "TIME");
    if (f2 == NULL)
      return -1;
    buf[0] = '\0';
    buf[1] = old_data[12];
    buf[2] = old_data[13];
    buf[3] = old_data[15];
    buf[4] = old_data[16];
    if (id3_frame_set_raw(f2, buf, 5) == -1)
      return -1;
  }

  return 1;
}

/* used by the following convert_time() converter */
static int
_convert_time_forward(id3_frame_t f)
{
  id3_t tag = f->id3;
  id3_frame_t tdrc_f;
  unsigned char *old_data;

  tdrc_f = id3_get_frame_by_id(tag, "TDRC");
  if (tdrc_f == NULL) {
    /* convert this frame to TDRC */
    old_data = id3_frame_get_raw(f);
    if (strcmp(f->id, "TDAT") == 0) {

      /* make sure the TIME frame is in the form DDMM */
      if (strlen((char *)old_data + 1) != 4) {
	/* non-standard TDAT frame, so drop it */
	return 1;
      }
      f->sz = 11;
      f->data = (unsigned char *)calloc(f->sz + 2, 1);
      /* leave space for the year if we find a TYER later */
      /* (string is split to avoid trigraphs) */
      sprintf((char *)f->data + 1, "????" "-%c%c-%c%c",
	      old_data[3], old_data[4], old_data[1], old_data[2]);
      free(old_data);

    } else if (strcmp(f->id, "TIME") == 0) {

      /* make sure the TIME frame is in the form HHmm */
      if (strlen((char *)old_data + 1) != 4) {
	/* non-standard TIME frame, so drop it */
	return 1;
      }
      f->sz = 17;
      f->data = (unsigned char *)calloc(f->sz + 2, 1);
      /* leave space for the year and date for later TYER and TDAT */
      /* (string is split to avoid trigraphs) */
      sprintf((char *)f->data + 1, "????" "-??" "-??T%c%c:%c%c",
	      old_data[1], old_data[2], old_data[3], old_data[4]);
      free(old_data);

    } else if (strcmp(f->id, "TYER") == 0) {
      /* leave as it is */
    } else {
      /* unrecognized from field */
      return -1;
    }

  } else {

    /* There's already a TDRC frame, so we just add to it */
    id3_frame_get_raw(f); /* make sure the data is read into f->data */
    if (strcmp(f->id, "TDAT") == 0) {

      /* make sure the TIME frame is in the form DDMM */
      if (strlen((char *)f->data + 1) != 4) {
	/* non-standard TDAT frame, so drop it */
	return 1;
      }
      /* make sure the TDRC frame can hold 11 bytes */
      if (tdrc_f->sz < 11) {
	/* We need to make it bigger */
	old_data = id3_frame_get_raw(tdrc_f);
	tdrc_f->sz = 11;
	tdrc_f->data = (unsigned char *)realloc(tdrc_f->data, tdrc_f->sz + 2);
	if (tdrc_f->data == NULL) {
	  tdrc_f->data = old_data;
	  return -1;
	}
	tdrc_f->data[11] = tdrc_f->data[12] = '\0'; /* ensure termination */
      }
      /* data will now be YYYY-MM-DD, so splice in the -MM-DD */
      sprintf((char *)tdrc_f->data + 5, "-%c%c-%c%c",
	      f->data[3], f->data[4], f->data[1], f->data[2]);

    } else if (strcmp(f->id, "TIME") == 0) {

      /* make sure the TIME frame is in the form HHmm */
      if (strlen((char *)f->data + 1) != 4) {
	/* non-standard TIME frame, so drop it */
	return 1;
      }
      /* make sure the TDRC frame can hold 17 bytes */
      if (tdrc_f->sz < 17) {
	/* We need to make it bigger */
	old_data = id3_frame_get_raw(tdrc_f);
	tdrc_f->sz = 17;
	tdrc_f->data = (unsigned char *)realloc(tdrc_f->data, tdrc_f->sz + 2);
	if (tdrc_f->data == NULL) {
	  tdrc_f->data = old_data;
	  return -1;
	}
	tdrc_f->data[17] = tdrc_f->data[18] = '\0'; /* ensure termination */
      }
      /* data will now be YYYY-MM-DDTHH:mm, so splice in the THH:mm */
      sprintf((char *)tdrc_f->data + 11, "T%c%c:%c%c",
	      f->data[1], f->data[2], f->data[3], f->data[4]);

    } else if (strcmp(f->id, "TYER") == 0) {

      /* make sure the TYER frame is in the form YYYY */
      if (strlen((char *)f->data + 1) != 4) {
	/* non-standard TYER frame, so drop it */
	return 1;
      }
      /* make sure the TDRC frame can hold 5 bytes */
      if (tdrc_f->sz < 5) {
	/* We need to make it bigger */
	old_data = id3_frame_get_raw(tdrc_f);
	tdrc_f->sz = 5;
	tdrc_f->data = (unsigned char *)realloc(tdrc_f->data, tdrc_f->sz + 2);
	if (tdrc_f->data == NULL) {
	  tdrc_f->data = old_data;
	  return -1;
	}
	tdrc_f->data[5] = tdrc_f->data[6] = '\0'; /* ensure termination */
      }
      /* splice the YYYY into the beginning of the data */
      memcpy(tdrc_f->data + 1, f->data + 1, 4);

    } else {
      /* unrecognized from field */
      return -1;
    }

    return 1;
  }

  return 0;
}

/* This converter changes the v2.3 frames TDAT, TIME, and TYER
 * to the v2.4 frame TDRC. */
static int
convert_time(id3_frame_t f, const char *from, const char *to, int backward)
{
  if (backward)
    return _convert_time_backward(f);
  else
    return _convert_time_forward(f);
}

/* This converter changes between v2.3 and v2.4 TCON frame formats. */
static int
convert_tcon(id3_frame_t f, const char *from, const char *to, int backward)
{
  char *src, *dest, *rparen;
  unsigned char *data;
  char *endptr;
  int i, newsz, len, last_was_nonnumeric;
  long gnum;

  data = id3_frame_get_raw(f);

  if (f->sz < 1)
    return 0;

  /* FIXME: need to test backwards -- fix mp3tag.c */
  if (backward) {

    /* "129" 0x00 "9" 0x00 ==> "(129)(9)" */
    /* "129" 0x00 "(I think)" 0x00 ==> "(129)((I think)" */

    /* frame can grow, so allocate new data pointer */
    newsz = f->sz + 2;
    for (i = 0; i < f->sz; i++) {
      if (data[i] == '(' || data[i] == '\0')
	newsz++;
    }
    f->data = (unsigned char *)calloc(newsz, 1);
    if (f->data == NULL) {
      f->data = data;
      return -1;
    }

    src = (char *)data;
    dest = (char *)f->data;
    *dest++ = *src++; /* text encoding byte */
    last_was_nonnumeric = 0;
    while (src - (char *)data < f->sz) {

      /* Check for a numeric string between 0 and 255, by itself */
      if (src[0] >= '0' && src[0] <= '9') { 
	gnum = strtol(src, &endptr, 10);
	if (*endptr == '\0' && gnum >= 0 && gnum <= 255) {
	  dest += sprintf(dest, "(%d)", (int)gnum);
	  src = endptr + 1;
	  last_was_nonnumeric = 0;
	  continue;
	}
      }

      /* It's a non-numeric string, so escape the lparens and copy */
      if (last_was_nonnumeric) {
	/* separate consecutive non-numeric strings by '/' */
	*dest++ = '/';
      }
      while (*src != '\0') {
	if (*src == '(')
	  *dest++ = '(';
	*dest++ = *src++;
      }
      src++; /* skip the '\0' */
      last_was_nonnumeric = 1;
    }
    newsz = dest - (char *)f->data;
    free(data);

  } else {

    /* "(129)(9)" ==> "129" 0x00 "9" 0x00 */
    /* "(129)((I think)" ==> "129" 0x00 "(I think)" 0x00 */

    src = dest = (char *)data + 1;
    while (src - (char *)data < f->sz) {

      if (src[0] == '(') {

	rparen = strchr(src, ')');
	if (src[1] == '(') {
	  /* if "((" is encountered, drop one '(' */
	  src++;
	  if (rparen)
	    len = rparen - src + 1;
	  else
	    len = f->sz - (src - (char *)data);
	  memmove(dest, src, len);
	  src += len;
	  dest += len;
	} else {
	  /* found start of a field */
	  if (rparen == NULL)
	    break;
	  *rparen = '\0';
	  if (dest[-1] == '\0')
	    src++;
	  else
	    *src = '\0';
	  len = rparen - src;
	  memmove(dest, src, len);
	  src += len;
	  dest += len;
	}

      } else {
	*dest++ = *src++;
      }
    }

    newsz = dest - (char *)data;
  }

  f->sz = newsz;
  f->data[f->sz] = '\0';

  return 0;
}

/*
 * We use experimental tags XRV and XRVA in v2.2 and v2.3 tags, since
 * they don't have useful native relative volume adjust frames like
 * RVA2. Their format is the same as RVA2, so convert_rva() doesn't
 * actually do any conversion; it's just to ensure that the format is
 * correct, since someone else could also be putting XRVA frames with
 * entirely different data in tags.  If we don't recogize the format,
 * we just drop the tag.
 */
static int
convert_rva(id3_frame_t f, const char *from, const char *to, int backward)
{
  int i, peakbytes;
  unsigned char *data;

  data = id3_frame_get_raw(f);

  /* check identification string */
  for (i = 0; i < f->sz; i++)
    if (data[i] == '\0')
      break;
  if (data[i] != '\0')
    return 1;

  i++;
  while (1) {
    /* check channel type */
    if (i >= f->sz || data[i] > 0x08)
      return 1;

    /* check peak data */
    i += 3;
    if (i >= f->sz)
      return 1;
    peakbytes = data[i] / 8;
    i += peakbytes;
    if (i >= f->sz)
      return 1;

    /* end of field; it's okay to end here */
    i++;
    if (i >= f->sz)
      break;
  }

  return 0;
}

/*** End of converter routines ***/

static const struct frame_convert *
find_converter(const struct frame_convert *map, const char *from, int backw)
{
  const char *s;

  while (map->from || map->to) {
    s = backw ? map->to : map->from;
    if (s && memcmp(from, s, 4) == 0)
      return map;
    map++;
  }
  return NULL;
}

static int
convert_frame(id3_frame_t f, int from_vers, int to_vers)
{
  const struct frame_convert *fc_map = NULL;
  int ret, backward = 0;
  const char *newid;

  if (from_vers == to_vers)
    return 0;

  if (from_vers - to_vers > 1) {
    ret = convert_frame(f, from_vers, to_vers + 1);
    if (ret == -1)
      return -1;
    from_vers = to_vers + 1;
  } else if (to_vers - from_vers > 1) {
    ret = convert_frame(f, from_vers, to_vers - 1);
    if (ret == -1)
      return -1;
    from_vers = to_vers - 1;
  }

  /* to and from are always one version apart at this point */
  switch (from_vers) {
  case 2:
    /* we can only be converting to v3 */
    fc_map = _convert_map_v2to3;
    backward = 0;
    break;
  case 3:
    switch (to_vers) {
    case 2:
      fc_map = _convert_map_v2to3;
      backward = 1;
      break;
    case 4:
      fc_map = _convert_map_v3to4;
      backward = 0;
      break;
    }
    break;
  case 4:
    /* we can only be converting to v3 */
    fc_map = _convert_map_v3to4;
    backward = 1;
    break;
  }

  fc_map = find_converter(fc_map, f->id, backward);
  if (fc_map) {
    if (fc_map->converter) {
      ret = fc_map->converter(f, fc_map->from, fc_map->to, backward);
      if (ret == -1)
	return -1;
      if (ret == 1) {
	id3_frame_delete(f);
	return 0;
      }
    }
    newid = backward ? fc_map->from : fc_map->to;
    if (newid) {
      strcpy(f->id, newid);
    } else {
      /* newid is NULL, so the frame should just be dropped */
      id3_frame_delete(f);
    }
  }

  return 0;
}

int
id3_set_version(id3_t tag, enum id3_version ver)
{
  int oldversion;
  id3_frame_t fr, nextfr;

  /* make sure headers have been read */
  if (id3_frame_count(tag) == -1)
    return -1;

  /* set version field */
  oldversion = tag->version;
  switch (ver) {
  case ID3_VERSION_2_3: tag->version = 3; break;
  case ID3_VERSION_2_4: tag->version = 4; break;
  case ID3_VERSION_2_2:
    /* we choose not to write ID3v2.2 tags, so fall through */
  default:
    errno = EINVAL;
    return -1;
  }

  if (tag->version == oldversion)
    return 0; /* nothing to do */
  if (oldversion < 2 || oldversion > 4) {
    /* we only know how to convert from v2.2, v2.3, and v2.4 */
    errno = EINVAL;
    return -1;
  }

  /* convert frames */
  fr = tag->frame_hd;
  while (fr) {
    /* fr may get removed in the conversion process, so grab the next
       frame before converting */
    nextfr = fr->next;
    if (convert_frame(fr, oldversion, tag->version) == -1)
      return -1;
    fr = nextfr;
  }

  return 0;
}

enum id3_version
id3_get_version(id3_t tag)
{
  if (id3_get_size(tag) == -1)
    return ID3_VERSION_NONE;
  switch (tag->version) {
  case 2: return ID3_VERSION_2_2;
  case 3: return ID3_VERSION_2_3;
  case 4: return ID3_VERSION_2_4;
  }
  return ID3_VERSION_NONE;
}
