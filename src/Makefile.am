bin_PROGRAMS = rioutil

AM_CPPFLAGS = -I$(top_srcdir)/include

rioutil_SOURCES = main.c main.h getopt.h

rioutil_LDADD = $(top_srcdir)/librioutil/librioutil.la
rioutil_LDFLAGS = $(PREBIND_FLAGS)
rioutil_DEPENDENCIES = $(top_srcdir)/librioutil/librioutil.la

AM_CFLAGS = -Wall -Wextra -pedantic
