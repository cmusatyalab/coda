#
# An interface that alerts users to potential problems and opportunities
#
# History :
#  96/5/28 : mre@cs.cmu.edu : code
#
#
#
# QUESTIONS:
#    Window visibility and indicator state changes
#	-- Should we change the state of the indicator lights while a
#	   window is visible (e.g. hoard walk)?
#	Answer (from Bonnie):  Go ahead an update the information
#	Implemented:  11/25/96
#
#    Window visibility and timeout
#	-- Should we timeout an interface while it is visible (e.g. weak
#	   miss questionnaire)?  It is very disconcerting to a user who is
#	   just taking a long time to answer the questions.  Perhaps the
#	   timeouts shouldn't apply unless we've popped-up the window.
#	Solution:  After timeout, popup a window asking if they need more
#		time.  The window should give the same information as before.
#		But, there should be a new button allowing them to have the
#		program wait indefinitely.  If this window also timesout,
#		then take the default action.
#	Implemented:  11/25/96
#
#    HoardWalkAdvice: 
#	Unfetch help popups when you click on "writing thesis"'s "stop asking" button.
#	This is because we are requesting that all of the subtasks of "writing thesis"
#	also be marked as "stop asking".  The problem is that those subtasks might be
#	included in other tasks in addition to the "writing thesis" task.   How should
#	this situation be handled?  In addition, the user should be able to explore in
#	the interface while the Unfetch questionnaire is visible.  Otherwise, how will
#	they know the answer?  Unfortunately, this opens a big can of worms.
#
#    Definition windows:
#	If you fill in the pathname areas (or the contains area) and later change
#	the name of the definition and hit return, the definition gets deleted.
#	What would be better?  
#	Answer (from Bonnie):  Don't clear it.
#	Implemented:  11/25/96
#    
#	What's the correct ordering for the indicator lights?  Should I make it alphabetical?
#
# THINGS TO TRAIN:
#	- Show them how to CLOSE the windows.
#	- My version of dran-n-drop.
#	- Funky mouse and watch pictures -- requires a click.  Perhaps just have observer 
#	  tell them about this if it comes up?
#	- Tell them that names of programs/data/tasks can overlap.  So, you can have a

##############################################################################
#
#
#  Space Indicator
#
#
##############################################################################

proc FindMax3 { a b c } {
    set max $a

    if { $b > $max } then { set max $b }
    if { $c > $max } then { set max $c }

    return $max
}

proc max { a b } {
    if { $a > $b } then { return $a } else { return $b }
}

proc SpaceMeter { framename labeltext righttext meterwidth allocated max usage } {
    global Dimensions
    global Statistics
    global Colors

    frame $framename

    set relative_width [max 50 [expr round($meterwidth * $allocated / $max)] ]

    set meterColor $Colors(Normal)

    if { $usage > $Statistics(NearlyFull) } then { 
        set meterColor $Colors(Critical)
    } elseif { $usage > $Statistics(Filling) } then {
        set meterColor $Colors(Warning)
    }

    tixCodaMeter $framename.meter \
	-height $Dimensions(Meter:Height) \
	-width $relative_width \
	-labelWidth 12 \
	-label $labeltext \
	-rightLabel $righttext \
	-state normal \
	-percentFull $usage \
	-foreground $meterColor

    pack $framename.meter -side left -padx 4 -pady 4 -anchor w -expand true
}

