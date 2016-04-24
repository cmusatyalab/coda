# Final installation script ... build /usr/coda with proper permissions.
# Parameters:  None:

PATH=/bin:/usr/bin:/usr/sbin
CODACONFPATH=/etc/coda
export CODACONFPATH

# default set up ... assume an existing one means a previous setup
if ! test -d /etc/coda ; then
   coda-client-setup coda.cs.cmu.edu 250000 n
fi

if ! test -d /usr/coda ; then
  mkdir /usr/coda
fi
if ! test -d /usr/coda/venus.cache ; then
  mkdir /usr/coda/venus.cache
fi
if ! test -d /usr/coda/spool ; then
  mkdir /usr/coda/spool
fi
if ! test -d /usr/coda/etc ; then
  mkdir /usr/coda/etc
fi

touch /usr/coda/{DATA,LOG}
touch /usr/coda/venus.cache/INIT

chmod 755 /usr/coda
chmod 755 /usr/coda/etc
chmod 770 /usr/coda/{spool,venus.cache}
chmod 600 /usr/coda/{DATA,LOG}

chown SYSTEM:Administrators /usr/coda/{DATA,LOG,etc,spool,venus.cache}
chown SYSTEM:Administrators /usr/coda

# install venus as a service
cygrunsrv -R venus
cygrunsrv -I venus -p /usr/sbin/venus -o -t manual
