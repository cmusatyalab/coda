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
# What should the size of the local cache be. If this is not specified or
# `0' the default value of 2048 or 2MB is chosen. Supported units are KB,
# MB, GB, and TB.
# Minimum value is 2048 or 2MB.
#
#cachesize=40MB
cachesize=100MB

#
# How many files should the venus cache hold. If this is not specified
# or `0' the default value is calculated as 'cacheblocks / 24'.
# Minimum value is 85.
#
#cachefiles=0

#
# What should the cache chunks block size be. If this is not specified or '0'
# the default value of 32KB is chosen. Supported units are KB, MB, GB and TB.
# This is relevant for VASTROs handling.
# Minimum value is 4096 or 4KB.
#
#cachechunkblocksize=4KB

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
errorlog=/var/log/coda/venus.err

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
rvm_log=/var/lib/coda/LOG
rvm_data=/var/lib/coda/DATA

#
# Where should venus keep it's pool of cached files.
# (default is /usr/coda/venus.cache)
#
#cachedir=/usr/coda/venus.cache
cachedir=/var/lib/coda/cache

#
# Where should venus store snapshots of the modification logs. This is
# primarily to provide some safe fallback when venus dies or messes up
# the CML during reintegration. (default is /usr/coda/spool)
#
#checkpointdir=/usr/coda/spool
checkpointdir=/var/lib/coda/spool

#
# Checkpoint archive format
#
# tar   - old BSD4.3 tar format
# ustar - POSIX.1 tar format
# odc	- old POSIX.1 cpio format
# newc  - new SVR4 cpio format
#
# This used to be 'tar', but it has a 100 character path name limit. ustar
# extends this to 255 characters. The cpio formats do not have such a strict
# path name limitation, but 'odc' doesn't support 32-bit inode/uid/gid values
# and 'newc' can only deal with < 4GB files, but Coda has the same limitation.
#
#checkpointformat=newc

#
# Where does venus store it's pidfile
#
#pid_file=/usr/coda/venus.cache/pid
pid_file=/var/run/coda-client.pid

#
# What file does vutil use to control venus
#
#run_control_file=/usr/coda/venus.cache/VENUS_CTRL
run_control_file=/var/run/coda-client.ctrl

#
# Venus log file
#
#logfile=/usr/coda/etc/venus.log
logfile=/var/log/coda/venus.log

#
# Mariner socket,
# On platforms that support unix domain sockets this is the socket that
# codacon and spy use to talk to venus. On other platforms they will
# connect to venus through tcp port 2430.
#
#marinersocket=/usr/coda/spool/mariner
marinersocket=/var/run/coda-client.mariner

#
# Dont use RVM,
# Avoid using persistent storage for metadata. Only useful on diskless
# clients or small handhelds when the venus.cache is stored in a ramdisk
# or ram filesystem. Beware, if you enable this option, venus will lose
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
# data is stored on a raw partition on Linux. Set to 1 to use private
# mappings, 0 to use anonymous mappings.
#
# We enabled this setting because venus uses files for RVM and the
# faster startup times and reduced memory pressure are very noticable.
#
#mapprivate=0
mapprivate=1

#
# Kernel device,
# The character device used by venus to communicate with the kernel module.
# Standard name is /dev/cfs0. Maybe we will switch to using /dev/coda0 at
# some point to avoid name conflicts.
#
#kerneldevice=/dev/cfs0,/dev/coda/0

#
# Masquerade port, if non-zero, bind the client to the specified port.
# When masquerade_port=0, an arbitrary port is used.
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
# ValidateAttrs RPC2 call. The maximum is 50, however the resulting outgoing
# packet is around 2900 bytes which will be fragmented by the IP layer. In
# some cases this IP fragmentation is not wanted (unreliable networks, IPSec,
# and in IPv6 networks). The default value is set to 15 which brings the
# packet size down to below the minimum IPv6 MTU.
#
# The previous default value was 21, resulting in a 1452 byte packet
# which was nice for typical ethernet LAN networks, but was in some
# cases causing connectivity problems. Lower than 15 is most likely not
# really useful. Many other packets would still be larger (f.i. SFTP
# data packets).
#
#validateattrs=15

