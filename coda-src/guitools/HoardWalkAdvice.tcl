#
# Inform the user that a request for hoard walk advice has arrived.
#
# History:
#  97/11/03: mre@cs.cmu.edu : moved out of HoardWalk.tcl
#

proc HoardWalkAdviceInit { } {
    global HWA

    set HWA(MainWindow) .hoardwalkadvice
    set HWA(Geometry) "675x475"    
    set HWA(MaxCost) 999

    InitHWAinput
    InitHWATree
}


proc InitHWAinput { } {
    global HWAinput

    if { [info exists HWAinput] } then {
	unset HWAinput
    }
}

proc InitHWATree { } {
    global HWATree

    if { [info exists HWATree] } then {
	unset HWATree
    }
}



proc HoardWalkAdvice { } {
    global HoardWalk
    global HWA

    if { [winfo exists $HWA(MainWindow)] } then {
	raise $HWA(MainWindow)
   	return 
    }

    toplevel $HWA(MainWindow)
    wm title $HWA(MainWindow) "Hoard Walk Advice"
    wm geometry $HWA(MainWindow) $HWA(Geometry)
    wm minsize $HWA(MainWindow) 675 475

    wm protocol $HWA(MainWindow) WM_DELETE_WINDOW {HWAComplete}

    MkHWAWindow $HoardWalk(AdviceInput)

    # Now wait for the user to respond...
    tkwait variable HWA(completed)

    destroy $HWA(MainWindow)
    return 0
}

proc HWAComplete { } {
    global HWA

    puts stderr "MARIA:  Implement HWAComplete"
    flush stderr

    set HWA(Geometry) [GeometryLocationOnly [wm geometry $HWA(MainWindow)]]
    destroy $HWA(MainWindow)
}


#
# This section of the sources contain routines for reading the input
# file produced by Venus
#

proc HWAProcessHeaderLines { inFile } {
    global HWAinput
    global Statistics

    # Read, process, and sanity check first line
    gets $inFile thisLine
    scan $thisLine "Cache Space Allocated: %d files (%d blocks)" \
        HWAinput(FilesAllocated) HWAinput(BlocksAllocated)
    if { ($HWAinput(FilesAllocated) != $Statistics(Files:Allocated)) ||
         ($HWAinput(BlocksAllocated) != $Statistics(Blocks:Allocated)) } then {
        puts stderr "Error:  File and/or Block allocation mismatch ==>"
        puts stderr [format "\tHoard input: Files = %d Blocks = %d" \
                 $HWAinput(FilesAllocated) $HWAinput(BlocksAllocated)]
        puts stderr [format "\tStatistics: Files = %d Blocks = %d" \
                 $Statistics(Files:Allocated) $Statistics(Blocks:Allocated)]
    }

    # Read and process second line
    gets $inFile thisLine
    scan $thisLine "Cache Space Occupied: %d files (%d blocks)" \
        HWAinput(FilesOccupied) HWAinput(BlocksOccupied)

    # Read and process third line
    gets $inFile thisLine
    scan $thisLine "Speed of Network Connection = %s" HWAinput(Bandwidth)

    set HWAinput(FilesAvailable) \
            [expr $HWAinput(FilesAllocated)-$HWAinput(FilesOccupied)]
    set HWAinput(BlocksAvailable) \
            [expr $HWAinput(BlocksAllocated)-$HWAinput(BlocksOccupied)]
    set HWAinput(FilesOccupiedAfter) $HWAinput(FilesOccupied)
    set HWAinput(BlocksOccupiedAfter) $HWAinput(BlocksOccupied)
    set HWAinput(FilesAvailableAfter) $HWAinput(FilesAvailable)
    set HWAinput(BlocksAvailableAfter) $HWAinput(BlocksAvailable)
}

proc HWAProcessAdviceLine { thisLine } {
    global HWAinput
    global HWA

    set theseFields [split $thisLine "&"]
    set askUser [string trim [lindex $theseFields 1]]
#    if { $askUser == 0 } {
	incr HWAinput(numLines)
	set HWAinput(FID:$HWAinput(numLines)) \
	    [string trim [lindex $theseFields 0]]
## ADDED:
        set HWAinput(FetchDefault:$HWAinput(numLines)) $askUser

	set HWAinput(Filename:$HWAinput(numLines)) \
	    [string trim [lindex $theseFields 2]]

	set HWAinput(Priority:$HWAinput(numLines)) \
	    [string trim [lindex $theseFields 3]]

	set HWAinput(Selected:$HWAinput(numLines)) 0

	set HWAinput(StopAsking:$HWAinput(numLines)) 0

	set HWAinput(Fetch:$HWAinput(numLines)) 0

	set HWAinput(Cost:$HWAinput(numLines)) \
	    [string trim [lindex $theseFields 4]]
	if { $HWAinput(Cost:$HWAinput(numLines)) == "??" } {
	    set HWAinput(Cost:$HWAinput(numLines)) $HWA(MaxCost)
	}

	set HWAinput(BlockDiff:$HWAinput(numLines)) \
	    [string trim [lindex $theseFields 5]]
	
	set len [string length $HWAinput(Filename:$HWAinput(numLines))]
	if {$len > $HWAinput(maxLength)} {
	    set HWAinput(maxLength) $len
	}
#    } elseif { $askUser == 1 } {
#	incr HWAinput(NumDefaults)
#	set cost [string trim [lindex $theseFields 4]]
#	if { $cost == "??" } {
#	    set HWAinput(TotalExpectedFetchTime) \
#		[expr $HWAinput(TotalExpectedFetchTime) + $HWA(MaxCost)]
#	} else {
#	    set HWAinput(TotalExpectedFetchTime) \
#		[expr $HWAinput(TotalExpectedFetchTime) + $cost]
#	}
#	incr HWAinput(TotalNumberOfObjects)
#    } elseif { $askUser == -1 } {
#	puts stderr [format "ERROR:  Cannot handle askUser value of -1"]
#    }
}


proc HWAProcessInput { inputFile } {
    global HWAinput
    global Statistics

    InitHWAinput

    set inFile [MyRenameAndOpen $inputFile {RDONLY}]
    set HWAinput(NumDefaults) 0
    set HWAinput(TotalNumberOfObjects) 0
    set HWAinput(TotalExpectedFetchTime) 0

    HWAProcessHeaderLines $inFile

    set HWAinput(maxLength) 0
    set HWAinput(numLines) 0
    while {[gets $inFile thisLine] >= 0} {
	HWAProcessAdviceLine $thisLine 
    }
    close $inFile
    return $HWAinput(numLines)
}


