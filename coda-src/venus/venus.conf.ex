# This file contains configuration information for venus, the Coda
# client. Most of the things specified here can be overridden by
# command-line flags.

# venus.conf.ex contains an entry for each configuration statement
# allowed in venus.conf.  Each statement is explained, and is then
# presented, commented out, with its default value.  Thus, if this 
# file is used as venus.conf, the behavior of venus will be the same
# as with no config file.  Further, uncommenting any of these
# statements in a venus.conf should have no effect.  It is suggested  
# that a venus.conf be created if needed, and that it contain only
# statements that differ from default values.

#
# What is the default realm, this is only used by clog/ctokens etc.
#
#realm=

#
# Which file is used for realm lookups, before we look at dns SRV and A
# records.
#
#realmtab=/etc/coda/realms

#
# What should the size of the local cache be in 1k blocks. If this is
# not specified or `0' the default value of 40000 (40MB) is chosen.
# Minimum value is 4096.
#
#cacheblocks=40000
cacheblocks=100000

#
# How many files should the venus cache hold. If this is not specified
# or `0' the default value is calculated as 'cacheblocks / 24'.
# Minimum value is 85.
#
#cachefiles=0

#
# How many modification log entries should venus have. If this is not
# specified or `0' the value is calculated as 'cacheblocks / 6'.
# (with the default number of cachefiles this is '4 * cachefiles')
# Minimum value is 256.
#
#cml_entries=0

#
# How many hoard database entries should venus have. If this is not
# specified or `0' the value is calculated as 'cacheblocks / 48'.
# (with the default number of cachefiles this is 'cachefiles / 2')
# Minimum value is 42.
#
#hoard_entries=0

#
# Which file should receive venus's stderr output.
# (default is /usr/coda/etc/console).
#
#errorlog=/usr/coda/etc/console

#
# What is the uid of the primary user of this system. The primary user
# has the rights to modify the entries in the hoard database. Normally
# venus can successfully figure this out by looking which user has the
# `console', or is logged in on tty1, the first virtual console. Root is
# always considered `primary user'.
# If a non-zero uid is specified here, venus will not attempt to guess
# who the primary user is.
#
#primaryuser=0

#
# What are the locations of the rvm log and data files or partitions.
# (default /usr/coda/{LOG,DATA}
#
# CAUTION: these are _unrelated_ to the server's rvm log and data. Also
# when specifying partitions, do NOT specify any partions which might
# have an existing filesystem, as you WILL lose all data during
# initialization.
#
#rvm_log=/usr/coda/LOG
#rvm_data=/usr/coda/DATA

#
# Where should venus keep it's pool of cached files.
# (default is /usr/coda/venus.cache)
#
#cachedir=/usr/coda/venus.cache

#
# Where should venus store snapshots of the modification logs. This is
# primarily to provide some safe fallback when venus dies or messes up
# the CML during reintegration. (default is /usr/coda/spool)
#
#checkpointdir=/usr/coda/spool
	
#
# Where does venus store it's pidfile
#
#pid_file=/usr/coda/venus.cache/pid
 
#
# What file does vutil use to control venus 
#
#run_control_file=/usr/coda/venus.cache/VENUS_CTRL

#
# Venus log file
#
#logfile=/usr/coda/etc/venus.log

#
# Mariner socket, 
# On platforms that support unix domain sockets this is the socket that
# codacon and spy use to talk to venus. On other platforms they will
# connect to venus through tcp port 2430.
# 
#marinersocket=/usr/coda/spool/mariner

#
# Dont use RVM,
# Avoid using persistent storage for metadata. Only useful on diskless
# clients or small handhelds when the venus.cache is stored in a ramdisk
# or ram filesystem. Beware, if you enable this option, venus will loose
# all not reintegrated modifications when it dies.
#
#dontuservm=0

#
# Map private,
# Whether to use a private mmaping of the RVM data file instead of a
# anonymous mmap. Using a private mmap will reduce startup times, as
# missing pages are paged in when they are accesses. Also swap usage is
# reduced as any unmodified pages do not have to be backed up by swap
# memory when memory gets tight. Private mappings do not work when rvm
# data is stored on a raw partition. Set to 1 to use private mappings,
# 0 to use anonymous mappings.
# 
#mapprivate=0
mapprivate=1

#
# Mountpoint,
# Where the coda filesystem is mounted, many application still depend on
# this being /coda. I am unsure whether it matters much on Windows95/98,
# but we normally use N: for those.
#
#mountpoint=N:
#mountpoint=/coda

#
# Kernel device,
# The character device used by venus to communicate with the kernel module.
# Standard name is /dev/cfs0, ignored by the Win95/98 port. Maybe we will
# switch to using /dev/coda0 at some point to avoid name conflicts.
#
#kerneldevice=/dev/cfs0,/dev/coda/0

#
# The masquerade option enables SFTP to use the same ports as RPC2 for
# bulk data transfers. This allows a Coda client to connect to servers
# from behind a masquerading firewall. The rpc2 library on both the
# client and the servers must be at least version 1.9 for this to work.
# 
#masquerade=0
masquerade=1

#
# Masquerade port, if masquerading bind the client to the specified port.
# when masquerade_port=0, an arbitrary port is used.
# 
#masquerade_port=0

#
# On NT with Cygwin, venus names are different than the NT real names.
# Venus names are relative to the root of the cygwin tree.  Replace
# the C:/cygwin with the actual root directory of your cygwin installation.
# Do not remove the /??/ part of the prefix.
#
#cache_prefix=/??/C:/cygwin

#
# Maximum number of fids & versionvectors that will be piggybacked with a
# ValidateAttrs RPC2 call. The default (and maximum) is 50, however the
# resulting outgoing packet is around 2900 bytes which will be fragmented by
# the IP layer. In some cases this IP fragmentation is not wanted (unreliable
# networks, IPSec, and in the future IPv6 networks). Setting this number to 15
# piggybacked validations limits the outgoing ValidateAttr packet size to
# about 1032 bytes, and 21 validations would result in a 1452 byte packet.
#
#validateattrs=50

#
# How many seconds between checks whether the servers are still alive. The
# default is 12 minutes. However masquerading firewalls will typically forget
# about UDP connections sooner. For instance, connection tracking in Linux's
# netfilter only remembers UDP streams for about 3 minutes, so 150 seconds
# would be a good choice here.
#
# If you are going through a firewall and regularily see messages that the
# servers nak'ed lowering this timeout could very well help a lot.
#
#serverprobe=720

#
# Kerberos 4/5 settings.
# These can also be placed in the "auth2.conf" file.
#
#kerberos4service="host"
#kerberos4realm=
#kerberos4kinit="kinit"

# kerberos5service contains "%s" which will be substituted with a hostname,
# for a usual DCE setup it would be "hosts/%s/self
#kerberos5service="host/%s"
#kerberos5realm=
#kerberos5kinit="kinit"

