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

proc SetTime { tag } {
    global TimingValues
    global TimeID

    set Timing($tag) [gettimeofday]
    set TimingValues($Timing($tag)) $tag
}

proc SetPrerecordedTime { tag time } {
    global TimingValues
    global Timing

    set TimingValues($time) $tag
    set Timing($tag) $time
}

proc MyCompare { a b } {
    set atime [lindex $a 0]
    set btime [lindex $b 0]

    if { $atime >= $btime } then {
        return 1
    } else {
        return 0
    }
}

proc OutputRawTimingValues { Type SubjectID } {
    global TimingValues
    global Timing

    set TimingFILE [open /tmp/${Type}RawTiming${SubjectID} {WRONLY CREAT TRUNC}]

    foreach name [array names Timing] {
        if { $Timing($name) <= 857964150 } then {
	    puts $TimingFILE "$name $Timing($name)"
	}
    }
    puts $TimingFILE ""
    flush $TimingFILE

    set TimingList [list ]
    foreach name [array names TimingValues] {
        lappend TimingList "$name $TimingValues($name)"
    }

    set SortedTimingList [lsort -command MyCompare $TimingList]

    for { set i 0 } {$i < [llength $SortedTimingList]} {incr i} {
        set element [lindex $SortedTimingList $i]
	if { [lindex $element 0] > 857964150 } then {
            puts $TimingFILE "[lindex $element 1] [lindex $element 0]"
	}
    }

    flush $TimingFILE
    close $TimingFILE
}

proc OutputProcessedTimingValues { Type SubjectID } {
    global Timing

    set TimingFILE [open /tmp/${Type}ProcessedTiming${SubjectID} {WRONLY CREAT TRUNC}]

    foreach name [array names Timing] {
	if { [regexp {(.*):(end$)} $name match name ending] == 1 } then {
	    if { [info exists Timing($name:begin)] == 1 } then {
		set Timing(Calculated$name) [expr $Timing($name:end)-$Timing($name:begin)]
	    }
	}
    }
    flush $TimingFILE

    if { $Type == "Tutorial" } then {
	for { set i 1 } { $i <= 11 } { incr i } { puts $TimingFILE "Introduction$i $Timing(Introduction$i)" }
	for { set i 1 } { $i <= 2 } { incr i } { puts $TimingFILE "UrgencyColors$i $Timing(UrgencyColors$i)" }
	for { set i 1 } { $i <= 3 } { incr i } { puts $TimingFILE "Network$i $Timing(Network$i)" }
	for { set i 1 } { $i <= 21 } { incr i } { puts $TimingFILE "Task$i $Timing(Task$i)" }
	for { set i 1 } { $i <= 5 } {incr i } { puts $TimingFILE "Advice$i $Timing(Advice$i)" }
	for { set i 1 } { $i <= 6 } { incr i } { puts $TimingFILE "HoardWalk$i $Timing(HoardWalk$i)" }
	for { set i 1 } { $i <= 4 } { incr i } { puts $TimingFILE "Space$i $Timing(Space$i)" }
	for { set i 1 } { $i <= 3 } { incr i } { puts $TimingFILE "Tokens$i $Timing(Tokens$i)" }
	for { set i 1 } { $i <= 5 } { incr i } { puts $TimingFILE "EventConfiguration$i $Timing(EventConfiguration$i)" }
        puts $TimingFILE "TokenExpiry $Timing(TokenExpiry)"

	foreach name [ array names Timing ] {
	    if { [regexp {^Calculated(.*)} $name match number] == 1 } then {
		puts $TimingFILE "$name $Timing($name)"
	    }
	}
	puts $TimingFILE "TotalTutorialTime $Timing(TotalTutorialTime)"
    } else {
  	foreach entry { ExercisesIntro \
		ChooseUrgencyColors \
		IdentifyCurrentNetworkConditions \
		DescribeOneOrMoreTasksUnavailable \
		FixOneOrMoreTasksUnavailable \
		WhenDoTokensExpire \
		DescribeTaskHoardingDetails \
		ProvideHoardWalkAdvice \
		DescribeSpaceStatus \
		DescribeMissEvents \
		FixMissEvents \
		ReadBlizzardBackground \
		DefineBugTask \
		DefineHeaderFilesTask \
		DefineCVTask \
		PrioritizeTasks \
		RequestImmediateHoardWalk \
		ReadIncidentBackground \
		ReadGameBackground \
		RunGame} {
	    puts $TimingFILE "$entry $Timing($entry)"
	}

	set numIncidents 0
	foreach name [array names Timing] {
	    if { [regexp {^CalculatedIncidentReport(.*)} $name match number] == 1 } then {
		incr numIncidents
	    }
	}
	for { set i 1 } { $i <= $numIncidents } { incr i } {
		if { [info exists Timing(CalculatedIncidentReport$i)] } then {
		    puts $TimingFILE "IncidentReport$i $Timing(CalculatedIncidentReport$i)"
		} else { set i [expr i - 1] }
	}
  	foreach entry { CalculatedPhaseOne CalculatedPhaseTwo CalculatedPhaseThree TotalExercisesTime } {
	    puts $TimingFILE "$entry $Timing($entry)"
	}
    }

    flush $TimingFILE
    close $TimingFILE
}

proc OutputTimingValues { Type SubjectID } {
    OutputRawTimingValues $Type $SubjectID
    OutputProcessedTimingValues $Type $SubjectID
}
