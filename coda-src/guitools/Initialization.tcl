
proc ReadAdviceConfiguration { } {
    global Pathnames
    global ControlPanel

    #
    # Format of the .advicerc file:
    #
    # Event:<stuff> where stuff is: 
    #                   <EventName>:<Urgency>:<Notify>:<Popup>:<Beep>:<Flash>
    # Color:<stuff> where stuff is:
    #                   <Normal|Warning|Critical>:<color>
    # Behavior:<stuff> where stuff is:
    #                   <data|program|task>:<on|off>
    # Lines beginning with a # are comments
    #
    MakeMilestone $Pathnames(advicerc) [nowString]
    set AdviceRC [MyRenameAndOpen $Pathnames(advicerc) {RDONLY CREAT}] 
    foreach line [split [read $AdviceRC] \n] {
	if { $line == "" } then {continue}
	if { [string index $line 0] == "#" } then {continue}

	set lineElements [split $line :]
	switch -exact [lindex $lineElements 0] {
	    Event { ParseEventConfiguration [lrange $lineElements 1 end]}
	    Color { ParseColorConfiguration [lrange $lineElements 1 end]}
	    Behavior { ParseBehaviorConfiguration [lrange $lineElements 1 end]}
	}	
    }
    close $AdviceRC

    RegisterEvents
}

proc ParseEventConfiguration { configList } {
    global Pathnames
    global Events

    set event [lindex $configList 0]
    set index [lsearch $Events(List) $event]
    if { $index == -1 } then {
        SendToStdErr "Error in $Pathnames(advicerc):  $event unrecognized event name"
        SendToStdErr "   Should be one of: $Events(List)"
    }

    switch -exact [lindex $configList 1] {
        U {set Events($event:Urgency) Unknown}
        N {set Events($event:Urgency) Normal}
        W {set Events($event:Urgency) Warning}
        C {set Events($event:Urgency) Critical}
        default {SendToStdErr "Error (.advicerc):  Urgency element of $event"}
    }

    switch -exact [lindex $configList 2] {
        0 {set Events($event:Notify) No}
        1 {set Events($event:Notify) Yes}
        default {SendToStdErr "Error (.advicerc):  Notify element of $event"}
    }

    switch -exact [lindex $configList 3] {
        0 {set Events($event:Notify:Popup) No}
        1 {set Events($event:Notify:Popup) Yes}
        default {SendToStdErr "Error (.advicerc):  Popup element of $event"}
    }

    switch -exact [lindex $configList 4] {
        0 {set Events($event:Notify:Beep) No}
        1 {set Events($event:Notify:Beep) Yes}
        default {SendToStdErr "Error (.advicerc):  Beep element of $event"}
    }

    switch -exact [lindex $configList 5] {
        0 {set Events($event:Notify:Flash) No}
        1 {set Events($event:Notify:Flash) Yes}
        default {SendToStdErr "Error (.advicerc):  Flash element of $event"}
    }
}

proc OutputEventConfiguration { outFile } {
    global Events

    puts $outFile "#\n# Event Configuration\n#";
    foreach event $Events(List) {
	puts -nonewline $outFile "Event:$event:"

	switch -exact $Events($event:Urgency) {
	    Unknown { puts -nonewline $outFile "U:"}
	    Normal { puts -nonewline $outFile "N:"}
	    Warning { puts -nonewline $outFile "W:"}
	    Critical { puts -nonewline $outFile "C:"}
        }

	foreach notification { Notify Notify:Popup Notify:Beep Notify:Flash } {
	    switch -exact $Events($event:$notification) {
		No { puts -nonewline $outFile "0" }
		Yes { puts -nonewline $outFile "1" }
	    }
	    if { $notification != "Notify:Flash" } then {
		puts -nonewline $outFile ":"
	    } else {
		puts $outFile ""
	    }
	}
    }
}

proc ParseColorConfiguration { configList } {
    global Colors

    set Colors([lindex $configList 0]) [lindex $configList 1]
}

proc OutputColorConfiguration { outFile } {
    global Colors

    puts $outFile "#\n# Color Configuration\n#";
    foreach urgency { Normal Warning Critical } {
        puts $outFile "Color:$urgency:$Colors($urgency)"
    }
}

proc ParseBehaviorConfiguration { configList } {
    global Behavior

    set Behavior(SafeDeletes:[lindex $configList 0]) [lindex $configList 1]
}

proc OutputBehaviorConfiguration { outFile } {
    global Behavior

    puts $outFile "#\n# Behavior Configuration\n#";
    foreach type { data program task } {
        puts $outFile "Behavior:$type:$Behavior(SafeDeletes:$type)"
    }
}