#
# This section of the sources contain routines for creating the 
# Hoard Walk Advice window
#

proc MkHWAWindow { inputFile } {
    global HWA
    global HWAinput
    global HWATree

    global Colors
    global Dimensions
    global DisplayStyle

    global Pathnames
    global Bandwidth
    global Statistics
    global Servers
    global Events

    InitHWATree

    set HWA(HoardAdviceLines) [HWAProcessInput $inputFile]
    if { $HWA(HoardAdviceLines) == 0 } then { return }
    set lines $HWA(HoardAdviceLines)

    set HWA(PendingAdvice) $HWA(MainWindow).advice
    frame $HWA(PendingAdvice) -relief flat
    pack $HWA(PendingAdvice) -side top -expand true -fill both

    # Create the BEGINNING summary information
    frame $HWA(PendingAdvice).top -relief groove -borderwidth 2
    pack $HWA(PendingAdvice).top -side top

    set topleft $HWA(PendingAdvice).top.left 
    frame $topleft -relief flat -borderwidth 2
    pack $topleft -side left

    tixLabelFrame $topleft.cache -label "Cache Status:"
    pack $topleft.cache -side top
    set cacheframe [$topleft.cache subwidget frame]

    SpaceMeter $cacheframe.files \
	"Files:" \
	$Statistics(Files:Allocated) \
	[expr $Dimensions(Meter:Width) / 2] \
	$Statistics(Files:Allocated) \
	$Statistics(Files:Allocated) \
	[expr $HWAinput(FilesOccupied) * 100 / $Statistics(Files:Allocated)] 
    SpaceMeter $cacheframe.blocks \
	"Space:\n(in MB)" \
	[format "%d" [expr $Statistics(Blocks:Allocated) / 1024]]\
	[expr $Dimensions(Meter:Width) / 2] \
	$Statistics(Blocks:Allocated) \
	$Statistics(Blocks:Allocated) \
	[GetCacheUsage]

    pack $cacheframe.files -side top -anchor w
    pack $cacheframe.blocks -side top -anchor w


    set topright $HWA(PendingAdvice).top.right
    frame $topright -relief flat -borderwidth 2
    pack $topright -side right

    tixLabelFrame $topright.network -label "Network Status:"
    pack $topright.network -side top
    set networkframe [$topright.network subwidget frame]
    set totalbw 0
    set ConnectionUnknown 0
    set Disconnected 0
    set WeaklyConnected 0
    for { set i 0 } {$i < $Servers(numServers)} {incr i} {
	set s $Servers($i)
	set bw [GetBandwidthEstimate $s]
	set totalbw [expr $totalbw + $bw]
	set serverstate [ServerDetermineState $s]
	if { $serverstate == "OperatingDisconnected" } then {
	    set Disconnected 1
	} elseif { $serverstate == "OperatingWeaklyConnected" } then {
	    set WeaklyConnected 1
	} elseif { $serverstate == "Unknown" } then {
	    set ConnectionUnknown
	}
    }
    if { $ConnectionUnknown } then {
	set color $Colors(Unknown)
    } elseif { $Disconnected } then {
	set color $Colors($Events(OperatingDisconnected:Urgency))
    } elseif { $WeaklyConnected } then {
	set color $Colors($Events(OperatingWeaklyConnected:Urgency))
    } else {
	set color $Colors($Events(OperatingStronglyConnected:Urgency))
    }
    set bwavg [expr $totalbw / $Servers(numServers)]
    tixCodaMeter $networkframe.speed \
	-height $Dimensions(Meter:Height) \
	-width [expr $Dimensions(Meter:Width) / 2] \
	-labelWidth 15 \
	-label "Bandwidth:" \
	-rightLabel "10 Mb/s" \
	-state normal \
	-percentFull $bwavg \
	-foreground $color
    pack $networkframe.speed -side top

    tixLabelFrame $topright.stats -label "Fetch Statistics:"
    pack $topright.stats -side top
    set statsframe [$topright.stats subwidget frame]

    set HWATree(TotalExpectedFetchTime) $HWAinput(TotalExpectedFetchTime) 
    updateFetchTime
    timewatch $statsframe.time "Expected Time (hh:mm:ss)" 
    pack $statsframe.time -side top

    set HWATree(TotalNumberOfObjects) $HWAinput(TotalNumberOfObjects)
    watch $statsframe.numobjs "Number of Objects" \
	HWATree(TotalNumberOfObjects)
    pack $statsframe.numobjs -side top

    # Create the MIDDLE listing
    frame $HWA(PendingAdvice).middle -relief groove -borderwidth 2 
    pack $HWA(PendingAdvice).middle -side top -expand true -fill both
    
    tixTree $HWA(PendingAdvice).middle.list -options {
	hlist.separator :
	hlist.background gray92
	hlist.highlightcolor purple
	hlist.selectbackground red
	hlist.columns 5
	hlist.drawBranch false
	hlist.header true
	hlist.ident 18
	hlist.width 515
    }
    pack $HWA(PendingAdvice).middle.list -side bottom -expand true -fill both
    
    set HWA(PendingAdvice:List) \
	[$HWA(PendingAdvice).middle.list subwidget hlist]
    
    # Create the title for the HList widget
    #  Requires that (1) the hlist.header option is set to true
    #            and (2) the hlist.columns option is set to 1 more than the # of columns
    # The hlist has one extra column so that the header doesn't look chopped off if the 
    #  user expands the window size.  We set it up so that the final column has 0 width 
    #  so that it is not shown unless the user has expanded the window.
    # We configure the hlist's first column to have plenty of space for reasonable 
    # length filenames.
    $HWA(PendingAdvice:List) header create 0 \
	-itemtype text \
	-text "Task Name" \
	-style $DisplayStyle(Header)
    $HWA(PendingAdvice:List) header create 1 \
	-itemtype text \
	-text "   Cost\n(seconds)" \
	-style $DisplayStyle(Header)
    $HWA(PendingAdvice:List) header create 2 \
	-itemtype text \
	-text "Fetch?" \
	-style $DisplayStyle(Header)
    $HWA(PendingAdvice:List) header create 3 \
	-itemtype text \
	-text "  Stop\nAsking?" \
	-style $DisplayStyle(Header)
#    Originally, the first column was 5" wide
#    $HWA(PendingAdvice:List) column width 0 "5i"
#    Now, we're going to try to let it expand to fit the widest pathname
    $HWA(PendingAdvice:List) column width 0 {}
    $HWA(PendingAdvice:List) column width 1 "1i"
    $HWA(PendingAdvice:List) column width 2 "1i"
    $HWA(PendingAdvice:List) column width 3 "1i"
    $HWA(PendingAdvice:List) column width 4 0
    
    HWAInsertRequests
    HWASetDefaultFetches

    $HWA(PendingAdvice).middle.list autosetmode
    
    # Create the BOTTOM buttons
    frame $HWA(PendingAdvice).buttons -relief flat
    pack $HWA(PendingAdvice).buttons -side bottom -fill x
    
    button $HWA(PendingAdvice).buttons.walk \
	-text "Finish Hoard Walk" \
	-relief groove -bd 6 \
	-command { HoardWalkNow }
    button $HWA(PendingAdvice).buttons.help -text "Help" \
	-relief groove -bd 2 -command { HoardWalkAdviceHelpWindow }
    
    pack $HWA(PendingAdvice).buttons.walk -side right -padx 4 -pady 4
    pack $HWA(PendingAdvice).buttons.help -side left -padx 4 -pady 4
}


