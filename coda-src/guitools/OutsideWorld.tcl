##############################################################################
#
#
#  Routines interacting with Venus or the Operating System
#
#
##############################################################################

proc MakeMilestone { filename id } {
#    exec cp $filename ${filename}.milestone_${id}
}

proc MyRenameAndOpen { filename accessFlags} {

    if { ([lsearch -exact $accessFlags "TRUNC"] >= 0) ||
         ([lsearch -exact $accessFlags "WRONLY"] >= 0) } then {
	if { [file exists $filename] } then {
	    exec cp $filename ${filename}.old
	}
    }
    return [open $filename $accessFlags]
}

proc GetEnv { variable } {
    global env

    return $env($variable)
}

proc GetCacheUsage { } {
    global Statistics

    CheckStatisticsCurrency $Statistics(Currency)
    set usage [expr $Statistics(Blocks:Occupied) * 100 / $Statistics(Blocks:Allocated)]
    return $usage
}

proc GetPartition { } {
    global Pathnames

    set cwd [pwd]
    cd $Pathnames(venus.cache)
    set partition [pwd]
    cd $cwd
    set partition [join [list {} [lindex [split $partition /] 1]] /]
    return $partition
}

    # Note that Allocated Partition size is 90% of its raw size...
    # The other 10% is "allocated" for Unix internal structures
proc GetPartitionAllocated { partition } {
    set two {{printf $2 " "}}
    return [expr [lindex [exec df $partition | awk $two] 1] * 0.9]
}

proc GetPartitionOccupied { partition } {
    set three {{printf $3 " "}}
    return [lindex [exec df $partition | awk $three] 1]
}

proc GetPartitionUsage { partition } {
    set five {{print $5}}
    return [string trimright [lindex [exec df $partition | awk $five] 1] %]
}

#proc GetRVMUsage { } {
#    global Statistics
#    global VENUSLOG

#    CheckStatisticsCurrency $Statistics(Currency)

#    set Statistics(RVMBytesAllocated) \
#	[expr $Statistics(RVMfreebytes) + $Statistics(RVMmallocdbytes)]
#    set usage \
#	[expr $Statistics(RVMmallocdbytes) * 100 / $Statistics(RVMBytesAllocated)]
#    return $usage
#}

# GetBandwidthEstimate returns the bandwidth estimate of a server as a percentage of
proc GetBandwidthEstimate { server } {
    global Bandwidth

    if { [array names Bandwidth $server] == "" } then {
	set Bandwidth($server) -1
    }
    set percentage [expr $Bandwidth($server) * 100 / $Bandwidth(Ethernet)]
    if { $percentage > 100 } then { return 100 } 
    if { ($percentage == 0) && ($Bandwidth($server) != 0) } then { return 1 }
    return $percentage 
}

proc GetNextHoardWalk { } {
    global HoardWalk

    puts stderr "MARIA:  Implement GetNextHoardWalk"
    if { $HoardWalk(State) == "Unknown" } then {
        return -1
    } else {
       return 7
    }
}

proc GetHoardWalkProgress { } {
    global HoardWalk

    return $HoardWalk(Progress)
}

proc InitTaskAvailability { } {
    puts stderr "MARIA:  Write InitTaskAvailability"
}

proc GetTaskAvailability { task } {
    global TaskAvailability

    if { [info exists TaskAvailability($task)] == 1 } then {
        return $TaskAvailability($task)
    } else {
        return -1
    }
}

proc SetTaskAvailability { task availability } {
    global TaskAvailability
 
    set TaskAvailability($task) $availability
}

proc SetTaskMaxSpace { task space_inMB space_inKB } {
    global TaskSpace

    set TaskSpace(MB:$task) $space_inMB
    set TaskSpace(KB:$task) $space_inKB
}

proc GetTaskCurrentSizeKB { task } {
    global TaskSpace

    if { [info exists TaskSpace(Current:$task)] == 1 } then {
	return $TaskSpace(Current:$task)
    } else {
	puts stderr "GetTaskCurrentSizeKB returns bogosity"
    }
}

proc SetTaskCurrentSizeKB { task size_inKB } {
    global TaskSpace

    set TaskSpace(Current:$task) $size_inKB
}

proc GetTaskMaxSpaceMB { task } {
    global TaskSpace

    if { [info exists TaskSpace(MB:$task)] == 1 } then {
	return $TaskSpace(MB:$task)
    } else {
	return -1
    }
}

proc GetTaskMaxSpaceKB { task } {
    global TaskSpace

    if { [info exists TaskSpace(KB:$task)] == 1 } then {
	return $TaskSpace(KB:$task)
    } else {
	return -1
    }
}


proc SendToAdviceMonitor { message } {

    Lock Output

    puts $message
    flush stdout

    UnLock Output
}