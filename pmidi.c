/*
 *
 * -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\"
 * -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\"
 * -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\"
 * -DPACKAGE=\"pmidi\" -DVERSION=\"1.7.1\"
 * -DHAVE_LIBASOUND=1 -DUSE_DRAIN=1
 * -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1
 * -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1
 * -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1
 * -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1
 * -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1
 * -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1
 * -DHAVE_ALSA_ASOUNDLIB_H=1 -DHAVE_VPRINTF=1
 *
 *
 *
 */
#define PACKAGE_NAME ""
#define PACKAGE_TARNAME ""
#define PACKAGE_VERSION ""
#define PACKAGE_STRING ""
#define PACKAGE_BUGREPORT ""
#define PACKAGE_URL ""
#define PACKAGE "pmidi"
#define VERSION "1.7.1"
#define HAVE_LIBASOUND 1
#define USE_DRAIN 1
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#define HAVE_UNISTD_H 1
#define HAVE_ALSA_ASOUNDLIB_H 1
#define HAVE_VPRINTF 1







#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


#if HAVE_ALSA_ASOUNDLIB_H
#include <alsa/asoundlib.h>
#else
#include <sys/asoundlib.h>
#endif














/*
 *
 * File: midi.h
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 */
/*
 * Midi status byte values.
 */
#define MIDI_NOTE_OFF	0x80
#define MIDI_NOTE_ON	0x90
#define MIDI_KEY_AFTERTOUCH	0xa0
#define MIDI_CONTROLER	0xb0
#define MIDI_PATCH	0xc0
#define MIDI_CHANNEL_AFTERTOUCH	0xd0
#define MIDI_PITCH_WHEEL	0xe0
#define MIDI_SYSEX	0xf0
#define MIDI_META	0xff

/* Meta event defines */
#define MIDI_META_SEQUENCE   0
/* The text type meta events */
#define MIDI_META_TEXT       1
#define MIDI_META_COPYRIGHT  2
#define MIDI_META_TRACKNAME  3
#define MIDI_META_INSTRUMENT 4
#define MIDI_META_LYRIC      5
#define MIDI_META_MARKER     6
#define MIDI_META_CUE        7
/* More meta events */
#define MIDI_META_CHANNEL      0x20
#define MIDI_META_PORT         0x21
#define MIDI_META_EOT          0x2f
#define MIDI_META_TEMPO        0x51
#define MIDI_META_SMPTE_OFFSET 0x54
#define MIDI_META_TIME         0x58
#define MIDI_META_KEY          0x59
#define MIDI_META_PROP         0x7f

/** The maximum of the midi defined text types */
#define MIDI_MAX_TEXT_TYPE    7

#define MIDI_HEAD_MAGIC 0x4d546864
#define MIDI_TRACK_MAGIC  0x4d54726b

struct rootElement *midi_read(FILE *fp);
struct rootElement *midi_read_file(char *name);































/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 * Modified: Steve Ratcliffe May 1999.  File exists so that pmidi
 * is not dependant on glib.
 */

#ifndef __G_LIB_H__
#define __G_LIB_H__


#include <stdlib.h>
#include <string.h>

/* system specific config file glibconfig.h provides definitions for
 * the extrema of many of the standard types. These are:
 *
 *  G_MINSHORT, G_MAXSHORT
 *  G_MININT, G_MAXINT
 *  G_MINLONG, G_MAXLONG
 *  G_MINFLOAT, G_MAXFLOAT
 *  G_MINDOUBLE, G_MAXDOUBLE
 *
 * It also provides the following typedefs:
 *
 *  gint8, guint8
 *  gint16, guint16
 *  gint32, guint32
 *  gint64, guint64
 *
 * It defines the G_BYTE_ORDER symbol to one of G_*_ENDIAN (see later in
 * this file).
 *
 * And it provides a way to store and retrieve a `gint' in/from a `gpointer'.
 * This is useful to pass an integer instead of a pointer to a callback.
 *
 *  GINT_TO_POINTER(i), GUINT_TO_POINTER(i)
 *  GPOINTER_TO_INT(p), GPOINTER_TO_UINT(p)
 *
 * Finally, it provide the following wrappers to STDC functions:
 *
 *  g_ATEXIT
 *    To register hooks which are executed on exit().
 *    Usually a wrapper for STDC atexit.
 *
 *  void *g_memmove(void *dest, const void *src, guint count);
 *    A wrapper for STDC memmove, or an implementation, if memmove doesn't
 *    exist.  The prototype looks like the above, give or take a const,
 *    or size_t.
 */

typedef signed char gint8;
typedef unsigned char guint8;
typedef signed short gint16;
typedef unsigned short guint16;
typedef signed int gint32;
typedef unsigned int guint32;

/* include varargs functions for assertment macros
 */
#include <stdarg.h>

/* optionally feature DMALLOC memory allocation debugger
 */
#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


#ifdef NATIVE_WIN32

/* On native Win32, directory separator is the backslash, and search path
 * separator is the semicolon.
 */
#define G_DIR_SEPARATOR '\\'
#define G_DIR_SEPARATOR_S "\\"
#define G_SEARCHPATH_SEPARATOR ';'
#define G_SEARCHPATH_SEPARATOR_S ";"

#else  /* !NATIVE_WIN32 */

/* Unix */

#define G_DIR_SEPARATOR '/'
#define G_DIR_SEPARATOR_S "/"
#define G_SEARCHPATH_SEPARATOR ':'
#define G_SEARCHPATH_SEPARATOR_S ":"

#endif /* !NATIVE_WIN32 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	NULL
#define	NULL	((void*) 0)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


/* Define G_VA_COPY() to do the right thing for copying va_list variables.
 * glibconfig.h may have already defined G_VA_COPY as va_copy or __va_copy.
 */
#if !defined (G_VA_COPY)
#  if defined (__GNUC__) && defined (__PPC__) && (defined (_CALL_SYSV) || defined (_WIN32))
#  define G_VA_COPY(ap1, ap2)	  (*(ap1) = *(ap2))
#  elif defined (G_VA_COPY_AS_ARRAY)
#  define G_VA_COPY(ap1, ap2)	  g_memmove ((ap1), (ap2), sizeof (va_list))
#  else /* va_list is a pointer */
#  define G_VA_COPY(ap1, ap2)	  ((ap1) = (ap2))
#  endif /* va_list is a pointer */
#endif /* !G_VA_COPY */


/* Provide convenience macros for handling structure
 * fields through their offsets.
 */
#define G_STRUCT_OFFSET(struct_type, member)	\
    ((gulong) ((gchar*) &((struct_type*) 0)->member))
#define G_STRUCT_MEMBER_P(struct_p, struct_offset)   \
    ((gpointer) ((gchar*) (struct_p) + (gulong) (struct_offset)))
#define G_STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) G_STRUCT_MEMBER_P ((struct_p), (struct_offset)))


/* inlining hassle. for compilers that don't allow the `inline' keyword,
 * mostly because of strict ANSI C compliance or dumbness, we try to fall
 * back to either `__inline__' or `__inline'.
 * we define G_CAN_INLINE, if the compiler seems to be actually
 * *capable* to do function inlining, in which case inline function bodys
 * do make sense. we also define G_INLINE_FUNC to properly export the
 * function prototypes if no inlining can be performed.
 * we special case most of the stuff, so inline functions can have a normal
 * implementation by defining G_INLINE_FUNC to extern and G_CAN_INLINE to 1.
 */
#ifndef G_INLINE_FUNC
#  define G_CAN_INLINE 1
#endif
#ifdef G_HAVE_INLINE
#  if defined (__GNUC__) && defined (__STRICT_ANSI__)
#    undef inline
#    define inline __inline__
#  endif
#else /* !G_HAVE_INLINE */
#  undef inline
#  if defined (G_HAVE___INLINE__)
#    define inline __inline__
#  else /* !inline && !__inline__ */
#    if defined (G_HAVE___INLINE)
#      define inline __inline
#    else /* !inline && !__inline__ && !__inline */
#      define inline /* don't inline, then */
#      ifndef G_INLINE_FUNC
#	 undef G_CAN_INLINE
#      endif
#    endif
#  endif
#endif
#ifndef G_INLINE_FUNC
#  ifdef __GNUC__
#    ifdef __OPTIMIZE__
#      define G_INLINE_FUNC extern inline
#    else
#      undef G_CAN_INLINE
#      define G_INLINE_FUNC extern
#    endif
#  else /* !__GNUC__ */
#    ifdef G_CAN_INLINE
#      define G_INLINE_FUNC static inline
#    else
#      define G_INLINE_FUNC extern
#    endif
#  endif /* !__GNUC__ */
#endif /* !G_INLINE_FUNC */


/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif


/* Provide macros to feature the GCC function attribute.
 */
#if	__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define G_GNUC_PRINTF( format_idx, arg_idx )	\
  __attribute__((format (printf, format_idx, arg_idx)))
#define G_GNUC_SCANF( format_idx, arg_idx )	\
  __attribute__((format (scanf, format_idx, arg_idx)))
#define G_GNUC_FORMAT( arg_idx )		\
  __attribute__((format_arg (arg_idx)))
#define G_GNUC_NORETURN				\
  __attribute__((noreturn))
#define G_GNUC_CONST				\
  __attribute__((const))
#define G_GNUC_UNUSED				\
  __attribute__((unused))
#else	/* !__GNUC__ */
#define G_GNUC_PRINTF( format_idx, arg_idx )
#define G_GNUC_SCANF( format_idx, arg_idx )
#define G_GNUC_FORMAT( arg_idx )
#define G_GNUC_NORETURN
#define G_GNUC_CONST
#define	G_GNUC_UNUSED
#endif	/* !__GNUC__ */


/* Wrap the gcc __PRETTY_FUNCTION__ and __FUNCTION__ variables with
 * macros, so we can refer to them as strings unconditionally.
 */
#ifdef	__GNUC__
#define	G_GNUC_FUNCTION		__FUNCTION__
#define	G_GNUC_PRETTY_FUNCTION	__PRETTY_FUNCTION__
#else	/* !__GNUC__ */
#define	G_GNUC_FUNCTION		""
#define	G_GNUC_PRETTY_FUNCTION	""
#endif	/* !__GNUC__ */

/* we try to provide a usefull equivalent for ATEXIT if it is
 * not defined, but use is actually abandoned. people should
 * use g_atexit() instead.
 */
#ifndef ATEXIT
# define ATEXIT(proc)	g_ATEXIT(proc)
#else
# define G_NATIVE_ATEXIT
#endif /* ATEXIT */

/* Hacker macro to place breakpoints for elected machines.
 * Actual use is strongly deprecated of course ;)
 */
#if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ __volatile__ ("int $03"); }G_STMT_END
#elif defined (__alpha__) && defined (__GNUC__) && __GNUC__ >= 2
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ __volatile__ ("bpt"); }G_STMT_END
#else	/* !__i386__ && !__alpha__ */
#define	G_BREAKPOINT()
#endif	/* __i386__ */


/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#  define g_new(type, count)	  \
      ((type *) malloc ((unsigned) sizeof (type) * (count)))
#  define g_new0(type, count)	  \
      ((type *) calloc ((unsigned) sizeof (type) * (count), 1))
#  define g_renew(type, mem, count)	  \
      ((type *) realloc (mem, (unsigned) sizeof (type) * (count)))

#define g_mem_chunk_create(type, pre_alloc, alloc_type)	( \
  g_mem_chunk_new (#type " mem chunks (" #pre_alloc ")", \
		   sizeof (type), \
		   sizeof (type) * (pre_alloc), \
		   (alloc_type)) \
)
#define g_chunk_new(type, chunk)	( \
  (type *) g_mem_chunk_alloc (chunk) \
)
#define g_chunk_new0(type, chunk)	( \
  (type *) g_mem_chunk_alloc0 (chunk) \
)
#define g_chunk_free(mem, mem_chunk)	G_STMT_START { \
  g_mem_chunk_free ((mem_chunk), (mem)); \
} G_STMT_END


#define g_string(x) #x


/* Provide macros for error handling. The "assert" macros will
 *  exit on failure. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  if G_DISABLE_ASSERT is not defined, in order to support gcc's
 *  __PRETTY_FUNCTION__ capability.
 */

#ifdef G_DISABLE_ASSERT

#define g_assert(expr)
#define g_assert_not_reached()

#else /* !G_DISABLE_ASSERT */

#ifdef __GNUC__

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d (%s): assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      __PRETTY_FUNCTION__,				\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		\
     g_log (G_LOG_DOMAIN,					\
	    G_LOG_LEVEL_ERROR,					\
	    "file %s: line %d (%s): should not be reached",	\
	    __FILE__,						\
	    __LINE__,						\
	    __PRETTY_FUNCTION__);	}G_STMT_END

#else /* !__GNUC__ */

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d: assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{	\
     g_log (G_LOG_DOMAIN,				\
	    G_LOG_LEVEL_ERROR,				\
	    "file %s: line %d: should not be reached",	\
	    __FILE__,					\
	    __LINE__);		}G_STMT_END

#endif /* __GNUC__ */

#endif /* !G_DISABLE_ASSERT */


#ifdef G_DISABLE_CHECKS

#define g_return_if_fail(expr)
#define g_return_val_if_fail(expr,val)

#else /* !G_DISABLE_CHECKS */

#ifdef __GNUC__

#define g_return_if_fail(expr)		G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return;							\
       };				}G_STMT_END

#define g_return_val_if_fail(expr,val)	G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return val;							\
       };				}G_STMT_END

#else /* !__GNUC__ */

#define g_return_if_fail(expr)		G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return;						\
       };				}G_STMT_END

#define g_return_val_if_fail(expr, val)	G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return val;						\
       };				}G_STMT_END

#endif /* !__GNUC__ */

#endif /* !G_DISABLE_CHECKS */


/* Provide type definitions for commonly used types.
 *  These are useful because a "gint8" can be adjusted
 *  to be 1 byte (8 bits) on all platforms. Similarly and
 *  more importantly, "gint32" can be adjusted to be
 *  4 bytes (32 bits) on all platforms.
 */

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char	guchar;
typedef unsigned short	gushort;
typedef unsigned long	gulong;
typedef unsigned int	guint;

typedef float	gfloat;
typedef double	gdouble;

/* HAVE_LONG_DOUBLE doesn't work correctly on all platforms.
 * Since gldouble isn't used anywhere, just disable it for now */

#if 0
#ifdef HAVE_LONG_DOUBLE
typedef long double gldouble;
#else /* HAVE_LONG_DOUBLE */
typedef double gldouble;
#endif /* HAVE_LONG_DOUBLE */
#endif /* 0 */

typedef void* gpointer;
typedef const void *gconstpointer;


typedef gint32	gssize;
typedef guint32 gsize;
typedef guint32 GQuark;
typedef gint32	GTime;


/* Portable endian checks and conversions
 *
 * glibconfig.h defines G_BYTE_ORDER which expands to one of
 * the below macros.
 */
#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN    4321
#define G_PDP_ENDIAN    3412		/* unused, need specific PDP check */


/* Basic bit swapping functions
 */
#define GUINT16_SWAP_LE_BE_CONSTANT(val)	((guint16) ( \
    (((guint16) (val) & (guint16) 0x00ffU) << 8) | \
    (((guint16) (val) & (guint16) 0xff00U) >> 8)))
#define GUINT32_SWAP_LE_BE_CONSTANT(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x000000ffU) << 24) | \
    (((guint32) (val) & (guint32) 0x0000ff00U) <<  8) | \
    (((guint32) (val) & (guint32) 0x00ff0000U) >>  8) | \
    (((guint32) (val) & (guint32) 0xff000000U) >> 24)))

