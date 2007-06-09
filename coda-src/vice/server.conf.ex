# This is an example server.conf file ... and documentation for all
# configurable options.
#
# Default values are left commented.

#
# Number of server processes running on this machine.
#
#numservers=1

#
# Prefix of server tree
#
#vicedir=/vice

#
# Should the server use private mmaps for RVM
#
#mapprivate=0

#
# RVM parameters
#
# For a single server on a machine use:
#
#rvm_log=
#rvm_data=
#rvm_data_length=
#hostname=

#
# When a server is confused about it's identity the server will complain.
# (i.e. gethostbyname(hostname()) == 127.x.x.x). Use this option to force
# the ip-address through which clients will be able to reach this machine.
#
# The actual cause seems to be a very common setup when the machine has no
# default interfaces configured and /etc/hosts contains a line similar to
# 127.0.0.1 (or .2)	myhostname.mydomain
#
# Removing that line from /etc/hosts also solves the problem.
#
#ipaddress=

#
# For multiple servers:  Define the above 4 in a file: server_X.conf
# for a server which is started with the parameter "-n X" or define the
# following replacing X with the server number.
#
#rvm_log_X=
#rvm_data_X=
#rvm_data_length_X=
#hostname_X=

# The advantage of using server_X.conf files is that other parameters
# besides the rvm parameters can be defined for each server.  All
# servers should have the same vicedir. server.conf will be read first
# and then the server_X.conf file will be read.  Any definition in
# server_X.conf will override a definition of the same variable in the
# server.conf file.  e.g.  mapprivate=1 in server.conf, mapprivate=0
# in server_2.conf will have the server started with "-n 2" use
# anonymous mapping while the "default" action is to run with private
# mappings.

# Other parameters as set by startserver script.  Defaults are commented
# out if default is different than startserver script.

#
# How full the log can get before it is truncated.
#
#rvmtruncate=0
rvmtruncate=5

#
# Specify the number of rpc2 buffers to keep in a circular log, this can be
# useful for debugging.
#
#trace=0	
trace=100

#
# Location of the vicetab file (relative to the vicedir prefix).
#
#vicetab=db/vicetab

#
# Whether the server should calculate SHA checksums, this adds CPU usage
# to GetAttrPlusSHA operations allows clients to use a local lookaside
# cache to avoid fetches.
#
allow_sha=1


#
# The number of threads a server uses to handle the authentication of
# new connections. This defaults to 5.
#
#auth_lwps=5

#
# The number of threads to handle Coda client requests. There is a
# hardcoded limit of 16 (NUM_LWPS in srv.cc) the default value is 10.
#
#lwps=10

#authenticate=1
#cbwait=240
#chk=30
#comparedirreps=1
#dumpvm=0
#forcesalvage=1
#large=500
#nodebarrenize=0
#pollandyield=1
#pathtiming=1
#resolution=1
#salvageonshutdown=0
#sendahead=8
#small=500
#stack=96
#timeout=60
#retrycnt=5
#windowsize=32
#zombify=0

#
# Kerberos 4/5 settings.
# These can also be placed in the "auth2.conf" file.
#
#kerberos4service="host"
#kerberos4realm=

# kerberos5service contains "%s" which will be substituted with a hostname,
# for a usual DCE setup it would be "hosts/%s/self
#kerberos5service="host/%s"
#kerberos5realm=
#kerberos5keytab=

