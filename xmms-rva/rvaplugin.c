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
#include <glib.h>
#include <gtk/gtk.h>
#include <xmms/plugin.h>
#include <xmms/xmmsctrl.h>
#include <xmms/util.h>

#if STDC_HEADERS
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#undef ENABLE_NLS /* FIXME */
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

#include "nid3.h"

#ifndef ROUND
# define ROUND(x) floor((x) + 0.5)
#endif
#define DBTOFRAC(x) pow(10,(x)/20.0)

static void rva_cleanup(void);
static void rva_about(void);
static int mod_samples(gpointer *, gint, AFormat, gint, gint);

static double lmtr_lvl = 0.5;

static EffectPlugin rva_ep = {
  NULL,
  NULL,
  NULL, /* Description */
  NULL,
  rva_cleanup,
  rva_about,
  NULL,
  mod_samples,
};

EffectPlugin *
get_eplugin_info(void)
{
  rva_ep.description = g_strdup_printf(_("Relative volume adjust plugin %s"), VERSION);
  return &rva_ep;
}

struct rva_info_struct {
  gint xmms_session;

  gchar *fname;        /* the file we're currently servicing */
  gint pos;            /* the current file's playlist position */
  gint length;         /* the current file's length */
  gboolean do_adjust;  /* are we adjusting this file? */
  gdouble gain;        /* the gain, which we've read from a tag */

  gint16 *lut;         /* lookup table, points somewhere in _lut, below */
  AFormat fmt;         /* the format for which the lookup table is relevant */
};

static gint16 _lut[65536];

static struct rva_info_struct rva_info = {
  0, NULL, -1, -1, FALSE, 0.0, NULL, 0
};

#undef DEBUG

/*
 * Read the ID3 tag from file fname.
 * Return non-zero if found and place contents in *gain.
 */