/* Intel specific stuff for speed
 */
#if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#  define GUINT16_SWAP_LE_BE_X86(val) \
     (__extension__					\
      ({ register guint16 __v;				\
	 if (__builtin_constant_p (val))		\
	   __v = GUINT16_SWAP_LE_BE_CONSTANT (val);	\
	 else						\
	   __asm__ __const__ ("rorw $8, %w0"		\
			      : "=r" (__v)		\
			      : "0" ((guint16) (val)));	\
	__v; }))
#  define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_X86 (val))
#  if !defined(__i486__) && !defined(__i586__) \
      && !defined(__pentium__) && !defined(__i686__) && !defined(__pentiumpro__)
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __const__ ("rorw $8, %w0\n\t"		\
			       "rorl $16, %0\n\t"		\
			       "rorw $8, %w0"			\
			       : "=r" (__v)			\
			       : "0" ((guint32) (val)));	\
	__v; }))
#  else /* 486 and higher has bswap */
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __const__ ("bswap %0"			\
			       : "=r" (__v)			\
			       : "0" ((guint32) (val)));	\
	__v; }))
#  endif /* processor specific 32-bit stuff */
#  define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_X86 (val))
#else /* !__i386__ */
#  define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_CONSTANT (val))
#  define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_CONSTANT (val))
#endif /* __i386__ */

#ifdef G_HAVE_GINT64
#  define GUINT64_SWAP_LE_BE_CONSTANT(val)	((guint64) ( \
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00000000000000ffU)) << 56) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x000000000000ff00U)) << 40) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x0000000000ff0000U)) << 24) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00000000ff000000U)) <<  8) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x000000ff00000000U)) >>  8) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x0000ff0000000000U)) >> 24) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0x00ff000000000000U)) >> 40) |	\
      (((guint64) (val) &						\
	(guint64) G_GINT64_CONSTANT(0xff00000000000000U)) >> 56)))
#  if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#    define GUINT64_SWAP_LE_BE_X86(val) \
	(__extension__						\
	 ({ union { guint64 __ll;				\
		    guint32 __l[2]; } __r;			\
	    if (__builtin_constant_p (val))			\
	      __r.__ll = GUINT64_SWAP_LE_BE_CONSTANT (val);	\
	    else						\
	      {							\
	 	union { guint64 __ll;				\
			guint32 __l[2]; } __w;			\
		__w.__ll = ((guint64) val);			\
		__r.__l[0] = GUINT32_SWAP_LE_BE (__w.__l[1]);	\
		__r.__l[1] = GUINT32_SWAP_LE_BE (__w.__l[0]);	\
	      }							\
	  __r.__ll; }))
#    define GUINT64_SWAP_LE_BE(val) (GUINT64_SWAP_LE_BE_X86 (val))
#  else /* !__i386__ */
#    define GUINT64_SWAP_LE_BE(val) (GUINT64_SWAP_LE_BE_CONSTANT(val))
#  endif
#endif

#define GUINT16_SWAP_LE_PDP(val)	((guint16) (val))
#define GUINT16_SWAP_BE_PDP(val)	(GUINT16_SWAP_LE_BE (val))
#define GUINT32_SWAP_LE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x0000ffffU) << 16) | \
    (((guint32) (val) & (guint32) 0xffff0000U) >> 16)))
#define GUINT32_SWAP_BE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x00ff00ffU) << 8) | \
    (((guint32) (val) & (guint32) 0xff00ff00U) >> 8)))

/* The G*_TO_?E() macros are defined in glibconfig.h.
 * The transformation is symmetric, so the FROM just maps to the TO.
 */
#define GINT16_FROM_LE(val)	(GINT16_TO_LE (val))
#define GUINT16_FROM_LE(val)	(GUINT16_TO_LE (val))
#define GINT16_FROM_BE(val)	(GINT16_TO_BE (val))
#define GUINT16_FROM_BE(val)	(GUINT16_TO_BE (val))
#define GINT32_FROM_LE(val)	(GINT32_TO_LE (val))
#define GUINT32_FROM_LE(val)	(GUINT32_TO_LE (val))
#define GINT32_FROM_BE(val)	(GINT32_TO_BE (val))
#define GUINT32_FROM_BE(val)	(GUINT32_TO_BE (val))

#ifdef G_HAVE_GINT64
#define GINT64_FROM_LE(val)	(GINT64_TO_LE (val))
#define GUINT64_FROM_LE(val)	(GUINT64_TO_LE (val))
#define GINT64_FROM_BE(val)	(GINT64_TO_BE (val))
#define GUINT64_FROM_BE(val)	(GUINT64_TO_BE (val))
#endif

#define GLONG_FROM_LE(val)	(GLONG_TO_LE (val))
#define GULONG_FROM_LE(val)	(GULONG_TO_LE (val))
#define GLONG_FROM_BE(val)	(GLONG_TO_BE (val))
#define GULONG_FROM_BE(val)	(GULONG_TO_BE (val))

#define GINT_FROM_LE(val)	(GINT_TO_LE (val))
#define GUINT_FROM_LE(val)	(GUINT_TO_LE (val))
#define GINT_FROM_BE(val)	(GINT_TO_BE (val))
#define GUINT_FROM_BE(val)	(GUINT_TO_BE (val))


/* Portable versions of host-network order stuff
 */
#define g_ntohl(val) (GUINT32_FROM_BE (val))
#define g_ntohs(val) (GUINT16_FROM_BE (val))
#define g_htonl(val) (GUINT32_TO_BE (val))
#define g_htons(val) (GUINT16_TO_BE (val))


/* Glib version.
 * we prefix variable declarations so they can
 * properly get exported in windows dlls.
 */
#ifdef NATIVE_WIN32
#  ifdef GLIB_COMPILATION
#    define GUTILS_C_VAR __declspec(dllexport)
#  else /* !GLIB_COMPILATION */
#    define GUTILS_C_VAR extern __declspec(dllimport)
#  endif /* !GLIB_COMPILATION */
#else /* !NATIVE_WIN32 */
#  define GUTILS_C_VAR extern
#endif /* !NATIVE_WIN32 */

GUTILS_C_VAR const guint glib_major_version;
GUTILS_C_VAR const guint glib_minor_version;
GUTILS_C_VAR const guint glib_micro_version;
GUTILS_C_VAR const guint glib_interface_age;
GUTILS_C_VAR const guint glib_binary_age;

#define GLIB_CHECK_VERSION(major,minor,micro)    \
    (GLIB_MAJOR_VERSION > (major) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION > (minor)) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION == (minor) && \
      GLIB_MICRO_VERSION >= (micro)))

/* Forward declarations of glib types.
 */
typedef struct _GAllocator	GAllocator;
typedef struct _GArray		GArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GCache		GCache;
typedef struct _GCompletion	GCompletion;
typedef	struct _GData		GData;
typedef struct _GDebugKey	GDebugKey;
typedef struct _GHashTable	GHashTable;
typedef struct _GHook		GHook;
typedef struct _GHookList	GHookList;
typedef struct _GList		GList;
typedef struct _GMemChunk	GMemChunk;
typedef struct _GNode		GNode;
typedef struct _GPtrArray	GPtrArray;
typedef struct _GRelation	GRelation;
typedef struct _GScanner	GScanner;
typedef struct _GScannerConfig	GScannerConfig;
typedef struct _GSList		GSList;
typedef struct _GString		GString;
typedef struct _GStringChunk	GStringChunk;
typedef struct _GTimer		GTimer;
typedef struct _GTree		GTree;
typedef struct _GTuples		GTuples;
typedef union  _GTokenValue	GTokenValue;
typedef struct _GIOChannel	GIOChannel;

typedef enum
{
  G_TRAVERSE_LEAFS	= 1 << 0,
  G_TRAVERSE_NON_LEAFS	= 1 << 1,
  G_TRAVERSE_ALL	= G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS,
  G_TRAVERSE_MASK	= 0x03
} GTraverseFlags;

typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER,
  G_LEVEL_ORDER
} GTraverseType;

/* Log level shift offset for user defined
 * log levels (0-7 are used by GLib).
 */
#define	G_LOG_LEVEL_USER_SHIFT	(8)

/* Glib log levels and flags.
 */
typedef enum
{
  /* log flags */
  G_LOG_FLAG_RECURSION		= 1 << 0,
  G_LOG_FLAG_FATAL		= 1 << 1,

  /* GLib log levels */
  G_LOG_LEVEL_ERROR		= 1 << 2,	/* always fatal */
  G_LOG_LEVEL_CRITICAL		= 1 << 3,
  G_LOG_LEVEL_WARNING		= 1 << 4,
  G_LOG_LEVEL_MESSAGE		= 1 << 5,
  G_LOG_LEVEL_INFO		= 1 << 6,
  G_LOG_LEVEL_DEBUG		= 1 << 7,

  G_LOG_LEVEL_MASK		= ~(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL)
} GLogLevelFlags;

/* GLib log levels that are considered fatal by default */
#define	G_LOG_FATAL_MASK	(G_LOG_FLAG_RECURSION | G_LOG_LEVEL_ERROR)


typedef gpointer	(*GCacheNewFunc)	(gpointer	key);
typedef gpointer	(*GCacheDupFunc)	(gpointer	value);
typedef void		(*GCacheDestroyFunc)	(gpointer	value);
typedef gint		(*GCompareFunc)		(gconstpointer	a,
						 gconstpointer	b);
typedef gchar*		(*GCompletionFunc)	(gpointer);
typedef void		(*GDestroyNotify)	(gpointer	data);
typedef void		(*GDataForeachFunc)	(GQuark		key_id,
						 gpointer	data,
						 gpointer	user_data);
typedef void		(*GFunc)		(gpointer	data,
						 gpointer	user_data);
typedef guint		(*GHashFunc)		(gconstpointer	key);
typedef void		(*GFreeFunc)		(gpointer	data);
typedef void		(*GHFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gboolean	(*GHRFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gint		(*GHookCompareFunc)	(GHook		*new_hook,
						 GHook		*sibling);
typedef gboolean	(*GHookFindFunc)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookMarshaller)	(GHook		*hook,
						 gpointer	 data);
typedef gboolean	(*GHookCheckMarshaller)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookFunc)		(gpointer	 data);
typedef gboolean	(*GHookCheckFunc)	(gpointer	 data);
typedef void		(*GHookFreeFunc)	(GHookList      *hook_list,
						 GHook          *hook);
typedef void		(*GLogFunc)		(const gchar   *log_domain,
						 GLogLevelFlags	log_level,
						 const gchar   *message,
						 gpointer	user_data);
typedef gboolean	(*GNodeTraverseFunc)	(GNode	       *node,
						 gpointer	data);
typedef void		(*GNodeForeachFunc)	(GNode	       *node,
						 gpointer	data);
typedef gint		(*GSearchFunc)		(gpointer	key,
						 gpointer	data);
typedef void		(*GScannerMsgFunc)	(GScanner      *scanner,
						 gchar	       *message,
						 gint		error);
typedef gint		(*GTraverseFunc)	(gpointer	key,
						 gpointer	value,
						 gpointer	data);
typedef	void		(*GVoidFunc)		(void);


struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GSList
{
  gpointer data;
  GSList *next;
};

struct _GString
{
  gchar *str;
  gint len;
};

struct _GArray
{
  gchar *data;
  guint len;
};

struct _GByteArray
{
  guint8 *data;
  guint	  len;
};

struct _GPtrArray
{
  gpointer *pdata;
  guint	    len;
};

struct _GTuples
{
  guint len;
};

struct _GDebugKey
{
  gchar *key;
  guint	 value;
};


/* Doubly linked lists
 */
void   g_list_push_allocator    (GAllocator     *allocator);
void   g_list_pop_allocator     (void);
GList* g_list_alloc		(void);
void   g_list_free		(GList		*list);
void   g_list_free_1		(GList		*list);
GList* g_list_append		(GList		*list,
				 gpointer	 data);
GList* g_list_prepend		(GList		*list,
				 gpointer	 data);
GList* g_list_insert		(GList		*list,
				 gpointer	 data,
				 gint		 position);
GList* g_list_insert_sorted	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GList* g_list_concat		(GList		*list1,
				 GList		*list2);
GList* g_list_remove		(GList		*list,
				 gpointer	 data);
GList* g_list_remove_link	(GList		*list,
				 GList		*llink);
GList* g_list_reverse		(GList		*list);
GList* g_list_copy		(GList		*list);
GList* g_list_nth		(GList		*list,
				 guint		 n);
GList* g_list_find		(GList		*list,
				 gpointer	 data);
GList* g_list_find_custom	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint   g_list_position		(GList		*list,
				 GList		*llink);
gint   g_list_index		(GList		*list,
				 gpointer	 data);
