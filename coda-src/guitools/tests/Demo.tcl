# BLURB gpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public Licence Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
# 
#                         Additional copyrights
#                            none currently
# 
#*/

proc Submit { seconds command message} {
    after [expr $seconds*1000] $command
    after [expr $seconds*1000] $message
}

proc Demo { } {
    set DemoStart 0

    Submit [expr $DemoStart + 1] \
    	    HoardWalkBeginEvent \
	    {puts "A hoard walk has begun" }
    Submit [expr $DemoStart + 3] \
	    HoardWalkPendingAdviceEvent \
	    {puts "A hoard walk could use advice"}

    Submit [expr $DemoStart + 4] \
	    {NeedsRepair /coda/usr/mre/thesis/dissertation} \
	    {}
    Submit [expr $DemoStart + 5] \
	    {DisconnectedCacheMissEvent /tmp/output2 /coda/usr/mre/newbuild/src/coda-src/venus/vice.c program2} \
	    {}

    Submit [expr $DemoStart + 8] \
	    {NeedsRepair /coda/usr/mre/OBJS/@sys/coda-src/venus} \
	    {}
    Submit [expr $DemoStart + 8] \
	    {WeakMissEvent /coda/usr/mre/newbuild/src/coda-src/venus/fso0.c program5 35} \
	    {}
    Submit [expr $DemoStart + 10] \
	    TokenExpiryEvent \
	    {puts "Tokens for this user have expired"}
    Submit [expr $DemoStart + 10] \
	    {ReadDisconnectedCacheMissEvent /coda/usr/mre/newbuild/src/coda-src/advice/CodaConsole program1} \
	    {}
    Submit [expr $DemoStart + 12] \
	    {NeedsRepair /coda/usr/mre/newbuild/src/coda-src/venus} \
	    {}
    Submit [expr $DemoStart + 12] \
	    {DisconnectedCacheMissEvent /tmp/output4 /coda/usr/mre/thesis/dissertation/thesis.ps program4} \
	    {}
    Submit [expr $DemoStart + 15] \
	    {WeakMissEvent /coda/usr/mre/thesis/dissertation/introduction.tex program7 17} \
	    {}
    Submit [expr $DemoStart + 16] \
	    {NeedsRepair /coda/usr/mre/newbuild/src/coda-src/advice} \
	    {}
    Submit [expr $DemoStart + 17] \
	    {ActivityPendingTokensEvent reintegrate /coda/usr/mre/OBJS} \
	    {puts "Reintegration pending tokens"}
    Submit [expr $DemoStart + 20] \
	    {ReadDisconnectedCacheMissEvent /coda/usr/mre/thesis/dissertation/FIGS program3} \
	    {}
    Submit [expr $DemoStart + 25] \
	    {ActivityPendingTokensEvent reintegrate /coda/usr/mre} \
	    {puts "Reintegration pending tokens"}
    Submit [expr $DemoStart + 25] \
	    {WeakMissEvent /coda/usr/mre/thesis/dissertation/conclusions.tex program6 6} \
	    {}
    Submit [expr $DemoStart + 30] \
	    TokensAcquiredEvent \
	    {puts "Tokens for this user have been obtained"}
}

