#! /usr/bin/perl

use Getopt::Long;

GetOptions("debug!" => \$debug, "kerberos!" => \$kerberos, "snapshot!" => \$snapshot, "rel=s" => \$rel, "version=s" => \$version);

if ( ! $rel || ! $version ) {
    print "Usage $0 --rel=release --version=cvs-version  {--debug} {--kerberos} {--snapshot}\n";
    exit 1;
}

if ( $snapshot == 1 ) {
    $tversion = $version . "-" . $rel
} else {
    $tversion = $version
}

if ( $debug == 1 ) {
    $codaname = "coda-debug" ;
} else {
    $codaname = "coda";
}

if ( $kerberos == 1 ) {
  $codaname = $codaname . "-kerberos";
  $auth2name="kauth2";
  $clogname="kclog";
  $auname="kau";
} else {
  $auth2name="auth2";
  $clogname="clog";
  $auname="au";
}

print "DEBUG is $debug, name is $codaname\n";


$dir="$codaname-$version-$rel";
$specfile="/usr/src/redhat/SPECS/$dir.spec";
print "Will create specfile $specfile\n";

open(SPEC, ">$specfile");
while ( <> ) {
    ~ s/\@REL\@/$rel/g;
    ~ s/\@CVER\@/$version/g;
    ~ s/\@VERSION\@/$tversion/g;
    ~ s/\@KVER\@/$kernel/g;
    ~ s/\@KV\@/$kv/g;
    ~ s/\@LIBC\@/$libc/g;
    ~ s/\@DEBUG\@/$debug/g;
    ~ s/\@CODA\@/$codaname/g;
    ~ s/\@AUTH2\@/$auth2name/g;
    ~ s/\@CLOG\@/$clogname/g;
    ~ s/\@AU\@/$auname/g;
    print SPEC $_;
    print $_
}

print "Now run as root: rpm -ba -v $specfile\n";

# exec(" rpm -ba -v $specfile ");
# @args= ("rpm",  "-ba", "-v", $specfile);
# system(@args);
print "Exit code $?\n";
