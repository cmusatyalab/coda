#!/bin/csh -f

#
# usage: classify_volumes.sh [-d <database>]
#

# parse args
if ($#argv != 2 && $#argv != 0) then
	echo "Usage: classify_volumes.sh [-d <database>]"
	exit -1
endif

@ max = $#argv
set i = 0

#
# set defaults - make as appropriate for environment
#

set DATABASE = codastats2

set i = 1

while ( $i < $max)
	set sw = $argv[$i]
	@ i = $i + 1
	set val = $argv[$i]
	switch ( $sw )
	case "-d":
		set DATABASE = $val
		breaksw
	default:
		echo "Usage: classify_volumes.sh [-d <database>]"
		breaksw
	endsw
	@ i = $i + 1
end		

#
# Check that we are on the SCM
#

if ( ! (-r /.scm) && (`cat /.hostname` != `cat /.scm`)) then
	echo "Must be run on the SCM --- sorry"
	exit -1
endif

#
# Datafile for the class info
#

set VCList = /vice/db/VCList

#
# Check that we can modify the VCList...
#

touch $VCList

if ($status != 0) then
	echo Not authorized to update $VCList
	exit -1
endif

#
# get the relevent fields from the VRList, and store it in a local temp
#
# An entry in the vrlist is:
# name volid(hex) repfactor vsgmember1 ... vsgmember8 VSGid
# we only care about $1, $2 (decimal), $3, and $12.  Get
# those into the local file system, then we can see...
#

awk '{print $1, $2, $3, $12}' /vice/db/VRList > /tmp/vrlist.$$

if ($status != 0) then
	echo Cannot access VRList
	exit -1
endif

#
# now, a gross way of creating the decimal volid we need...
#

set tmp = `wc -l /tmp/vrlist.$$`
set lines = $tmp[1]

cat /dev/null > /tmp/vrlist.$$.sql

# i == the line we are currently working on...

set i = 1

while ($i <= $lines)
	set this = `head -$i /tmp/vrlist.$$ | tail -1`
	set hexvolid = `echo $this[2] | tr a-f A-F `	
	cat << EOF > /tmp/tmp.$$
obase = 10
ibase = 16
$hexvolid
EOF
	set decvolid = `bc < /tmp/tmp.$$`
	rm /tmp/tmp.$$
	# see if we know anything about this volume yet
	set volType = `grep $this[1] /vice/db/VCList`
	if ($status == 0) then
	    set class = $volType[2]
            goto ENDOFGUESS
	endif
	# Now, guess at the volume type...
	# classes are <u>ser = user volume
        #             <p>roject = project owned volume
        #             <s>ystem = system binaries
        #             <o>ther = test volumes, etc.
	# default is o
	set class = o
	# if it contains a usr, it's probably a user volume
	if ($this[1] =~ *usr*) then
	    set class = u
	else
	    # if it contains either project, src, lib, or obj it's
            # probably a project volume
	    if ($this[1] =~ *project* \
                || $this[1] =~ *src* \
                || $this[1] =~ *lib* \
                || $this[1] =~ *obj*) then
		set class = p
            else
                # if it contains i386 or bin, it's probably
                # a system volume
                if ($this[1] =~ *i386* || $this[1] =~ *bin*) then
                    set class = s
                endif
            endif
        endif
	echo -n Enter class for volume $this[1] "["$class"]" ""
	set _class_ = "$<"
	switch (_class_)
	case "u":
	case "U":
	case "user":
	case "User":
		set class = u
		breaksw
	case "p":
	case "P":
	case "project":
	case "Project":
		set class = p
		breaksw
	case "s":
	case "S":
	case "system":
	case "System":
		set class = s
		breaksw
	case "o":
	case "O":
	case "other":
	case "Other":
		set class = o
		breaksw
	endsw
	echo $this[1] $class >> /vice/db/VCList
ENDOFGUESS:
	echo 'insert into volume_info values ("'$this[1]'",' $decvolid',' $this[3]', "'$this[4]'", "'$class'");' >> /tmp/vrlist.$$.sql
	@ i = $i + 1
end

#
# Enter this info into a table
#

if ( ! $?SCYLLA ) then
	set SCYLLA = /afs/cs.cmu.edu/misc/scylla/@sys/omega/bin/isql
endif

# does the table exist?

cat << EOF > /tmp/tmp.$$.sql
info columns for volume_info
EOF

$SCYLLA $DATABASE /tmp/tmp.$$ 

# if it doesn't, create it; if it does, drop it and create a new one

if ($status != 0) then
	cat << EOF > /tmp/tmp.$$.sql
create table volume_info (
	name		char(65),
	volid		integer,
	repfactor	integer,
	species		char(16),
	type		char(1)
);
EOF
	$SCYLLA $DATABASE /tmp/tmp.$$ 
else
	cat << EOF > /tmp/tmp.$$.sql
drop table volume_info;
create table volume_info (
	name		char(65),
	volid		integer,
	repfactor	integer,
	species		char(16),
	type		char(1)
);
EOF
	$SCYLLA $DATABASE /tmp/tmp.$$ 
endif

$SCYLLA $DATABASE /tmp/vrlist.$$.sql

if ($status != 0) then
	echo "insert failed, please check"
else
	rm /tmp/*$$*
endif