proc SpaceVisualStart { } {
    global Window
    global Space
    global Colors
    global Dimensions
    global Statistics

    CheckStatisticsCurrency $Statistics(Currency)

    if { [winfo exists $Space(MainWindow)] == 1 } then {
	raise $Space(MainWindow)
	return
    }
    toplevel $Space(MainWindow)
    wm title $Space(MainWindow) "Space Information"
    wm geometry $Space(MainWindow) $Space(Geometry)
    wm protocol $Space(MainWindow) WM_DELETE_WINDOW {SpaceVisualEnd}
    wm minsize $Space(MainWindow) 

    set MaxBytes [FindMax3 [expr $Statistics(RVM:Allocated)/1024] \
		          $Statistics(Partition:Allocated) \
			  $Statistics(Blocks:Allocated)]

    set Space(CacheUsage) [GetCacheUsage]
    SpaceMeter $Space(MainWindow).cache \
	"Coda Cache:\n(in MB)" \
        [format "%d" [expr round($Statistics(Blocks:Allocated) / 1024)]] \
	$Dimensions(Meter:Width) \
	$Statistics(Blocks:Allocated) \
	$MaxBytes \
	$Space(CacheUsage)
	
    set Space(PartitionUsage) [GetPartitionUsage $Statistics(Partition)]
    SpaceMeter $Space(MainWindow).partition \
	"Local Disk:\n(in MB)" \
        [format "%d" [expr round($Statistics(Partition:Allocated) / 1024)]] \
	$Dimensions(Meter:Width) \
	$Statistics(Partition:Allocated) \
	$MaxBytes \
	$Space(PartitionUsage)

    set Space(RVMUsage) $Statistics(RVM:Usage)
    SpaceMeter $Space(MainWindow).rvm \
	"RVM:\n(in MB)" \
	[format "%d" [expr round($Statistics(RVM:Allocated) / 1024 / 1024)]] \
	$Dimensions(Meter:Width) \
	[expr round($Statistics(RVM:Allocated) / 1024) ] \
	$MaxBytes \
	$Space(RVMUsage)

    pack $Space(MainWindow).cache -side top -anchor w -expand true
    pack $Space(MainWindow).partition -side top -anchor w -expand true
    pack $Space(MainWindow).rvm -side top -anchor w -expand true

    button $Space(MainWindow).spacehelp -text "Help" -relief groove -bd 2 \
	-anchor w -command {SpaceHelpWindow}
#    button $Space(MainWindow).spaceclose -text "Close" -anchor w \
#	-command { SpaceVisualEnd }
    pack $Space(MainWindow).spacehelp -side left -expand true
#    pack $Space(MainWindow).spaceclose -side right
}

proc SpaceVisualEnd { } {
    global Space

    LogAction "Space Window: close"
    SetTime "SpaceWindow:close"
    set Space(Geometry) [wm geometry $Space(MainWindow)]
    destroy $Space(MainWindow)
}

proc SpaceInitData { } {
    global Space

    set Space(MainWindow) .space
    set Space(MinHeight) 410
    set Space(MinWidth) 218
    set Space(Geometry) "$Space(MinHeight)x$Space(MinWidth)"
}

proc SpaceDetermineState { } {
    global Statistics
    global Space

    CheckStatisticsCurrency $Statistics(Currency)

    set cache [GetCacheUsage]
    set partition [GetPartitionUsage $Statistics(Partition)]
    set rvm $Statistics(RVM:Usage)

    if { ($cache > $Statistics(NearlyFull)) || 
	 ($partition > $Statistics(NearlyFull)) || 
	 ($rvm > $Statistics(NearlyFull)) } then {
        return Critical
    } elseif { ($cache > $Statistics(Filling)) || 
	       ($partition > $Statistics(Filling)) || 
	       ($rvm > $Statistics(Filling)) } then {
        return Warning
    } else {
        return Normal
    }
}

proc SpaceCheckVisibilityAndUpdateScreen { } {
    global Space

    if { [info exists Space(MainWindow)] == 1 } then {
        if { [winfo exists $Space(MainWindow)] == 1 } then {
            set Space(Geometry) [wm geometry $Space(MainWindow)]
	    destroy $Space(MainWindow)
   	    SpaceVisualStart
	}
    }
}

proc SpaceBindings { } {
    global Window
    bind $Window.indicators.space <Double-1> { 
	LogAction "Space Indicator: double-click"
	SetTime "SpaceIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	SpaceVisualStart 
	update idletasks
	SetTime "SpaceWindow:visible"
    }
}

