#!/usr/misc/bin/ksh 
#ifndef _BLURB_
#define _BLURB_
#/*
#
#            Coda: an Experimental Distributed File System
#                             Release 4.0
#
#          Copyright (c) 1987-1996 Carnegie Mellon University
#                         All Rights Reserved
#
#Permission  to  use, copy, modify and distribute this software and its
#documentation is hereby granted,  provided  that  both  the  copyright
#notice  and  this  permission  notice  appear  in  all  copies  of the
#software, derivative works or  modified  versions,  and  any  portions
#thereof, and that both notices appear in supporting documentation, and
#that credit is given to Carnegie Mellon University  in  all  documents
#and publicity pertaining to direct or indirect use of this code or its
#derivatives.
#
#CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
#SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
#FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
#DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
#RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
#ANY DERIVATIVE WORK.
#
#Carnegie  Mellon  encourages  users  of  this  software  to return any
#improvements or extensions that  they  make,  and  to  grant  Carnegie
#Mellon the rights to redistribute these changes without encumbrance.
#*/
#
#static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/scripts/reinit.sh,v 1.1 1996/11/22 19:06:56 braam Exp $";
#endif /*_BLURB_*/


#ifndef _BLURB_
#define _BLURB_
#/*
#
#            Coda: an Experimental Distributed File System
#                             Release 3.1
#
#          Copyright (c) 1987-1995 Carnegie Mellon University
#                         All Rights Reserved
#
#Permission  to  use, copy, modify and distribute this software and its
#documentation is hereby granted,  provided  that  both  the  copyright
#notice  and  this  permission  notice  appear  in  all  copies  of the
#software, derivative works or  modified  versions,  and  any  portions
#thereof, and that both notices appear in supporting documentation, and
#that credit is given to Carnegie Mellon University  in  all  documents
#and publicity pertaining to direct or indirect use of this code or its
#derivatives.
#
#CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
#SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
#FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
#DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
#RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
#ANY DERIVATIVE WORK.
#
#Carnegie  Mellon  encourages  users  of  this  software  to return any
#improvements or extensions that  they  make,  and  to  grant  Carnegie
#Mellon the rights to redistribute these changes without encumbrance.
#*/
#
#static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/scripts/reinit.sh,v 1.1 1996/11/22 19:06:56 braam Exp $";
#endif /*_BLURB_*/


############################################################################
# reinit.sh
###########
# Automate the volume restoration process when reiniting a server.  Running
# this script on the SCM *before* reiniting any of the servers will 
# produce a script that will re-create the volumes on the reinited servers.
############################################################################

OUTFILE=/tmp/post-reinit	# Script to run after reinit.
SKIPFILE=/tmp/not-created	# Volumes not created by reinit.sh
CLIENTFILE=/tmp/client-reinit	# Name of script $OUTFILE will generate.
MOUNTPT=/coda/tmp		# Location to mount restored & inited vols
SERVERS=$@			# Set SERVERS to the input arguments
export OUTFILE SERVERS SKIPFILE

# abort - mv $OUTFILE and exit.
abort() {
    echo "moving $OUTFILE to $OUTFILE.error"
    mv $OUTFILE $OUTFILE.error
    if [ -f $SKIPFILE ] ; then
	$echo "moving $SKIPFILE to $SKIPFILE.error"
	mv $SKIPFILE $SKIPFILE.error
    fi
    exit 2
}


