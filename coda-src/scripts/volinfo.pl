#!/usr/bin/perl

sub stripleading {
    $_ = $_[0];
    substr($_, 0, 1) = "";
    $_;
}

format STDOUT_TOP=
               Volume Overview

name                             volid     size
-------------------------------  --------  --------------
.

format STDOUT=
@<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  @<<<<<<<  
$size, $name, $id
.

while ( <> ) {

    chop ;
    ($name, $id, $host, $part, $m, $M, $size, $C, $D, $B, $A ) =
	split ;

    $name = &stripleading($name);
    $size = hex(&stripleading($size));
    $id   = &stripleading($id);
    $volsize{$name} = $size;

    write ;


}