proc SpaceHelpWindow { } {
    global Statistics
    global SysAdmin
    global Space

    LogAction "Space Window: help"
    set state [SpaceDetermineState]

    set messageList [list ]
    lappend messageList "This window shows the status of the available cache space, the available disk space (on the partition of the local disk where the Venus cache resides), and the available RVM space (an internal data structure).  Should any number exceed a reasonable threshold, the instructions below will tell you what corrective action to take.\n\n-----------------------------------------\n"

    if { $state == "Normal" } then {
	lappend messageList "The cache, disk partition and RVM usage are within reasonable limits."
    } else {
        if { $Space(CacheUsage) > $Statistics(NearlyFull) } then {
	    lappend messageList "The cache is nearly full.  Check that you are not hoarding too much data.  If so, reduce the amount of data you have hoarded.  Otherwise, everything is okay."
        } elseif { $Space(CacheUsage) > $Statistics(Filling) } then {
	    lappend messageList "The cache is filling.  Be careful not to hoard more data than will fit in your cache."
        } else {
	    lappend messageList "The cache space is within reasonable limits."
	}
  	lappend messageList ""

        if { $Space(PartitionUsage) > $Statistics(NearlyFull) } then {
	    lappend messageList "The disk partition ($Statistics(Partition)) is nearly full.  Please reduce the disk usage on this partition or contact your Coda system administrator ($SysAdmin(Coda)) immediately."
        } elseif { $Space(PartitionUsage) > $Statistics(Filling) } then {
	    lappend messageList "The disk parition ($Statistics(Partition)) is filling up.  Please reduce the disk usage on this partition or contact your Coda system administrator ($SysAdmin(Coda)) at your earliest convenience."
        } else {
	    lappend messageList "The disk partition is within reasonable limits."
	}
  	lappend messageList ""

        if { $Space(RVMUsage) > $Statistics(NearlyFull) } then {
	    lappend messageList "The RVM space allocated is nearly full.  Please contact your Coda system administrator immediately ($SysAdmin(Coda))."
        } elseif { $Space(RVMUsage) > $Statistics(Filling) } then {
	    lappend messageList "The RVM space allocated is filling up.  Please contact your Coda system administrator at your earliest convenience ($SysAdmin(Coda))."
        } else {
	    lappend messageList "The RVM space usage is within reasonable limits."
	}
  	lappend messageList ""

    }

    PopupHelp "Space Help Window" .spacehelp [join $messageList "\n"] 5i
}

proc SpaceNormalEvent { } {
    global Indicator

    set Indicator(Space:State) Normal

    NotifyEvent SpaceNormal Space SpaceVisualStart
    SetTime "SpaceIndicator:normal"
    SpaceCheckVisibilityAndUpdateScreen
}

proc SpaceWarningEvent { } {
    global Indicator

    set Indicator(Space:State) Warning

    NotifyEvent SpaceWarning Space SpaceVisualStart
    SetTime "SpaceIndicator:warning"
    SpaceCheckVisibilityAndUpdateScreen
}

proc SpaceCriticalEvent { } {
    global Indicator

    set Indicator(Space:State) Critical

    NotifyEvent SpaceCritical Space SpaceVisualStart
    SetTime "SpaceIndicator:error"
    SpaceCheckVisibilityAndUpdateScreen
}

proc UpdateSpaceStatistics { filesOccupied blocksOccupied rvmOccupied } {
    global Statistics
    global Indicator

    set OriginalState $Indicator(Space:State)

    set Statistics(LastUpdateTime) [nowSeconds]

    set Statistics(Files:Occupied)  $filesOccupied
    set Statistics(Files:Usage)	    [expr $Statistics(Files:Occupied) * 100 \
					  / $Statistics(Files:Allocated)]

    set Statistics(Blocks:Occupied)  $blocksOccupied
    set Statistics(Blocks:Usage)     [expr $Statistics(Blocks:Occupied) * 100 \
					   / $Statistics(Blocks:Allocated)]

    set Statistics(RVM:Occupied)      $rvmOccupied
    set Statistics(RVM:Usage)          [expr $Statistics(RVM:Occupied) * 100 \
					    / $Statistics(RVM:Allocated)]

    # This is sleazy.  If one area is already at either a warning or error, and a 
    # different space area deteriorates, we will not notify the user.  Minor, but
    # probably something to fix eventually.  Shouldn't be difficult, just more bookkeeping.
    set NewState [SpaceDetermineState]
    if { $OriginalState != $NewState } then { 
        if { $NewState == "Normal" } { SpaceNormalEvent }
        if { $NewState == "Warning" } { SpaceWarningEvent }
        if { $NewState == "Critical" } { SpaceCriticalEvent }
    } else {
        SpaceCheckVisibilityAndUpdateScreen
    }
}

proc UpdateDiskSpaceStatistics { } {
    global Statistics

    set OriginalState [SpaceDetermineState]

    set Statistics(Partition:Occupied)  [GetPartitionOccupied $Statistics(Partition)]
    set Statistics(Partition:Usage)     [GetPartitionUsage $Statistics(Partition)]

    # This is sleazy.  If one area is already at either a warning or error, and a 
    # different space area deteriorates, we will not notify the user.  Minor, but
    # probably something to fix eventually.  Shouldn't be difficult, just more bookkeeping.
    set NewState [SpaceDetermineState]
    if { $OriginalState != $NewState } then { 
        if { $NewState == "Normal" } { SpaceNormalEvent }
        if { $NewState == "Warning" } { SpaceWarningEvent }
        if { $NewState == "Critical" } { SpaceCriticalEvent }
    } else {
        SpaceCheckVisibilityAndUpdateScreen
    }
}