proc SetStatistics { fa fo ba bo ra ro } {
    global Statistics

    set Statistics(LastUpdateTime) [nowSeconds]

    set Statistics(Files:Allocated) $fa
    set Statistics(Files:Occupied) $fo

    set Statistics(Blocks:Allocated) $ba
    set Statistics(Blocks:Occupied) $bo

    set Statistics(RVM:Allocated) $ra
    set Statistics(RVM:Occupied) $ro

    CalculateDerivedStatistics

    set Statistics(Partition:Allocated) [GetPartitionAllocated \
					    $Statistics(Partition)]
    set Statistics(Partition:Occupied)  [GetPartitionOccupied \
					    $Statistics(Partition)]
    set Statistics(Partition:Usage)     [GetPartitionUsage \
					    $Statistics(Partition)]

    set Statistics(Done) 1
}

proc CalculateDerivedStatistics { } {
    global Statistics

    set Statistics(Files:Usage)	    [expr $Statistics(Files:Occupied) * 100 \
					  / $Statistics(Files:Allocated)]
    set Statistics(Blocks:Usage)     [expr $Statistics(Blocks:Occupied) * 100 \
					   / $Statistics(Blocks:Allocated)]
    set Statistics(RVM:Usage)		[expr $Statistics(RVM:Occupied) * 100 \
					      / $Statistics(RVM:Allocated)]    
}

proc InitStatistics { } {
    global Statistics

    set Statistics(LastUpdateTime) 0

    # How recent do the statistics need to be for us to rely on them? (in secs)
    set Statistics(Currency) 20

    # What are the thresholds for notifying the user? (in percent)
    set Statistics(Full) 100
    set Statistics(NearlyFull) 90
    set Statistics(Filling) 80

    # What are the units of measurement?
    set Statistics(Files:Units)	    units
    set Statistics(Blocks:Units)    kbytes
    set Statistics(Partition:Units) kbytes
    set Statistics(RVM:Units)	    bytes

    # Where is the venus.cache directory?
    set Statistics(Partition)           [GetPartition]

    CheckStatisticsCurrency $Statistics(Currency)
}

proc CheckStatisticsCurrency { currency } {
    global Statistics

    set now [nowSeconds]

    set age [expr $now - $Statistics(LastUpdateTime)]
    if { $age > $currency } then {
	set Statistics(Done) 0

#	Lock Output
#	puts "GetCacheStatistics"
#	puts ""
#	flush stdout
#	UnLock Output
	SendToAdviceMonitor "GetCacheStatistics"

	set answer [gets stdin]

	SetStatistics [lindex $answer 0] [lindex $answer 1] \
	    [lindex $answer 2] [lindex $answer 3] \
	    [lindex $answer 4] [lindex $answer 5]
    }
}

proc GetListOfServerNames { } {

	SendToAdviceMonitor "GetListOfServerNames"

	set answer [gets stdin]

	return $answer
}

proc InitServers { } {
    global Servers
    global Bandwidth

    # Ethernet bandwidth = 10 mb/sec == 1310720 B/sec
    set Bandwidth(Ethernet) 1310720

    # Weak threshold = 1/10th of maximum ethernet speed
    set Bandwidth(WeakThreshold) [expr $Bandwidth(Ethernet) / 10]

    set servers [GetListOfServerNames]
    set Max [expr [llength $servers] / 2]

    # Get output lock
    Lock Output

    # Print Initial String
    puts "GetNetworkConnectivityInformation"
    puts [format "client = %s" [exec hostname]]
    for { set i 0; set j 0 }  \
	{ $i < [llength $servers] }  \
	{ set i [expr $i + 2]; incr j } {
	    set Servers($j) [lindex $servers $i]
            set bw [lindex $servers [expr $i+1]]
            if { $bw == -1 } then { set bw $Bandwidth(Ethernet) }
	    set Bandwidth($j) $bw
	    set Bandwidth($Servers($j)) $bw

            # Send command overs to Advice Monitor, assume no filters until we hear otherwise
	    puts [format "server = %s" [lindex $servers $i]]
	    set Servers([lindex $servers $i]:Mode) "natural"

	    # Assume server is available and strongly connected
	    set Servers([lindex $servers $i]:State) "on"
            set Servers([lindex $servers $i]:Connectivity) "strong"
            
    }
    # Print final string
    puts "END"
    puts ""
    flush stdout

    # Release output lock
    UnLock Output

    set Servers(numServers) $j
}


