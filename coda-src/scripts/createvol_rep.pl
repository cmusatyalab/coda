#!/usr/bin/perl

#open <ARGV>;
#$PROGRAMNAME = $ARGV;
#close ARGV;

use Sys::Hostname;
use FileHandle;

# The following variables could become global to a Coda Perl Package/Module
$ALLVOLUMESFILE = "/vice/vol/AllVolumes";
$VRLISTFILE =     "/vice/vol/VRList";
$SERVERFILE =     "/vice/hostname";
$SCMFILE =        "/vice/db/scm";
$VSGDBFILE =      "/vice/db/VSGDB";
$MAXGROUPIDFILE = "/vice/vol/maxgroupid";
$DUMPLISTFILE =   "/vice/db/dumplist";
$VICEHOSTFILE =   "/vice/db/hosts";
$VOLUMELISTFILE = "/vice/vol/VolumeList";
$REMOTEDIR =      "/vice/vol/remote";
$BIGVOLUMEFILE =  "/vice/vol/BigVolumeList";

# The following are only needed for this script.
$newvolumename=$ARGV[0];
$vsgaddr=$ARGV[1];
$partition=$ARGV[2];
$groupid=$ARGV[3];

###### This is temporary...
print "===> ARGV[0]: $ARGV[0] ";
print "ARGV[1]: $ARGV[1] ";
print "ARGV[2]: $ARGV[2] ";
print "ARGV[3]: $ARGV[3]\n";

# returns undefined if $SCMFILE is empty
sub getcodahost {
  open SERVERFILE or die "$SCMFIILE: No such file or directory";
  $SERVERNAME = <SERVERFILE>;
  chop $SERVERNAME;
  close SCMFILE;

  return $SERVERNAME;
}


sub getcodascm {
  open SCMFILE or die "$SCMFIILE: No such file or directory";
  $SCMNAME = <SCMFILE>;
  chop $SCMNAME;
  close SCMFILE;

  return $SCMNAME;

}

# Returns the broken out VRList entry on sucess, empty on failure
sub getvrlistentry {
  my ($vrtmp,$volname,$vsg,$repnum,$volid0,$volid1,$volid2,$volid3,$volid4,$volid4,$volid5,$volid6,$volid7,$vsgno);
  $vrtmp =  $_[0];
  open VRLISTFILE or die "$VRLISTFILE: no such file or directory";
  while(<VRLISTFILE>) {
        chop;
	($volname, $vsg, $repnum, $volid0, $volid1, $volid2, $volid3, $volid4, $volid5, $volid6, $volid7, $vsgno) = split;
        if($volname eq $vrtmp) {
           close VRLISTFILE;
           return ($volname, $vsg, $repnum, $volid0, $volid1, $volid2, $volid3, $volid4, $volid5, $volid6, $volid7, $vsgno);
	 }
   }
   close VRLISTFILE;

   return ("");   # This can be used to tell if a volume exists or not...
}  

# Returns the broken out AllVolumes on Sucess, empty on failure
sub getallvolumesentry {
  my ($avtmp,$volname,$volid,$volhost,$hostpartition,$A,$B,$C,$rw,$D,$E);
  $avtmp = $_[0];
  open ALLVOLUMESFILE or die "$ALLVOLUMESFILE: no such file or directory";
  while(<ALLVOLUMESFILE>) {
        chop;
	($volname, $volid, $volhost, $hostpartition, $A, $B, $C, $rw, $D, $E) = split;
        if($volname eq $avtmp) {
           close ALLVOLUMESFILE;

           return ($volname, $volid, $volhost, $hostpartition, $A, $B, $C, $rw, $D, $E);
	 }
   }
   close ALLVOLUMESFILE;

   return ("");
}