proc HWASetDefaultFetches { } {
    global HWAinput
    global HWATree

    foreach inputLine [array names HWAinput FetchDefault*] {
	set lineNumber [string range $inputLine 13 end]

	if { $HWAinput(FetchDefault:$lineNumber) == 1 } then {
	    set ID $HWATree([lindex $HWAinput(MatchList:$lineNumber) 0]\\$HWAinput(Filename:$lineNumber))
	    set HWATree(Fetch$ID) 1
	    HoardWalkFetchListElement $ID
	}
    }
}



proc HWAMungeHoardListWithTaskDefinitions { } {
    global HWA
    global HWAinput

    for { set i 1} {$i <= $HWA(HoardAdviceLines)} {incr i} {
	set HWAinput(MatchList:$i) \
	    [HWAFindMatches $HWAinput(Filename:$i) $HWAinput(Cost:$i)]
    }
}




#
# The purpose of this routine is to get the Tree identifier (e.g. :, :1, or :1:1)
# associated with a path.  This association is made by HWAInsertTaskPathElement
#
proc HWATreeGetIDTaskPath { path } {
    global HWATree

    if { [info exists HWATree($path)] } then {
	return $HWATree(${path})
    } else {
	return ":"
    }
}



#
# The purpose of this routine is to ensure that element is inserted in the
# Tree of the HoardWalkAdvice window under the taskRoot.  This routine
# assumes that the components of taskRoot have already been inserted!
#
proc HWAInsertTaskPathElement { element taskRoot cost numObjects matchList } {
    global HWATree

    set NULL [list ]

    set RootID [HWATreeGetIDTaskPath $taskRoot]

    # Look to see if element is already inserted under this root
    if { [info exists HWATree(List$RootID)] } then {
	if { [lsearch -exact $HWATree(List$RootID) $element] >= 0 } then {
	    return
	}
    }

    # If not, insert this puppy
    lappend HWATree(List$RootID) $element
    if { ![info exists HWATree(Count$RootID)] } then {
	set HWATree(Count$RootID) 0
    }
    set thisID $RootID:$HWATree(Count$RootID)

    if { $taskRoot == "" } then {
	set HWATree($element) $thisID
    } else {
	set HWATree($taskRoot\\$element) $thisID
    }

    #
    # Determine whether or not we should hide this element initially.
    # We initially hide all but the top-level task definitions.  We
    # can figure out if we've got a top-level task definition because 
    # the last : that appears in the ID will be in the second positio
    # of the string (e.g. ::1, ::2, ::3 are all top-level task definitions,
    # but ::1:1 is not).  (Of course, we do not hide the root of the 
    # tree: "All Tasks Needing Data".)
    #
    if { [string last : $thisID] == 1 } then {
	set hideMe 0
    } else {
	set hideMe 1
    }

    # Initialize Tree data
    set HWATree(Cost$thisID) $cost
    set HWATree(Objects$thisID) $numObjects
    set HWATree(Match$thisID) $matchList

    HoardWalkAddListElement frame:$thisID $thisID $element 0 \
	HWATree(Cost$thisID) HWATree(Objects$thisID) \
	HWATree(Fetch$thisID) HWATree(StopAsking$thisID)\
	$HWATree(Match$thisID) $hideMe

    set HWATree($thisID) "$element"

    # Increment the counter for this node
    incr HWATree(Count$RootID)
}


#
# This routine inserts an object into the Tree after adding the object's
# pathname to the task paths in the match list.
#
proc HWAInsertObject { objectPath taskPath cost matchList } {

    set newMatchList [list ]

    set length [llength $matchList]
    for { set i 0 } { $i < $length } { incr i } {
	set piece [lindex $matchList $i]
	lappend newMatchList $piece\\$objectPath
    }
    HWAInsertTaskPathElement $objectPath $taskPath $cost 1 $newMatchList
}



#
# The purpose of this routine is to insert each component on the
# taskPath into the Tree of the HoardWalkAdvice window.  To do this,
# I identify each component and its task root.  For example, if the
# taskPath is "a\b\c\d", then this routine will call HWAInsertTaskPathElement
# four times.  Once with component a and root "", once with component b
# and root a, once with component c and root a\b, and once with component
# d and root a\b\c.  For each successive call, the level is incremented by 1.
#
proc HWAInsertTaskPath { taskPath cost matchList } {
    global HoardList
    global HWATree

    set tPath $taskPath
    # pPtr = index of prefix of the nextComponent
    set pPtr -1
    while { $tPath != "" } {

	set firstBackSlash [string first \\ $tPath]
	if { $firstBackSlash == -1 } then {
	    set nextComponent $tPath
	    set tPath ""
	} else {
	    set nextComponent [string range $tPath 0 [expr $firstBackSlash - 1]]
	    set tPath [string range $tPath [expr $firstBackSlash +1] end]
	}
	set taskRoot [string range $taskPath 0 [expr $pPtr - 1]]

	# Setup this nodes matchlist
	set thisMatchList [list ]
	foreach element $matchList {
	    # Compare the end of our element with the end of this element
	    if { $tPath == "" } then {
		set last [string last $nextComponent $element]
	    } else {
		set last [string last $nextComponent\\$tPath $element]
	    }

	    if { $last >= 0 } then {
		set thisMatch "[string range $element 0 [expr $last - 1]]$nextComponent"
		
		lappend thisMatchList $thisMatch
	    }
	}
	
	HWAInsertTaskPathElement $nextComponent $taskRoot $cost -1 $thisMatchList

	set pPtr [expr $firstBackSlash+$pPtr+1]
    }

}

