#! /usr/bin/perl

use Getopt::Long;

GetOptions("debug!" => \$debug, "rel=s" => \$rel, "version=s" => \$version);

if ( ! $rel || ! $version ) {
    print "Usage $0 --rel=release --version=cvs-version  {--debug}\n";
    exit 1;
}

if ( $debug == 1 ) {
    $codaname = "coda-debug" ;
} else {
    $codaname = "coda";
}

print "DEBUG is $debug, name is $codaname\n";


$dir="$codaname-$version-$rel";
$specfile="/usr/src/redhat/SPECS/$dir.spec";
print "Will create specfile $specfile\n";

open(SPEC, ">$specfile");
while ( <> ) {
    ~ s/\@REL\@/$rel/g;
    ~ s/\@VERSION\@/$version/g;
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
