#!/bin/csh -f

#
# usage: trunclog.sh <database>
#
# Safely truncates the log of database <database>
# Must be run as root from the machine the database is physically on.
# (Note that this implies root is a DBA for <database>
#
# Assumes that the logfile for a given database is named
# <database>.log
#
# See the Informix-ESQL/C manual for further details
#

if ($#argv != 1) then
	echo "Usage: trunclog <database>"
	exit -1
endif

echo "Truncating log for database" $1
sleep 10

/usr/scylla/etc/stop $1
rm /usr/scylla/dbs/$1.dbs/$1.log
touch /usr/scylla/dbs/$1.dbs/$1.log
/etc/chown misc.misc /usr/scylla/dbs/$1.dbs/$1.log
chmod 660 /usr/scylla/dbs/$1.dbs/$1.log
/usr/scylla/etc/start $1
