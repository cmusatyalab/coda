#!/usr/bin/perl
# BLURB gpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public Licence Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

unshift(@INC, "/vice/lib/perl");
use POSIX;
use FileHandle;
autoflush STDOUT 1;

require ConfigReader::DirectiveStyle;

use File::Basename;
($prog, $base, $p_suffix) = fileparse("$0");

#
# get options
#

use Getopt::Long;
$noop=0;                           # if noop is given, right tape is in drive
$tape="/dev/nrst0";                # tape device (non-rewinding!!)
$backupdir = "/backup";            # backup directory
$config = "/vice/db/config.tape";  # backup configuration file (see below)
$vicetab = "/vice/db/vicetab";     # spool partitions are found here
$help = 0;                         # 
$size = 2000000;                   # size of the tapes in KB
$blocksize = 500;                  # size of tape block

GetOptions("dir=s" => \$backupdir, "config=s" => \$config, "size=i" => \$size,
	   "tape=s" => \$tape, "noop!" => \$noop, "help!" => \$help);
if ( $help ) {
    printf "Usage: $prog [--dir backupdir --config configfile --tape tapedrive  --size maxkbontape  --noop --help]\n";
    exit 0;
}

#
# get config params
#
my $conf = new ConfigReader::DirectiveStyle;

directive $conf 'dbdir', undef, '/vice/db';
directive $conf 'labelsdb', undef, '/vice/db/TAPELABELS';
directive $conf 'sleep_interval', undef, 200;
directive $conf 'notify_cutoff', undef, 5;
directive $conf 'maxretries', undef, 5;
required $conf 'backupmachine', undef;
# commands to notify operator and mail to sysad. These take message from stdin.
required $conf 'message', undef;
required $conf 'notify', undef;

$conf->load($config);

$message = $conf->value('message');
$backupmachine = $conf->value('backupmachine');
$notify = $conf->value('notify');
$labelsdb = $conf->value('labelsdb');
$sleep_interval = $conf->value('sleep_interval');
$notify_cutoff = $conf->value('notify_cutoff');
$maxretries = $conf->value('maxretries');
$dbdir = $conf->value('dbdir');

#
# two important variables
#
$subdir = strftime("%d%b%Y", localtime);
$dumpdir = $backupdir . "/" . $subdir;
print "Running tape.pl with  backupdir $backupdir, dumpdir $dumpdir,\n";
print "tape $tape on host $backupmachine.\n";

#
# copy out the database directory
#
if ( ! -d $dumpdir ) {
    print "Error: no dumpdir $dumpdir!\n";
    exit 1;
}
system("cp -p $dbdir/* $dumpdir");

#
#  the following interacts with operators and tapes to check for the right tape
#

if ( $noop == 0 )  {
#
# First, unmount whatever tape may be in the tape drive.  Ignore the result.
#
    system("mt -f ". $tape ." rewoffl");

#
# Determine the tape label for todays backup
#
    $label = &TapeLabel();
    print "Label: $label\n";

    $j = 0;
    while ( $j < $maxretries ) {
	$j++;
	&MountTape();
	# tape is in, verify the TAPELABEL
	$rc = &VerifyTape();
	if ( $rc ) {
	    print "Wrong tape!\n";
	    system("mt -f $tape rewoffl");
	} else {
	    print "Correct or empty tape found. Rewinding.\n";
	    $rc = 0xffff & system("mt -f $tape rewind");
	    if ( $rc ) {
		print "Error rewinding tape!\n";
		exit 1;
	    }
	    last;
	}
    }
    if ( $rc ) {
	print "Could not get correct tape, from operators!\n";
	exit 1;
    }
}


#
# find the partitions, write the lists out in the first partition
#
$partitions[0] = $backupdir;
system("echo '-----> List for $backupdir' > $partitions[0]/LIST");
system("find  $backupdir -ls >> $partitions[0]/LIST 2>1");
# spool partitions next
open (FD, $vicetab);
while ( <FD> ) {
    chop;
    ($host, $part, $type) = split;
    if ( $type eq "backup" ) {
	push( @partitions, $part );
	system("echo '-----> List for $part' >> $partitions[0]/LIST");
	system("find $part -ls >> $partitions[0]/LIST 2>1");
    }
}
close FD;
print "Partitions to be dumped: @partitions\n";

#
# prepare the TAPELABEL file
#
open(FD, ">$backupdir/TAPELABEL");
print FD "$label\n";
close FD;