#
#  We should probably add a Tree element, called "Miscellaneous"
#  or something, that matches anything.  It could then collect any
#  objects that Venus is requesting advice about but which the
#  interface wasn't responsible for hoarding.  (For example, if the
#  user issues a hoard command outside the interface.)  For now, 
#  we ignore this case.  (Sorry!)
#
proc HWAFindMatches { path cost } {
    global HoardList
    global HWAinput
    global HWATree

    # Build a list of matches
    set matchList [list ]
    for { set i 0 } { $i < $HoardList(linenum) } { incr i } {
	if { $HoardList(Meta$i) == "" } then {
	    if { [string compare $path $HoardList(Path$i)] == 0 } then {
		lappend matchList $HoardList(TaskPath$i)
	    }
	} elseif { $HoardList(Meta$i) == "c" } then {
	    if { [string first $HoardList(Path$i) $path] == 0 } then {

		set finalStart [expr [string length $HoardList(Path$i)] + 1]
		set finalComponents [string range $path $finalStart end]

		set firstSlash [string first "/" $finalComponents]
		if { $firstSlash == -1 } then {
		    lappend matchList $HoardList(TaskPath$i)
		}
		if { $firstSlash == [expr [string length $finalComponents] - 1] } then {
		    lappend matchList $HoardList(TaskPath$i)
		}
	    }
	} elseif { $HoardList(Meta$i) == "d" } then {
	    if { [string first $HoardList(Path$i) $path] == 0 } then {
		lappend matchList $HoardList(TaskPath$i)
	    }
	}
    }

    #
    # Insert the objects in the match list into the Tree
    # The insertions occur here rather than above so that the match list
    # is complete.
    #
    foreach element $matchList {
	HWAInsertTaskPath $element -1 $matchList
	HWAInsertObject $path $element $cost $matchList
    }

    return $matchList
}



#
# This routine insert each object for which Venus has requested advice
# into the Tree structured by the task definitions.
#
proc HWAInsertRequests { } {
    global HWA
    global HWATree

    set NULL [list ]

    # The empty spaces at the end of the task name below make the minimum width
    # of first column.  The problem is that, although I can setup the width of
    # the first column to be as wide as the widest entry, I cannot specify the
    # minimum width.  The spaces allow me to make the column approximately 5".
    set AllTasksName "ALL Tasks Needing Data                                                                                 "


    # Insert the top-level entry
    set HWATree(List:) [list ]
    set HWATree(Match:) [list ]
    HoardWalkAddListElement frame: : $AllTasksName \
	0 HWATree(Cost:) HWATree(Objects:) \
	HWATree(Fetch:) HWATree(StopAsking:) $NULL 0


    HWAMungeHoardListWithTaskDefinitions

    HWACalculateNodeCosts :
    HWACalculateNodeObjectCounts :
    HWAFixCounts

    # Change Match Lists from being path-based to being ID-based
    foreach element [array names HWATree Match*] {
	set newMatchList [list ]
	set length [llength $HWATree($element)]
	for { set i 0 } { $i < $length } { incr i } {
	    set piece [lindex $HWATree($element) $i]
	    lappend newMatchList $HWATree($piece)
	}
	set HWATree($element) $newMatchList
    }
}

proc HWACalculateTotalExpectedFetchTime { } {
    global HWATree

    set totalExpectedFetchTime 0
    set totalObjects 0
    foreach element [array names HWATree :*] {
	if { [HoardWalkIsLeaf $element] } then {
	    if { $HWATree(Fetch$element) == 1 } then {
		set tmp [lindex $HWATree(Match$element) 0]
		if { [string compare $element $tmp] == 0 } then {
		    incr totalExpectedFetchTime $HWATree(Cost$element)
		    incr totalObjects
		}
	    }
	}
    }
    set HWATree(TotalExpectedFetchTime) $totalExpectedFetchTime
    set HWATree(TotalNumberOfObjects) $totalObjects
    updateFetchTime
}

# 
# This routine uses a naive approach for determining the cost of 
# the nodes in the Tree, given the costs of the leaves.
#
proc HWACalculateNodeCosts { nodeID } {
    global HWATree

    if { ![info exists HWATree(Count$nodeID)] } then {
	return
    }

    set HWATree(Cost$nodeID) 0
    for { set i 0 } { $i < $HWATree(Count$nodeID) } { incr i } {
	set thisID $nodeID:$i
	HWACalculateNodeCosts $thisID
	incr HWATree(Cost$nodeID) $HWATree(Cost$thisID)
    }

}

#
# This routine uses a naive approach for determining the number of 
# objects associated with each node in the Tree.
#
proc HWACalculateNodeObjectCounts { nodeID } {
    global HWATree

    if { [info exists HWATree(Objects$nodeID)] } then {
	if { $HWATree(Objects$nodeID) != -1 } then {
	    return
	}
    }

    set HWATree(Objects$nodeID) 0
    for { set i 0 } { $i < $HWATree(Count$nodeID) } { incr i } {
	set thisID $nodeID:$i
	HWACalculateNodeObjectCounts $thisID
	incr HWATree(Objects$nodeID) $HWATree(Objects$thisID)
    }
}


#
# This routine accounts for duplicates in the Cost and Objects counts.
#
proc HWAFixCounts { } {
    global HWATree

    # Make a list of match lists that need to be fixed.  Make sure that
    # we only insert each match list once.
    set fixList [list ]
    foreach nodeID [array names HWATree :*] {
	if { [HoardWalkIsLeaf $nodeID] } then {
	    if { [llength $HWATree(Match$nodeID)] > 1 } then {
		if { [lsearch -exact $fixList $HWATree(Match$nodeID)] == -1 } then {
		    lappend fixList $HWATree(Match$nodeID)
		}
	    }
	}
    }

    # For each unique match in the fixlist, fix the counts of the common ancestors.
    foreach match $fixList { 
	set testList $match
	set i 0
	foreach element $match {
	    incr i
	    set commonAncestors [FindCommonAncestors $element [lrange $match $i end]]

	    # Fix commonAncestors counts
	    set cost $HWATree(Cost$HWATree($element))
	    set objects $HWATree(Objects$HWATree($element))
	    for { set i 0 } { $i < [llength $commonAncestors] } {incr i } {
		set thisAncestor [lindex $commonAncestors $i]
		set HWATree(Cost$thisAncestor) [expr $HWATree(Cost$thisAncestor) - $cost]
		set HWATree(Objects$thisAncestor) \
		                         [expr $HWATree(Objects$thisAncestor) - $objects]
	    }
	}
    }
}


#
# This routine returns the minimum of two integers 
#
proc MIN { x y } {
    if { $x < $y } then {
	return $x
    } else {
	return $y 
    }
}