GList* g_list_last		(GList		*list);
GList* g_list_first		(GList		*list);
guint  g_list_length		(GList		*list);
void   g_list_foreach		(GList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
GList* g_list_sort              (GList          *list,
		                 GCompareFunc    compare_func);
gpointer g_list_nth_data	(GList		*list,
				 guint		 n);
#define g_list_previous(list)	((list) ? (((GList *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((GList *)(list))->next) : NULL)


/* Singly linked lists
 */
void    g_slist_push_allocator  (GAllocator     *allocator);
void    g_slist_pop_allocator   (void);
GSList* g_slist_alloc		(void);
void	g_slist_free		(GSList		*list);
void	g_slist_free_1		(GSList		*list);
GSList* g_slist_append		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_prepend		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_insert		(GSList		*list,
				 gpointer	 data,
				 gint		 position);
GSList* g_slist_insert_sorted	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GSList* g_slist_concat		(GSList		*list1,
				 GSList		*list2);
GSList* g_slist_remove		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_remove_link	(GSList		*list,
				 GSList		*llink);
GSList* g_slist_reverse		(GSList		*list);
GSList*	g_slist_copy		(GSList		*list);
GSList* g_slist_nth		(GSList		*list,
				 guint		 n);
GSList* g_slist_find		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_find_custom	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint	g_slist_position	(GSList		*list,
				 GSList		*llink);
gint	g_slist_index		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_last		(GSList		*list);
guint	g_slist_length		(GSList		*list);
void	g_slist_foreach		(GSList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
GSList*  g_slist_sort           (GSList          *list,
		                 GCompareFunc    compare_func);
gpointer g_slist_nth_data	(GSList		*list,
				 guint		 n);
#define g_slist_next(slist)	((slist) ? (((GSList *)(slist))->next) : NULL)


/* Hash tables
 */
GHashTable* g_hash_table_new		(GHashFunc	 hash_func,
					 GCompareFunc	 key_compare_func);
void	    g_hash_table_destroy	(GHashTable	*hash_table);
void	    g_hash_table_insert		(GHashTable	*hash_table,
					 gpointer	 key,
					 gpointer	 value);
void	    g_hash_table_remove		(GHashTable	*hash_table,
					 gconstpointer	 key);
gpointer    g_hash_table_lookup		(GHashTable	*hash_table,
					 gconstpointer	 key);
gboolean    g_hash_table_lookup_extended(GHashTable	*hash_table,
					 gconstpointer	 lookup_key,
					 gpointer	*orig_key,
					 gpointer	*value);
void	    g_hash_table_freeze		(GHashTable	*hash_table);
void	    g_hash_table_thaw		(GHashTable	*hash_table);
void	    g_hash_table_foreach	(GHashTable	*hash_table,
					 GHFunc		 func,
					 gpointer	 user_data);
guint	    g_hash_table_foreach_remove	(GHashTable	*hash_table,
					 GHRFunc	 func,
					 gpointer	 user_data);
guint	    g_hash_table_size		(GHashTable	*hash_table);


/* Caches
 */
GCache*	 g_cache_new	       (GCacheNewFunc	   value_new_func,
				GCacheDestroyFunc  value_destroy_func,
				GCacheDupFunc	   key_dup_func,
				GCacheDestroyFunc  key_destroy_func,
				GHashFunc	   hash_key_func,
				GHashFunc	   hash_value_func,
				GCompareFunc	   key_compare_func);
void	 g_cache_destroy       (GCache		  *cache);
gpointer g_cache_insert	       (GCache		  *cache,
				gpointer	   key);
void	 g_cache_remove	       (GCache		  *cache,
				gpointer	   value);
void	 g_cache_key_foreach   (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);
void	 g_cache_value_foreach (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);


/* Balanced binary trees
 */
GTree*	 g_tree_new	 (GCompareFunc	 key_compare_func);
void	 g_tree_destroy	 (GTree		*tree);
void	 g_tree_insert	 (GTree		*tree,
			  gpointer	 key,
			  gpointer	 value);
void	 g_tree_remove	 (GTree		*tree,
			  gpointer	 key);
gpointer g_tree_lookup	 (GTree		*tree,
			  gpointer	 key);
void	 g_tree_traverse (GTree		*tree,
			  GTraverseFunc	 traverse_func,
			  GTraverseType	 traverse_type,
			  gpointer	 data);
gpointer g_tree_search	 (GTree		*tree,
			  GSearchFunc	 search_func,
			  gpointer	 data);
gint	 g_tree_height	 (GTree		*tree);
gint	 g_tree_nnodes	 (GTree		*tree);



/* N-way tree implementation
 */
struct _GNode
{
  gpointer data;
  GNode	  *next;
  GNode	  *prev;
  GNode	  *parent;
  GNode	  *children;
};

#define	 G_NODE_IS_ROOT(node)	(((GNode*) (node))->parent == NULL && \
				 ((GNode*) (node))->prev == NULL && \
				 ((GNode*) (node))->next == NULL)
#define	 G_NODE_IS_LEAF(node)	(((GNode*) (node))->children == NULL)

void     g_node_push_allocator  (GAllocator       *allocator);
void     g_node_pop_allocator   (void);
GNode*	 g_node_new		(gpointer	   data);
void	 g_node_destroy		(GNode		  *root);
void	 g_node_unlink		(GNode		  *node);
GNode*	 g_node_insert		(GNode		  *parent,
				 gint		   position,
				 GNode		  *node);
GNode*	 g_node_insert_before	(GNode		  *parent,
				 GNode		  *sibling,
				 GNode		  *node);
GNode*	 g_node_prepend		(GNode		  *parent,
				 GNode		  *node);
guint	 g_node_n_nodes		(GNode		  *root,
				 GTraverseFlags	   flags);
GNode*	 g_node_get_root	(GNode		  *node);
gboolean g_node_is_ancestor	(GNode		  *node,
				 GNode		  *descendant);
guint	 g_node_depth		(GNode		  *node);
GNode*	 g_node_find		(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gpointer	   data);

/* convenience macros */
#define g_node_append(parent, node)				\
     g_node_insert_before ((parent), NULL, (node))
#define	g_node_insert_data(parent, position, data)		\
     g_node_insert ((parent), (position), g_node_new (data))
#define	g_node_insert_data_before(parent, sibling, data)	\
     g_node_insert_before ((parent), (sibling), g_node_new (data))
#define	g_node_prepend_data(parent, data)			\
     g_node_prepend ((parent), g_node_new (data))
#define	g_node_append_data(parent, data)			\
     g_node_insert_before ((parent), NULL, g_node_new (data))

/* traversal function, assumes that `node' is root
 * (only traverses `node' and its subtree).
 * this function is just a high level interface to
 * low level traversal functions, optimized for speed.
 */
void	 g_node_traverse	(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gint		   max_depth,
				 GNodeTraverseFunc func,
				 gpointer	   data);

/* return the maximum tree height starting with `node', this is an expensive
 * operation, since we need to visit all nodes. this could be shortened by
 * adding `guint height' to struct _GNode, but then again, this is not very
 * often needed, and would make g_node_insert() more time consuming.
 */
guint	 g_node_max_height	 (GNode *root);

void	 g_node_children_foreach (GNode		  *node,
				  GTraverseFlags   flags,
				  GNodeForeachFunc func,
				  gpointer	   data);
void	 g_node_reverse_children (GNode		  *node);
guint	 g_node_n_children	 (GNode		  *node);
GNode*	 g_node_nth_child	 (GNode		  *node,
				  guint		   n);
GNode*	 g_node_last_child	 (GNode		  *node);
GNode*	 g_node_find_child	 (GNode		  *node,
				  GTraverseFlags   flags,
				  gpointer	   data);
gint	 g_node_child_position	 (GNode		  *node,
				  GNode		  *child);
gint	 g_node_child_index	 (GNode		  *node,
				  gpointer	   data);

GNode*	 g_node_first_sibling	 (GNode		  *node);
GNode*	 g_node_last_sibling	 (GNode		  *node);

#define	 g_node_prev_sibling(node)	((node) ? \
					 ((GNode*) (node))->prev : NULL)
#define	 g_node_next_sibling(node)	((node) ? \
					 ((GNode*) (node))->next : NULL)
#define	 g_node_first_child(node)	((node) ? \
					 ((GNode*) (node))->children : NULL)


/* Callback maintenance functions
 */
#define G_HOOK_FLAG_USER_SHIFT	(4)
typedef enum
{
  G_HOOK_FLAG_ACTIVE	= 1 << 0,
  G_HOOK_FLAG_IN_CALL	= 1 << 1,
  G_HOOK_FLAG_MASK	= 0x0f
} GHookFlagMask;

#define	G_HOOK_DEFERRED_DESTROY	((GHookFreeFunc) 0x01)

struct _GHookList
{
  guint		 seq_id;
  guint		 hook_size;
  guint		 is_setup : 1;
  GHook		*hooks;
  GMemChunk	*hook_memchunk;
  GHookFreeFunc	 hook_free; /* virtual function */
  GHookFreeFunc	 hook_destroy; /* virtual function */
};

struct _GHook
{
  gpointer	 data;
  GHook		*next;
  GHook		*prev;
  guint		 ref_count;
  guint		 hook_id;
  guint		 flags;
  gpointer	 func;
  GDestroyNotify destroy;
};

#define	G_HOOK_ACTIVE(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_ACTIVE) != 0)
#define	G_HOOK_IN_CALL(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_IN_CALL) != 0)
#define G_HOOK_IS_VALID(hook)		(((GHook*) hook)->hook_id != 0 && \
					 G_HOOK_ACTIVE (hook))
#define G_HOOK_IS_UNLINKED(hook)	(((GHook*) hook)->next == NULL && \
					 ((GHook*) hook)->prev == NULL && \
					 ((GHook*) hook)->hook_id == 0 && \
					 ((GHook*) hook)->ref_count == 0)

void	 g_hook_list_init		(GHookList		*hook_list,
					 guint			 hook_size);
void	 g_hook_list_clear		(GHookList		*hook_list);
GHook*	 g_hook_alloc			(GHookList		*hook_list);
void	 g_hook_free			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_ref			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_unref			(GHookList		*hook_list,
					 GHook			*hook);
gboolean g_hook_destroy			(GHookList		*hook_list,
					 guint			 hook_id);
void	 g_hook_destroy_link		(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_prepend			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_insert_before		(GHookList		*hook_list,
					 GHook			*sibling,
					 GHook			*hook);
void	 g_hook_insert_sorted		(GHookList		*hook_list,
					 GHook			*hook,
					 GHookCompareFunc	 func);
GHook*	 g_hook_get			(GHookList		*hook_list,
					 guint			 hook_id);
GHook*	 g_hook_find			(GHookList		*hook_list,
					 gboolean		 need_valids,
					 GHookFindFunc		 func,
					 gpointer		 data);
GHook*	 g_hook_find_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 data);
GHook*	 g_hook_find_func		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func);
GHook*	 g_hook_find_func_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func,
					 gpointer		 data);
/* return the first valid hook, and increment its reference count */
GHook*	 g_hook_first_valid		(GHookList		*hook_list,
					 gboolean		 may_be_in_call);
/* return the next valid hook with incremented reference count, and
 * decrement the reference count of the original hook
 */
GHook*	 g_hook_next_valid		(GHookList		*hook_list,
					 GHook			*hook,
					 gboolean		 may_be_in_call);

/* GHookCompareFunc implementation to insert hooks sorted by their id */
gint	 g_hook_compare_ids		(GHook			*new_hook,
					 GHook			*sibling);

/* convenience macros */
#define	 g_hook_append( hook_list, hook )  \
     g_hook_insert_before ((hook_list), NULL, (hook))

/* invoke all valid hooks with the (*GHookFunc) signature.
 */
void	 g_hook_list_invoke		(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke all valid hooks with the (*GHookCheckFunc) signature,
 * and destroy the hook if FALSE is returned.
 */
void	 g_hook_list_invoke_check	(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke a marshaller on all valid hooks.
 */
void	 g_hook_list_marshal		(GHookList		*hook_list,
					 gboolean		 may_recurse,
					 GHookMarshaller	 marshaller,
					 gpointer		 data);
void	 g_hook_list_marshal_check	(GHookList		*hook_list,
					 gboolean		 may_recurse,
					 GHookCheckMarshaller	 marshaller,
					 gpointer		 data);


/* Fatal error handlers.
 * g_on_error_query() will prompt the user to either
 * [E]xit, [H]alt, [P]roceed or show [S]tack trace.
 * g_on_error_stack_trace() invokes gdb, which attaches to the current
 * process and shows a stack trace.
 * These function may cause different actions on non-unix platforms.
 * The prg_name arg is required by gdb to find the executable, if it is
 * passed as NULL, g_on_error_query() will try g_get_prgname().
 */
void g_on_error_query (const gchar *prg_name);
void g_on_error_stack_trace (const gchar *prg_name);


/* Logging mechanism
 */
extern	        const gchar		*g_log_domain_glib;
guint		g_log_set_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_levels,
					 GLogFunc	 log_func,
					 gpointer	 user_data);
void		g_log_remove_handler	(const gchar	*log_domain,
					 guint		 handler_id);
void		g_log_default_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*message,
					 gpointer	 unused_data);
void		g_log			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 ...) G_GNUC_PRINTF (3, 4);
void		g_logv			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 va_list	 args);
GLogLevelFlags	g_log_set_fatal_mask	(const gchar	*log_domain,
					 GLogLevelFlags	 fatal_mask);
GLogLevelFlags	g_log_set_always_fatal	(GLogLevelFlags	 fatal_mask);
#ifndef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN	((gchar*) 0)
#endif	/* G_LOG_DOMAIN */

#define	g_error(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_ERROR, \
					       format, ##args)
#define	g_message(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_MESSAGE, \
					       format, ##args)
#define	g_warning printf



/* Memory allocation and debugging
 */
#define g_malloc(size)	     ((gpointer)malloc(size))
#define g_malloc0(size)	     ((gpointer)calloc(size, 1))
#define g_realloc(mem,size)  ((gpointer)realloc(mem, size))
#define g_free(mem)	     free(mem)


void	 g_mem_profile (void);
void	 g_mem_check   (gpointer  mem);


/* String utility functions that modify a string argument or
 * return a constant string that must not be freed.
 */
#define	 G_STR_DELIMITERS	"_-|> <."
gchar*	 g_strdelimit		(gchar	     *string,
				 const gchar *delimiters,
				 gchar	      new_delimiter);
gdouble	 g_strtod		(const gchar *nptr,
				 gchar	    **endptr);
gchar*	 g_strerror		(gint	      errnum);
gchar*	 g_strsignal		(gint	      signum);
gint	 g_strcasecmp		(const gchar *s1,
				 const gchar *s2);
gint	 g_strncasecmp		(const gchar *s1,
				 const gchar *s2,
				 guint 	      n);
void	 g_strdown		(gchar	     *string);
void	 g_strup		(gchar	     *string);
void	 g_strreverse		(gchar	     *string);
/* removes leading spaces */
gchar*   g_strchug              (gchar        *string);
/* removes trailing spaces */
gchar*  g_strchomp              (gchar        *string);
/* removes leading & trailing spaces */
#define g_strstrip( string )	g_strchomp (g_strchug (string))

/* String utility functions that return a newly allocated string which
 * ought to be freed from the caller at some point.
 */
gchar*	 g_strdup		(const gchar *str);
gchar*	 g_strdup_printf	(const gchar *format,
				 ...) G_GNUC_PRINTF (1, 2);
gchar*	 g_strdup_vprintf	(const gchar *format,
				 va_list      args);
gchar*	 g_strndup		(const gchar *str,
				 guint	      n);
gchar*	 g_strnfill		(guint	      length,
				 gchar	      fill_char);
gchar*	 g_strconcat		(const gchar *string1,
				 ...); /* NULL terminated */
gchar*   g_strjoin		(const gchar  *separator,
				 ...); /* NULL terminated */
gchar*	 g_strescape		(gchar	      *string);
gpointer g_memdup		(gconstpointer mem,
				 guint	       byte_size);

/* NULL terminated string arrays.
 * g_strsplit() splits up string into max_tokens tokens at delim and
 * returns a newly allocated string array.
 * g_strjoinv() concatenates all of str_array's strings, sliding in an
 * optional separator, the returned string is newly allocated.
 * g_strfreev() frees the array itself and all of its strings.
 */
gchar**	 g_strsplit		(const gchar  *string,
				 const gchar  *delimiter,
				 gint          max_tokens);
gchar*   g_strjoinv		(const gchar  *separator,
				 gchar       **str_array);
void     g_strfreev		(gchar       **str_array);



/* calculate a string size, guarranteed to fit format + args.
 */
guint	g_printf_string_upper_bound (const gchar* format,
				     va_list	  args);


/* Retrive static string info
 */
gchar*	g_get_user_name		(void);
gchar*	g_get_real_name		(void);
gchar*	g_get_home_dir		(void);
gchar*	g_get_tmp_dir		(void);
gchar*	g_get_prgname		(void);
void	g_set_prgname		(const gchar *prgname);


/* Miscellaneous utility functions
 */
guint	g_parse_debug_string	(const gchar *string,
				 GDebugKey   *keys,
				 guint	      nkeys);
gint	g_snprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 ...) G_GNUC_PRINTF (3, 4);
gint	g_vsnprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 va_list      args);
gchar*	g_basename		(const gchar *file_name);
/* Check if a file name is an absolute path */
gboolean g_path_is_absolute	(const gchar *file_name);
/* In case of absolute paths, skip the root part */
gchar*  g_path_skip_root	(gchar       *file_name);

/* strings are newly allocated with g_malloc() */
gchar*	g_dirname		(const gchar *file_name);
gchar*	g_get_current_dir	(void);
gchar*  g_getenv		(const gchar *variable);


/* we use a GLib function as a replacement for ATEXIT, so
 * the programmer is not required to check the return value
 * (if there is any in the implementation) and doesn't encounter
 * missing include files.
 */
void	g_atexit		(GVoidFunc    func);


/* Bit tests
 */
G_INLINE_FUNC gint	g_bit_nth_lsf (guint32 mask,
				       gint    nth_bit);
