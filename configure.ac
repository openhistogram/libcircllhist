AC_INIT(src/circllhist.c)

node=`hostname | cut -f1 -d '.' | tr '[A-Z]' '[a-z]'`
if test -f ".configure.$node"; then
	echo "***********************************************************"
	echo "* Loading node specific configure settings for $node."
	echo "***********************************************************"
	. ./.configure.$node
fi

AC_CANONICAL_HOST
AC_CONFIG_HEADER(src/circllhist_config.h)

AC_ARG_ENABLE(strict,
	[AC_HELP_STRING([--enable-strict],
		[Compile with warnings treated as errors])],
	enable_strict="$enableval",
	enable_strict=no)

AC_PROG_CC
AC_C_INLINE
AC_C_BIGENDIAN
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(AR, ar)
AC_PATH_PROGS(PERL, perl)
AC_PATH_PROG(NROFF, nroff)
AC_PATH_PROG(CTFCONVERT, ctfconvert)
AC_PATH_PROG(CTFMERGE, ctfmerge)

if test "x$GCC" = "xyes" ; then
	C99FLAG="-std=c99"
	CFLAGS="$CFLAGS -g $C99FLAG"
	DEPFLAGS="-MM"
else
	C99FLAG="-xc99=all"
	CFLAGS="$CFLAGS -g $C99FLAG"
	DEPFLAGS="-xM1"
fi
SHLDFLAGS="$LDFLAGS"

CPPFLAGS="$CPPFLAGS -D _REENTRANT -D _XOPEN_SOURCE=600 -D _POSIX_SOURCE -D _POSIX_C_SOURCE=200112L -D _DEFAULT_SOURCE "'-I$(top_srcdir)/src'
DOTSO=.so

case $host in
*-*-darwin*)
	DOTSO=
	DOTDYLIB=dylib
	DTRACE=/usr/sbin/dtrace
	DTRACE_ENABLED=1
	PICFLAGS="-fPIC"
	CPPFLAGS="$CPPFLAGS -D_DARWIN_C_SOURCE -DMAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_6 -Wno-deprecated-declarations"
	MODULELD="$CC -bundle -flat_namespace -undefined suppress"
	SHLD="$CC -dynamiclib -single_module -undefined dynamic_lookup -fPIC"
  DOTDYLIB=.dylib
	MODULEEXT=bundle
	;;
*-*-solaris*)
	DTRACE=/usr/sbin/dtrace
	DTRACE_ENABLED=1
	CFLAGS="$CFLAGS"
	CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__"
	if test "x$GCC" != "xyes" ; then
		CFLAGS="$CFLAGS -mt"
		PICFLAGS="-Kpic"
		MODULELD="$CC -G"
		SHLD="$CC -G"
	else
		PICFLAGS="-fPIC"
		MODULELD="$CC -shared"
		SHLD="$CC -shared"
		CLINKFLAGS="-shared-libgcc"
	fi
	MODULEEXT=so
	RLDFLAG="-R"
	WHOLE_ARCHIVE="-Wl,-z -Wl,allextract"
	NOWHOLE_ARCHIVE="-Wl,-z -Wl,defaultextract"
	;;
*-*-openbsd*)
	PICFLAGS="-fpic"
	LDFLAGS="$LDFLAGS -Wl,-E"
	MODULELD="$CC -shared"
	SHLD="$CC -shared"
	MODULEEXT=so
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	CFLAGS="$CFLAGS -I/usr/local/include"
	RLDFLAG="-Wl,--rpath="
	WHOLE_ARCHIVE="-Wl,--whole-archive"
	NOWHOLE_ARCHIVE="-Wl,--no-whole-archive"
	;;
*-*-linux*)
	LDFLAGS="$LDFLAGS -Wl,-E"
	CPPFLAGS="$CPPFLAGS -D_SVID_SOURCE -D_BSD_SOURCE -D_GNU_SOURCE"
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	SHLD="$CC -shared"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	WHOLE_ARCHIVE="-Wl,--whole-archive"
	NOWHOLE_ARCHIVE="-Wl,--no-whole-archive"
	;;
