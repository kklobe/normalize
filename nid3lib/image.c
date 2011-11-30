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
 * Image frame (APIC) manipulation routines
 */

#define _POSIX_C_SOURCE 2

#include "config.h"

#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STRING_H
#  include <string.h>
# endif
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif

#include "nid3P.h"


/*
 * For a PIC or APIC frame, save the image in file fname, with the
 * proper extension added (e.g. fname.jpg if the mime type is
 * image/jpeg).
 */
int
id3_frame_save_image(id3_frame_t f, const char *fname)
{
  id3_t tag = f->id3;
  unsigned char *data;
  FILE *out;
  char *fullname = NULL, *mimetype;
  int i, err_save;

  data = id3_frame_get_raw(f);
  if (data == NULL)
    return -1;

  switch (id3_get_version(tag)) {
  case ID3_VERSION_2_2:
    if (strcmp(f->id, "PIC") != 0)
      return -1;
    if (f->sz < 4)
      return -1;
    fullname = (char *)malloc(strlen(fname) + 4 + 1);
    if (fullname == NULL)
      return -1;
    sprintf(fullname, "%s.%c%c%c", fname, data[1], data[2], data[3]);
    data = data + 5;
    break;

  case ID3_VERSION_2_3:
  case ID3_VERSION_2_4:
    if (strcmp(f->id, "APIC") != 0)
      return -1;
    /* make sure the mime type is terminated */
    for (i = 1; i < f->sz; i++)
      if (data[i] == '\0')
	break;
    if (data[i] != '\0' || i < 8)
      return -1;
    mimetype = (char *)data + 7;
    fullname = (char *)malloc(strlen(fname) + strlen(mimetype) + 2);
    if (fullname == NULL)
      return -1;
    sprintf(fullname, "%s.%s", fname, mimetype);
    data = data + strlen(mimetype) + 6 + 3;
  default:
    ;
  }

  /* skip description string */
  switch (f->data[0]) {
  case ID3_TEXT_UTF16:
  case ID3_TEXT_UTF16BE:
    while (!(data[0] == '\0' && data[1] == '\0') && data - f->data < f->sz)
      data += 2;
    if (!(data[0] == '\0' && data[1] == '\0'))
      goto error_free;
    data += 2;
    break;
  case ID3_TEXT_UTF8:
  case ID3_TEXT_ISO:
    while (data[0] != '\0' && data - f->data < f->sz)
      data++;
    if (data[0] != '\0')
      goto error_free;
    data++;
    break;
  }

  if (data - f->data >= f->sz)
    goto error_free;

  out = fopen(fullname, "wb");
  if (out == NULL)
    goto error_close;
  if (fwrite(data, f->sz - (data - f->data), 1, out) < 1)
    goto error_close;
  fclose(out);
  free(fullname);

  return 0;

 error_close:
  err_save = errno;
  fclose(out);
  free(fullname);
  errno = err_save;
  return -1;

 error_free:
  free(fullname);
  errno = EINVAL;
  return -1;
}
