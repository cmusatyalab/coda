#!/bin/sh

REAL_SCRIPT=/tmp/reinit_script	# The script to be run when done
SCRIPT=$REAL_SCRIPT.$$		# Build the script here, helps prevent re-runs.
NOCREATE=/tmp/not_created	# List of volumes not being recreated

echon() {
    if [ "`echo -n`" ] ; then
        echo "$@"\c
    else
        echo -n "$@"
    fi
}

# Make sure we aren't running this by mistake!
if [ -f $REAL_SCRIPT ] ; then
    echo "WARNING: $REAL_SCRIPT exists!"
    echon "Are you sure you want to continue? "
    read _ans_
    case "$_ans_" in
        [Yy] | [Yy][Ee][Ss])
	    echo ""
            ;;
	*)
	    echo "exiting..."
            exit 1
            ;;
    esac
fi

/bin/rm -f $REAL_SCRIPT
/bin/rm -f $NOCREATE

# Create files for appending
echo "#!/bin/sh" > $SCRIPT
echo "" >> $SCRIPT
touch $NOCREATE

# First remove unwanted entries from VolumeList (egrep), 
# then extract the data we need (awk).
egrep -v '^P' /vice/vol/VolumeList | egrep -v '\.backup ' | \
	egrep -v '\.restored ' | \
	awk '{print substr($4, 2), substr($1, 2), substr($2, 2)}' | \
    while read part name volid ; do 

	# Get the replicated volume id (repid).
	repid=`grep $volid /vice/vol/VRList | awk '{print $2}'`
	if [ "$repid" = "" ] ; then
	    echo "WARNING: Cannot find repid for volume \"$name\" ($volid)!"
	    echo "    Check that you have a backup before reinitializing!"
	    continue
	fi

	# Rearrange the arguments and put the call into a script file
	echo "volutil create_rep $part $name $repid $volid" >> $SCRIPT

    done

# Create a list of all volumes that are not being recreated.
egrep -v '^P' /vice/vol/VolumeList  | awk '{print substr($1, 2)}' | \
    while read name ; do
	ok=`grep " $name " $SCRIPT`
	if [ "$ok" = "" ] ; then
	    echo $name >> $NOCREATE
	fi
    done


echo ""
echo ""
if [ -s $NOCREATE ] ; then
    echo "Some volumes will not be recreated.  Please look through"
    echo "	$NOCREATE"
    echo "to check the list of volumes that will be lost."
    echo ""
fi

echo "To recreate the volumes after you reinit the server, run"
echo "	$REAL_SCRIPT"

mv $SCRIPT $REAL_SCRIPT
chmod u+x $REAL_SCRIPT