#ifdef	G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_lsf (guint32 mask,
	       gint    nth_bit)
{
  do
    {
      nth_bit++;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit < 32);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC gint	g_bit_nth_msf (guint32 mask,
				       gint    nth_bit);
#ifdef G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_msf (guint32 mask,
	       gint    nth_bit)
{
  if (nth_bit < 0)
    nth_bit = 32;
  do
    {
      nth_bit--;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit > 0);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC guint	g_bit_storage (guint number);
#ifdef G_CAN_INLINE
G_INLINE_FUNC guint
g_bit_storage (guint number)
{
  register guint n_bits = 0;

  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
}
#endif	/* G_CAN_INLINE */

/* String Chunks
 */
GStringChunk* g_string_chunk_new	   (gint size);
void	      g_string_chunk_free	   (GStringChunk *chunk);
gchar*	      g_string_chunk_insert	   (GStringChunk *chunk,
					    const gchar	 *string);
gchar*	      g_string_chunk_insert_const  (GStringChunk *chunk,
					    const gchar	 *string);


/* Strings
 */
GString* g_string_new	    (const gchar *init);
GString* g_string_sized_new (guint	  dfl_size);
void	 g_string_free	    (GString	 *string,
			     gint	  free_segment);
GString* g_string_assign    (GString	 *lval,
			     const gchar *rval);
GString* g_string_truncate  (GString	 *string,
			     gint	  len);
GString* g_string_append    (GString	 *string,
			     const gchar *val);
GString* g_string_append_c  (GString	 *string,
			     gchar	  c);
GString* g_string_prepend   (GString	 *string,
			     const gchar *val);
GString* g_string_prepend_c (GString	 *string,
			     gchar	  c);
GString* g_string_insert    (GString	 *string,
			     gint	  pos,
			     const gchar *val);
GString* g_string_insert_c  (GString	 *string,
			     gint	  pos,
			     gchar	  c);
GString* g_string_erase	    (GString	 *string,
			     gint	  pos,
			     gint	  len);
GString* g_string_down	    (GString	 *string);
GString* g_string_up	    (GString	 *string);
void	 g_string_sprintf   (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);
void	 g_string_sprintfa  (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);


/* Resizable arrays, remove fills any cleared spot and shortens the
 * array, while preserving the order. remove_fast will distort the
 * order by moving the last element to the position of the removed
 */

#define g_array_append_val(a,v)	  g_array_append_vals (a, &v, 1)
#define g_array_prepend_val(a,v)  g_array_prepend_vals (a, &v, 1)
#define g_array_insert_val(a,i,v) g_array_insert_vals (a, i, &v, 1)
#define g_array_index(a,t,i)      (((t*) (a)->data) [(i)])

GArray* g_array_new	          (gboolean	    zero_terminated,
				   gboolean	    clear,
				   guint	    element_size);
void	g_array_free	          (GArray	   *array,
				   gboolean	    free_segment);
GArray* g_array_append_vals       (GArray	   *array,
				   gconstpointer    data,
				   guint	    len);
GArray* g_array_prepend_vals      (GArray	   *array,
				   gconstpointer    data,
				   guint	    len);
GArray* g_array_insert_vals       (GArray          *array,
				   guint            index,
				   gconstpointer    data,
				   guint            len);
GArray* g_array_set_size          (GArray	   *array,
				   guint	    length);
GArray* g_array_remove_index	  (GArray	   *array,
				   guint	    index);
GArray* g_array_remove_index_fast (GArray	   *array,
				   guint	    index);

/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends appends a pointer.  Remove fills any
 * cleared spot and shortens the array. remove_fast will again distort
 * order.
 */
#define	    g_ptr_array_index(array,index) (array->pdata)[index]
GPtrArray*  g_ptr_array_new		   (void);
void	    g_ptr_array_free		   (GPtrArray	*array,
					    gboolean	 free_seg);
void	    g_ptr_array_set_size	   (GPtrArray	*array,
					    gint	 length);
gpointer    g_ptr_array_remove_index	   (GPtrArray	*array,
					    guint	 index);
gpointer    g_ptr_array_remove_index_fast  (GPtrArray	*array,
					    guint	 index);
gboolean    g_ptr_array_remove		   (GPtrArray	*array,
					    gpointer	 data);
gboolean    g_ptr_array_remove_fast        (GPtrArray	*array,
					    gpointer	 data);
void	    g_ptr_array_add		   (GPtrArray	*array,
					    gpointer	 data);

/* Byte arrays, an array of guint8.  Implemented as a GArray,
 * but type-safe.
 */

GByteArray* g_byte_array_new	           (void);
void	    g_byte_array_free	           (GByteArray	 *array,
					    gboolean	  free_segment);
GByteArray* g_byte_array_append	           (GByteArray	 *array,
					    const guint8 *data,
					    guint	  len);
GByteArray* g_byte_array_prepend           (GByteArray	 *array,
					    const guint8 *data,
					    guint	  len);
GByteArray* g_byte_array_set_size          (GByteArray	 *array,
					    guint	  length);
GByteArray* g_byte_array_remove_index	   (GByteArray	 *array,
					    guint	  index);
GByteArray* g_byte_array_remove_index_fast (GByteArray	 *array,
					    guint	  index);

#endif /* __G_LIB_H__ */
































/*
 *
 * File: elements.h
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *
 *
 *
 */

#define MD_CONTAINER_BEGIN  50  /* Begining of container types */

/*
 * The basic element type.  All elements have a time associated with
 * them and most use the device and channel field.
 */
struct element {
	short	type;			/* Element type */
	guint32 element_time;	/* Time for this element */
	short device_channel;	/* Device/channel for this element */
};
#define MD_ELEMENT(e) \
	((struct element *)md_check_cast((struct element *)(e), MD_TYPE_ELEMENT))


struct containerElement {
	struct element parent;
	GPtrArray *elements;	/* List of elements */
};
#define MD_CONTAINER(e) \
	((struct containerElement *)md_check_cast((struct element *)(e), MD_TYPE_CONTAINER))


struct rootElement {
	struct containerElement parent;
	short  format;	/* Midi format */
	short  tracks;	/* Number of tracks */
	short  time_base;	/* Time base value */
};
#define MD_ROOT(e) \
	((struct rootElement *)md_check_cast((struct element *)(e), MD_TYPE_ROOT))


struct trackElement {
	struct containerElement parent;
	guint32 final_time;
};
#define MD_TRACK(e) \
	((struct trackElement *)md_check_cast((struct element *)(e), MD_TYPE_TRACK))


struct tempomapElement {
	struct containerElement parent;
};
#define MD_TEMPOMAP(e) \
	((struct tempomapElement *)md_check_cast((struct element *)(e), MD_TYPE_TEMPOMAP))


struct noteElement {
	struct element parent;
	short  note;
	short  vel;
	int    length;
	short  offvel;	/* Note Off velocity */
};
#define MD_NOTE(e) \
	((struct noteElement *)md_check_cast((struct element *)(e), MD_TYPE_NOTE))


struct partElement {
	struct containerElement parent;
	guint32 final_time;
};
#define MD_PART(e) \
	((struct partElement *)md_check_cast((struct element *)(e), MD_TYPE_PART))


struct controlElement {
	struct element parent;
	short  control;	/* Controller number */
	short  value;	/* Controller value */
};
#define MD_CONTROL(e) \
	((struct controlElement *)md_check_cast((struct element *)(e), MD_TYPE_CONTROL))


struct programElement {
	struct element parent;
	int  program;	/* Program number */
};
#define MD_PROGRAM(e) \
	((struct programElement *)md_check_cast((struct element *)(e), MD_TYPE_PROGRAM))


struct keytouchElement {
	struct element parent;
	int  note;
	int  velocity;
};
#define MD_KEYTOUCH(e) \
	((struct keytouchElement *)md_check_cast((struct element *)(e), MD_TYPE_KEYTOUCH))


struct pressureElement {
	struct element parent;
	int  velocity;
};
#define MD_PRESSURE(e) \
	((struct pressureElement *)md_check_cast((struct element *)(e), MD_TYPE_PRESSURE))


struct pitchElement {
	struct element parent;
	int  pitch;
};
#define MD_PITCH(e) \
	((struct pitchElement *)md_check_cast((struct element *)(e), MD_TYPE_PITCH))


struct sysexElement {
	struct element parent;
	int  status;
	unsigned char *data;
	int  length;
};
#define MD_SYSEX(e) \
	((struct sysexElement *)md_check_cast((struct element *)(e), MD_TYPE_SYSEX))


struct metaElement {
	struct element parent;
};
#define MD_META(e) \
	((struct metaElement *)md_check_cast((struct element *)(e), MD_TYPE_META))


struct mapElement {
	struct metaElement parent;
};
#define MD_MAP(e) \
	((struct mapElement *)md_check_cast((struct element *)(e), MD_TYPE_MAP))


struct keysigElement {
	struct mapElement parent;
	char key;		/* Key signature */
	char minor;		/* Is this a minor key or not */
};
#define MD_KEYSIG(e) \
	((struct keysigElement *)md_check_cast((struct element *)(e), MD_TYPE_KEYSIG))


struct timesigElement {
	struct mapElement parent;
	short top;		/* 'top' of timesignature */
	short bottom;	/* 'bottom' of timesignature */
	short clocks;	/* Can't remember what this is */
	short n32pq;	/* Thirtysecond notes per quarter */
};
#define MD_TIMESIG(e) \
	((struct timesigElement *)md_check_cast((struct element *)(e), MD_TYPE_TIMESIG))


struct tempoElement {
	struct mapElement parent;
	int  micro_tempo;	/* The tempo in microsec per quarter note */
};
#define MD_TEMPO(e) \
	((struct tempoElement *)md_check_cast((struct element *)(e), MD_TYPE_TEMPO))


struct textElement {
	struct element parent;
	int  type;	/* Type of text (lyric, copyright etc) */
	char *name;	/* Type as text */
	char *text; /* actual text */
	int  length; /* length of the text (including a null?) */
};
#define MD_TEXT(e) \
	((struct textElement *)md_check_cast((struct element *)(e), MD_TYPE_TEXT))


struct smpteoffsetElement {
	struct element parent;
	short  hours;
	short  minutes;
	short  seconds;
	short  frames;
	short  subframes;
};
#define MD_SMPTEOFFSET(e) \
	((struct smpteoffsetElement *)md_check_cast((struct element *)(e), MD_TYPE_SMPTEOFFSET))


struct element *md_element_new();
struct containerElement *md_container_new();
struct rootElement *md_root_new(void);
struct trackElement *md_track_new(void);
struct tempomapElement *md_tempomap_new();
struct noteElement *md_note_new(short note, short vel, int length);
struct partElement *md_part_new(void);
struct controlElement *md_control_new(short control, short value);
struct programElement *md_program_new(int program);
struct keytouchElement *md_keytouch_new(int note, int vel);
struct pressureElement *md_pressure_new(int vel);
struct pitchElement *md_pitch_new(int val);
struct sysexElement *md_sysex_new(int status, unsigned char *data, int len);
struct metaElement *md_meta_new();
struct mapElement *md_map_new();
struct keysigElement *md_keysig_new(short key, short minor);
struct timesigElement *md_timesig_new(short top, short bottom, short clocks,
        short n32pq);
struct tempoElement *md_tempo_new(int m);
struct textElement *md_text_new(int type, char *text);
struct smpteoffsetElement *md_smpteoffset_new(short hours, short minutes,
        short seconds, short frames, short subframes);
void md_add(struct containerElement *c, struct element *e);
void md_free(struct element *el);
struct element *md_check_cast(struct element *el, int type);


/* Defines for types */
#define MD_TYPE_PART (0 + MD_CONTAINER_BEGIN)
#define MD_TYPE_ROOT (1 + MD_CONTAINER_BEGIN)
#define MD_TYPE_KEYTOUCH 2
#define MD_TYPE_TEXT 3
#define MD_TYPE_PITCH 4
#define MD_TYPE_PROGRAM 5
#define MD_TYPE_META 6
#define MD_TYPE_PRESSURE 7
#define MD_TYPE_NOTE 8
#define MD_TYPE_ELEMENT 9
#define MD_TYPE_SMPTEOFFSET 10
#define MD_TYPE_TEMPO 11
#define MD_TYPE_TEMPOMAP (12 + MD_CONTAINER_BEGIN)
#define MD_TYPE_SYSEX 13
#define MD_TYPE_TRACK (14 + MD_CONTAINER_BEGIN)
#define MD_TYPE_KEYSIG 15
#define MD_TYPE_TIMESIG 16
#define MD_TYPE_CONTAINER 17
#define MD_TYPE_MAP 18
#define MD_TYPE_CONTROL 19














/*
 *
 * except.m - XXX
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 */

#include <setjmp.h>

struct except {
	int  set;
	sigjmp_buf  buf;
};

extern struct except *formatError;	/* Bad file format */
extern struct except *ioError;	/* Error reading/writing file */
extern struct except *debugError;	/* Debugging 'shouldn't happen' errors */

void except(struct except *e, char *message, ...);



























/*
 * intl.h - Internationalisation for melys
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifdef I18N

#include "libintl.h"

#define _(_STRING) gettext(_STRING)
#define N_(_STRING) _STRING

#else

#define _(_STRING) _STRING
#define N_(_STRING) _STRING

#endif





















/*
 * File: md.h
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *
 */

/* Defines for md_walk() */
#define MD_WALK_ALL	1	/* Include all elements including deleted, hidden*/

/* Defines for walk callback flags */
#define MD_WALK_START 0x001	/* This is a start element */
#define MD_WALK_END   0x002	/* This is the end of an element */
#define MD_WALK_EMPTY (MD_WALK_START|MD_WALK_END) /* Empty element */

/* Typedef for md_walk callback function */
typedef void (*walkFunc)(struct element *, void *, int);


/*
 * Structure to keep track of the position on each track that
 * is being merged.
 */
struct sequenceState {
	int  nmerge;	/* Number of tracks in trackPos to merge */
	struct trackPos *track_ptrs; /* Position pointers */
	struct rootElement *root; /* Root to be returned first */
	unsigned long endtime;	/* End time */
};
struct trackPos {
	int  len;	/* Total length of this container element */
	int  count;	/* Current position count */
	struct element **currel; /* Pointer to current position */
};

void md_walk(struct containerElement *c, walkFunc fn, void *arg, int flags);
int iscontainer(struct element *el);
struct sequenceState *md_sequence_init(struct rootElement *root);
struct element *md_sequence_next(struct sequenceState *seq);
void md_sequence_end(struct sequenceState *seq);
unsigned long md_sequence_end_time(struct sequenceState *seq);















/*
 * File: seqlib.h
 *
 * Copyright (C) 1999-2003 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 * ---------
 * This file is a set of interfaces to provide a little
 * higher level view of the sequencer.  It is mainly experimental
 * to investigate new approaches.  If successful they will be proposed
 * as additions/replacements to the existing seq lib.
 *
 *
 *
 */
//#if HAVE_ALSA_ASOUNDLIB_H
//#include <alsa/asoundlib.h>
//#else
//#include <sys/asoundlib.h>
//#endif

typedef struct seq_context seq_context_t;

seq_context_t *seq_create_context();
seq_context_t *seq_new_client(seq_context_t *ctxp);
void seq_free_context(seq_context_t *cxtp);
int seq_new_port(seq_context_t *ctxp);
void seq_destroy_port(seq_context_t *cxtp, int port);
int seq_connect_add(seq_context_t *ctxp, int client, int port);
int seq_init_tempo(seq_context_t *ctxp, int resolution, int tempo,
        int realtime);
void seq_set_queue(seq_context_t *ctxp, int q);
int seq_sendto(seq_context_t *ctxp, snd_seq_event_t *ev, int client, int port);
int seq_send_to_all(seq_context_t *ctxp, snd_seq_event_t *ep);
snd_seq_addr_t * seq_dev_addr(seq_context_t *ctxp, int dev);
void seq_start_timer(seq_context_t *ctxp);
void seq_stop_timer(seq_context_t *ctxp);
void seq_control_timer(seq_context_t *ctxp, int onoff);
int seq_write(seq_context_t *ctxp, snd_seq_event_t *ep);
void *seq_handle(seq_context_t *ctxp);

void seq_midi_event_init(seq_context_t *ctxp, snd_seq_event_t *ep,
        unsigned long time, int devchan);
void seq_midi_note(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note,
        int vel, int length);
void seq_midi_note_on(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note,
        int vel, int length);
void seq_midi_note_off(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note,
        int vel, int length);
void seq_midi_keypress(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note,
        int value);
void seq_midi_control(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int control,
        int value);
void seq_midi_program(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int program);
void seq_midi_chanpress(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan,
        int pressure);
void seq_midi_pitchbend(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int bend);
void seq_midi_tempo(seq_context_t *ctxp, snd_seq_event_t *ep, int tempo);
void seq_midi_sysex(seq_context_t *ctxp, snd_seq_event_t *ep, int status,
        unsigned char *data, int length);
void seq_midi_echo(seq_context_t *ctxp, unsigned long time);
























/*
 * File: seqpriv.h
 *
 * Copyright (C) 1999-2003 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 * ---------
 * This file is a set of interfaces to provide a little
 * higher level view of the sequencer.  It is mainly experimental
 * to investigate new approaches.  If successful they will be proposed
 * as additions/replacements to the existing seq lib.
 *
 *
 *
 */

struct seq_context {
	snd_seq_t *handle; /* The snd_seq handle to /dev/snd/seq */
	int  client;/* The client associated with this context */
	int  queue; /* The queue to use for all operations */
	snd_seq_addr_t  source;	/* Source for events */
	GArray  *destlist;	/* Destination list */
#define ctxndest destlist->len
#define ctxdest  destlist->data

	char  timer_started;	/* True if timer is running */
	int   port_count;		/* Ports allocated */

	struct seq_context *main; /* Pointer to the main context */
	GSList *ctlist;		/* Context list if a main context */
};


































/*
 *
 * File: elements.m - Operation on all elements
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"
//#include "md.h"

static void md_container_init(struct containerElement *e);

/*
 * Create and initialise a element element.
 */
struct element *
md_element_new()
{
	struct element *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_ELEMENT;
	return new;
}

/*
 * Create and initialise a container element.
 */
struct containerElement *
md_container_new()
{
	struct containerElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_CONTAINER;
	return new;
}

/*
 * Initialize data structures within the element. This is
 * only really required when pointers need allocating.
 *  Arguments:
 *    e         - Element to init
 */
static void
md_container_init(struct containerElement *e)
{

	e->elements = g_ptr_array_new();
}

/*
 * Create and initialise a root element.
 *  Arguments:
 *              -
 */
struct rootElement *
md_root_new(void)
{
	struct rootElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_ROOT;
	md_container_init(MD_CONTAINER(new));
	return new;
}

/*
 * Create and initialise a track element.
 *  Arguments:
 *              -
 */
struct trackElement *
md_track_new(void)
{
	struct trackElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_TRACK;
	md_container_init(MD_CONTAINER(new));
	return new;
}

/*
 * Create and initialise a tempomap element.
 */
struct tempomapElement *
md_tempomap_new()
{
	struct tempomapElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_TEMPOMAP;
	md_container_init(MD_CONTAINER(new));
	return new;
}

/*
 * Create and initialise a note element.
 *  Arguments:
 *    note      -
 *    vel       -
 *    length    -
 */
struct noteElement *
md_note_new(short note, short vel, int length)
{
	struct noteElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_NOTE;
	new->note = note;
	new->vel  = vel;
	new->length = length;
	new->offvel = 0;
	return new;
}

/*
 * Create and initialise a part element.
 *  Arguments:
 *              -
 */
struct partElement *
md_part_new(void)
{
	struct partElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_PART;
	md_container_init(MD_CONTAINER(new));
	return new;
}

/*
 * Create and initialise a control element.
 *  Arguments:
 *    control   -
 *    value     -
 */
struct controlElement *
md_control_new(short control, short value)
{
	struct controlElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_CONTROL;
	new->control = control;
	new->value = value;
	return new;
}

/*
 * Create and initialise a program element.
 *  Arguments:
 *    program   -
 */
struct programElement *
md_program_new(int program)
{
	struct programElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_PROGRAM;
	new->program = program;
	return new;
}

/*
 * Create and initialise a keytouch element.
 *  Arguments:
 *    note      -
 *    vel       -
 */
struct keytouchElement *
md_keytouch_new(int note, int vel)
{
	struct keytouchElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_KEYTOUCH;
	new->note = note;
	new->velocity = vel;
	return new;
}

/*
 * Create and initialise a pressure element.
 *  Arguments:
 *    vel       -
 */
struct pressureElement *
md_pressure_new(int vel)
{
	struct pressureElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_PRESSURE;
	new->velocity = vel;
	return new;
}

/*
 * Create and initialise a pitch element.
 */
struct pitchElement *
md_pitch_new(int val)
{
	struct pitchElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_PITCH;
	new->pitch = val;
	return new;
}

/*
 * Create and initialise a sysex element.
 */
struct sysexElement *
md_sysex_new(int status, unsigned char *data, int len)
{
	struct sysexElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_SYSEX;
	new->status = status;
	new->data = data;
	new->length = len;
	return new;
}

/*
 * Create and initialise a meta element.
 */
struct metaElement *
md_meta_new()
{
	struct metaElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_META;
	return new;
}

/*
 * Create and initialise a map element.
 */
struct mapElement *
md_map_new()
{
	struct mapElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_MAP;
	return new;
}

/*
 * Create and initialise a keysig element.
 */
struct keysigElement *
md_keysig_new(short key, short minor)
{
	struct keysigElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_KEYSIG;
	new->key = key;
	new->minor = minor != 0? 1: 0;
	return new;
}

/*
 * Create and initialise a timesig element.
 */
struct timesigElement *
md_timesig_new(short top, short bottom, short clocks, short n32pq)
{
	struct timesigElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_TIMESIG;
	new->top = top;
	new->bottom = bottom;
	new->clocks = clocks;
	new->n32pq = n32pq;
	return new;
}

/*
 * Create and initialise a tempo element.
 */
struct tempoElement *
md_tempo_new(int m)
{
	struct tempoElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_TEMPO;
	new->micro_tempo = m;
	return new;
}

/*
 * Create and initialise a text element.
 */
struct textElement *
md_text_new(int type, char *text)
{
	struct textElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_TEXT;
	{
	static char *typenames[] = {
		"",
		"text",
		"copyright",
		"trackname",
		"instrument",
		"lyric",
		"marker",
		"cuepoint",
	};
	new->type = type;
	new->name = typenames[type];
	new->text = text;
	if (text)
		new->length = strlen(text);
	else
		new->length = 0;
	}
	return new;
}

/*
 * Create and initialise a smpteoffset element.
 */
struct smpteoffsetElement *
md_smpteoffset_new(short hours, short minutes, short seconds, short frames,
        short subframes)
{
	struct smpteoffsetElement *  new;

	new = g_malloc0(sizeof(*new));
	MD_ELEMENT(new)->type = MD_TYPE_SMPTEOFFSET;
	new->hours = hours;
	new->minutes = minutes;
	new->seconds = seconds;
	new->frames = frames;
	new->subframes = subframes;
	return new;
}

/*
 * Add an element to a container element.
 */
void
md_add(struct containerElement *c, struct element *e)
{
	g_ptr_array_add(c->elements, e);
}

/*
 * Free a complete element tree.
 */
void
md_free(struct element *el)
{
	struct containerElement *c;
	int  i;

	if (el->type >= MD_CONTAINER_BEGIN) {
		c = MD_CONTAINER(el);
		for (i = 0; i < c->elements->len; i++) {
			struct element *p = g_ptr_array_index(c->elements, i);
			md_free(p);
		}
		g_ptr_array_free(c->elements, 1);
	}
	switch (el->type) {
	case MD_TYPE_TEXT:
		g_free(MD_TEXT(el)->text);
		break;
	case MD_TYPE_SYSEX:
		g_free(MD_SYSEX(el)->data);
		break;
	}
	g_free(el);

}

/*
 * Check that the given element can be casted to the given type.
 * This is mainly for debugging as mismatches will not happen
 * in proper use. In particular do not use this routine to check
 * types unless you want the program to exit if the check fails.
 *  Arguments:
 *    el        - Element to be cast
 *    type      - type to cast to
 */
struct element *
md_check_cast(struct element *el, int type)
{

	switch (type) {
	case MD_TYPE_CONTAINER:
		if (!iscontainer(el))
			except(debugError, "Cast to container from %d", el->type);
		return el;
	case MD_TYPE_ELEMENT:
		/* Anything can be cast to an element */
		if (el->type > 100 || el->type < 0)
			break;	/* Sanity check */
		return el;
	case MD_TYPE_META:
	case MD_TYPE_MAP:
		/* TEMP: this is a parent type */
		return el;
	case MD_TYPE_TRACK:
		if (el->type == MD_TYPE_TEMPOMAP)
			return el;
		break;
	}

	if (type == el->type)
		return el;

	except(debugError, "Cast from %d to %d not allowed", el->type, type);
	return NULL;/* not reached */

}




/*
 *
 * except.m - error handling code
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#include <stdarg.h>
//#include "stdio.h"


static struct except format;
static struct except io;
static struct except debug;

struct except *formatError = &format;	/* Bad file format */
struct except *ioError = &io;	/* I/o error to file */
struct except *debugError = &debug;	/* Debugging 'shouldn't happen' errors */

/*
 * Deal with errors. At present just exit, will allow error
 * recovery in the future.
 *  Arguments:
 *    e         - Exception thrown
 *    message   - Message
 *              - Takes variable number of arguments
 */
void
except(struct except *e, char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	putc('\n', stderr);

#ifdef DEBUG
	g_on_error_stack_trace("a.out");
#endif
	exit(1);
}








/*
 * File: glib.c Replacements for glib functions used by pmidi
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * Code is derived from garray.c which is copyright:
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 */

//#include "glib.h"

/*
 * There is no need to use this file, linking against glib is preferable
 * if you have it.
 */

#define MIN_ARRAY_SIZE  16


static gint
g_nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

/* Pointer Array
 */
typedef struct _GRealPtrArray  GRealPtrArray;

struct _GRealPtrArray
{
  gpointer *pdata;
  guint     len;
  guint     alloc;
};


GPtrArray*
g_ptr_array_new(void)
{
GRealPtrArray *array;

	array = g_new(GRealPtrArray, 1);

	array->pdata = NULL;
	array->len = 0;
	array->alloc = 0;

	return (GPtrArray*) array;
}

void
g_ptr_array_free(GPtrArray   *array, gboolean  free_segment)
{

	if (free_segment)
		g_free(array->pdata);

	g_free(array);
}

static void
g_ptr_array_maybe_expand(GRealPtrArray *array, gint len)
{
	guint old_alloc;

	if ((array->len + len) > array->alloc)
	{
		old_alloc = array->alloc;

		array->alloc = g_nearest_pow(array->len + len);
		array->alloc = MAX(array->alloc, MIN_ARRAY_SIZE);
		if (array->pdata) {
			array->pdata = g_realloc(array->pdata,
				sizeof(gpointer) * array->alloc);
		} else {
			array->pdata = g_new0(gpointer, array->alloc);
		}

		memset(array->pdata + old_alloc, 0, array->alloc - old_alloc);
	}
}


gpointer
g_ptr_array_remove_index_fast(GPtrArray* farray, guint index)
{
	GRealPtrArray* array = (GRealPtrArray*)farray;
	gpointer result;


	result = array->pdata[index];

	if (index != array->len - 1)
		array->pdata[index] = array->pdata[array->len - 1];

	array->pdata[array->len - 1] = NULL;

	array->len -= 1;

	return result;
}



typedef struct _GRealArray  GRealArray;

struct _GRealArray
{
  guint8 *data;
  guint   len;
  guint   alloc;
  guint   elt_size;
  guint   zero_terminated : 1;
  guint   clear : 1;
};

void
g_ptr_array_add(GPtrArray* farray, gpointer data)
{
	GRealPtrArray* array = (GRealPtrArray*) farray;

	g_ptr_array_maybe_expand(array, 1);

	array->pdata[array->len++] = data;
}

static void
g_array_maybe_expand (GRealArray *array, gint len)
{
	guint want_alloc = (array->len + len + array->zero_terminated) * array->elt_size;
	if (want_alloc > array->alloc) {
		guint old_alloc = array->alloc;

		array->alloc = g_nearest_pow (want_alloc);
		array->alloc = MAX(array->alloc, MIN_ARRAY_SIZE);

		array->data = g_realloc (array->data, array->alloc);

		if (array->clear || array->zero_terminated)
			memset (array->data + old_alloc, 0, array->alloc - old_alloc);
	}
}

GArray*
g_array_new (gboolean zero_terminated, gboolean clear, guint elt_size)
{
	GRealArray *array;

	array = g_new(GRealArray, 1);

	array->data            = NULL;
	array->len             = 0;
	array->alloc           = 0;
	array->zero_terminated = (zero_terminated ? 1 : 0);
	array->clear           = (clear ? 1 : 0);
	array->elt_size        = elt_size;

	return (GArray*) array;
}

void
g_array_free (GArray  *array, gboolean free_segment)
{
	if (free_segment)
		g_free (array->data);

	g_free(array);
}

GArray*
g_array_append_vals(GArray *farray, gconstpointer data, guint len)
{
	GRealArray *array = (GRealArray*) farray;

	g_array_maybe_expand (array, len);

	memcpy (array->data + array->elt_size * array->len, data, array->elt_size * len);

	array->len += len;

	return farray;
}








/*
 * File: mdutil.m - Utility routines for manipulating MD
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#include "md.h"

/*
 * Given a container c then md_walk iterates through all
 * elements and calls fn for each one with the following
 * arguments:
 *
 *    fn(element, arg, start-end-flags)
 *
 * The flag MD_WALK_START is set if this is an element start and
 * MD_WALK_END is set if this is an element end. Both are set for
 * empty elements (also can use MD_WALK_EMPTY)
 *
 *  Arguments:
 *    c         - Container to be walked
 *    fn        - Callback function to call on each element
 *    arg       - Argument passed to fn
 *    flags     - Flags eg. include all elements or just current ones.
 */
void
md_walk(struct containerElement *c, walkFunc fn, void *arg, int flags)
{
	struct element *el;
	GPtrArray *ar;
	int  i;

	fn(MD_ELEMENT(c), arg, MD_WALK_START);

	ar = c->elements;
	for (i = 0; i < ar->len; i++) {
		el = g_ptr_array_index(ar, i);
		if (iscontainer(el)) {
			md_walk(MD_CONTAINER(el), fn, arg, flags);
		} else {
			fn(el, arg, MD_WALK_EMPTY);
		}
	}

	fn(MD_ELEMENT(c), arg, MD_WALK_END);
}

/*
 * Return true if the element is some kind of container.
 *  Arguments:
 *    el        - Element to be tested
 */
int
iscontainer(struct element *el)
{

	if (el->type >= MD_CONTAINER_BEGIN)
		return 1;
	else
		return 0;
}

/*
 * Return an object that can be passed to md_sequence_next to
 * iterate over all the elements in a tree in time sequenced
 * order. This is what you want if you actually want to play the
 * song for example. The elements in the container must
 * already be in sorted order within their own tracks or
 * subcontainers.
 *
 *  Arguments:
 *    root      - Root to sequence over
 */
struct sequenceState *
md_sequence_init(struct rootElement *root)
{
	struct sequenceState *state;
	int  ntracks;
	int  i;

	state = g_new(struct sequenceState, 1);

	ntracks = MD_CONTAINER(root)->elements->len;
	state->nmerge = ntracks;
	state->track_ptrs = g_new(struct trackPos, ntracks);
	state->root = root;
	state->endtime = 0;

	/* XXX time ignored for present */
	/* Initialise the pointers */
	for (i = 0; i < ntracks; i++) {
		struct containerElement *c;
		/* c is a track or a tempo element */
		c = MD_CONTAINER(MD_CONTAINER(root)->elements->pdata[i]);
		state->track_ptrs[i].len = c->elements->len;
		state->track_ptrs[i].count = 0;
		state->track_ptrs[i].currel = (struct element **)c->elements->pdata;
		if (MD_ELEMENT(c)->type == MD_TYPE_TRACK)
			if (MD_TRACK(c)->final_time > state->endtime)
				state->endtime = MD_TRACK(c)->final_time;
	}

	return state;
}

/*
 * Return the next element as sorted by time.
 *  Arguments:
 *    seq       - Sequence state information
 */
struct element *
md_sequence_next(struct sequenceState *seq)
{
	struct element *el;
	unsigned long leasttime;
	struct trackPos *besttp;
	struct trackPos *tp;
	int  i;

	if (seq->root) {
		/* The first time we return the root element */
		el = MD_ELEMENT(seq->root);
		seq->root = NULL;
		return el;
	}

	leasttime = -1;
	besttp = NULL;

	for (i = 0; i < seq->nmerge; i++) {
		tp = &seq->track_ptrs[i];
		/* Still some left? */
		if (tp->count < tp->len) {
			el = *tp->currel;
			if (leasttime == -1 || el->element_time < leasttime) {
				leasttime = el->element_time;
				besttp = tp;
			}
		}
	}
	if (besttp) {
		el = *besttp->currel;
		besttp->count++;
		besttp->currel++;
		return el;
	}

	/* Nothing left */
	return NULL;
}

/*
 * Finish with a sequence object. Frees all resources. The
 * object cannot be used any more.
 *  Arguments:
 *    seq       - Sequence state information
 */
void
md_sequence_end(struct sequenceState *seq)
{

	if (!seq)
		return;
	if (seq->track_ptrs)
		g_free(seq->track_ptrs);
	g_free(seq);
}

/*
 * Get the final time for the song.
 *  Arguments:
 *    seq       - Sequence state information
 */
unsigned long
md_sequence_end_time(struct sequenceState *seq)
{
	return seq->endtime;
}















/*
 *
 * File: midiread.m - Read in a midi file.
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#include <stdio.h>
//#include "midi.h"

/*
 * This structure is used to keep track of the state while
 * reading in a midi file.
 */
struct midistate {
	FILE *fp;		/* File being read */
	int  current_time;	/* Current midi time */
	int  port;		/* Midi port number */
	int  device;	/* Midi device number */
	int  track_count;	/* Count of current track */
	int  chunk_size;	/* Size of current chunk */
	int  chunk_count;	/* Count within current chunk */
	GPtrArray *notes;	/* Currently on notes */

	struct tempomapElement *tempo_map;	/* The tempo map */
};

static struct rootElement *read_head(struct midistate *msp);
static struct trackElement *read_track(struct midistate *msp);
static void handle_status(struct midistate *msp, struct trackElement *track,
        int status);
static struct metaElement *handle_meta(struct midistate *msp, int type,
        unsigned char *data);
static int read_int(struct midistate *msp, int n);
static unsigned char *read_data(struct midistate *msp, int length);
static gint32 read_var(struct midistate *msp);
static void put_back(struct midistate *msp, char c);
static struct element *save_note(struct midistate *msp, int note, int vel);
static void finish_note(struct midistate *msp, int note, int vel);
static void skip_chunk(struct midistate *msp);

/*
 * Read in a midi file from the specified open file pointer, fp
 * and return an mtree structure tree representing the file.
 *
 *  Arguments:
 *    fp        - Input file pointer
 */
struct rootElement *
midi_read(FILE *fp)
{
	struct midistate mState;
	struct midistate *msp;
	struct rootElement *root;
	struct element *el;
	int  i;

	msp = &mState;
	msp->fp = fp;
	msp->tempo_map = md_tempomap_new();
	msp->notes = g_ptr_array_new();
	msp->port = 0;

	root = read_head(msp);
	md_add(MD_CONTAINER(root), NULL); /* Leave room for the tempo map */
	for (i = 0; i < root->tracks; i++) {
		el = MD_ELEMENT(read_track(msp));

		/* If format 1 then the first track is really the tempo map */
		if (root->format == 1
				&& i == 0
				&& MD_CONTAINER(el)->elements->len == 0) {
			/* It will be added after the loop */
			md_free(el);
			continue;
		}

		md_add(MD_CONTAINER(root), el);
	}

	g_ptr_array_index(MD_CONTAINER(root)->elements, 0) = msp->tempo_map;
	msp->tempo_map = NULL;

	g_ptr_array_free(msp->notes, 1);

	return root;
}

/*
 * Read in a midi file from the specified file name.
 *
 *  Arguments:
 *    name      - File name to read
 */
struct rootElement *
midi_read_file(char *name)
{
	FILE *fp;
	struct rootElement *root;

	fp = fopen(name, "rb");
	if (fp == NULL)
		except(ioError, _("Could not open file %s"), name);

	root = midi_read(fp);

	fclose(fp);

	return root;
}

/*
 * Read the header information from a midi file
 *
 *  Arguments:
 *    msp       - current midi state
 */
static struct rootElement *
read_head(struct midistate *msp)
{
	guint32  magic;
	int  length;
	struct rootElement *root;

	root = md_root_new();

	/* The first word just identifies the file as a midi file */
	magic = read_int(msp, 4);
	if (magic != MIDI_HEAD_MAGIC)
		except(formatError, _("Bad header (%x), probably not a real midi file"),
			magic);

	/* The header chunk should be 6 bytes, (perhaps longer in the future) */
	length = read_int(msp, 4);
	if (length < 6)
		except(formatError, _("Bad header length, probably not a real midi file"));

	root->format = read_int(msp, 2);
	root->tracks = read_int(msp, 2);
	root->time_base = read_int(msp, 2);

	/* Should skip any extra bytes, (may not be seekable) */
	while (length > 6) {
		length--;
		(void) getc(msp->fp);
	}

	return root;
}

/*
 * Read in one track from the file, and return an element tree
 * describing it.
 *
 *  Arguments:
 *    msp       - Midi state
 */
static struct trackElement *
read_track(struct midistate *msp)
{
	int  status, laststatus;
	int  head;
	int  length;
	int  delta_time;
	struct trackElement *track;
	int  i;

	laststatus = 0;
	head = read_int(msp, 4);
	if (head != MIDI_TRACK_MAGIC)
		except(formatError,
			_("Bad track header (%x), probably not a midi file"),
			head);

	length = read_int(msp, 4);
	msp->chunk_size = length;
	msp->chunk_count = 0;	/* nothing read yet */

	track = md_track_new();

	msp->current_time = 0;
	while (msp->chunk_count < msp->chunk_size) {

		delta_time = read_var(msp);
		msp->current_time += delta_time;

		status = read_int(msp, 1);
		if ((status & 0x80) == 0) {

			/*
			 * This is not a status byte and so running status is being
			 * used.  Re-use the previous status and push back this byte.
			 */
			put_back(msp, status);
			status = laststatus;
		} else {
			laststatus = status;
		}

		handle_status(msp, track, status);
	}

  restart:
	for (i = 0; i < msp->notes->len; i++) {
		struct noteElement *ns;
		ns = g_ptr_array_index(msp->notes, i);
		msp->device = MD_ELEMENT(ns)->device_channel;
printf("Left over note, finishing\n");
		finish_note(msp, ns->note, 0);
		goto restart;
	}

	msp->track_count++;

	return track;
}

/*
 * Complete the reading of the status byte. The parsed midi
 * command will be added to the specified track .
 *
 *  Arguments:
 *    msp       - Current midi file status
 *    track     - Current track
 *    status    - Status byte, ie. current command
 */
static void
handle_status(struct midistate *msp, struct trackElement *track, int status)
{
	int  ch;
	int  type;
	int  device;
	int  length;
	short note, vel, control;
	int  val;
	unsigned char *data;
	struct element *el;

	ch = status & 0x0f;
	type = status & 0xf0;

	/*
	 * Do not set the device if the type is 0xf0 as these commands are
	 * not channel specific
	 */
	device = msp->port<<4;
	if (type != 0xf0)
		device += ch;
	msp->device = device;

	el = NULL;

	switch (type) {
	case MIDI_NOTE_OFF:
		note = read_int(msp, 1);
		vel = read_int(msp, 1);

		finish_note(msp, note, vel);
		break;

	case MIDI_NOTE_ON:
		note = read_int(msp, 1);
		vel = read_int(msp, 1);

		if (vel == 0) {
			/* This is really a note off */
			finish_note(msp, note, vel);
		} else {
			/* Save the start, so it can be matched with the note off */
			el = save_note(msp, note, vel);
		}
		break;

	case MIDI_KEY_AFTERTOUCH:
		note = read_int(msp, 1);
		vel = read_int(msp, 1);

		/* new aftertouchElement */
		el = MD_ELEMENT(md_keytouch_new(note, vel));
		break;

	case MIDI_CONTROLER:
		control = read_int(msp, 1);
		val = read_int(msp, 1);
		el = MD_ELEMENT(md_control_new(control, val));

		break;

	case MIDI_PATCH:
		val = read_int(msp, 1);
		el = MD_ELEMENT(md_program_new(val));
		break;

	case MIDI_CHANNEL_AFTERTOUCH:
		val = read_int(msp, 1);
		el = MD_ELEMENT(md_pressure_new(val));
		break;
	case MIDI_PITCH_WHEEL:
		val = read_int(msp, 1);
		val |= read_int(msp, 1) << 7;
		val -= 0x2000;	/* Center it around zero */
		el = MD_ELEMENT(md_pitch_new(val));
		break;

	/* Now for all the non-channel specific ones */
	case 0xf0:
		/* Deal with the end of track event first */
		if (ch == 0x0f) {
			type = read_int(msp, 1);
			if (type == 0x2f) {
				/* End of track - skip to end of real track */
				track->final_time = msp->current_time;
				skip_chunk(msp);
				return;
			}
		}

		/* Get the length of the following data */
		length = read_var(msp);
		data = read_data(msp, length);
		if (ch == 0x0f) {
			el = (struct element *)handle_meta(msp, type, data);
		} else {
			el = (struct element *)md_sysex_new(status, data, length);
		}
		break;
	default:
		except(formatError, _("Bad status type 0x%x"), type);
		/*NOTREACHED*/
	}

	if (el != NULL) {
		el->element_time = msp->current_time;
		el->device_channel = device;

		md_add(MD_CONTAINER(track), el);
	}
}

/*
 * Do extra handling of meta events. We want to save time
 * signature and key for use elsewhere, for example. This
 * routine create the correct type of class and returns it.
 *
 *  Arguments:
 *    msp       - The midi file state
 *    type      - The meta event type
 *    data      - The data for the event
 */
static struct metaElement *
handle_meta(struct midistate *msp, int type, unsigned char *data)
{
	struct metaElement *el = NULL;
	struct mapElement *map = NULL;
	int  micro_tempo;

	switch (type) {
	case MIDI_META_SEQUENCE:
		break;
	case MIDI_META_TEXT:
	case MIDI_META_COPYRIGHT:
	case MIDI_META_TRACKNAME:
	case MIDI_META_INSTRUMENT:
	case MIDI_META_LYRIC:
	case MIDI_META_MARKER:
	case MIDI_META_CUE:
		/* Text based events */
		el = MD_META(md_text_new(type, (char*)data));
		break;
	case MIDI_META_CHANNEL:
		break;
	case MIDI_META_PORT:
		msp->port = data[0];
		g_free(data);
		break;
	case MIDI_META_EOT:
		break;
	case MIDI_META_TEMPO:
		micro_tempo = ((data[0]<<16) & 0xff0000)
			+ ((data[1]<<8) & 0xff00) + (data[2] & 0xff);
		map = MD_MAP(md_tempo_new(micro_tempo));
		g_free(data);
		break;
	case MIDI_META_SMPTE_OFFSET:
		el = MD_META(md_smpteoffset_new(data[0], data[1], data[2], data[3],
			data[4]));
		break;
	case MIDI_META_TIME:
		map = MD_MAP(md_timesig_new(data[0], 1<<data[1],
			data[2], data[3]));
		g_free(data);
		break;
	case MIDI_META_KEY:
		map = MD_MAP(md_keysig_new(data[0], (data[1]==1)? 1: 0));
		g_free(data);
		break;
	case MIDI_META_PROP:
		/* Proprietry sequencer specific event */
		/* Just throw it out */
		break;
	default:
		g_warning(_("Meta event %d not implemented\n"), type);
		break;
	}

	/* If this affected the tempo map then add it */
	if (map) {
		MD_ELEMENT(map)->element_time = msp->current_time;
		md_add(MD_CONTAINER(msp->tempo_map), MD_ELEMENT(map));
	}

	return el;
}

/*
 * Reads an interger from the midi file. The number of bytes to
 * be read is specified in n .
 *
 *  Arguments:
 *    msp       - Midi file state
 *    n         - Number of bytes to read
 */
static int
read_int(struct midistate *msp, int n)
{
	int  val;
	int  c;
	int  i;

	val = 0;

	for (i = 0; i < n; i++) {
		val <<= 8;
		c = getc(msp->fp);
		msp->chunk_count++;
		if (c == -1)
			except(formatError, _("Unexpected end of file"));

		val |= c;
	}

	return val;
}

/*
 * Read in a specified amount of data from the file. The return
 * is allocated data which must be freed by the caller. An extra
 * null byte is appended for the sake of text events.
 *  Arguments:
 *    msp       - Midifile state
 *    length    - Length of data to read
 */
static unsigned char *
read_data(struct midistate *msp, int length)
{

	unsigned char *data = g_malloc(length+1);

	if (length == 0) {
		data[0] = 0;
		return data;
	}

	if (fread(data, length, 1, msp->fp) == 1) {
		msp->chunk_count += length;
		data[length] = '\0';
		return data;
	} else {
		except(formatError, _("Unexpected end of file"));
		/*NOTREACHED*/
	}
	return NULL;
}

/*
 * Read a variable length integer from the midifile and return
 * it. Returns an int32 so cannot really deal with more than
 * four bytes.
 *
 *  Arguments:
 *    msp       - Midi file state
 */
static gint32
read_var(struct midistate *msp)
{
	int  val;
	int  c;

	val = 0;
	do {
		c = getc(msp->fp);
		msp->chunk_count++;
		if (c == -1)
			except(formatError, _("Unexpected end of file"));
		val <<= 7;
		val |= (c & 0x7f);
	} while ((c & 0x80) == 0x80);

	return val;
}

/*
 * Push back a character. Have to also keep track of the
 * chunk_count.
 *  Arguments:
 *    msp       - Midi input state
 *    c         - Character to push back
 */
static void
put_back(struct midistate *msp, char c)
{
	ungetc(c, msp->fp);
	msp->chunk_count--;
}

/*
 * Save the initial note-on message. This will later be paired
 * with a note off message. We have to keep track of channel and
 * device that the note-on is for so that it can be correctly
 * matched.
 *
 *  Arguments:
 *    msp       - Midi file state
 *    note      - Note number
 *    vel       - Velocity
 */
static struct element *
save_note(struct midistate *msp, int note, int vel)
{
	struct noteElement *n;

	/* Create a new note and set its length to -1
	 * this will be filled in later, when the note-off arrives
	 */
	n = md_note_new(note, vel, -1);

	/* Save it so that we match up with the note off */
	g_ptr_array_add(msp->notes, n);

	return MD_ELEMENT(n);
}

/*
 * Called when a note off is seen. Finds the corresponding note
 * on and constructs a note element.
 *
 *  Arguments:
 *    msp       - Midi file state
 *    note      - Note number
 *    vel       - Note velocity
 */
static void
finish_note(struct midistate *msp, int note, int vel)
{
	int  i;
	GPtrArray *notes;
	struct noteElement *n;
	int  len;

	notes = msp->notes;
	n = NULL;
	for (i = notes->len-1; i >= 0; i--) {
		n = g_ptr_array_index(notes, i);
		if (n->note == note && MD_ELEMENT(n)->device_channel == msp->device) {
			len = msp->current_time - MD_ELEMENT(n)->element_time;
			n->offvel = vel;
			n->length = len;
			if (n->length < 0) {
				printf("Len neg: msp->time%d, s->time%d, note=%d, s.vel%d\n",
					msp->current_time, MD_ELEMENT(n)->element_time,
					note, n->vel);
				n->length = 0;
			}
			g_ptr_array_remove_index_fast(notes, i);
			break;
		}
	}
}

/*
 * Skip to the end of the chunk. Note that the input may not be
 * seekable, so we just read bytes until at the end of the chunk.
 *
 *  Arguments:
 *    msp       - Midi file state
 */
static void
skip_chunk(struct midistate *msp)
{
	while (msp->chunk_count < msp->chunk_size)
		(void) read_int(msp, 1);
}






/*
 *
 * Copyright (C) 1999-2003 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#if HAVE_ALSA_ASOUNDLIB_H
//#include <alsa/asoundlib.h>
//#else
//#include <sys/asoundlib.h>
//#endif

//#include <stdio.h>
//#include "seqlib.h"
//#include "md.h"
//#include "midi.h"
//#include <stdlib.h>
//#include <unistd.h>
//#include <getopt.h>
//#include <string.h>
//#include <errno.h>
//#include <signal.h>

/*
 * Play a midi file
 *	pmidi [-p client:port,... ] [-l] [-d delay] file ...
 *
 *	  -p client:port  - A alsa client and port number to send midi to
 *	  -l              - List possible output ports that could be used
 *	  -d delay        - Delay after song ends (default 2 sec)
 *	  -v              - Print version and exit
 */


/* Options for the command */
#define HAS_ARG 1
static struct option long_opts[] = {
	{"port", HAS_ARG, NULL, 'p'},
	{"list", 0, NULL, 'l'},
	{"delay", HAS_ARG, NULL, 'd'},
	{"version", 0, NULL, 'v'},
	{0, 0, 0, 0},
};

/* Delay at the end of a song */
static int  delay = 2;

/* Pointer to root context */
seq_context_t *g_ctxp;

/* Number of elements in an array */
#define NELEM(a) ( sizeof(a)/sizeof((a)[0]) )

#define ADDR_PARTS 4 /* Number of part in a port description addr 1:2:3:4 */
#define SEP ", \t"	/* Separators for port description */

static seq_context_t *openports(char *portdesc);
static void playfile(seq_context_t *ctxp, char *filename);
static void showlist();
static void showusage();
static void play(void *arg, struct element *el);
static void no_errors_please(const char *file, int line, const char *function, int err, const char *fmt, ...);
static void set_signal_handler(seq_context_t *ctxp);
static void signal_handler(int sig);
static void showversion(void);

/*
 * Play a midi file
 *   pmidi [-p client:port ...] [-l] [-d delay] file ...
 *
 *   -p, --port=client:port  - An ALSA client and port number to use
 *   -l, --list              - List possible output ports that could be used
 *   -d, --delay=SECS        - Delay after song ends (default 2 sec)
 *   -v, --version           - Show version number
 *  Arguments:
 *    argc      - arg count
 *    argv      - arg vector
 */
int
main_TODO(int argc, char **argv) /* TODO */
{
	char opts[NELEM(long_opts) * 2 + 1];
	char *portdesc;
	seq_context_t *ctxp;
	char *cp;
	int  c;
	struct option *op;

	/* Build up the short option string */
	cp = opts;
	for (op = long_opts; op < &long_opts[NELEM(long_opts)]; op++) {
		*cp++ = op->val;
		if (op->has_arg)
			*cp++ = ':';
	}

	/* Libaries shouldn't print errors by default! */
	snd_lib_error_set_handler(no_errors_please);

	portdesc = NULL;

	/* Deal with the options */
	for (;;) {
		c = getopt_long(argc, argv, opts, long_opts, NULL);
		if (c == -1)
			break;

		switch(c) {
		case 'p':
			portdesc = optarg;
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'l':
			showlist();
			exit(0);
			break;
		case 'v':
			showversion();
			exit(0);
		case '?':
			showusage();
			exit(1);
		}
	}

	if (portdesc == NULL || portdesc[0] == '\0') {
		portdesc = getenv("ALSA_OUTPUT_PORTS");
		/* Try the old name for the environment variable */
		if (portdesc == NULL)
			portdesc = getenv("ALSA_OUT_PORT");
		if (portdesc == NULL) {
			fprintf(stderr, "No client/port specified.\n"
				"You must supply one with the -p option or with the\n"
				"environment variable ALSA_OUTPUT_PORTS\n"
				);
			exit(1);
		}
	}

	ctxp = openports(portdesc);
	if (ctxp == NULL)
		return 1;

	/* Set signal handler */
	set_signal_handler(ctxp);

	/* Now play all the files */
	for (; optind < argc; optind++)
		playfile(ctxp, argv[optind]);

	seq_free_context(ctxp);

	/* Restore signal handler */
	signal(SIGINT, SIG_DFL);

	return 0;
}

/*
 * Read a list of client/port specifications and return an
 * array of snd_seq_addr_t that describes them.
 *
 *  Arguments:
 *    portdesc  -
 */
static seq_context_t *
openports(char *portdesc)
{
	char *astr;
	char *cp;
	seq_context_t *ctxp;
	snd_seq_addr_t *addr;
	snd_seq_addr_t *ap;
	int a[ADDR_PARTS];
	int count, naddr;
	int i;

	if (portdesc == NULL)
		return NULL;

	ctxp = seq_create_context();

	addr = g_new(snd_seq_addr_t, strlen(portdesc));

	naddr = 0;
	ap = addr;

	for (astr = strtok(portdesc, SEP); astr; astr = strtok(NULL, SEP)) {
		for (cp = astr, count = 0; cp && *cp; cp++) {
			if (count < ADDR_PARTS)
				a[count++] = atoi(cp);
			cp = strchr(cp, ':');
			if (cp == NULL)
				break;
		}

		switch (count) {
		case 2:
			ap->client = a[0];
			ap->port = a[1];
			break;
		default:
			printf("Addresses in %d parts not supported yet\n", count);
			break;
		}
		ap++;
		naddr++;
	}

	count = 0;
	for (i = 0; i < naddr; i++) {
		int  err;

		err = seq_connect_add(ctxp, addr[i].client, addr[i].port);
		if (err < 0) {
			fprintf(stderr, _("Could not connect to port %d:%d\n"),
				addr[i].client, addr[i].port);
		} else
			count++;
	}

	g_free(addr);

	if (count == 0) {
		seq_free_context(ctxp);
		return NULL;
	}

	return ctxp;
}

/*
 * Play a single file.
 *  Arguments:
 *    ctxp      -
 *    filename  -
 */
static void
playfile(seq_context_t *ctxp, char *filename)
{
	struct rootElement *root;
	struct sequenceState *seq;
	struct element *el;
	unsigned long end;
	snd_seq_event_t *ep;

	if (strcmp(filename, "-") == 0)
		root = midi_read(stdin);
	else
		root = midi_read_file(filename);
	if (!root)
		return;

	/* Loop through all the elements in the song and play them */
	seq = md_sequence_init(root);
	while ((el = md_sequence_next(seq)) != NULL) {
		play(ctxp, el);
	}


	/* Get the end time for the tracks and echo an event to
	 * wake us up at that time
	 */
	end = md_sequence_end_time(seq);
	seq_midi_echo(ctxp, end);

#ifdef USE_DRAIN
	snd_seq_drain_output(seq_handle(ctxp));
#else
	snd_seq_flush_output(seq_handle(ctxp));
#endif

	/* Wait for all the events to be played */
	snd_seq_event_input(seq_handle(ctxp), &ep);

	/* Wait some extra time to allow for note to decay etc */
	sleep(delay);
	seq_stop_timer(ctxp);

	md_free(MD_ELEMENT(root));
}

/*
 * Show a list of possible output ports that midi could be sent
 * to.
 */
static void
showlist()
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;
	int  client;
	int  err;
	snd_seq_t *handle;

	err = snd_seq_open(&handle, "hw", SND_SEQ_OPEN_DUPLEX, 0);
	if (err < 0)
		except(ioError, _("Could not open sequencer %s"), snd_strerror(errno));

#ifdef USE_DRAIN
	/*
	 * NOTE: This is here so that it will give an error if the wrong
	 * version of alsa is used with USE_DRAIN set.  An incompatible change
	 * was made to ALSA on 29 Sep 2000 so that after that date you should
	 * define USE_DRAIN.
	 *
	 * Unfortunately with USE_DRAIN defined with an earlier version of alsa
	 * it will appear to compile fine but simply not work properly.  This
	 * unneeded call will give an error in this case.
	 *
	 * IF you see an error about this line, comment out the line beginning
	 * with USE_DRAIN in the make.conf file or compile with:
	 *    make USE_DRAIN=''
	 */
	snd_seq_drop_output(handle);
#endif

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	printf(_(" Port     %-30.30s    %s\n"), _("Client name"), _("Port name"));

	while (snd_seq_query_next_client(handle, cinfo) >= 0) {
		client = snd_seq_client_info_get_client(cinfo);
		snd_seq_port_info_alloca(&pinfo);
		snd_seq_port_info_set_client(pinfo, client);

		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(handle, pinfo) >= 0) {
			int  cap;

			cap = (SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_CAP_WRITE);
			if ((snd_seq_port_info_get_capability(pinfo) & cap) == cap) {
				printf("%3d:%-3d   %-30.30s    %s\n",
					snd_seq_port_info_get_client(pinfo),
					snd_seq_port_info_get_port(pinfo),
					snd_seq_client_info_get_name(cinfo),
					snd_seq_port_info_get_name(pinfo)
					);
			}
		}
	}
}