# This function returns the VSGDB Address and associate hosts; NULL on failure.
sub getvsgaddr {
   # This function doesn't test for a maximum number of servers since it
   # reads in a most 8 servers.
   # If only a vsgaddr exists without at least 1 host, this is a fatal error.
   # ALSO NOTE: the number of servers is $#getvsgaddr - 1 for a VSG.
   my ($gtmp,$vsgaddr,$host0,$host1,$host2,$host3,$host4,$host5,$host6,$host7);
   $gtmp = $_[0];
   open VSGDBFILE or die "$VSGDBFILE: no such file or directory";
   while(<VSGDBFILE>) {
       chop;
       ($vsgaddr, $host0, $host1, $host2, $host3, $host4, $host5, $host6, $host7) = split;
       if($vsgaddr eq $gtmp) {
         print "===> vsgaddr: $vsgaddr\n";
         print "===> gtmp: $gtmp\n";
         close VSGDBFILE;
         if ($host0 eq "") {
	    printf stderr "$vsgaddr: invalid formation in $VSGDBFILE\n";
            exit -1;
	 } elsif ($host1 eq "") {
            return ($vsgaddr, $host0);
	 } elsif ($host2 eq "") {
            return ($vsgaddr, $host0, $host1);
	 } elsif ($host3 eq "") {
            return ($vsgaddr, $host0, $host1, $host2);
	 } elsif ($host4 eq "") {
            return ($vsgaddr, $host0, $host1, $host2, $host3);
	 } elsif ($host5 eq "") {
            return ($vsgaddr, $host0, $host1, $host2, $host3, $host4);
	 } elsif ($host6 eq "") {
            return ($vsgaddr, $host0, $host1, $host2, $host3, $host4, $host5);
	 } elsif ($host7 eq "") {
            return ($vsgaddr, $host0, $host1, $host2, $host3, $host4, $host5, $host6);
         }
         return ($vsgaddr, $host0, $host1, $host2, $host3, $host4, $host5, $host6, $host7);
       }
   } 
   close VSGDBFILE;
   printf STDERR "$gtmp: no such volume storage group.\n";
   exit -1;
}

# gets the current maxgroupid; NOTE: IT DOES NOT RESET IT TO A NEW VALUE!
# This function returns the hexidecimal maximum group id currently set.
sub getmaxgroupid {
    my ($idtmp);
    open MAXGROUPIDFILE or die "$MAXGROUPIDFILE: no such file or directory";
    $idtmp = <MAXGROUPIDFILE>;
    close MAXGROUPIDFILE;
    return sprintf "%lX", $idtmp;
}

# make a new maxgroupid value and then update $MAXGROUPFILE
sub setmaxgroupid {
    my ($newgroupid);
    $newgroupid = sprintf "%lX", (hex(&getmaxgroupid)+1);
    print "===> $newgroupid\n";
    open(MAXGROUPIDFILE, "+>$MAXGROUPIDFILE");
    print MAXGROUPIDFILE hex($newgroupid);
    close(MAXGROUPIDFILE);
    return $newgroupid;
}

# This is a kludge for the moment...
# But I can't get system() to excute a shell script properly so
# I got started on this anyway...
# Return 0 on sucess
sub bldvldb {
  my(@serverlist,$count,$error);
  $error = 0;
  @serverlist = @_;
  $count = 0;
  while($count <= $#serverlist) {
    $error = system("updatefetch -h $serverlist[$count] -r $VOLUMELISTFILE -l $REMOTEDIR/$server[$count].list.new");
    rename("$REMOTEDIR/$server[$count].list.new","$REMOTEDIR/$server[$count].lis") or die "Having trouble getting remote files";
    $count = 0;
    while($count <= $#serverlist) {
      $error = system("cat $REMOTEDIR/$serverlist[$count].list > $BIGVOLUMEFILE");
      ++$count;
    }
    $error = &makevldb;
  }
  return $error;
}

# Return 0 on sucess
sub makvrdb {
  my ($error);
  $error = system("volutil makevrdb $VRLISTFILE");
  if($error != 0) {
    return $error;
  }
  return 0;
}

# Return 0 on sucess
sub makvldb {
  my ($error);
  $error = system("volutil makevldb $BIGVOLUMEFILE");
  if($error != 0) {
    return $error;
  }
  return 0;
}

########################
# Begin Body of Porgram#
########################

if($#ARGV < 2 || $#ARGV >= 4) {
  printf STDERR "Usage: $PROGRAMNAME <volumename> <vsgaddr> <partition> [groupid]\n";
  exit -1;
}

