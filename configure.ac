dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.53)

AC_INIT([normalize],[0.7.7])
AC_CONFIG_SRCDIR(src/normalize.c)

AC_CONFIG_AUX_DIR(config)
AC_CONFIG_FILES(config/Makefile)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_LN_S

AC_DOSFILE

dnl Checks for headers
AC_HEADER_STDC([])
AC_CHECK_HEADERS([string.h math.h ctype.h fcntl.h unistd.h byteswap.h sys/types.h sys/stat.h sys/mman.h locale.h stdint.h inttypes.h errno.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
if test "$cross_compiling" = "yes"; then
    dnl Try to guess the endianness from the host cpu type
    case "$host_cpu" in
	i?86 | alpha ) ENDIAN=little;;
	*) ENDIAN=big; AC_DEFINE(WORDS_BIGENDIAN);;
    esac
    AC_MSG_WARN([I'm guessing that the target machine is ]$ENDIAN[-endian])
else
    AC_C_BIGENDIAN
fi

dnl Checks for libraries
AC_CHECK_LIB(m, sqrt, , AC_MSG_ERROR([You don't seem to have a math library!]))
AC_CHECK_FUNCS(strerror strtod strchr memcpy ftruncate)

dnl Word sizes...
if test x"$cross_compiling" = xyes -a x"$ac_cv_sizeof_long" = x; then
  # if cross-compiling, with no cached values, just assume something common. 
  ac_cv_sizeof_char=1
  ac_cv_sizeof_short=2
  ac_cv_sizeof_int=4
  ac_cv_sizeof_long=4
  AC_MSG_WARN([Cross-compiling, so cannot check type sizes; assuming short=2, int=4, long=4])
fi
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
SIZEOF_CHAR=$ac_cv_sizeof_char
SIZEOF_SHORT=$ac_cv_sizeof_short
SIZEOF_INT=$ac_cv_sizeof_int
SIZEOF_LONG=$ac_cv_sizeof_long

AH_TEMPLATE([__TYPE8__], [Define to the 8-bit integer type on your system.])
AH_TEMPLATE([__TYPE16__], [Define to the 16-bit integer type on your system.])
AH_TEMPLATE([__TYPE32__], [Define to the 32-bit integer type on your system.])

AC_MSG_CHECKING([for an 8-bit type])
if test $SIZEOF_CHAR -eq 1; then
    AC_DEFINE(__TYPE8__, char)
    TYPE8=char
elif test $SIZEOF_SHORT -eq 1; then
    AC_DEFINE(__TYPE8__, short)
    TYPE8=short
elif test $SIZEOF_INT -eq 1; then
    AC_DEFINE(__TYPE8__, int)
    TYPE8=int
elif test $SIZEOF_LONG -eq 1; then
    AC_DEFINE(__TYPE8__, long)
    TYPE8=long
else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([I can't seem to find an 8-bit integer type!])
fi
AC_MSG_RESULT($TYPE8)

AC_MSG_CHECKING([for a 16-bit type])
if test $SIZEOF_CHAR -eq 2; then
    AC_DEFINE(__TYPE16__, char)
    TYPE16=char
elif test $SIZEOF_SHORT -eq 2; then
    AC_DEFINE(__TYPE16__, short)
    TYPE16=short
elif test $SIZEOF_INT -eq 2; then
    AC_DEFINE(__TYPE16__, int)
    TYPE16=int
elif test $SIZEOF_LONG -eq 2; then
    AC_DEFINE(__TYPE16__, long)
    TYPE16=long
else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([I can't seem to find a 16-bit integer type!])
fi
AC_MSG_RESULT($TYPE16)

AC_MSG_CHECKING([for a 32-bit type])
if test $SIZEOF_CHAR -eq 4; then
    AC_DEFINE(__TYPE32__, char)
    TYPE32=char
elif test $SIZEOF_SHORT -eq 4; then
    AC_DEFINE(__TYPE32__, short)
    TYPE32=short
elif test $SIZEOF_INT -eq 4; then
    AC_DEFINE(__TYPE32__, int)
    TYPE32=int
elif test $SIZEOF_LONG -eq 4; then
    AC_DEFINE(__TYPE32__, long)
    TYPE32=long
else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([I can't seem to find a 32-bit integer type!])
fi
AC_MSG_RESULT($TYPE32)

dnl  Solaris puts int8_t, etc in inttypes.h for some reason,
dnl  and cygwin puts it in sys/types.h
AC_CHECK_TYPES([int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t],,,
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif]
)

AH_BOTTOM(
[#if !HAVE_INT8_T
typedef signed __TYPE8__ int8_t;
#endif
#if !HAVE_INT16_T
typedef signed __TYPE16__ int16_t;
#endif
#if !HAVE_INT32_T
typedef signed __TYPE32__ int32_t;
#endif
#if !HAVE_UINT8_T
typedef unsigned __TYPE8__ uint8_t;
#endif
#if !HAVE_UINT16_T
typedef unsigned __TYPE16__ uint16_t;
#endif
#if !HAVE_UINT32_T
typedef unsigned __TYPE32__ uint32_t;
#endif])

AC_CACHE_SAVE

dnl *** Helper programs used by the normalize-mp3 script ***
AC_ARG_ENABLE(helper-search,
    AC_HELP_STRING([--disable-helper-search],
		   [do not look for helper programs]),
    [ case "$enableval" in
	  yes) do_helper_search=true ;;
	  no) do_helper_search=false ;;
	  *) AC_MSG_ERROR(bad value $withval for --enable-helper-search) ;;
      esac ],
    do_helper_search=true)

AC_PATH_PROG(PERL, perl, /usr/bin/perl)
if test x$do_helper_search = xtrue; then
    AC_CHECK_PROGS(MP3DECODE, madplay mpg123, [])
    AC_CHECK_PROGS(MP3ENCODE, lame notlame bladeenc, [])
dnl AC_CHECK_PROGS(MP3INFO, mp3info id3cp, [])
    AC_CHECK_PROGS(OGGDECODE, oggdec ogg123, [])
    AC_CHECK_PROGS(OGGENCODE, oggenc, [])
    AC_CHECK_PROGS(VORBISCOMMENT, vorbiscomment, [])
    AC_CHECK_PROGS(FLACDECODE, flac, [])
    AC_CHECK_PROGS(FLACENCODE, flac, [])
    AC_CHECK_PROGS(METAFLAC, metaflac, [])
    if test x$MP3DECODE = xmpg123; then
	MP3DECODE="$MP3DECODE -q -w %w %m"
    else
	MP3DECODE="$MP3DECODE -q -o %w %m"
    fi
    if test x$MP3ENCODE = xlame -o x$MP3ENCODE = xnotlame; then
	MP3ENCODE="$MP3ENCODE --quiet -h -b %b %w %m"
    else
	MP3ENCODE="$MP3ENCODE -quiet %w %m"
    fi
    if test x$OGGDECODE = xoggdec; then
	OGGDECODE="$OGGDECODE -Q -o %w %m"
    else
	OGGDECODE="$OGGDECODE -q -d wav -f %w %m"
    fi
    if test x$OGGENCODE != x; then
	OGGENCODE="$OGGENCODE -Q -b %b -o %m %w"
    fi
    if test x$FLACDECODE != x; then
	FLACDECODE="$FLACDECODE -s -d -o %w %m"
    fi
    if test x$FLACENCODE != x; then
	FLACENCODE="$FLACENCODE -s -o %m %w"
    fi
fi

dnl *** lookup table enable/disable ***
AH_TEMPLATE([USE_LOOKUPTABLE],
	    [Define if you want to use lookup tables
	     (faster, but uses memory).])
AC_ARG_ENABLE(lookup-table,
    AC_HELP_STRING([--disable-lookup-table],
		   [do not use lookup tables (saves memory, but slow)]),
    [ case "$enableval" in
	  yes) AC_DEFINE(USE_LOOKUPTABLE) ;;
	  no) ;;
	  *) AC_MSG_ERROR(bad value $withval for --with-lookuptable) ;;
      esac ],
    AC_DEFINE(USE_LOOKUPTABLE))

dnl *** Stuff for xmms plugin ***
have_xmms=true
AM_PATH_GLIB(1.2.2, , [ have_xmms=false ])
AM_PATH_GTK(1.2.2, , [ have_xmms=false ], gthread)
AM_PATH_XMMS(1.0.0, , [ have_xmms=false ])
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_ARG_ENABLE(xmms,
    AC_HELP_STRING([--enable-xmms],
		   [build the volume adjust plugin for xmms (default yes)]),
    [ case "$enableval" in
	  yes) enable_xmms=true ;;
	  no) enable_xmms=false ;;
	  *) AC_MSG_ERROR(bad value $enableval for --enable-xmms) ;;
      esac ])

use_xmms=false
if test x$enable_xmms != xfalse; then
    if test x$have_xmms = xtrue; then
	use_xmms=true
	PLUGINS="xmms-rva $PLUGINS"
    elif test x$enable_xmms = xtrue; then
	AC_MSG_ERROR([--enable-xmms specified, but xmms not found])
    fi
fi
AC_SUBST(PLUGINS)


dnl *** Stuff for audiofile library ***
dnl v0.2.1 and before have bugs with 24-bit LE files
AM_PATH_AUDIOFILE([ 0.2.2 ], [ have_audiofile=true ])
dnl AC_CHECK_LIB(audiofile, afSetVirtualSampleFormat, have_audiofile=true)
AH_TEMPLATE([USE_AUDIOFILE],
	    [Define if you want to use the audiofile library.])
AC_ARG_WITH(audiofile,
    AC_HELP_STRING([--with-audiofile],
		   [use the audiofile library (default yes)]),
    [ case "$withval" in
	  no) with_audiofile=false ;;
	  *) with_audiofile=true ;;
      esac ])

use_audiofile=false
if test x$with_audiofile != xfalse; then
    if test x$have_audiofile = xtrue; then
	AC_DEFINE(USE_AUDIOFILE)
	use_audiofile=true
dnl	AUDIOFILELIBS="-laudiofile"
    elif test x$with_audiofile = xtrue; then
	AC_MSG_ERROR([--with-audiofile specified, but audiofile library not found])
    fi
fi
dnl AC_SUBST(AUDIOFILELIBS)
test x$use_audiofile = xfalse && AUDIOFILE_LIBS=
test x$use_audiofile = xfalse && AUDIOFILE_CFLAGS=
AM_CONDITIONAL(AUDIOFILE, test x$use_audiofile = xtrue)


dnl *** Stuff for mad mpeg audio decoder library ***
AH_TEMPLATE([USE_MAD],
	    [Define if you want to use the mad mpeg audio decoder library.])
AC_ARG_WITH(mad-prefix,
    AC_HELP_STRING([--with-mad-prefix=PFX],
		   [prefix where MAD library is installed]),
    [ CPPFLAGS="$CPPFLAGS -I$withval/include"
      LDFLAGS="$LDFLAGS -L$withval/lib"
      with_mad=true ])
AC_ARG_WITH(mad,
    AC_HELP_STRING([--with-mad],
		   [use the mad mpeg audio library (default yes)]),
    [ case "$withval" in
	  no) with_mad=false ;;
	  *) with_mad=true ;;
      esac ])
