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

proc InitLog { } {
    global Log

    set Log(State) "off"
}

proc BeginLogging { SubjectID } {
    global Log

    set Log(File) [open /tmp/ActionLog.${SubjectID} {WRONLY CREAT TRUNC}]
    puts $Log(File) "Begin Logging at [gettimeofday]"
    set Log(State) "on"
}

proc EndLogging { } {
    global Log

    set Log(State) "off"
    puts $Log(File) "End Logging at [gettimeofday]"
    flush $Log(File)
    close $Log(File)
}

proc LogActionHeader { headerargs } {
    global Log

    if { $Log(State) == "on" } {
        puts $Log(File) ""
        for { set i 0 } { $i < [llength $headerargs] } { incr i } {
            puts -nonewline $Log(File) [lindex $headerargs $i]
	}
	puts $Log(File) ""
	flush $Log(File)
    }
}

proc LogAction { action } {
    global Log

    if { $Log(State) == "on" } {
        puts $Log(File) "    $action"
	flush $Log(File)
    }
}