*-*-freebsd*)
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	LDFLAGS="$LDFLAGS -export-dynamic -L/usr/local/lib"
	CFLAGS="$CFLAGS -DCOMPAT_43TTY -I/usr/local/include"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	;;
*)
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	;;
esac
AC_SUBST(DOTSO)
AC_SUBST(DOTDYLIB)
AC_SUBST(CLINKFLAGS)
AC_SUBST(SHLD)
AC_SUBST(RLDFLAG)
AC_SUBST(DEPFLAGS)
AC_SUBST(MODULELD)
AC_SUBST(MODULEEXT)
AC_SUBST(WHOLE_ARCHIVE)
AC_SUBST(NOWHOLE_ARCHIVE)

# Checks for data types
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short int, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long int, 8)
AC_CHECK_SIZEOF(void *, 1)

AC_CHECK_HEADER(mtev_config.h, [], [AC_MSG_ERROR(*** libmtev (headers) required ***)])
AC_CHECK_LIB(mtev, mtev_main, , [AC_MSG_ERROR(*** libmtev is required ***)])
AC_CHECK_LIB(umem, umem_cache_create, , )
AC_CHECK_LIB(z, compress2, , [AC_MSG_ERROR([*** zlib is required ***])])
AC_CHECK_LIB(rt, sem_init, , [
  AC_CHECK_LIB(rt, clock_gettime, , )
])
AC_CHECK_LIB(posix4, sem_wait, , )

LIBMTEV_LIBS=$LIBS
AC_SUBST(LIBMTEV_LIBS)

AC_CACHE_CHECK([for htobe64], ac_cv_have_htobe64, [
	AC_TRY_LINK(
		[ #include <endian.h> ],
		[ long long int a; a = htobe64(1);],
		[ ac_cv_have_htobe64="yes" ],
		[ ac_cv_have_htobe64="no" ]
	)
])
AC_CACHE_CHECK([for htobe64], ac_cv_have_be64toh, [
  AC_TRY_LINK(
    [ #include <endian.h> ],
    [ long long int a; a = be64toh(1);],
    [ ac_cv_have_be64toh="yes" ],
    [ ac_cv_have_be64toh="no" ]
  )
])
AC_CACHE_CHECK([for htonll], ac_cv_have_htonll, [
	AC_TRY_LINK(
		[ #include <endian.h> ],
		[ long long int a; a = htonll(1);],
		[ ac_cv_have_htonll="yes" ],
		[ ac_cv_have_htonll="no" ]
	)
])

if test "x$ac_cv_have_htonll" = "xno" -a "x$ac_cv_have_htobe64" = "xyes" ; then
	AC_DEFINE(MAKE_HTOBE64_HTONLL)
fi
if test "x$ac_cv_have_htonll" = "xno" -a "x$ac_cv_have_be64toh" = "xyes" ; then
  AC_DEFINE(MAKE_BE64TOH_NTOHLL)
fi


AC_CACHE_CHECK([for u_int type], ac_cv_have_u_int, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ], 
		[ u_int a; a = 1;], 
		[ ac_cv_have_u_int="yes" ],
		[ ac_cv_have_u_int="no" ]
	)
])
if test "x$ac_cv_have_u_int" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT)
	have_u_int=1
fi

AC_CACHE_CHECK([for intXX_t types], ac_cv_have_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ], 
		[ int8_t a; int16_t b; int32_t c; a = b = c = 1;], 
		[ ac_cv_have_intxx_t="yes" ],
		[ ac_cv_have_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_INTXX_T)
	have_intxx_t=1
fi
	
AC_CACHE_CHECK([for int64_t type], ac_cv_have_int64_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ], 
		[ int64_t a; a = 1;], 
		[ ac_cv_have_int64_t="yes" ],
		[ ac_cv_have_int64_t="no" ]
	)
])
if test "x$ac_cv_have_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_INT64_T)
	have_int64_t=1
fi
	