proc InitUserDataData { } {
    global UserData
    global Pathnames

    set UserData(WindowID) -1

    set UserData(List) [list {New...}]
    set UserData(New...:Definition) [list ]

    set hoardDir $Pathnames(newHoarding)
    MakeMilestone ${hoardDir}/Data [nowString]
    set DataFILE [MyRenameAndOpen ${hoardDir}/Data {RDONLY CREAT}]

    set state lookingForTaskname
    set taskname ""
    foreach line [split [read $DataFILE] \n] {
	if { $line == "" } then {
	    set taskname ""
	    set state "lookingForTaskname"
	    continue
	} 
	if { $state == "lookingForTaskname" } then {
	    set taskname [string trim $line ": "]
	    set state "definingTask"
	    lappend UserData(List) $taskname
	    set UserData(${taskname}:Definition) [list ]
 	} else {
	    set definition [split [string trim $line]]
	    set pathname [lindex $definition 0]
	    set meta [lindex $definition 1]
	    lappend UserData(${taskname}:Definition) [format "%s:%s" $pathname $meta]
   	}
    }
    close $DataFILE

    set UserData(List) [lsort -command MyCompare $UserData(List)]
    set UserData(NewData) [list ]
}

proc InitProgramData { } {
    global Program
    global Pathnames

    set Program(WindowID) -1

    set Program(List) [list {New...}]
    set Program(New...:Definition) [list ]

    set hoardDir $Pathnames(newHoarding)
    MakeMilestone ${hoardDir}/Programs [nowString]
    set ProgramFILE [MyRenameAndOpen ${hoardDir}/Programs {RDONLY CREAT}]

    set state lookingForProgramname
    set programname ""
    foreach line [split [read $ProgramFILE] \n] {
	if { $line == "" } then {
	    set programname ""
	    set state "lookingForProgramname"
	    continue
	} 
	if { $state == "lookingForProgramname" } then {
	    set programname [string trim $line ": "]
	    set state "definingProgram"
	    lappend Program(List) $programname
	    set Program(${programname}:Definition) [list ]
 	} else {
	    lappend Program(${programname}:Definition) [string trim $line]
   	}
    }
    close $ProgramFILE

    set Program(List) [lsort -command MyCompare $Program(List)]
    set Program(NewPrograms) [list ]
}

proc InitTaskData { } {
    global Task
    global Pathnames

    set Task(WindowID) -1

    set Task(TaskList) [list {New...}]
	set Task(New...:userDataList) [list ]
	set Task(New...:programsList) [list ]
	set Task(New...:subtasksList) [list ]

    set hoardDir $Pathnames(newHoarding)
    set TaskFILE [MyRenameAndOpen ${hoardDir}/Tasks {RDONLY CREAT}]
    MakeMilestone ${hoardDir}/Tasks [nowString]
    foreach line [split [read $TaskFILE] \n] {
        if { $line == "" } then { continue }

	set task ""
	set data ""
	set programs ""
	set subtasks ""

	set elements [split $line :)] 
	set task [lindex $elements 0]
	set data [string trimleft [lindex $elements 2] (]
	set programs [string trimleft [lindex $elements 4] (]
	set subtasks [string trimleft [lindex $elements 6] (]

	lappend Task(TaskList) $task
	set Task(${task}:userDataList) [list ]
	set Task(${task}:programsList) [list ]
	set Task(${task}:subtasksList) [list ]

	foreach d [split $data ,] {
	    lappend Task(${task}:userDataList) [string trim $d]
	}
	foreach p [split $programs ,] {
	    lappend Task(${task}:programsList) [string trim $p]
	}
	foreach s [split $subtasks ,] {
	    lappend Task(${task}:subtasksList) [string trim $s]
	}
    }
    close $TaskFILE

    set Task(TaskList) [lsort -command MyCompare $Task(TaskList)]
}

proc InitSelectedTaskList { } {
    global Task
    global Pathnames

    set Task(SelectedTaskList) [list ]

    set hoardDir $Pathnames(newHoarding)
    MakeMilestone ${hoardDir}/SelectedTasks [nowString]
    set SelectedFILE [MyRenameAndOpen ${hoardDir}/SelectedTasks {RDONLY CREAT}]

    foreach line [split [read $SelectedFILE] \n] {
	if { $line == "" } then {
	    continue
	} else {
	    lappend Task(SelectedTaskList) $line
	}
    }
    close $SelectedFILE

    # Do NOT sort this list since order = priority
}

proc InitData { } {
    ControlPanelInitData
    TokensInitData
    SpaceInitData
    NetworkInitData
    AdviceInitData
    HoardWalkInitData
    RepairInitData
    ReintegrationInitData
    TaskInitData
}