#
# This routine returns the index at which str1 differs from str2
#
proc StringPiecewiseCompare { str1 str2 } {

    set lengthStr1 [string length $str1]
    set lengthStr2 [string length $str2]
    set minLength [MIN $lengthStr1 $lengthStr2]

    if { $minLength == 0 } then { puts stderr "zero-length string"; return -1 }

    for { set i 0 } { $i < $minLength } { incr i } {
	if { [string index $str1 $i] == [string index $str2 $i] } then { 
	    continue 
	} else {
	    return $i 
	}
    }
    return 0
    
}


#
# This routine looks at an element and a list and returns a list of
# the common ancestors between the element and each entry in the list.
# The list that gets returned should have the same number of elements
# as the list that was sent in.
#
proc FindCommonAncestors { element list } {
    global HWATree

    if { $list == "" } then { return }

    set commonAncestors [list ]

    foreach listElement $list {
	set common [StringPiecewiseCompare $element $listElement] 

	if { ($common == 2) && ([string match ::* $element] == 1) } then {
	    lappend commonAncestors :
	} elseif { $common != 0 } then {
	    set endOfAncestor [expr $common - 1]
	    set realEndOfAncestor \
		       [string last \\ [string range $element 0 $endOfAncestor]]
	    set ancestor [string range $element 0 [expr $realEndOfAncestor - 1]]

	    lappend commonAncestors $HWATree($ancestor)
			  
	} else {
	    lappend commonAncestors :
	}
    }

    return $commonAncestors
}


#
# This routine determines whether or not a given ID is a leaf or a node.
# It does so by searching for children of the node in the HWATree array.
#
proc HoardWalkIsLeaf { node } {
    global HWATree

    set pattern ${node}:*
    set children [array names HWATree $pattern]
    if { [llength $children] == 0 } then { return 1 } else { return 0 }
}

#
# This routine gets the parent of an element by lopping off everything
# after the last colon.
#
proc HWAGetParentID { index } {
    set lastColon [string last : $index]
    set ID [string range $index 0 [expr $lastColon - 1]]
    return $ID
}


#
# This routine gets the children of an element
#
proc HWAGetChildrenList { node } {
    global HWATree

    set children [list ]

    set nodeLength [string length $node]
    foreach descendant [array names HWATree $node:*] {

	set descendantTail [string range $descendant [expr $nodeLength+1] end]
	set firstColon [string first : $descendantTail]

	if { $firstColon == -1 } then {  # If there are no more colons, its a child
	    lappend children $descendant
	}
    }
    return $children
}

proc HWAGetDescendantsList { node } {
    global HWATree

    set descendants [list ]

    foreach descendant [ array names HWATree $node:*] {
	lappend descendants $descendant
    }
    return $descendants
}

#
#
#
proc HWAGetSiblingList { index } {
    global HWATree

    set parent [HWAGetParentID $index]
    if { $parent == "" } then { return [list ] }

    set childrenList [HWAGetChildrenList $parent]
    set me [lsearch -exact $childrenList $index]
    if { $me == -1 } then { 
	puts stderr "ERROR:  HWAGetSiblingList: childrenList missing me"
	exit
    }
    set siblingList [lreplace $childrenList $me $me]
    return $siblingList
}

#
# A peer is defined as an element of a node's matchlist.
# The peerList returned does NOT include this node.
#
proc HWAGetPeerList { index } {
    global HWATree

    set matchList $HWATree(Match$index)
    if { [llength $matchList] == 0 } then { return [list ] }

    set me [lsearch -exact $matchList $index]
    if { $me == -1 } then {
	puts stderr "ERROR:  HWAGetPeerList: matchList missing me"
	exit
    }
    set peerList [lreplace $matchList $me $me]
    return $peerList
}

#
#
#  These next seven routines handle the click state.
#
#  The first two propagate virtual clicks and unclicks around the
#  nodes of the tree.
#
#  The third one unclicks the descendants of a node that has recently
#  been clicked.
#
#  The fourth one checks a nodes ancestors to see if any have been clicked.
#
#  The fifth, sixth and seventh check that we have not violated our invariants:
#      INVARIANT #1:  If a node is clicked, none of its ancestors are clicked.
#      INVARIANT #2:  If a node is clicked, it is also fetched.
#      INVARIANT #3:  If a node is clicked, none of its descendants are clicked.
#
#
proc HWAPropagateUnclick { index clickType } {
    global HWATree

    # If none of our ancestors have been clicked, get out of here.
    if { ![HWACheckAncestorsForClick $index $clickType] } then {
	return
    }

    set ID $index
    while { $HWATree(${clickType}Click$ID) == 0 } {
	foreach sibling [HWAGetSiblingList $ID] {
	    set HWATree(${clickType}Click$sibling) 1
	}
	set ID [HWAGetParentID $ID]
    }
    set HWATree(${clickType}Click$ID) 0
}

proc HWAPropagateClick { index clickType } {
    global HWATree

    # If we're at the root of the tree, get out of here.
    if { $index == ":" } then { return }

    # If any of our siblings is unclicked, get out of here.
    set siblingList [HWAGetSiblingList $index] 
    foreach sibling $siblingList {
	if { $HWATree(${clickType}Click$sibling) == 0 } then { return }
    }

    # If we get here, we know that all of my siblings are clicked.
    # The thing to do now is to unclick all of my siblings and click
    # our parent.

    # Propagate the clicks up from here.
    set parent [HWAGetParentID $index]
    set HWATree(${clickType}Click$parent) 1

    # Unclick my siblings and me
    set HWATree(${clickType}Click$index) 0
    foreach sibling $siblingList {
	set HWATree(${clickType}Click$sibling) 0
    }

    # PropagateClick on our parent, since now he may need to
    # propagate his click up to his parent
    HWAPropagateClick $parent $clickType
}

proc HWAUnclickDescendants { index clickType } {
    global HWATree

    foreach descendant [HWAGetDescendantsList $index] {
	set HWATree(${clickType}Click$descendant) 0
    }
}

proc HWACheckAncestorsForClick { index clickType } {
    global HWATree

    set ID $index
    while { $ID != "" } {
	if { $HWATree(${clickType}Click$ID) } then { return 1}

	set ID [HWAGetParentID $ID]
    }

    return 0
}

proc HWAVerifyNoDescendantsClicked { index clickType } {
    global HWATree

    foreach descendant [HWAGetDescendantsList $index] {
	if { $HWATree(${clickType}Click$descendant) == 1 } then {
	    puts stderr "INVARIANT \#3 VIOLATED!  $index is clicked for $clickType, so is $descendant"
	}
    }
}

