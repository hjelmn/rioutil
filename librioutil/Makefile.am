libdir = $(prefix)/lib

INCLUDES = -I$(top_srcdir)/include

EXTRA_DIST =    rio.c rioio.c mp3.c downloadable.c byteorder.c \
		cksum.c util.c driver_libusb.c playlist.c \
		driver_file.c genre.h log.c \
		song_management.c id3.c file_list.c

if MACOSX
PREBIND_FLAGS = -no-undefined -Wl,-prebind -Wl,-seg1addr,0x01686000
endif

if WITH_LIBUSB
DRIVER = driver_libusb.c
else
DRIVER = driver_file.c
endif

# new libtool eliminates the need for seperate OS X section
lib_LTLIBRARIES = librioutil.la

librioutil_la_SOURCES = rio.c rioio.c mp3.c downloadable.c \
			byteorder.c song_management.c cksum.c util.c \
			log.c playlist.c id3.c  file_list.c $(DRIVER)

librioutil_la_LDFLAGS = -version-info 6:0:5 $(PREBIND_FLAGS)