#
# How many seconds between checks whether the servers are still alive. The
# default used to be 12 minutes. However masquerading firewalls typically
# forget about UDP connections a lot sooner. For instance, connection tracking
# in Linux's netfilter only remembers UDP streams for about 3 minutes, so the
# default value has been lowered to trigger a probe every 150 seconds.
#
# To keep masqueraded ports open in VMWare this needs to be 45 seconds.
# Alternatively you can configure venus to use a fixed port using the
# masquerade_port option and set up a static redirection in
# /etc/vmware/vmnet8/nat/nat.conf
#
#serverprobe=150

#
# RPC2 timeout and retry parameters.
# These used to be set to 15 (seconds) and 4 retries. However useful these are
# for demos, the new more conservative values should significantly increase
# reliability by avoiding unexpected disconnections.
#
#RPC2_timeout=60
#RPC2_retries=5

#
# Enable backfetches
#
# File data is pushed to the server in small chunks (trickle reintegrate).
#
# A backfetch happens when a client reintegrates a store record without
# pushing the data to the server first. The server needs the data to commit
# the store and performs a 'backfetch' . This avoids unnecessary data
# transfers when the client retries an already completed reintegration after
# an unexpected disconnection. However, the backfetch is done before the
# reintegration RPC can return and the volume ends up being locked for a
# longer period on both the client and the server. Also the backfetch uses the
# callback connection, as a result unrelated changes in other volumes may end
# up delayed until the backfetch completes if the server needs to send a
# callback to the currently reintegrating client.
#
#allow_backfetch=0

#
# Default reintegration parameters
#
# These are the default values set for newly instantiated volumes. They can be
# changed for cached volumes with 'cfs wd -age N -time M'. These settings are
# persistent across venus restarts.
#
# reintegration_age specifies how old a logged change should be before it is
# eligible for reintegration. A longer time will increase the chance that the
# operation can be optimized away (create/store/remove -> (none)), but a
# shorter aging value improves consistency because we push pending changes
# back faster.
#
# reintegration_time defines an upper limit on a reintegration 'cycle'. The
# actual duration is typically longer since we estimate the time it takes to
# reintegrate from the amount of bulk (SFTP) data divided by the estimated
# bandwidth and we don't account for the overhead of the RPC2 calls.
#
# Setting reintegration_time to 0 forces the client to use synchronous
# reintegration mode. It will try to push any pending changes back to the
# server before returning to the application.
#
#reintegration_age=0
#reintegration_time=15

#
# Should the server detect retried reintegration attempts.
#
# Operations are identified by a store identifier value. If we disconnect
# during a reintegration and retry the same set of operations after
# reconnection, the server can use the store identifier to detect if any
# operation have already been applied and avoid conflicts.
#
# However this detection only works when the identifiers are in a strict
# incrementing order, if for any reason we reuse older values the server
# will incorrectly reject new operations. This can for instance happen
# reverting a virtual machine to an earlier snapshot.
#
#detect_reintegration_retry=1

#
# Enable tweaks when running in a virtual machine, disabled by default.
#
# - overwrite container files with zeros before truncation to reduce the
#   amount of dirty state we need to write back during suspend.
# - avoid reintegration replay detection as we may revert back to an older
#   snapshot after discarding local state and reuse store identifiers.
#
#isr=0

#
# Fork a helper process to handle client-server communication.
#
#codatunnel=1

#
# Only use tcp tunnels for client-server communication (depends on codatunnel)
#
#onlytcp=0

#
# Enable or disable automatic mounting of the Coda File System on /coda
# using the Coda specific kernel implementation
#
#codafs=1

#
# Expose the Coda File System through a Plan9 File System server over
# the UNIX or TCP port that is configured as the mariner port.
#
#plan9server=0

#
# Size of files above which it's partially cached, regardless of bandwidth.
# The default value is 50MB.
#
#wholefilemaxsize=50MB

#
# Size of files below which it's NEVER partially cached, regardless of
# bandwidth. The default value is 4MB.
#
#wholefileminsize=4MB

#
# If the file's size is between wholefileminsize and wholefilemaxsize then
# wholefilemaxstall is the maximum time to wait for whole file fetch in
# seconds. This quantity depends on the bandwidth between the client and the
# server hosting the corresponding file. The default value is 10 seconds.
#
#wholefilemaxstall=10

#
# Ratio of cache files that can be handle as partially cached files in
# percentage. The default value is 1. A value of 1 means that 1% of the total
# number of cache files could be partially cached.
#
#partialcachefilesratio=1
