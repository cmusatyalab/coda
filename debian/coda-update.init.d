#!/bin/sh
#
# Written by Miquel van Smoorenburg <miquels@cistron.nl>.
# Modified for Debian GNU/Linux by Ian Murdock <imurdock@gnu.org>.
# Modified for Debian by Christoph Lameter <clameter@debian.org>

PATH=/bin:/usr/bin:/sbin:/usr/sbin
RPC2PORTMAP=/usr/sbin/rpc2portmap
UPDATECLNT=/usr/sbin/updateclnt
UPDATESRV=/usr/sbin/updatesrv

FLAGS="defaults 40"
 
vicedir=/vice
test -f /etc/coda/server.conf && . /etc/coda/server.conf

test -d $vicedir/db || exit 0
test -f $vicedir/db/scm || exit 0
SCM=`cat $vicedir/db/scm`
HOST=`hostname -f`

test -f $UPDATECLNT || exit 0
test $SCM != $HOST -o -f $RPC2PORTMAP || exit 0
test $SCM != $HOST -o -f $UPDATESRV || exit 0

case "$1" in
  start)
    [ $SCM != $HOST ] || start-stop-daemon --start --verbose --exec $RPC2PORTMAP
    [ $SCM != $HOST ] || start-stop-daemon --start --verbose --exec $UPDATESRV
    start-stop-daemon --start --verbose --exec $UPDATECLNT
    ;;
  stop)
    start-stop-daemon --stop --verbose --exec $UPDATECLNT
    [ $SCM != $HOST ] || start-stop-daemon --stop --verbose --exec $UPDATESRV
    [ $SCM != $HOST ] || start-stop-daemon --stop --verbose --exec $RPC2PORTMAP
    ;;
  #reload)
    #
    # If the daemon can reload its config files on the fly
    # for example by sending it SIGHUP, do it here.
    #
    # If the daemon responds to changes in its config file
    # directly anyway, make this a do-nothing entry.
    #
    # start-stop-daemon --stop --signal 1 --verbose --exec $RPC2PORTMAP
    # [ $SCM != $HOST ] || start-stop-daemon --stop --signal 1 --verbose --exec $UPDATESRV
    # start-stop-daemon --stop --signal 1 --verbose --exec $UPDATECLNT
    # ;;
  restart|force-reload)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: /etc/init.d/coda-update {start|stop|restart|force-reload}"
    exit 1
    ;;
esac

exit 0