proc HWAVerifyNoAncestorsClicked { index clickType } {
    global HWATree

    set ID $index
    while { $ID != "" } {
	if { $HWATree(${clickType}Click$ID) == 1 } then { 
	    puts stderr "INVARIANT \#1 VIOLATED!  $index is clicked for $clickType, so its ancestor ($ID)"
	}
	set ID [HWAGetParentID $ID]
    }
}

proc HWACheckClickInvariant { index clickType } {
    global HWATree

    if { $HWATree(${clickType}Click$index) == 1 } then {
	if { $HWATree(${clickType}$index) == 0 } then {
	    puts stderr "INVARIANT \#2 VIOLATED:  $index:  Click is on, but $clickType is off"
	}
	HWAVerifyNoDescendantsClicked $index $clickType
    } else {
	foreach child [HWAGetChildrenList $index] {
	    HWACheckClickInvariant $child $clickType
	}
    }
    
}

#
# This routine determines whether or not a node is an ancestor of another node
#
proc HoardWalkIsAncestor { potentialAncestor node } {
    return [string match ${potentialAncestor}* $node]
}


#
# This routine adds an element to the Tree
#
proc HoardWalkAddListElement { frameid task text cost 
			       costvar objectvar fetchvar stopvar 
			       duplicatelist hide} {
    global HWA
    global HWATree
    global DisplayStyle

    set HWA(Cost$task) $cost
    set HWA(Blocks$task) [expr $cost * 9600 / 8 / 1024]
    set HWA(Fetch$task) 0
    set HWA(StopAsking$task) 0
    set HWATree(FetchClick$task) 0
    set HWATree(StopAskingClick$task) 0
    set HWA(DuplicateList$task) $duplicatelist
    set HWA(ID$task) $text

    set this $HWA(PendingAdvice:List)

    set fudge 5

    if { [llength $duplicatelist] > 1 } then {
	$HWA(PendingAdvice:List) add $task \
	    -itemtype text \
	    -text $text \
	    -style $DisplayStyle(Italic)
        label $this.cost$task -width 9 -text $cost  -anchor e \
	    -textvariable $costvar \
	    -background gray92 \
	    -font *times-medium-i-*-*-14*
    } else {
	$HWA(PendingAdvice:List) add $task \
	    -itemtype text \
	    -text $text \
	    -style $DisplayStyle(Normal)
        label $this.cost$task -width 9 -text $cost  -anchor e \
	    -background gray92 \
	    -textvariable $costvar 
    }
	$HWA(PendingAdvice:List) item create $task 1 \
	-itemtype window \
	-window $this.cost$task

    checkbutton $this.fetch$task \
	-variable $fetchvar \
	-command "HoardWalkFetchListElement $task" \
	-background gray92 \
	-anchor e 
    $HWA(PendingAdvice:List) item create $task 2 \
	-itemtype window \
	-window $this.fetch$task

    checkbutton $this.stopasking$task \
	-variable $stopvar \
	-command "HoardWalkStopAskingListElement $task" \
        -background gray92 \
	-anchor e 
    $HWA(PendingAdvice:List) item create $task 3 \
	-itemtype window \
	-window $this.stopasking$task

    if { $hide == 1 } then { $HWA(PendingAdvice:List) hide entry $task }
}



proc HWAIncrementAncestorCosts { index match } {
    global HWATree
   
    if { [llength $match] == 0 } then { return }

    set commonAncestor [FindCommonAncestors $index $match]
    set ID [HWAGetParentID $index]
    while { $ID != $commonAncestor } {
	set HWATree(Cost$ID) [expr $HWATree(Cost$ID) + $HWATree(Cost$match)]
	set ID [HWAGetParentID $ID]
    }
}

proc HWADecrementAncestorCosts { index match } {
    global HWATree
   
    if { [llength $match] == 0 } then { return }
    if { $HWATree(Cost$match) == "--" } then { return }

    set commonAncestor [FindCommonAncestors $index $match]
    set ID [HWAGetParentID $index]
    while { $ID != $commonAncestor } {
	set newCost [expr $HWATree(Cost$ID) - $HWATree(Cost$match)]
	if { $newCost == 0 } then {
	    set HWATree(Cost$ID) "--"
	} else {
	    set HWATree(Cost$ID) $newCost
	}
	set ID [HWAGetParentID $ID]
    }

}

proc HWACheckAllDescendantsOff { index type } {
    global HWATree

    if { $HWATree(${type}$index) == 1 } then { return 0 }
    foreach descendant [HWAGetDescendantsList $index] {
	if { $HWATree(${type}$descendant) == 1 } then { return 0 }
    }
    return 1
}

proc FixMe { index } {
    global HWATree

    if { $index == "" } then { return }

    set oneFetchOff 0
    set oneFetchOn 0

    set oneStopOff 0
    set oneStopOn 0

    foreach child [HWAGetChildrenList $index] {
	if { $HWATree(Fetch$child) == 1 } then { incr oneFetchOn }
	if { $HWATree(Fetch$child) == 0 } then { incr oneFetchOff }

	if { $HWATree(StopAsking$child) == 1 } then { incr oneStopOn }
	if { $HWATree(StopAsking$child) == 0 } then { incr oneStopOff }
    }


    if { ($oneFetchOff == 0) && ($oneFetchOn > 0) } then { 
	## all children are to be fetched
	if { $HWATree(Fetch$index) == 0 } then {
	    set HWATree(Fetch$index) 1
	    FixMe [HWAGetParentID $index]
	}
    }

    if { $oneFetchOff > 0 } then { 
	## one or more children are not to be fetched
	if { $HWATree(Fetch$index) == 1 } then {
	    set HWATree(Fetch$index) 0
	    FixMe [HWAGetParentID $index]
	}
    }

    if { ($oneStopOff == 0) && ($oneStopOn > 0) } then { 
	## all children are to be stop-asked
	if { $HWATree(StopAsking$index) == 0 } then {
	    set HWATree(StopAsking$index) 1
	    FixMe [HWAGetParentID $index]
	}
    }

    if { $oneStopOff > 0 } then { 
	## one or more children are not to be stop-asked
	if { $HWATree(StopAsking$index) == 1 } then {
	    set HWATree(StopAsking$index) 0
	    FixMe [HWAGetParentID $index]
	}
    }
}

proc HWAFetchElementHelper { index } {
    global HWATree

    if { $index == "" } then { return }

    set HWATree(Fetch$index) 1
    set HWATree(StopAsking$index) 0
}