static int
get_adjustment(gchar *fname, double *gain)
{
  id3_t tag;
  float ret;
  tag = id3_open(fname, ID3_RDONLY);
  if (tag) {
    ret = id3_rva_get(tag, NULL, ID3_CHANNEL_MASTER);
    id3_close(tag);
  }
  if (ret != 0.0) {
#if DEBUG
    fprintf(stderr, "rva plugin: Found RVA frame, value %f\n", ret);
#endif
    *gain = ret;
    return 1;
  }
  return 0;
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
 * Build a lookup table in rva_info.lut
 */
static void
build_lookuptable(double gain, AFormat fmt)
{
  int samplemin, samplemax, i;

  /*
   * To build the table, we pretend the format is always signed, and
   * offset the table point accordingly.  If it turns out that the
   * format is unsigned, we just move the pointer.
   */

  rva_info.lut = _lut + 32768;
  if (fmt == FMT_U8 || fmt == FMT_S8) {
    samplemin = -128; samplemax = 127;
  } else {
    samplemin = -32768; samplemax = 32767;
  }

  if (gain > 1.0) {
    /* apply gain, and apply limiter to avoid clipping */
    for (i = samplemin; i < 0; i++)
      rva_info.lut[i]
	= ROUND(-samplemin * limiter(i * gain / (double)-samplemin));
    for (; i <= samplemax; i++)
      rva_info.lut[i]
	= ROUND(samplemax * limiter(i * gain / (double)samplemax));
  } else {
    /* just apply gain if it's less than 1 */
    for (i = samplemin; i <= samplemax; i++)
      rva_info.lut[i] = i * gain;
  }

#if DEBUG
  {
    /* write the lookup table function for display in gnuplot */
    FILE *tblout = fopen("lut.dat", "w");
    for (i = samplemin; i <= samplemax; i++)
      fprintf(tblout, "%d %d\n", i, rva_info.lut[i]);
    fclose(tblout);
  }
#endif

  switch (fmt) {
  case FMT_U8:
  case FMT_U16_LE:
  case FMT_U16_BE:
  case FMT_U16_NE:
    rva_info.lut = _lut;
    break;
  default:
    /* leave at _lut + 32768 */
    break;
  }

  rva_info.fmt = fmt;

#if DEBUG
  fprintf(stderr, "Built lookup table for format %d\n", fmt);
#endif
}

static void
update_rva_info(gint pos, AFormat fmt)
{
  gchar *fname = NULL;
  double gain;

  fname = xmms_remote_get_playlist_file(rva_info.xmms_session, pos);

  /* if the file is the same as before, don't bother */
  if (fname && rva_info.fname && strcmp(fname, rva_info.fname) == 0) {
    g_free(fname);
    return;
  }

  if (rva_info.fname)
    g_free(rva_info.fname);
  rva_info.fname = fname;
  rva_info.pos = pos;
  rva_info.length = xmms_remote_get_playlist_time(rva_info.xmms_session, pos);

  if (fname == NULL) {
    rva_info.do_adjust = 0;
    return;
  }

  /* read tag */
  rva_info.do_adjust = get_adjustment(fname, &gain);
  /*gain = DBTOFRAC(gain);*/

  if (!rva_info.do_adjust)
    return;

  /* if the gain and format happen to be the same as before, we don't
     have to rebuild the lookup table */
  if (rva_info.lut && fmt == rva_info.fmt && gain == rva_info.gain)
    return;

  rva_info.gain = gain;

  /* update lookup table */
  build_lookuptable(DBTOFRAC(gain), fmt);

#if DEBUG
  fprintf(stderr, "rva_info updated\n");
#endif
}

static int
mod_samples(gpointer *data, gint length, AFormat fmt, gint srate, gint nch)
{
  gint i, pos, flen;
  gint16 *ptr16 = NULL;
  guint16 *ptru16 = NULL;
  gint8 *ptr8 = NULL;
  guint8 *ptru8 = NULL;

  pos = xmms_remote_get_playlist_pos(rva_info.xmms_session);
  flen = xmms_remote_get_playlist_time(rva_info.xmms_session, pos);

  if (flen != rva_info.length)
    update_rva_info(pos, fmt);

  if (!rva_info.do_adjust)
    return length;

#if 0
  fprintf(stderr, "adjusting buffer, length=%d\n", length);
#endif

  switch (fmt) {
  case FMT_U8:
    ptru8 = (guint8 *)*data;
    for (i = 0; i < length; i++) {
      *ptru8 = rva_info.lut[*ptru8];
      ptru8++;
    }
    break;
  case FMT_S8:
    ptr8 = (gint8 *)*data;
    for (i = 0; i < length; i++) {
      *ptr8 = rva_info.lut[*ptr8];
      ptr8++;
    }
    break;
  case FMT_U16_LE:
    ptru16 = (guint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptru16 = GUINT16_TO_LE(rva_info.lut[GUINT16_FROM_LE(*ptru16)]);
      ptru16++;
    }
    break;
  case FMT_U16_BE:
    ptru16 = (guint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptru16 = GUINT16_TO_BE(rva_info.lut[GUINT16_FROM_BE(*ptru16)]);
      ptru16++;
    }
    break;
  case FMT_U16_NE:
    ptru16 = (guint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptru16 = rva_info.lut[*ptru16];
      ptru16++;
    }
    break;
  case FMT_S16_LE:
    ptr16 = (gint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptr16 = GINT16_TO_LE(rva_info.lut[GINT16_FROM_LE(*ptr16)]);
      ptr16++;
    }
    break;
  case FMT_S16_BE:
    ptr16 = (gint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptr16 = GINT16_TO_BE(rva_info.lut[GINT16_FROM_BE(*ptr16)]);
      ptr16++;
    }
    break;
  case FMT_S16_NE:
    ptr16 = (gint16 *)*data;
    for (i = 0; i < length; i += 2) {
      *ptr16 = rva_info.lut[*ptr16];
      ptr16++;
    }
    break;
  }

  return length;
}

static void
rva_cleanup(void)
{
}

static void
rva_about(void)
{
  static GtkWidget *dialog;
  static gchar text[1024];
  gchar curradj[80];

  if (dialog != NULL)
    return;

  if (rva_info.do_adjust)
    g_snprintf(curradj, 80, _("Adjustment of %0.4fdB currently in use."),
	       rva_info.gain);
  else
    strcpy(curradj, _("No adjustment found in current file."));

  g_snprintf(text, 1024, "%s%s",
	     _("Relative Volume Adjust Plugin\n\n"
	       "A plugin to apply the volume adjustments found in ID3 tags.\n"
	       "(Such as those rendered by the \"normalize\" program.)\n"
	       "by Chris Vaill <chrisvaill@gmail.com>\n"), curradj);

  dialog = xmms_show_message(
		_("About Relative Volume Adjust Plugin"),
		text,
		_("Ok"), FALSE, NULL, NULL);

  gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
		     GTK_SIGNAL_FUNC(gtk_widget_destroyed),
		     &dialog);
}