/*
 * Show a usage message
 */
static void
showusage()
{
char **cpp;
static char *msg[] = {
N_("Usage: pmidi [-p client:port ...] [-l] [-d delay] file ..."),
"",
N_("  -p client:port  - A alsa client and port number to send midi to"),
N_("  -l              - List possible output ports that could be used"),
N_("  -d delay        - Delay after song ends (default 2 sec)"),
N_("  -v              - Print version and exit"),
};

	for (cpp = msg; cpp < msg+NELEM(msg); cpp++) {
		fprintf(stderr, "%s\n", _(*cpp));
	}
}

static void
showversion()
{
	printf("pmidi-%s\n", VERSION);
}

static void
play(void *arg, struct element *el)
{
	seq_context_t *ctxp = arg;
	snd_seq_event_t ev;

	seq_midi_event_init(ctxp, &ev, el->element_time, el->device_channel);
	switch (el->type) {
	case MD_TYPE_ROOT:
		seq_init_tempo(ctxp, MD_ROOT(el)->time_base, 120, 1);
		seq_start_timer(ctxp);
		break;
	case MD_TYPE_NOTE:
		seq_midi_note(ctxp, &ev, el->device_channel, MD_NOTE(el)->note, MD_NOTE(el)->vel,
			MD_NOTE(el)->length);
		break;
	case MD_TYPE_CONTROL:
		seq_midi_control(ctxp, &ev, el->device_channel, MD_CONTROL(el)->control,
			MD_CONTROL(el)->value);
		break;
	case MD_TYPE_PROGRAM:
		seq_midi_program(ctxp, &ev, el->device_channel, MD_PROGRAM(el)->program);
		break;
	case MD_TYPE_TEMPO:
		seq_midi_tempo(ctxp, &ev, MD_TEMPO(el)->micro_tempo);
		break;
	case MD_TYPE_PITCH:
		seq_midi_pitchbend(ctxp, &ev, el->device_channel, MD_PITCH(el)->pitch);
		break;
	case MD_TYPE_PRESSURE:
		seq_midi_chanpress(ctxp, &ev, el->device_channel, MD_PRESSURE(el)->velocity);
		break;
	case MD_TYPE_KEYTOUCH:
		seq_midi_keypress(ctxp, &ev, el->device_channel, MD_KEYTOUCH(el)->note,
			MD_KEYTOUCH(el)->velocity);
		break;
	case MD_TYPE_SYSEX:
		seq_midi_sysex(ctxp, &ev, MD_SYSEX(el)->status, MD_SYSEX(el)->data,
			MD_SYSEX(el)->length);
		break;
	case MD_TYPE_TEXT:
	case MD_TYPE_KEYSIG:
	case MD_TYPE_TIMESIG:
	case MD_TYPE_SMPTEOFFSET:
		/* Ones that have no sequencer action */
		break;
	default:
		printf("WARNING: play: not implemented yet %d\n", el->type);
		break;
	}
}

