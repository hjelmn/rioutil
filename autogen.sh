#!/bin/sh
# Run this to generate all the initial makefiles, etc.

PROJECT=rioutil

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

autoreconf -ivf || exit 1

./configure "$@" || exit 1

echo 
echo "Now type 'make' to compile $PROJECT."
