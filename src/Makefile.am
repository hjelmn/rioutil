bin_PROGRAMS = rioutil

INCLUDES = -I$(top_srcdir)/include -I/usr/local/include

rioutil_SOURCES = main.c getopt.c getopt1.c main.h getopt.h

if MACOSX
rioutil_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la -lIOKit
PREBIND_FLAGS = -prebind
else
rioutil_LDADD = -L/usr/local/lib $(top_srcdir)/librioutil/librioutil.la
endif

rioutil_LDFLAGS = $(PREBIND_FLAGS)
rioutil_DEPENDENCIES = $(top_srcdir)/librioutil/librioutil.la
