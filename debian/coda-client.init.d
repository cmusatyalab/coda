#!/bin/sh
#
# coda-client
#
# chkconfig: 345 97 01
# description: The Coda file system client.

### BEGIN INIT INFO
# Provides:	     coda-client
# Required-Start:    $local_fs $time
# Required-Stop:     $local_fs
# Should-Start:      $network $named
# Should-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:	     0 1 6
# Short-Description: Start the Coda file system client.
### END INIT INFO

. /lib/lsb/init-functions

subsys=coda-client

exec=/usr/sbin/venus
prog=venus
pid_file=/var/run/coda-client.pid
config=/etc/coda/venus.conf

lockfile=/var/lock/subsys/$subsys

[ -e /etc/sysconfig/$subsys ] && . /etc/sysconfig/$subsys

# coda-client specific settings
mountpoint=/coda
[ -x /usr/sbin/codaconfedit ] && . `/usr/sbin/codaconfedit venus.conf`


start () {
    [ -x $exec ] || exit 5
    [ -f $config ] || exit 6
    echo -n $"Starting $subsys:"
    echo -n " kernel"
    /sbin/modprobe coda
    /sbin/udevadm settle

    echo -n " $prog"
    /usr/sbin/vutil --swaplogs >/dev/null 2>&1 || true
    start_daemon -p $pid_file $exec 2>&1 >/dev/null
    retval=$?

    [ $retval -eq 0 ] && log_success_msg || log_failure_msg
    [ $retval -eq 0 -a -d /var/lock/subsys ] && touch $lockfile
    return $retval
}

stop () {
    echo -n $"Stopping $subsys:"
    echo -n " unmounting $mountpoint"
    umount $mountpoint >/dev/null 2>&1
    retval=$?

    if [ $retval -eq 0 ] ; then
        echo -n " $prog"
	killproc -p $pid_file $prog
	retval=$?
    fi

    [ $retval -eq 0 ] && log_success_msg || log_failure_msg
    [ $retval -eq 0 -a -d /var/lock/subsys ] && rm -f $lockfile
    return $retval
}

restart() {
    stop
    start
}

checkstatus() {
    pidofproc -p $pid_file $prog >/dev/null 2>&1
}

case "$1" in
    start)
	checkstatus && exit 0
	$1
	;;
    stop)
	checkstatus || exit 0
	$1
	;;
    hardstop)
	stop
	;;
    restart)
	restart
	;;
    condrestart|try-restart)
	checkstatus || exit 0
	restart
	;;
    reload|force-reload)
	echo $"$1 not available"
	exit 3
	;;
    status)
	checkstatus
	retval=$?
	if [ $retval -eq 0 ] ; then
	    echo "$prog is running"
	else
	    echo "$prog is not running"
	fi
	exit $retval
	;;
    *)
	echo $"Usage: $0 {start|stop|status|restart|condrestart|try-restart|reload|force-reload|hardstop}"
	exit 2
esac
exit $?