AC_CACHE_CHECK([for u_intXX_t types], ac_cv_have_u_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ], 
		[ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;], 
		[ ac_cv_have_u_intxx_t="yes" ],
		[ ac_cv_have_u_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_u_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INTXX_T)
	have_u_intxx_t=1
fi

AC_CACHE_CHECK([for u_int64_t types], ac_cv_have_u_int64_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ], 
		[ u_int64_t a; a = 1;], 
		[ ac_cv_have_u_int64_t="yes" ],
		[ ac_cv_have_u_int64_t="no" ]
	)
])
if test "x$ac_cv_have_u_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT64_T)
	have_u_int64_t=1
fi

if (test -z "$have_u_intxx_t" || test -z "$have_intxx_t" && \
	test "x$ac_cv_header_sys_bitypes_h" = "xyes")
then
	AC_MSG_CHECKING([for intXX_t and u_intXX_t types in sys/bitypes.h])
	AC_TRY_COMPILE(
		[
#include <sys/bitypes.h>
		], 
		[
			int8_t a; int16_t b; int32_t c;
			u_int8_t e; u_int16_t f; u_int32_t g;
			a = b = c = e = f = g = 1;
		], 
		[
			AC_DEFINE(HAVE_U_INTXX_T)
			AC_DEFINE(HAVE_INTXX_T)
			AC_MSG_RESULT(yes)
		],
		[AC_MSG_RESULT(no)]
	) 
fi

if test -z "$have_u_intxx_t" ; then
	AC_CACHE_CHECK([for uintXX_t types], ac_cv_have_uintxx_t, [
		AC_TRY_COMPILE(
			[
#include <sys/types.h>
			], 
			[ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1; ], 
			[ ac_cv_have_uintxx_t="yes" ],
			[ ac_cv_have_uintxx_t="no" ]
		)
	])
	if test "x$ac_cv_have_uintxx_t" = "xyes" ; then
		AC_DEFINE(HAVE_UINTXX_T)
	fi
fi

AC_CACHE_CHECK([for size_t], ac_cv_have_size_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ size_t foo; foo = 1235; ],
		[ ac_cv_have_size_t="yes" ],
		[ ac_cv_have_size_t="no" ]
	)
])
if test "x$ac_cv_have_size_t" = "xyes" ; then
	AC_DEFINE(HAVE_SIZE_T)
fi

AC_CACHE_CHECK([for ssize_t], ac_cv_have_ssize_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ ssize_t foo; foo = 1235; ],
		[ ac_cv_have_ssize_t="yes" ],
		[ ac_cv_have_ssize_t="no" ]
	)
])
if test "x$ac_cv_have_ssize_t" = "xyes" ; then
	AC_DEFINE(HAVE_SSIZE_T)
fi

AC_SUBST(BUILD_MODULES)

SHCFLAGS="$PICFLAGS $CFLAGS"
AC_SUBST(SHCFLAGS)
AC_SUBST(LD)
AC_SUBST(SHLDFLAGS)

SUBPREFIX="$prefix"
if test "x$prefix" = "xNONE"; then
	SUBPREFIX="$ac_default_prefix"
fi
AC_MSG_CHECKING([Resolving libexec path...])
fullpath=$libexecdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`

AC_MSG_CHECKING([Resolving lib path...])
fullpath=$libdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`
AC_MSG_RESULT([$fullpath])
LIB_DIR=${fullpath}

AC_MSG_CHECKING([Resolving sysconfdir path...])
fullpath=$sysconfdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`
AC_MSG_RESULT([$fullpath])
ETC_DIR=${fullpath}
AC_SUBST(ETC_DIR)

if test -n "`$CC -V 2>&1 | grep 'Sun C'`"; then
	if test "$enable_string" != "no"; then
		CFLAGS="$CFLAGS -errwarn=%all"
	fi
else
	CFLAGS="$CFLAGS -Wall"
	if test "$enable_strict" != "no"; then
		CFLAGS="$CFLAGS -Werror"
	fi
fi

AC_OUTPUT([
Makefile
src/Makefile
])
