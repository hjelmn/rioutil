libdir = $(prefix)/lib

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Wextra -pedantic $(libusb_CFLAGS)

# new libtool eliminates the need for seperate OS X section
lib_LTLIBRARIES = librioutil.la

librioutil_la_SOURCES = rio.c rioio.c mp3.c downloadable.c \
			byteorder.c song_management.c cksum.c util.c \
			log.c playlist_file.c playlist.c id3.c \
                        driver_libusb.c file_list.c $(DRIVER)

librioutil_la_LDFLAGS = -version-info 6:0:5 $(PREBIND_FLAGS)
librioutil_la_LIBADD = $(libusb_LIBS)
