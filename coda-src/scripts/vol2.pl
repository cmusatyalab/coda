#!/usr/bin/perl


$VRLIST="/vice/db/VRList";
open VRLIST or die "$VRLIST: no such file or directory, so there!";

while ( <VRLIST> ) {

  chop;
  ($name, $repid, $N, $volid1, $volid2, $volid3, $volid4, $volid5, $volid6, $volid7, $volid8, $storagegroup ) = split ;

  print $name,"\n";
}


