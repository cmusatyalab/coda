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

proc InitLocks { } {
    global Locks

    set Locks(Output) 0
}

proc Lock { what } {
    global Locks

    while { $Locks($what) == 1 } {
	tkwait variable Lock($what)
    }

    set Locks($what) 1
}

proc UnLock { what } {
    global Locks

    set Locks($what) 0
}
