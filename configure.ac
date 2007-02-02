dnl Process this file with autoconf to produce a configure script.
AC_INIT(aclocal.m4)

AM_INIT_AUTOMAKE(rioutil,1.5.0)
AM_CONFIG_HEADER(include/config.h)

AM_PROG_LIBTOOL

AH_TOP(
#undef WITH_LIBUSB
#undef __MacOSX__
)

dnl AC_CANONICAL_HOST is invoked by AM_PROG_LIBTOOL
dnl AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for header files.
AC_CHECK_HEADERS(fcntl.h limits.h strings.h sys/ioctl.h unistd.h getopt.h libgen.h bswap.h)

AC_CHECK_LIB(gnugetopt, getopt_long)

dnl Checks for library functions.
AC_CHECK_FUNCS(basename memcmp)

dnl libusb is now the default method
libusb=yes

AC_MSG_CHECKING(whether build target is macosx/darwin)
case $host in
  *-darwin*)
    osx_support=yes
    AC_MSG_RESULT(yes)
    AC_SUBST(MACOSX)
    AC_DEFINE_UNQUOTED(__MacOSX__)

    AC_CHECK_LIB(IOKit,IOCloseConnection)
    AC_CHECK_HEADER(unistd.h)
    AC_CHECK_FUNCS(optarg getopt optind opterr optopt)
    ;;
  *)
    osx_support=no
    AC_MSG_RESULT(no)
    ;;
esac

AC_MSG_CHECKING(whether to use libusb)
AC_ARG_WITH(libusb,
[  --with-libusb     Include libusb support (default)
  --without-libusb  Don't include libusb support],
[ case "$withval" in
  yes)
    libusb=yes
    ;;
  *)
    libusb=no
    AC_MSG_RESULT(no)
    ;;
  esac],
[
  if test "x$libusb" = "xno"; then
    AC_MSG_RESULT(no)
  fi
]
)

if test "x$libusb" = "xyes"; then
    AC_MSG_RESULT(yes)
    AC_DEFINE_UNQUOTED(WITH_LIBUSB)

    AC_PATH_PROG(LIBUSB_CONFIG,libusb-config)
    if test -n "${LIBUSB_CONFIG}"; then
       CFLAGS="`${LIBUSB_CONFIG} --cflags` $CFLAGS"
       LIBS="`${LIBUSB_CONFIG} --libs` $LIBS"
    else
       AC_MSG_ERROR(Can't find libusb)
    fi

    AC_CHECK_LIB(usb, usb_open)
    AC_SUBST(WITH_LIBUSB)
else
    AC_MSG_WARN(Compiling rioutil to read from a device file. A kernel-level driver is needed to use rioutil without libusb.)
fi

AM_CONDITIONAL(MACOSX, test "$osx_support" = "yes")
AM_CONDITIONAL(WITH_LIBUSB, test "$libusb" = "yes")

PACKAGE=rioutil
VERSION=1.5.0

AC_OUTPUT([
  Makefile
  librioutil/Makefile
  include/Makefile
  src/Makefile
  man/Makefile
  spec/Makefile
  spec/rioutil.spec
  spec/rioutil-usbdevfs.spec
  spec/rioutil-libusb.spec
  man/rioutil.1
  linux_driver/Makefile
  debian/Makefile
])
