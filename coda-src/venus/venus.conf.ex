# This file contains configuration information for venus, the Coda
# client. Most of the things specified here can be overridden by either
# command-line flags, or /usr/coda/etc/vstab (if that file exists)

#
# Which are the root servers we should contact for user authentication,
# and volume location information.
#
rootservers=testserver.coda.cs.cmu.edu

#
# What should the size of the local cache be in 1k blocks. If this is
# not specified or `0' the default value of 40000 (40MB) is chosen.
# Minimum value is 2048.
#
cacheblocks=100000

#
# How many files should the venus cache hold. If this is not specified
# or `0' the default value is calculated as 'cacheblocks / 24'.
# Minimum value is 85.
#
cachefiles=0

#
# How many modification log entries should venus have. If this is not
# specified or `0' the value is calculated as 'cacheblocks / 6'.
# (with the default number of cachefiles this is '4 * cachefiles')
# Minimum value is 256.
#
cml_entries=0

#
# How many hoard database entries should venus have. If this is not
# specified or `0' the value is calculated as 'cacheblocks / 48'.
# (with the default number of cachefiles this is 'cachefiles / 2')
# Minimum value is 42.
#
hoard_entries=0

#
# Which file should receive venus's stderr output.
# (default is /usr/coda/etc/console).
#
errorlog=/usr/coda/etc/console

#
# What is the uid of the primary user of this system. The primary user
# has the rights to modify the entries in the hoard database. Normally
# venus can successfully figure this out by looking which user has the
# `console', or is logged in on tty1, the first virtual console. Root is
# always considered `primary user'.
# If a non-zero uid is specified here, venus will not attempt to guess
# who the primary user is.
#
primaryuser=0

#
# What are the locations of the rvm log and data files or partitions.
# (default /usr/coda/{LOG,DATA}
#
# CAUTION: these are _unrelated_ to the server's rvm log and data. Also
# when specifying partitions, do NOT specify any partions which might
# have an existing filesystem, as you WILL lose all data during
# initialization.
#
rvm_log=/usr/coda/LOG
rvm_data=/usr/coda/DATA

#
# Where should venus keep it's pool of cached files.
# (default is /usr/coda/venus.cache)
#
cachedir=/usr/coda/venus.cache

#
# Where should venus store snapshots of the modification logs. This is
# primarily to provide some safe fallback when venus dies or messes up
# the CML during reintegration. (default is /usr/coda/spool)
#
checkpointdir=/usr/coda/spool
	
#
# Where does venus store it's pidfile
#
pid_file=/usr/coda/venus.cache/pid
 
#
# What file does vutil use to control venus 
#
run_control_file=/usr/coda/venus.cache/VENUS_CTRL

#
# Venus log file
#
logfile=/usr/coda/venus.cache/venus.log

#
# Mariner socket, 
# On platforms that support unix domain sockets this is the socket that
# codacon and spy use to talk to venus. On other platforms they will
# connect to venus through tcp port 2430.
# 
marinersocket=/usr/coda/spool/mariner

#
# Map private,
# Whether to use a private mmaping of the RVM data file instead of a
# anonymous mmap. Using a private mmap will reduce startup times, since
# missing pages are paged in when they are accesses. Also swap usage is
# reduced as any unmodified pages do not have to be backed up by swap
# memory when memory gets tight. On the other hand, private mappings are
# still somewhat experimental and do not work when rvm data is stored on
# a raw partition. Set to 1 to use private mappings, 0 to use anonymous
# mappings.
# 
mapprivate=0

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
# switch # to using /dev/coda0 at some point to avoid name conflicts.
#
kerneldevice=/dev/cfs0