# generate_client_file - generate the code to generate the client script.
#			 if $# != 1 then its a replicated volume.
generate_client_file() {
    name=$1
    echo "echo \"# Volume: $name\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"echo Copying volume: $name\" >> \$COUTFILE" >> $OUTFILE

    if [ $# -eq 2 ] ; then
	nservers=$2
	rname=${name}.0
	n=0
	while [ $n -lt $nservers ] ; do
	    echo "echo \"cfs mkm \${MOUNTPT}/restored/${name}.$n ${name}.${n}.restored\" >> \$COUTFILE" >> $OUTFILE
	n=`expr $n + 1`
	done
    else
	rname=$name
	echo "echo \"cfs mkm \${MOUNTPT}/restored/${name} ${name}.restored\" >> \$COUTFILE" >> $OUTFILE
    fi

    echo "echo \"cfs mkm \${MOUNTPT}/inited/${name} ${name}\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"if check_vols $name $rname ; then \" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"    return\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"fi\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"cd \${MOUNTPT}/restored/${rname}\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"tar cf - . | (cd \${MOUNTPT}/inited/${name}; tar xf -)\" >> \$COUTFILE" >> $OUTFILE
    echo "echo \"find . -type d -print | set_acls $name\" >> \$COUTFILE" >> $OUTFILE

    echo "echo \"\" >> \$COUTFILE" >> $OUTFILE
}



# recreate_replicated_vols
# Generate the code to remove the old entry (from VRList) of a replicated 
# volume that resides on a subset of $SERVERS and recreate a new replicated 
# volume with the same name.  If we find a  volume that is both on one of 
# $SERVERS and another server, then we print out an error message and abort.
recreate_replicated_vols() {
    while read name repid numreps rep0 rep1 rep2 rep3 rep4 rep5 rep6 rep7 vsgaddr
    do
	vol_servers=`awk '$1 ~ /^'$vsgaddr'$/ {print $2, $3, $4, $5, $6, $7, $8, $9}' /vice/db/VSGDB`
	outset=0
	inset=0
	for vol_server in $vol_servers ; do
	    found=0
	    for SERVER in $SERVERS ; do
		if [ "$vol_server" = "$SERVER" ] ; then
		    inset=1
		    found=1
		fi
	    done
	    if [ $found -eq 0 ] ; then
		outset=1
	    fi
	done
	if [ $inset -eq 1 -a $outset -eq 1 ] ; then
	    # This volume is on a server being reinited and a server not
	    # being reinited!
	    echo "ERROR: Volume $name straddles servers being reinited and"
	    echo "servers not being reinited!, aborting..."
	    abort
	fi
	if [ $inset -eq 1 ] ; then
	    # We need to recreate this one.  Add code to remove the old
	    # entry from the VRList and then call recreatevol_rep
	    echo "Generating code for replicated volume: $name"
	    echo "# Volume $name:" >> $OUTFILE
	    echo "echo \"Recreating volume $name\"" >> $OUTFILE
	    echo "sed /\"$name $repid $numreps $rep0 $rep1 $rep2 $rep3 $rep4 $rep5 $rep6 $rep7 $vsgaddr\"/d /vice/vol/VRList > /tmp/VRList.\$\$" >> $OUTFILE
	    echo "mv /tmp/VRList.\$\$ /vice/vol/VRList" >> $OUTFILE
	    
	    # Now figure out which partition the volume was on.  We only
	    # need to check one of the replicas since they are all on the
	    # same partition.
	    partition=`awk '$1 == "'${name}.0'" {print $4}' /vice/vol/AllVolumes`

	    # If the volume is being backed-up, get the backup pattern, 
	    # and remove the old entry from dumplist.
	    backup=`awk '$3 == "'$name'" {print $2}' /vice/db/dumplist`
            if [ "$backup" != "" ] ; then
 		echo "awk ' \$3 !~ '\"$name\"' { print } ' /vice/db/dumplist >/vice/db/dumplist.\$\$" >> $OUTFILE
		echo "mv -f /vice/db/dumplist.\$\$ /vice/db/dumplist" >> $OUTFILE
	    fi

	    echo "/vice/bin/recreatevol_rep $name $vsgaddr $partition $backup" >> $OUTFILE
	    generate_client_file $name $numreps
	    echo "" >> $OUTFILE
	fi
    done
}

	    
	

# Make sure we are running as root.
if [ "`whoami | awk '{print substr($1, 1, 4)}'`" != "root" ] ; then
    echo "$0 must be run as root"
    exit 1
fi


# Make sure we are on a Coda server.
if [ ! -f /.scm -o ! -f /.hostname ] ; then
    echo "$0 must be run on the Coda Sytem Control Machine (SCM)"
    exit 1
fi

# Make sure we are on the SCM.
if [ "`cat /.scm`" != "`cat /.hostname`" ] ; then
    SCM = "`cat /.scm`"
    echo "$0 must be run on the SCM ($SCM)"
    exit 1
fi


# Safety checks to be sure we really want to run this.
echo "WARNING: This script generates a script that helps to automate the"
echo "         re-initialization of Coda file servers."
echo -n "Are you sure you want to continue? "
read _ans_
case "$_ans_" in 
    [Yy] | [Yy][Ee][Ss])
        echo "Generating post-reinitialization script $OUTFILE"
	echo "servers are: $SERVERS"
	;;
    *) 
	exit 1
	;;
esac

# Make the user answer lots of questions :-)
if [ -f $OUTFILE ] ; then
    echo ""
    echo "WARNING: There is already a post-reinit script, $OUTFILE"
    echo -n "Do you want to continue? "
    read _ans_
    case "$_ans_" in 
        [Yy] | [Yy][Ee][Ss])
            echo "Moving $OUTFILE to $OUTFILE.old"
  	    mv -f $OUTFILE $OUTFILE.old
	    mv -f $SKIPFILE $SKIPFILE.old
            ;;
	*) 
	    exit 1
	    ;;
    esac
fi

# Create the file as a zero length file.  This will make maintainence a
# bit easier.
touch $OUTFILE
echo "#!/bin/csh -f" >> $OUTFILE
touch $SKIPFILE

