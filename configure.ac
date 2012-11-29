dnl Process this file with autoconf to produce a configure script.
AC_INIT([rioutil],[1.5.3])
AC_CONFIG_SRCDIR([librioutil/rio.c])
AM_CONFIG_HEADER([include/config.h])
AM_INIT_AUTOMAKE

AM_PROG_LIBTOOL

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for header files.
AC_CHECK_HEADERS(fcntl.h limits.h strings.h sys/ioctl.h unistd.h getopt.h libgen.h bswap.h)

AC_CHECK_LIB(gnugetopt, getopt_long)

dnl Checks for library functions.
AC_CHECK_FUNCS(basename memcmp)

PKG_CHECK_MODULES([libusb], [libusb-1.0])

PACKAGE=rioutil
VERSION=1.5.3

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
