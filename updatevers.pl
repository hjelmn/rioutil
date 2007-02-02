#!/usr/bin/perl

if ($ARGV[0] eq "") {
    print "An argument is required\n";
    exit(1);
}

print "Creating backup of configure.ac\n";
system ("cp -f configure.ac configure.ac.bak");

open(CONFIGURE_IN, "<configure.in");
open(CONFIGURE_OUT, ">configure.out");
while (<CONFIGURE_IN>) {
    if (/AM_INIT_AUTOMAKE/) {
	print CONFIGURE_OUT "AM_INIT_AUTOMAKE\(rioutil,$ARGV[0]\)\n";
    } elsif (/VERSION\=/) {
	print CONFIGURE_OUT "VERSION\=$ARGV[0]\n";
    } else {
	print CONFIGURE_OUT;
    }
}

close(CONFIGURE_IN);
close(CONFIGURE_OUT);

system("mv -f configure.out configure.ac");
print "configure.ac ready for version $ARGV[0]\n";
print "rerunning autoconf...\n";
system("autoconf");