proc HWAUnFetchElementHelper { index } {
    global HWATree

    if { $index == "" } then { return }

    set HWATree(Fetch$index) 0
}

proc HWAFetchElement { index } {
    global HWATree

    # Mark myself
    HWAFetchElementHelper $index 

    # Mark my children
    foreach child [HWAGetChildrenList $index] {
	HWAFetchElement $child
    }
    
    # Mark my peers 
    foreach peer [HWAGetPeerList $index] {
	HWAFetchElementHelper $peer
	FixMe [HWAGetParentID $peer]
    }

    # Fix my parent if necessary
    FixMe [HWAGetParentID $index]
}


proc HWAUnFetchElement { index } {
    global HWATree

    set troubleList [list ]

    # Check to see if my peers have been clicked.  
    set peerList [HWAGetPeerList $index]
    foreach peer $peerList {
	if { [HWACheckAncestorsForClick $peer Fetch] } then {
	    lappend troubleList $peer
	}
    }
    # If so, we have trouble so get out.
    if { [llength $troubleList] > 0 } then { 
	set HWATree(Fetch$index) 1
	return $troubleList 
    }

    # Unmark myself
    HWAUnFetchElementHelper $index

    # Unmark my (trouble-free) peers
    foreach peer $peerList {
	HWAUnFetchElementHelper $peer
	FixMe [HWAGetParentID $peer]
    }

    # Unmark my children
    foreach child [HWAGetChildrenList $index] {
	lappend troubleList [HWAUnFetchElement $child]
    }

    # Fix up my node and my peers
#
    FixMe $index
    FixMe [HWAGetParentID $index]
    foreach peer $peerList { 
	FixMe $peer 
	FixMe [HWAGetParentID $peer]
    }

    return $troubleList
}

proc HWACheckForTrouble { index clickType } {

    set troubleList [list ]

    foreach peer [HWAGetPeerList $index] {
	if { [HWACheckAncestorsForClick $peer $clickType] } then {
	    if { [lsearch -exact $troubleList $peer] == -1 } then {
		lappend troubleList $peer
	    }
	}
    }

    if { [llength $troubleList] != 0 } then { return $troubleList }

    foreach child [HWAGetChildrenList $index] {
	set childTroubles [HWACheckForTrouble $child $clickType]
	if { [llength $childTroubles] > 0 } then {
	    foreach t $childTroubles {
		if { [lsearch -exact $troubleList $t] == -1 } then {
		    lappend troubleList $t
		}
	    }
	}
    }

    return $troubleList
}

#
# This routine is invoked whenever the user clicks on a Fetch? button.
#
proc HoardWalkFetchListElement { index } {
    global HWATree

    if { $HWATree(Fetch$index) == 1 } then {

	set HWATree(StopAsking$index) 0
	HoardWalkStopAskingListElement $index
	if { ![HWACheckAllDescendantsOff $index StopAsking] } then {
	    set HWATree(Fetch$index) 0
	    return
	}

	set HWATree(FetchClick$index) 1
	HWAPropagateClick $index Fetch
	HWAUnclickDescendants $index Fetch

	HWAFetchElement $index

    } else {

	HWAPropagateUnclick $index Fetch
	HWAUnclickDescendants $index Fetch

	set troubles [HWACheckForTrouble $index Fetch]
	if { [llength $troubles] > 0 } then {
	    puts stderr "MARIA: Pop-up warning message: $index has trouble with $troubles"
	}

	HWAUnFetchElement $index 
    }
    HWACalculateTotalExpectedFetchTime
    HWACheckClickInvariant $index Fetch
}

proc HWAStopAskingElementHelper { index } {
    global HWATree

    if { $index == "" } then { return }

    set HWATree(Fetch$index) 0
    set HWATree(StopAsking$index) 1
}

proc HWAUnStopAskingElementHelper { index } {
    global HWATree

    if { $index == "" } then { return }

    set HWATree(StopAsking$index) 0
}


proc HWAStopAskingElement { index } {
    global HWATree

    # Mark myself
    HWAStopAskingElementHelper $index

    # Mark my children
    foreach child [HWAGetChildrenList $index] {
	HWAStopAskingElement $child 
    }
    
    # Mark my matches
    foreach peer [HWAGetPeerList $index] {
	HWAStopAskingElementHelper $peer
	FixMe [HWAGetParentID $peer]
    }

    # Fix my parent if necessary
    FixMe [HWAGetParentID $index]
}


proc HWAUnStopAskingElement { index } {
    global HWATree

    set troubleList [list ]

    # Check to see if my peers have been clicked.
    set peerList [HWAGetPeerList $index]
    foreach peer $peerList {
	if { [HWACheckAncestorsForClick $peer StopAsking] } then {
	    lappend troubleList $peer
	}
    }
    # If so, we have trouble so get out.
    if { [llength $troubleList] > 0 } then {
	set HWATree(StopAsking$index) 1
	return $troubleList
    }

    # Unmark myself
    HWAUnStopAskingElementHelper $index 

    # Unmark my (trouble-free) peers
    foreach peer $peerList {
	HWAUnStopAskingElementHelper $peer
	FixMe [HWAGetParentID $peer]
    }

    # Unmark my children
    foreach child [HWAGetChildrenList $index] {
	lappend troubleList [HWAUnStopAskingElement $child]
    }
    
    FixMe $index
    FixMe [HWAGetParentID $index]
    foreach peer $peerList {
	FixMe $peer
	FixMe [HWAGetParentID $peer]
    }
}

proc HoardWalkStopAskingListElement { index } {
    global HWATree

    if { $HWATree(StopAsking$index) == 1 } then {

	set HWATree(Fetch$index) 0
	HoardWalkFetchListElement $index
	if { ![HWACheckAllDescendantsOff $index Fetch] } then {
	    set HWATree(StopAsking$index) 0
	    return
	}

	set HWATree(StopAskingClick$index) 1
	HWAPropagateClick $index StopAsking
	HWAUnclickDescendants $index StopAsking

	HWAStopAskingElement $index

    } else {

	HWAPropagateUnclick $index StopAsking
	HWAUnclickDescendants $index StopAsking

	set troubles [HWACheckForTrouble $index StopAsking]
	if { [llength $troubles] > 0 } then {
	    puts stderr "MARIA: Pop-up warning message: $index has trouble with $troubles"
	}

	HWAUnStopAskingElement $index 
    }
    HWACalculateTotalExpectedFetchTime
    HWACheckClickInvariant $index StopAsking
}



