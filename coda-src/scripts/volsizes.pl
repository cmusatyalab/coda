#!/usr/bin/perl

$VRLIST="/vice/db/VRList";
open VRLIST or die "$VRLIST: no such file or directory, so there!";

sub stripleading {
    $_ = $_[0];
    substr($_, 0, 1) = "";
    $_;
}

while (<VRLIST>) {
    chop;
    ($name, $repid, $N, $volid1, $volid2, $volid3, $volid4, $volid5, $volid6, $volid7, $volid8, $storagegroup ) = split ;
    
     push @VOLUMENAMES, $name; 
     print "Adding $name to VOLUMENAMES\n";
     shift;
}

close VRLIST;

# print "==========> $#VOLUMENAMES\n";
#while (@VOLUMENAMES) {

#    $name = pop @VOLUMENAMES;
#    print $name;
#    print ${name};
#    print "\n";
#}

format STDOUT_TOP=
               Volume Overview

volume name                      replica0   replica1   replica2
------------------------------   ---------  ---------  ---------
.

format STDOUT=
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<< @<<<<<<<<<< @<<<<<<<<<< @<<<<<<<
$name $size0{$name} $size1{$name} $size2{$name} $different
.

$VL="/vice/vol/BigVolumeList";
open VL;

while (<VL>) {

    chop ;
    ($name, $id, $host, $part, $m, $M, $size, $C, $D, $B, $A ) =
     split ;
    $line = $_;
    $name = &stripleading($name);
    $name =~ s/\.(\d)$//; 
    $repno = $1;
    $size = hex(&stripleading($size));
    $id   = &stripleading($id);
    $volsize{$name} = $size;
    #print "$name $repno\n";
    foreach $repvol (@VOLUMENAMES) {
      if( $repvol eq $name ) {
	if ( $repno == 0 ) {
             $size0{$name} = $size;
	   }
	if ( $repno == 1 ) {
             $size1{$name} = $size;
	   }
	if ( $repno == 2 ) {
             $size2{$name} = $size;
	   }
      }
    }
   
}

foreach $name (@VOLUMENAMES) {
  if ( $size0{$name}  != $size1{$name}  ||  $size0{$name}  != $size2{$name} || $size1{$name}  != $size2{$name} ) {
    $different = "*"; 
  } else {
    $different = "";
  }
  write;
}

close VL;