AC_CHECK_LIB(mad, mad_decoder_init, have_mad=true)

use_mad=false
if test x$with_mad != xfalse; then
    if test x$have_mad = xtrue; then
	AC_DEFINE(USE_MAD)
	use_mad=true
	MADLIBS="-lmad"
    elif test x$with_mad = xtrue; then
	AC_MSG_ERROR([--with-mad specified, but mad library not found])
    fi
fi
AC_SUBST(MADLIBS)
AM_CONDITIONAL(MAD, test x$use_mad = xtrue)

dnl *** make the name "normalize-audio" like the debian folks like it ***
AC_ARG_ENABLE(longer-name,
    AC_HELP_STRING([--enable-longer-name],
		   [name the binary "normalize-audio" instead of "normalize"]),
    [ case "$enableval" in
	  yes) NORMALIZE_BIN=normalize-audio ;;
	  no) NORMALIZE_BIN=normalize ;;
	  *) AC_MSG_ERROR(bad value $withval for --enable-longer-name) ;;
      esac ],
    NORMALIZE_BIN=normalize)
AC_SUBST(NORMALIZE_BIN)

AM_GNU_GETTEXT([external])

AC_CONFIG_FILES(src/normalize-mp3, [chmod +x src/normalize-mp3])
dnl AC_CONFIG_FILES(packaging/normalize.spec packaging/normalize.list)
AC_CONFIG_FILES(Makefile doc/Makefile doc/fr/Makefile po/Makefile.in
		src/Makefile nid3lib/Makefile xmms-rva/Makefile test/Makefile)
AC_OUTPUT

echo
echo "Configuration:"
echo "    audiofile library:         $use_audiofile"
echo "    mpeg audio support:        $use_mad"
echo "    xmms volume adjust plugin: $use_xmms"
echo