/**
 * alsa-lib has taken to printing errors on system call failures, even
 * though such failed calls are part of normal operation.  So we have to
 * install our own handler to shut it up and prevent it messing up the list
 * output.
 *
 * Possibly not needed any more Dec 2003.
 */
static void
no_errors_please(const char *file, int line,
		const char *function, int err, const char *fmt, ...)
{
}

static void
set_signal_handler(seq_context_t *ctxp)
{
	struct sigaction *sap = calloc(1, sizeof(struct sigaction));

	g_ctxp = ctxp;

	sap->sa_handler = signal_handler;
	sigaction(SIGINT, sap, NULL);
}

/* signal handler */
static void
signal_handler(int sig)
{
	/* Close device */
	if (g_ctxp) {
		seq_free_context(g_ctxp);
	}

	exit(1);
}









/*
 * File: seqlib.m - Interface to the alsa sequencer library.
 *
 * Copyright (C) 1999-2003 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#include <stdio.h>
//#include <errno.h>
//#include <string.h>
//#include "seqlib.h"
//#include "seqpriv.h"

static void set_channel(snd_seq_event_t *ep, int chan);

/*
 * Create a main client for an application. A queue is
 * allocated and a client is created.
 */
seq_context_t *
seq_create_context()
{
	seq_context_t *ctxp;
	int  q;

	ctxp = g_new(seq_context_t, 1);
	ctxp->main = ctxp; /* This is the main context */

	if (snd_seq_open(&ctxp->handle, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0)
		except(ioError, "Could not open sequencer: %s", snd_strerror(errno));

	q = snd_seq_alloc_queue(ctxp->handle);
	ctxp->client = snd_seq_client_id(ctxp->handle);
	ctxp->queue = q;

	ctxp->destlist = g_array_new(0, 0, sizeof(snd_seq_addr_t));

	ctxp->queue = q;
	ctxp->source.client = ctxp->client;
	ctxp->source.port = 0;

	seq_new_port(ctxp);

	return ctxp;
}

/*
 * Create another client context based on the specified
 * context. The same queue will be used for both clients. A new
 * client will be created.
 *  Arguments:
 *    ctxp      -
 */
seq_context_t *
seq_new_client(seq_context_t *ctxp)
{

	return NULL;/*XXX*/

}

/*
 * Free a context and any associated data and resources. If the
 * main context is freed then the sequencer is closed. TODO:
 * link all together so can all be closed at once.
 */
void
seq_free_context(seq_context_t *ctxp)
{

	if (ctxp->main == ctxp) {
		/* This is the main context */
		snd_seq_event_t ev;
		unsigned long t;

		snd_seq_drop_output(ctxp->handle);

		t = 0;
		seq_midi_event_init(ctxp, &ev, t, 0);
		seq_midi_control(ctxp, &ev, 0, MIDI_CTL_ALL_SOUNDS_OFF, 0);
		seq_send_to_all(ctxp, &ev);
		snd_seq_drain_output(ctxp->handle);

		snd_seq_free_queue(ctxp->handle, ctxp->queue);
		snd_seq_close(ctxp->handle);
	}

	g_free(ctxp);
}

/*
 * Creates a new port on the specified context and returns the
 * port number.
 *  Arguments:
 *    ctxp      - Context to create the port for
 */
int
seq_new_port(seq_context_t *ctxp)
{
	int  ret;

	ret = snd_seq_create_simple_port(ctxp->handle, NULL,
					 SND_SEQ_PORT_CAP_WRITE |
					 SND_SEQ_PORT_CAP_SUBS_WRITE |
					 SND_SEQ_PORT_CAP_READ,
					 SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if (ret < 0) {
		printf("Error creating port %s\n", snd_strerror(errno));
		return -1;
	}
	ctxp->port_count++;
	ctxp->source.port = ret;

	return ret;/*XXX*/
}

void
seq_destroy_port(seq_context_t *ctxp, int port)
{
}

/*
 * Set up the context so that all subsequent events will be sent
 * to the specified client and port combination. A
 * subscription is made to the client/port combination.
 *  Arguments:
 *    ctxp      - Context to modify
 *    client    - Client to send subsequent events to
 *    port      - Port on the client to send events to
 */
int
seq_connect_add(seq_context_t *ctxp, int client, int port)
{
	snd_seq_addr_t addr;
	int  ret;

	memset(&addr, 0, sizeof(addr));
	addr.client = client;
	addr.port = port;

	g_array_append_val(ctxp->destlist, addr);

	ret = snd_seq_connect_to(ctxp->handle, ctxp->source.port, client, port);

	return ret;
}

/*
 * Set the initial time base and tempo. This should only be used
 * for initialisation when there is nothing playing. To
 * change the tempo during a song tempo change events are used.
 * If realtime is false the resolution is in ticks per quarter
 * note. If true, the the resolution is microseconds. There is
 * a macro XXX to convert from SMPTE codes.
 *
 *  Arguments:
 *    ctxp      - Application context
 *    resolution - Ticks per quarter note or realtime resolution
 *    tempo     - Beats per minute
 *    realtime  - True if absolute time base
 */
int
seq_init_tempo(seq_context_t *ctxp, int resolution, int tempo, int realtime)
{
	snd_seq_queue_tempo_t *qtempo;
	int  ret;

	snd_seq_queue_tempo_alloca(&qtempo);
	memset(qtempo, 0, snd_seq_queue_tempo_sizeof());
	snd_seq_queue_tempo_set_ppq(qtempo, resolution);
	snd_seq_queue_tempo_set_tempo(qtempo, 60*1000000/tempo);

	ret = snd_seq_set_queue_tempo(ctxp->handle, ctxp->queue, qtempo);

	return ret;
}

/*
 * Set the context to use the specified queue. All events sent
 * on this context will now use this queue.
 *
 *  Arguments:
 *    ctxp      - Context to modify
 *    q         - The queue number
 */
void
seq_set_queue(seq_context_t *ctxp, int q)
{
}

/*
 * Send the event to the specified client and port.
 *
 *  Arguments:
 *    ctxp      - Client context
 *    ev        - Event to send
 *    client    - Client to send the event to
 *    port      - Port to send the event to
 */
int
seq_sendto(seq_context_t *ctxp, snd_seq_event_t *ev, int client, int port)
{
	ev->source = ctxp->source;
	ev->queue = ctxp->queue;
	ev->dest.client = client;
	ev->dest.port = port;
	seq_write(ctxp, ev);

	return 0;
}

/*
 * seq_send_to_all:
 * Send the event to all the connected devices across all
 * possible channels. The messages are sent to the blocking
 * control port and so should not have timestamps in the
 * future. This function is intended for all-sounds-off type
 * controls etc.
 *
 *  Arguments:
 * 	ctxp: Client context
 * 	ep: Event prototype to be sent
 */
int
seq_send_to_all(seq_context_t *ctxp, snd_seq_event_t *ep)
{
	int  dev;
	int  chan;
	snd_seq_addr_t *addr;

	for (dev = 0; ; dev++) {
		addr = seq_dev_addr(ctxp, dev);
		if (addr == NULL)
			break; /* No more */

		ep->queue = ctxp->queue;
		ep->dest = *addr;
		for (chan = 0; chan < 16; chan++) {
			set_channel(ep, chan);
			(void) seq_write(ctxp, ep);
		}
	}

	return 0;
}

/*
 * seq_dev_addr:
 * Return the address corresponding to the specified logical
 * device.
 *
 * 	Arguments:
 * 	dev: Device to get the address of.
 * 	inout: Input or output device
 */
snd_seq_addr_t *
seq_dev_addr(seq_context_t *ctxp, int dev)
{
	GArray *list;
	snd_seq_addr_t *addr;

	list = ctxp->destlist;

	if (dev >= (int)list->len)
		return NULL;

	addr = &g_array_index(list, snd_seq_addr_t, dev);

	return addr;
}

/*
 * Start the timer. (What about timers other than the system
 * one?)
 */
void
seq_start_timer(seq_context_t *ctxp)
{
	seq_control_timer(ctxp, SND_SEQ_EVENT_START);
}

/*
 * Stop the timer. (What about timers other than the system
 * one?)
 */
void
seq_stop_timer(seq_context_t *ctxp)
{
	seq_control_timer(ctxp, SND_SEQ_EVENT_STOP);
}

void
seq_control_timer(seq_context_t *ctxp, int onoff)
{

	if (onoff == SND_SEQ_EVENT_START)
		snd_seq_start_queue(ctxp->handle, ctxp->queue, 0);
	else
		snd_seq_stop_queue(ctxp->handle, ctxp->queue, 0);

#ifdef USE_DRAIN
	snd_seq_drain_output(ctxp->handle);
#else
	snd_seq_flush_output(ctxp->handle);
#endif
}

/*
 * Write out the event. This routine blocks until
 * successfully written.
 *
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event
 */
int
seq_write(seq_context_t *ctxp, snd_seq_event_t *ep)
{
	int  err = 0;

	err = snd_seq_event_output(ctxp->handle, ep);
	if (err < 0)
		return err;

	return err;
}

/*
 * Return the handle to the underlying snd_seq stream.
 *  Arguments:
 *    ctxp      - Application context
 */
void *
seq_handle(seq_context_t *ctxp)
{
	return ctxp->handle;
}

static void
set_channel(snd_seq_event_t *ep, int chan)
{

	switch (ep->type) {
	case SND_SEQ_EVENT_NOTE:
	case SND_SEQ_EVENT_NOTEON:
	case SND_SEQ_EVENT_NOTEOFF:
		ep->data.note.channel = chan;
		break;
	case SND_SEQ_EVENT_KEYPRESS:
	case SND_SEQ_EVENT_PGMCHANGE:
	case SND_SEQ_EVENT_CHANPRESS:
	case SND_SEQ_EVENT_PITCHBEND:
	case SND_SEQ_EVENT_CONTROL14:
	case SND_SEQ_EVENT_NONREGPARAM:
	case SND_SEQ_EVENT_REGPARAM:
	case SND_SEQ_EVENT_CONTROLLER:
		ep->data.control.channel = chan;
		break;
	default:
		if (snd_seq_ev_is_channel_type(ep))
			g_warning("Missed a case in set_channel");
		break;
	}
}








/*
 * File: seqmidi.m - convert to the sequencer events
 *
 * Copyright (C) 1999 Steve Ratcliffe
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


//#include "glib.h"
//#include "elements.h"
//#include "except.h"
//#include "intl.h"

//#include <string.h>
//#include "seqlib.h"
//#include "seqpriv.h"

/*
 * Initialize a midi event from the context. The source and
 * destination addresses will be set from the information
 * stored in the context, which depends on the previous
 * connection calls. In addition the time and channel are set.
 * This should be called first before any of the following
 * functions.
 *
 *  Arguments:
 *    ctxp      - Client application
 *    ep        - Event to init
 *    time      - Midi time
 *    devchan   - Midi channel
 */
void
seq_midi_event_init(seq_context_t *ctxp, snd_seq_event_t *ep,
        unsigned long time, int devchan)
{
	int  dev;

	dev = devchan >> 4;

	/*
	 * If insufficient output devices have been registered, then we
	 * just scale the device back to fit in the correct range.  This
	 * is not necessarily what you want.
	 */
	if (dev >= ctxp->ctxndest)
		dev = dev % ctxp->ctxndest;

	snd_seq_ev_clear(ep);
	snd_seq_ev_schedule_tick(ep, ctxp->queue, 0, time);
	ep->source = ctxp->source;
	if (ctxp->ctxndest > 0)
		ep->dest = g_array_index(ctxp->destlist, snd_seq_addr_t, dev);
}

/*
 * Send a note event.
 *  Arguments:
 *    ctxp      - Client context
 *    ep        - Event template
 *    note      - Pitch of note
 *    vel       - Velocity of note
 *    length    - Length of note
 */
void
seq_midi_note(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note, int vel,
        int length)
{
	ep->type = SND_SEQ_EVENT_NOTE;

	ep->data.note.channel = devchan & 0xf;
	ep->data.note.note = note;
	ep->data.note.velocity = vel;
	ep->data.note.duration = length;

	seq_write(ctxp, ep);
}

/*
 * Send a note on event.
 *  Arguments:
 *    ctxp      - Client context
 *    ep        - Event template
 *    note      - Pitch of note
 *    vel       - Velocity of note
 *    length    - Length of note
 */
void
seq_midi_note_on(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note, int vel,
        int length)
{
	ep->type = SND_SEQ_EVENT_NOTEON;

	ep->data.note.channel = devchan & 0xf;
	ep->data.note.note = note;
	ep->data.note.velocity = vel;
	ep->data.note.duration = length;

	seq_write(ctxp, ep);
}

/*
 * Send a note off event.
 *  Arguments:
 *    ctxp      - Client context
 *    ep        - Event template
 *    note      - Pitch of note
 *    vel       - Velocity of note
 *    length    - Length of note
 */
void
seq_midi_note_off(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note, int vel,
        int length)
{
	ep->type = SND_SEQ_EVENT_NOTEOFF;

	ep->data.note.channel = devchan & 0xf;
	ep->data.note.note = note;
	ep->data.note.velocity = vel;
	ep->data.note.duration = length;

	seq_write(ctxp, ep);
}

/*
 * Send a key pressure event.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    note      - Note to be altered
 *    value     - Pressure value
 */
void
seq_midi_keypress(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int note,
        int value)
{
	ep->type = SND_SEQ_EVENT_KEYPRESS;

	ep->data.control.channel = devchan & 0xf;
	ep->data.control.param = note;
	ep->data.control.value = value;
	seq_write(ctxp, ep);
}

/*
 * Send a control event.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    control   - Controller to change
 *    value     - Value to set it to
 */
void
seq_midi_control(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int control,
        int value)
{
	ep->type = SND_SEQ_EVENT_CONTROLLER;

	ep->data.control.channel = devchan & 0xf;
	ep->data.control.param = control;
	ep->data.control.value = value;
	seq_write(ctxp, ep);
}

/*
 * Send a program change event.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    program   - Program to set
 */
void
seq_midi_program(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int program)
{
	ep->type = SND_SEQ_EVENT_PGMCHANGE;

	ep->data.control.channel = devchan & 0xf;
	ep->data.control.value = program;
	seq_write(ctxp, ep);
}

/*
 * Send a channel pressure event.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    pressure  - Pressure value
 */
void
seq_midi_chanpress(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int pressure)
{
	ep->type = SND_SEQ_EVENT_CHANPRESS;

	ep->data.control.channel = devchan & 0xf;
	ep->data.control.value = pressure;
	seq_write(ctxp, ep);
}

/*
 * Send a pitchbend message. The bend parameter is centered on
 * zero, negative values mean a lower pitch.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    bend      - Bend value, centered on zero.
 */
void
seq_midi_pitchbend(seq_context_t *ctxp, snd_seq_event_t *ep, int devchan, int bend)
{
	ep->type = SND_SEQ_EVENT_PITCHBEND;

	ep->data.control.channel = devchan & 0xf;
	ep->data.control.value = bend;
	seq_write(ctxp, ep);
}

/*
 * Send a tempo event. The tempo parameter is in microseconds
 * per beat.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    tempo     - New tempo in usec per beat
 */
void
seq_midi_tempo(seq_context_t *ctxp, snd_seq_event_t *ep, int tempo)
{
	ep->type = SND_SEQ_EVENT_TEMPO;

	ep->data.queue.queue = ctxp->queue;
	ep->data.queue.param.value = tempo;
	ep->dest.client = SND_SEQ_CLIENT_SYSTEM;
	ep->dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
	seq_write(ctxp, ep);
}

/*
 * Send a sysex event. The status byte is to distiguish
 * continuation sysex messages.
 *  Arguments:
 *    ctxp      - Application context
 *    ep        - Event template
 *    status    - Status byte for sysex
 *    data      - Data to send
 *    length    - Length of data
 */
void
seq_midi_sysex(seq_context_t *ctxp, snd_seq_event_t *ep, int status,
        unsigned char *data, int length)
{
	unsigned char *ndata;
	int  nlen;

	ep->type = SND_SEQ_EVENT_SYSEX;

	ndata = g_malloc(length + 1);
	nlen = length +1;

	ndata[0] = status;
	memcpy(ndata+1, data, length);

	snd_seq_ev_set_variable(ep, nlen, ndata);

	seq_write(ctxp, ep);

	g_free(ndata);
}

/*
 * Send an echo event back to the source client at the specified
 * time.
 *
 *  Arguments:
 *    ctxp      - Application context
 *    time      - Time of event
 */
void
seq_midi_echo(seq_context_t *ctxp, unsigned long time)
{
	snd_seq_event_t ev;

	seq_midi_event_init(ctxp, &ev, time, 0);
	/* Loop back */
	ev.dest = ctxp->source;
	seq_write(ctxp, &ev);
}




