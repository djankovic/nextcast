/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* build only butt-client */
/* #undef CLIENT_ONLY */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#define ENABLE_NLS 1

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Use dbus to get current tracks */
#define HAVE_DBUS 1

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `gethostbyaddr' function. */
#define HAVE_GETHOSTBYADDR 1

/* Define to 1 if you have the `gethostbyname' function. */
#define HAVE_GETHOSTBYNAME 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `crypto' library (-lcrypto). */
#define HAVE_LIBCRYPTO 1

/* Define to 1 if you have the `curl' library (-lcurl). */
#define HAVE_LIBCURL 1

/* Define to 1 if you have the `FLAC' library (-lFLAC). */
#define HAVE_LIBFLAC 1

/* Define to 1 if you have the `mp3lame' library (-lmp3lame). */
#define HAVE_LIBMP3LAME 1

/* Define to 1 if you have the `ogg' library (-logg). */
#define HAVE_LIBOGG 1

/* Define to 1 if you have the `opus' library (-lopus). */
#define HAVE_LIBOPUS 1

/* Define to 1 if you have the `portaudio' library (-lportaudio). */
#define HAVE_LIBPORTAUDIO 1

/* Define to 1 if you have the `portmidi' library (-lportmidi). */
#define HAVE_LIBPORTMIDI 1

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `samplerate' library (-lsamplerate). */
#define HAVE_LIBSAMPLERATE 1

/* Define to 1 if you have the `ssl' library (-lssl). */
#define HAVE_LIBSSL 1

/* Define to 1 if you have the `vorbis' library (-lvorbis). */
#define HAVE_LIBVORBIS 1

/* Define to 1 if you have the `vorbisenc' library (-lvorbisenc). */
#define HAVE_LIBVORBISENC 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the `pow' function. */
/* #undef HAVE_POW */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strpbrk' function. */
#define HAVE_STRPBRK 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `X509_check_host' function. */
#define HAVE_X509_CHECK_HOST 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Name of package */
#define PACKAGE "butt"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "BUG-REPORT-ADDRESS"

/* Define to the full name of this package. */
#define PACKAGE_NAME "butt"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "butt 1.41.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "butt"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.41.1"

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.41.1"

/* build butt and butt-client */
/* #undef WITH_CLIENT */

/* Enable support for Radio.co authentification */
/* #undef WITH_RADIOCO */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