$SCMNAME = &getcodascm;
if (&getcodahost ne $SCMNAME) {
  print "This must be run from the scm: $SCMNAME.\n";
  exit -1;
}

# This will create the files if they do not already exist
# This is probably not necessary anymore...
system('touch', $ALLVOLUMESFILE);
system('touch', $VRLISTFILE);

@allvolumesentry = getallvolumesentry($newvolumename);
if($#allvolumesentry != 0) {
  print "$newvolumename: exists in $ALLVOLUMESFILE\n";
  exit -1;
}

@vrlistentry = getvrlistentry($newvolumename);
if($#vrlistentry != 0) {
  print "$newvolumename: volume exists in $VRLISTFILE\n";
  exit -1;
}

@vsgdbentry = getvsgaddr($vsgaddr);
if($#vsgdbentry == 0) {
  print "$vsgaddr: not a valid Volume Storage Group Address\n";
  exit -1;
}

if($groupid eq "") {
  $groupid = &setmaxgroupid;
}

# Create the volume(s) now that we have gotten this far...
$count = 0;
$vsgdbvalue = shift @vsgdbentry; # the vsgdbvalue is saved for later use...
while ($count <= $#vsgdbentry) {
   @systemargs = ("volutil", "-h", "$vsgdbentry[$count]", "create_rep", "$partition", "$newvolumename.$count", "$groupid");
   system(@systemargs) == 0 or die "system(@systemargs) failed: $?.\n";
   ++$count;
}

if(&bldvldb != 0) {
  die "rebuilding of the databases failed";
}

# Update the VRList
$count = 0;
$nservers = $#vsgdbentry+1; # Remember, perl counts from 0 to ...
@volidgroup = ("0","0","0","0","0","0","0","0");
while($count <= $#vsgdbentry) {
   @AVENTRY = getallvolumesentry("$newvolumename.$count");
   $volidgroup[$count] = $AVENTRY[1];
   ++$count;
}
open VRLISTFILE, "+>>$VRLISTFILE";
print VRLISTFILE "$newvolumename $groupid $nservers @volidgroup $vsgdbvalue\n";
close VRLISTFILE;
@systemargs = ("volutil", "makevrdb", "$VRLISTFILE");
system(@systemargs) == 0 or die "system(@systemargs) failed: $?.";
print "Volume: $newvolumename $groupid $nservers @volidgroup $vsgdbvalue sucessfully added.\n";

# To be backed up, or not to be backed up...
print "Would you like $newvolumename backuped up? (y/n) [n] ";
if(<STDIN> =~ /^[yY]/) {
  print "What day would you like $newvolumename backed up? [Mon] ";
  $day = <STDIN>;
  chop $day;
  if($day eq "") {
     $day = "Mon";   # default is Monday
  }
  if($day =~ /sun/i) {
     $day_of_week = "FIIIIII";
   } elsif($day =~ /mon/i) {
     $day_of_week = "IFIIIII";
   } elsif($day =~ /tue/i) {
     $day_of_week = "IIFIIII";
   } elsif ($day =~ /wed/i) {
     $day_of_week = "IIIFIII";
   } elsif ($day =~ /thu/i) {
     $day_of_week = "IIIIFII"; 
   } elsif ($day =~ /fri/i) {
     $day_of_week = "IIIIIFI"; 
   } elsif ($day =~ /sat/i) {
     $day_of_week = "IIIIIIF"; 
   } else {
     printf STDERR "$day: Invalid day of the week.  $newvolumename will not be backed up!\n"
   }
   open DUMPLISTFILE or die "$DUMPLISTFILE: no such file or directory";
   while(<DUMPLISTFILE>) {
     chop;
     ($hexgroup, $dayofweek, $volname) = split;
     if($volname eq $newvolumename) {
       print "$newvolumename: Already exist in $DUMPLISTFILE.\n";
       print "Current backup for $newvolumename is: $hexgroup $dayofweek $volname\n";
       exit -1;
     }
   }
   open DUMPLISTFILE, "+>>$DUMPLISTFILE";
   print DUMPLISTFIL1E "$groupid        $day_of_week         $newvolumename\n";
   close DUMPLISTFILE;
   print "$groupid  $day_of_week $newvolumename added to $DUMPLISTFILE\n";
}
