#!/bin/sh
#
# Written by Miquel van Smoorenburg <miquels@cistron.nl>.
# Modified for Debian GNU/Linux by Ian Murdock <imurdock@gnu.org>.
# Modified for Debian by Christoph Lameter <clameter@debian.org>

PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/sbin/venus

FLAGS="defaults 50"

test -f $DAEMON || exit 0

case "$1" in
  start)
    start-stop-daemon --start --verbose --exec $DAEMON
    ;;
  stop)
    start-stop-daemon --stop --verbose --exec $DAEMON
    umount /coda
    ;;
  #reload)
    #
    # If the daemon can reload its config files on the fly
    # for example by sending it SIGHUP, do it here.
    #
    # If the daemon responds to changes in its config file
    # directly anyway, make this a do-nothing entry.
    #
    # start-stop-daemon --stop --signal 1 --verbose --exec $DAEMON
    # ;;
  restart|force-reload)
  #
  # If the "reload" option is implemented, move the "force-reload"
  # option to the "reload" entry above. If not, "force-reload" is
  # just the same as "restart".
  #
    start-stop-daemon --stop --verbose --exec $DAEMON
    umount /coda
    sleep 1
    start-stop-daemon --start --verbose --exec $DAEMON
    ;;
  *)
    echo "Usage: /etc/init.d/coda-client {start|stop|restart|force-reload}"
    exit 1
    ;;
esac

exit 0