#
# dump the filesystems
#
print "\n\nNow dumping the tape\n";
foreach $part ( @partitions ) { 
    my $time = strftime("%T", localtime);
    print "\n\n---------->$time: doing partition $part\n";
    print     "---------->command: dump 0sBf $blocksize $size  $tape $part\n";
    $rc = 0xffff & system("dump 0sBf $blocksize $size $tape $part");
    if ( $rc ) { 
	printf "Error dumping: dump 0sBf $blocksize $tape $part\n";
	exit 1;
    }
}

#
# verify
# 
print "Dumping done.\n\nRewinding and verifying the tape:\n";
system("mt -f $tape rewind");
$partno = 1;
foreach $part ( @partitions ) { 
    print "\n\n---------> Partition $part:\n";
    print     "---------> command: mt -f $tape rewind\n";    
    print     "---------> command: restore -b $blocksize -s $partno -f $tape -t /\n";
    $rc = 0xffff & system("mt -f $tape rewind");
    $rc = 0xffff & system("restore -b $blocksize -s $partno -f $tape -t /");
    if ( $rc ) { 
	printf "Error verifying dump of $part\n";
	exit 1;
    }
    $partno++;
}
system("mt -f $tape rewind");
    
system("rm -f $backupdir/TAPELABEL $backupdir/LIST"); 
foreach $part ( @partitions ) {
    system("rm -rf $part/$subdir");
}

exit 0;



#
# Utility routines below
#
sub VerifyTape {
    my $rc, $coda, $level, $date, $result, $today;
    
    # try to extract the TAPELABEL; if it isn' there it's OK, if 
    # it is then it must be todays incremental tape.
    chdir "/tmp";
    unlink("TAPELABEL");
    print "REWINDING TAPE...\n";
    system("mt -f $tape rewind");
    printf "Done. Now extracting.\n";
    open( CFD , "|restore -b $blocksize -x -f $tape TAPELABEL");
    print CFD "1\nn\n";
    close CFD;
    printf "Done.\n";
    if ( -f "/tmp/TAPELABEL" ) {
	open(FD, "/tmp/TAPELABEL") || die "TAPELABEL found but cannot read\n";
	$tl = <FD>;
	chop $tl;
	($coda,  $level,  $date ) = split " ", $tl;
	close FD;
	
	$today = strftime("%A", localtime);
	if ( ($level eq "I")  && ($date eq $today) ) {
	    print "Right: label $coda $level $date, today is $today.\n";
	    return 0;
	} else {
	    print " wrong  tape or full tape! Label: $tl, today is $today\n";
	    return 1;
	}
    } else {
	print "tape appears empty! excellent!\n";
	return 0;
    }
}

sub MountTape {
    my $rc, $i, $j;
# Now loop until we get the correct tape. Perhaps we should quit after a point?
    $rc = 1;  # tape is OK when this is 0
    $i = 0;  # when this hits $notify_cutoff it's time to email the gurus
    $j = 0;  # when this reaches max retries we give up.
    while ( $rc && $j < $maxretries ) {
	$i++;
	$j++;
	$now = strftime("%T", localtime);
	&Notify("$now: Please mount tape \"$label\" in $tape in $backupmachine.");
	sleep($sleep_interval);
    print "REWINDING TAPE in Mount...";
	$rc = 0xffff & system("mt -f " . $tape . "  rewind"); 
    print "done...";
	if (  $rc == 0 ) {
	    print "Tape ready!\n";
	    last;
	}
	if ( $i == 5 ) {
	    $now = strftime("%T", localtime);
	    &Message("Backup problem Have asked for tape $label 5 times! Will continue.");
	    $i = 0;
	}
    }
}

sub Notify {
    my $msg = @_[0];
    open( CFD, "|". $notify ) || die "Cannot open pipe to $notify\n";
    print CFD $msg;
    close CFD;
    print "Notification: @_";
    sleep 1;  # to hopefully get notify to run
}

sub Message {
    my $msg = @_[0];
    open( CFD, "|". $message ) || die "Cannot open pipe to $notify\n";
    print CFD $msg;
    close CFD;
    print "Message sent: @_";
    sleep 1;
}

sub TapeLabel {
    if ( -e $dumpdir . "/FULLDUMP"  ) {
	my $number, $coda, $level, $date, $label;
	print "Fulldump found\n";
	open( DB, $labelsdb );
	$number = 0;
	while ( <DB> ) { 
	    chop;
	    ( $coda,  $level, $number, $date ) = split;
	}
	close DB;
#    print "$coda $level $number $date\n";
	open(DB, ">>". $labelsdb);
	$coda = "CODA"; 
	$level = "F";
	$date = strftime("%a-%b-%d-%Y", localtime );
	$number++;
	$label = sprintf ("%s %s %s %s", $coda,$level,$number,$date);
	print DB "$label\n";
	close DB;
    } else {
	$label = strftime("Coda I %A", localtime);
    }
    $label;
}
