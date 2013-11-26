#!/bin/sh
#
# coda-update
#
# chkconfig: - 95 02
# description: The Coda update server and client.

### BEGIN INIT INFO
# Provides:	     coda-update
# Required-Start:    $local_fs $network $named $time $remote_fs
# Required-Stop:     $local_fs $network $named $time $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:	     0 1 6
# Short-Description: Start the Coda update daemons.
### END INIT INFO

. /lib/lsb/init-functions

subsys=coda-update

exec1=/usr/sbin/updatesrv
prog1=updatesrv
pid_file1=/vice/misc/updatesrv.pid
config=/vice/db/scm

exec2=/usr/sbin/updateclnt
prog2=updateclnt
pid_file2=/vice/misc/updateclnt.pid

lockfile=/var/lock/subsys/$subsys

[ -e /etc/sysconfig/$subsys ] && . /etc/sysconfig/$subsys

start () {
    [ -x $exec1 -a -x $exec2 ] || exit 5
    [ -f $config ] || exit 0

    echo -n $"Starting $subsys:"
    retval1=0
    retval2=0
    allret=0
    anyret=1

    if [ "`cat /vice/hostname`" = "`cat /vice/db/scm`" ] ; then
	echo -n " $prog1"
	if ! pidofproc -p $pid_file1 $prog1 >/dev/null 2>&1 ; then
	    start_daemon -p $pid_file1 $exec1
	    retval1=$?
	    allret=$retval1
	    anyret=$retval1
	fi
    fi

    echo -n " $prog2"
    if ! pidofproc -p $pid_file2 $prog2 >/dev/null 2>&1 ; then
	start_daemon -p $pid_file2 $exec2 -h "`cat /vice/db/scm`"
	retval2=$?
    fi

    [ $allret -eq 0 ] && allret=$retval2
    [ $anyret -ne 0 ] && anyret=$retval2

    [ $allret -eq 0 ] && log_success_msg || log_failure_msg
    [ $anyret -eq 0 -a -d /var/lock/subsys ] && touch $lockfile
    return $allret
}

stop () {
    echo -n $"Stopping $subsys:"

    echo -n " $prog1"
    killproc -p $pid_file1 $exec1
    retval1=$?

    echo -n " $prog2"
    killproc -p $pid_file2 $exec2
    retval2=$?

    [ $retval1 -eq 0 ] && allret=$retval2 || allret=$retval1
    [ $allret -eq 0 ] && log_success_msg || log_failure_msg
    [ $allret -eq 0 -a -d /var/lock/subsys ] && rm -f $lockfile
    return $allret
}

restart() {
    stop
    start
}

reportstatus() {
    pidofproc -p $pid_file1 $prog1 >/dev/null 2>&1
    retval1=$?
    if [ $retval1 -eq 0 ] ; then
	echo "$prog1 is running"
    else
	echo "$prog1 is not running"
    fi

    pidofproc -p $pid_file2 $prog2 >/dev/null 2>&1
    retval2=$?
    if [ $retval2 -eq 0 ] ; then
	echo "$prog2 is running"
    else
	echo "$prog2 is not running"
    fi

    [ $retval1 -eq 0 ] && allret=$retval2 || allret=$retval1
    return $allret
}

checkall() {
    reportstatus >/dev/null 2>&1
}

checkany() {
    pidofproc -p $pid_file1 $prog1 >/dev/null 2>&1
    retval1=$?

    pidofproc -p $pid_file2 $prog2 >/dev/null 2>&1
    retval2=$?

    [ $retval1 -eq 0 ] && anyret=$retval1 || anyret=$retval2
    return $anyret
}


case "$1" in
    start)
	checkall && exit 0
	$1
	;;
    stop)
	checkany || exit 0
	$1
	;;
    hardstop)
	stop
	;;
    restart)
	restart
	;;
    condrestart|try-restart)
	checkany || exit 0
	restart
	;;
    reload|force-reload)
	echo $"$1 not available"
	exit 3
	;;
    status)
	reportstatus
	;;
    *)
	echo $"Usage: $0 {start|stop|status|restart|condrestart|try-restart|reload|force-reload|hardstop}"
	exit 2
esac
exit $?