# Put some kind for blurb in $OUTFILE
echo "# This file was created by $0" >> $OUTFILE
echo "# Created: " `date` >> $OUTFILE
echo "# Servers:  $SERVERS" >> $OUTFILE
echo "# Run this script *AFTER* the servers have been reinitialized." >> $OUTFILE
echo "# It will re-create the volumes that were on the servers." >> $OUTFILE

# Ok, we know we are the right person and on the right machine.  Make
# Sure the post-reinit scripts know this as well.
cat << _____EOF_____ >> $OUTFILE


# Make sure we are running as root.'
if (\`whoami | awk '{print substr(\$1, 1, 4)}'\` != "root") then
    echo "\$argv[0] must be run as root"
    exit 1
endif

# Make sure we are on a Coda server.
if ( ! -e /.scm || ! -e /.hostname ) then
    echo "\$argv[0] must be run on the Coda Sytem Control Machine (SCM)"
    exit 1
endif

# Make sure we are on the SCM.
if (\`cat /.scm\` != \`cat /.hostname\`) then
    set SCM = \`cat /.scm\`
    echo "\$argv[0] must be run on the SCM (\$SCM)"
    exit 1
endif

# Safety checks to be sure we really want to run this.
echo "WARNING: This script automates part of a Coda server re-initialization"
echo "DO NOT RUN THIS SCRIPT UNLESS YOU KNOW WHAT YOU ARE DOING"
echo -n "Are you sure you want to continue? "
set _ans_="\$<"
if ( "\$_ans_" =~ [Yy] || "\$_ans_" =~ [Yy][Ee][Ss]) then
    echo "Recreating volumes on: $SERVERS"
else
    exit 1
endif

# More annoying questions:
echo ""
echo "This script is to be run *AFTER* the servers have been reinitialized"
echo "and their volumelist have been removed.  The servers being"
echo "reinitialized are: $SERVERS"
echo -n "Have you reinitialized RVM on each server? "
set _ans_="\$<"
if ( "\$_ans_" =~ [Yy] || "\$_ans_" =~ [Yy][Ee][Ss]) then
    echo ""
else
    echo "exitting..."
    exit 1
endif

echo ""
echo -n "Have you made /vice/vol/VolumeList a 0 length file on each server? "
set _ans_="\$<"
if ( "\$_ans_" =~ [Yy] || "\$_ans_" =~ [Yy][Ee][Ss]) then
    echo ""
else
    echo "exitting..."
    exit 1
endif


set MOUNTPT=$MOUNTPT
set COUTFILE=$CLIENTFILE

rm -f \$COUTFILE

echo '#\!/usr/misc/bin/ksh'                               	>  \$COUTFILE
echo ''								>> \$COUTFILE
echo 'set_acls () {'                                      	>> \$COUTFILE
echo '    name=\$1' 				  	  	>> \$COUTFILE
echo '    while read path ; do'                           	>> \$COUTFILE
echo '        acl=\`/usr/coda/etc/cfs la \$path\`'        	>> \$COUTFILE
echo '        /usr/coda/etc/cfs sa -clear '\${MOUNTPT}'/inited/\$name/\$path \$acl' >> \$COUTFILE
echo '    done'                                           	>> \$COUTFILE
echo '}'                                                  	>> \$COUTFILE
echo ''								>> \$COUTFILE

echo 'check_vols () {'					  	>> \$COUTFILE
echo '    name=\$1'					  	>> \$COUTFILE
echo '    rname=\$2'					  	>> \$COUTFILE
echo '    if [ \! -d '\${MOUNTPT}'/inited/\${name} ] ; then'   	>> \$COUTFILE
echo '        echo '\${MOUNTPT}'/inited/\${name} is not a directory, skipping' >> \$COUTFILE
echo '        return 0'					  	>> \$COUTFILE   
echo '    fi'						  	>> \$COUTFILE
echo '    if [ \! -d '\${MOUNTPT}'/restored/\${rname} ] ; then'	>> \$COUTFILE
echo '        echo '\${MOUNTPT}'/restored/\${rname} is not a directory, skipping' >> \$COUTFILE
echo '        return 0'					  	>> \$COUTFILE   
echo '    fi'						  	>> \$COUTFILE
echo '    return 1'						>> \$COUTFILE
echo '}'							>> \$COUTFILE
echo ''								>> \$COUTFILE


_____EOF_____

# Next, rebuild the vldb.  This takes care of having to delete entries
# from /vice/vol/AllVolumes and simplifies the script in general.  Unfortunately,
# We still need to remove entries from VRList
echo "# Rebuild the vldb.  This takes care of having to delete" >> $OUTFILE
echo "# entries from /vice/vol/AllVolumes and simplifies the script" >> $OUTFILE
echo "# in general.  Unfortunately, We still need to remove entries" >> $OUTFILE
echo "# from VRList" >> $OUTFILE
echo "echo \"/vice/bin/bldvldb.sh\"" >> $OUTFILE
echo "/vice/bin/bldvldb.sh" >> $OUTFILE
echo "" >> $OUTFILE

# Set the new maximum volume id in RVM on each server.
echo "# Now set the maximum volume id on each server" >> $OUTFILE
for SERVER in $SERVERS ; do
    maxvolid="`/vice/bin/volutil -h $SERVER getmaxvol | awk '/volume id/ {print substr($5, 3)}'`"
    if [ "$maxvolid" = "" ] ; then
	echo "ERROR: Cannot get maxvolid from $SERVER, exitting"
	abort
    fi

    rm -f /tmp/$$
    cat << _____EOF_____ > /tmp/$$ 
obase=16
ibase=16
$maxvolid + 1
_____EOF_____
    maxvolid="`bc < /tmp/$$`"
    rm /tmp/$$
    if [ "$maxvolid" = "" ] ; then       # A little paranoia never hurt.
	echo "ERROR: Cannot get maxvolid from $SERVER, exitting"
	abort
    fi
    
    echo "Will change maxvolid on $SERVER to $maxvolid"
    echo "/vice/bin/volutil -h $SERVER setmaxvol $maxvolid" >> $OUTFILE
done
echo "" >> $OUTFILE


# Now deal with the replicated volumes.  Any replicated volume that
# is on a subset of $SERVERS needs to be recreated.  
cat /vice/vol/VRList | recreate_replicated_vols

# Deal with the rest of the volumes.  Grok through AllVolumes for volume
# names that are not in the VRList.
awk '{print $1, $3, $4, $8}' /vice/vol/AllVolumes | \
    while read name server partition read_write; do
	# get the first component of server and lower case it.
	server=`echo $server | awk -F. '{print $1}' | tr A-Z a-z`

	# Is this volume on a server to be reinited?
	found=0
	for SERVER in $SERVERS ; do
	    if [ "$server" = "$SERVER" ] ; then
		found=1
	    fi
	done
	if [ $found -eq 0 ] ; then
	    # Not on a server to be reinitialized.
	    continue
	fi

	# Remove the last element of name and see if its a replicated volume
	# This "sed" will remove something of the form ".#" from the end
	# of $name where # is a single digit.  (really only need to look 
	# for 0-7).
	short_name=`echo $name | sed 's/\(.*\)\(\.[0-9]\)$/\1/'`
	rep_vol="`fgrep \"$short_name \" /vice/vol/VRList`"
	if [ "$rep_vol" != "" ] ; then
	    # The volume is a replica.
	    continue
	fi

	if [ "$read_write" = "B" ] ; then
	    # Backup volume, skip it.
 	    echo "Skipping backup volume: $name"
	    continue
	fi

	if [ "$read_write" = "R" ] ; then
	    # Read-only volume, append it to $SKIPFILE and continue.
	    echo "Skipping read-only volume: $name"
	    echo "$name" >> $SKIPFILE
	    continue
	fi


	backup=`awk '$3 == "'$name'" {print $2}' /vice/db/dumplist`
        if [ "$backup" != "" ] ; then
 	    echo "awk ' \$3 !~ '\"$name\"' { print } ' /vice/db/dumplist >/vice/db/dumplist.\$\$" >> $OUTFILE
	    echo "mv -f /vice/db/dumplist.\$\$ /vice/db/dumplist" >> $OUTFILE
	fi

	echo "Generating code for non-replicated volume: $name"
	echo "# Volume name: $name" >> $OUTFILE
	echo "echo \"Creating non-replicated volume: $name\"" >> $OUTFILE
	echo "/vice/bin/recreatevol $name $server $partition $backup" >> $OUTFILE
	generate_client_file $name
	echo "" >> $OUTFILE
    done

	

# rebuild the vrdb and vldb
echo "# Rebuild the vrdb and vldb" >> $OUTFILE
echo "/vice/bin/bldvldb.sh" >> $OUTFILE
echo "/vice/bin/volutil makevrdb /vice/vol/VRList" >> $OUTFILE

echo "echo \"\$argv[0] done.\"" >> $OUTFILE
echo "echo \"Now retore the original data from tape and\"" >> $OUTFILE
echo "echo \"run \$COUTFILE on a client machine to restore the data.\"" >> $OUTFILE

# Set $OUTFILE as an executable and tell the user where it is.
chmod 500 $OUTFILE

echo ""
echo "$0 done."
echo "Post reinit script:          $OUTFILE"
echo "Volumes not being recreated: $SKIPFILE"
echo ""
echo "DO NOT RUN ANY OF THE POST-REINIT SCRIPTS BEFORE YOU REINIT THE SERVERS"
echo "Make sure that /vice/vol/VolumeList gets truncated on each server"
echo "as it gets reinitialized"