proc HoardWalkOutputResults { OutFileName } {
    global HWATree
    global HWAinput

    set OutFILE [open $OutFileName {WRONLY CREAT TRUNC}]

    foreach element [array names HWATree :*] {

	if { ![HoardWalkIsLeaf $element] } then { continue }

#	puts stderr "Attempting to find input line matching $HWATree($element)"

	# Find the FID associated with this element
	set lineNumber -1
	foreach inputLine [array names HWAinput Filename*] {
#	    puts stderr "    Comparing with: $HWAinput($inputLine)"
	    if { ![string compare $HWATree($element) $HWAinput($inputLine)] } then {
		continue
	    }
	    
#	    puts stderr "  Matched: $HWAinput($inputLine) (lineNumber= $lineNumber)"
	    set lineNumber [string range $inputLine 9 end]
	    break
	}
	if { $lineNumber == -1 } then {
	    puts stderr "==> HoardWalkOutputResults: Couldn't find the FID for $element\n"
	    flush stderr
	}

	if { $HWATree(Fetch$element) == 1 } then {
	    puts $OutFILE "$HWAinput(FID:$lineNumber) 0"
	} elseif { $HWATree(StopAsking$element) == 1 } then {
	    puts $OutFILE "$HWAinput(FID:$lineNumber) 1"
	}

	if { ($HWATree(Fetch$element) == 0) && ($HWAinput(FetchDefault:$lineNumber) == 1) } then {
	    puts $OutFILE "$HWAinput(FID:$lineNumber) 3"
	}
    }

    flush $OutFILE    
    close $OutFILE
}

proc HoardWalkNow { } {
    global Pathnames
    global HWA
    global HoardWalk
    global Indicator

    # Cancel the hoard walk state
    if { $HoardWalk(State:secondary) != "PendingAdvice" } then {
	puts stderr "Assertion failed: HoardWalk(State) != PendingAdvice"
	flush stderr
	exit
    }
    set HWA(State) "Active"
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    IndicatorUpdate HoardWalk
    update idletasks

    # Remove the advice entry
#    set AdviceEntry [GetHoardWalkAdviceID]
#    if { $AdviceEntry != -1 } then {
#        AdviceRemove "advice$AdviceEntry"
	# Goal: prevents Advice indicator from flashing to Red if user
	#       answers the request for advice before it's done flashing).
#	CancelPendingIndicatorEvents Advice
#    }

    # Get the data out of the interface, then alert the interface that
    # the advice is complete
    HoardWalkOutputResults $HoardWalk(AdviceOutput)
    SendToAdviceMonitor "Hoard Advice Available"

    set HWA(completed) 1
    set HoardWalk(State:secondary) ""
}

proc HoardWalkAdviceHelpWindow { } {
    global HWA

    set helptext "The Hoard Walk Advice window allows you to give advice to the hoard daemon about what tasks should be fetched.\n\nThe upper left hand corner contains information about the status of the cache, including the percentage of cache file slots used and the percentage of cache blocks used.  The upper right hand corner contains information about the network status (expressed as a percentage of ethernet bandwidth) and information about the number of objects to be fetched during this hoard walk and the amount of time those fetches are expected to take.\n\nThe main body of this window shows the tasks for which one or more objects need to be fetched.  These tasks are shown hierarchically.  Click on:\n\n\t\"+\" to expand an entry (or double-click the task name)\n\t\"Fetch?\" to request that this entry be fetched, and\n\t\"Stop Asking?\" to prevent this entry from being listed in the future.\n\nElements shown in italics are hoarded by more than one task.  If you select one of the tasks that hoard such a task, the cost associated with the other tasks will decrease.\n\nThe hoard walk will proceed once you click on the \"Finish Hoard Walk\" button."
    set HWA(HoardWalkAdviceHelpWindow) .hoardwalkadvicehelp
    PopupHelp "Hoard Walk Advice Help" $HWA(HoardWalkAdviceHelpWindow) $helptext "7i"
}

proc timewatch { framename labelname } {
    global HWATree

    frame $framename -relief flat -borderwidth 2
    label $framename.label -text "$labelname = "
    label $framename.hours -textvariable HWATree(TotalExpectedFetchTime:hours)
    label $framename.sep1 -text ":"
    label $framename.mins -textvariable HWATree(TotalExpectedFetchTime:minutes)
    label $framename.sep2 -text ":"
    label $framename.secs -textvariable HWATree(TotalExpectedFetchTime:seconds)
    pack $framename.label -side left
    pack $framename.secs -side right
    pack $framename.sep2 -side right
    pack $framename.mins -side right
    pack $framename.sep1 -side right
    pack $framename.hours -side right
}


proc updateFetchTime { } {
    global HWA
    global HWATree

    set totalTime $HWATree(TotalExpectedFetchTime)

    if { $totalTime < 0 } then {
	puts stderr "ERROR: HWATree(TotalExpectedFetchTime) = $HWATree(TotalExpectedFetchTime) (and is less than 0!)"
	set HWATree(TotalExpectedFetchTime:hours) -0
	set HWATree(TotalExpectedFetchTime:minutes) 0
	set HWATree(TotalExpectedFetchTime:seconds) 0
	return
    }

    set HWATree(TotalExpectedFetchTime:hours) \
	[expr $totalTime / 3600]
    set HWATree(TotalExpectedFetchTime:minutes) \
	[expr ($totalTime - ($HWATree(TotalExpectedFetchTime:hours) * 3600)) / 60]
    set HWATree(TotalExpectedFetchTime:seconds) \
	[expr $totalTime - ($HWATree(TotalExpectedFetchTime:hours) * 3600) - \
	     ($HWATree(TotalExpectedFetchTime:minutes) * 60)]


    set HWATree(TotalExpectedFetchTime:hours) \
	[addLeadingZero $HWATree(TotalExpectedFetchTime:hours)]
    set HWATree(TotalExpectedFetchTime:minutes) \
	[addLeadingZero $HWATree(TotalExpectedFetchTime:minutes)]
    set HWATree(TotalExpectedFetchTime:seconds) \
	[addLeadingZero $HWATree(TotalExpectedFetchTime:seconds)]
}


# For Testing
#lappend auto_path /usr/mre/russianSave/coda-src/guitools

#set Window ""
#set user [exec whoami]
#set password ""

#wm withdraw .

#InitLocks
#InitSystemAdministrator
#InitPathnamesArray
#InitDimensionsArray
#InitColorArray
#InitDisplayStyleArray
#InitVenusLog
#InitStatistics
#InitServers 
#InitEventsArray
#InitData
#InitLog

#HoardWalkAdviceInit

#Hoard

#HoardWalkAdvice /usr/mre/russianSave/coda-src/guitools/hoardlist.samplein /tmp/foo



