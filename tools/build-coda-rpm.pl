#! /usr/bin/perl

use Getopt::Long;

GetOptions("debug!" => \$debug, "rel=s" => \$rel, "version=s" => \$version, "libc=s" => \$libc);

if ( ! $rel || ! $version || ! $libc ) {
    print "Usage $0 --rel=release --version=cvs-version --libc={libc,glibc} {--debug} \n";
    exit 1;
}

if ( $libc != "glibc" && $libc != "libc" ) {
    print "Usage $0 --rel=release --version=cvs-version --libc={libc,glibc} {--debug} \n";
    exit 1;
}



if ( $debug == 1 ) {
    $codaname = "coda-debug" ;
} else {
    $codaname = "coda";
}
print "DEBUG is $debug, name is $codaname\n";

$kernel =`uname -r`;
chop $kernel;

print "kernel: $kernel\n";

$dir="$codaname-$kernel-$version-$rel";
$specfile="/usr/src/redhat/SPECS/$dir.spec";
$specin="/usr/src/redhat/SPECS/coda.spec.in";
print "Will create specfile $specfile\n";

open(FD, "<$specin");
open(SPEC, ">$specfile");
while ( <FD> ) {
    ~ s/\@REL\@/$rel/g;
    ~ s/\@CVER\@/$version/g;
    ~ s/\@KVER\@/$kernel/g;
    ~ s/\@LIBC\@/$libc/g;
    ~ s/\@DEBUG\@/$debug/g;
    ~ s/\@CODA\@/$codaname/g;
    print SPEC $_;
    print $_
}

print "Now run as root: rpm -ba -v $specfile\n";

# exec(" rpm -ba -v $specfile ");
# @args= ("rpm",  "-ba", "-v", $specfile);
# system(@args);
print "Exit code $?\n";
