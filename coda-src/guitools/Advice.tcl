##############################################################################
#
#
#  Advice Indicator
#
#
##############################################################################


proc AdviceInitData { } {
    global Advice
    global DiscoMiss
    global WeakMiss
    global ReadMiss

    set Advice(IDnum) -1
    set Advice(MainWindow) .advice
    set Advice(Geometry) +0+0
    set Advice(Count) 0

    set DiscoMiss(Geometry) +0+0
    set WeakMiss(Geometry) +0+0
    set ReadMiss(Geometry) +0+0

    Consider_Init
}

proc AdviceVisualStart { } {
    global DisplayStyle
    global Advice
    global Colors

    if { [winfo exists $Advice(MainWindow)] == 1 } then {
	raise $Advice(MainWindow)
	return
    }
    toplevel $Advice(MainWindow)
    wm title $Advice(MainWindow) "Advice Information"
    wm geometry $Advice(MainWindow) 525x400+0+0
    wm protocol $Advice(MainWindow) WM_DELETE_WINDOW {AdviceVisualEnd}

    # Now make two labelled frames...
    # ...one for Venus requests for advice
    set Advice(pendingAdviceFrame) $Advice(MainWindow).f
    tixLabelFrame $Advice(pendingAdviceFrame) -label "Advice Needed:" -options {
	label.padX 5
    }
    pack $Advice(pendingAdviceFrame) -side top \
	-padx 10 -pady 10 -expand true -fill both

    # ...and one for Venus informational advice
    set Advice(pendingInformationFrame) $Advice(MainWindow).i
    tixLabelFrame $Advice(pendingInformationFrame) -label "Advice Offered:" -options {
	label.padX 5
    }
    pack $Advice(pendingInformationFrame) -side top \
	-padx 10 -pady 10 -expand true -fill both

    # Now make the scrolled hlists...
    # ...one for Venus requests for advice
    set pendingAdviceHList [$Advice(pendingAdviceFrame) subwidget frame]
    tixScrolledHList $pendingAdviceHList.list -height 100
    pack $pendingAdviceHList.list -side top \
	-padx 10 -pady 10 -expand yes -fill both 
    set Advice(pendingAdviceHList) [$pendingAdviceHList.list subwidget hlist]
    $Advice(pendingAdviceHList) configure -command AskAdvice
    for {set i 0} {$i <= $Advice(IDnum)} {incr i} {
	if { $Advice(advice$i:State) == "pendingAdvice" } then {
	    AdviceLine $Advice(pendingAdviceHList) advice$i \
		$Advice(advice$i:Text) $Advice(advice$i:Urgency)
	    $Advice(pendingAdviceHList) add advice$i \
    	        -itemtype window -window $Advice(pendingAdviceHList).advice$i
	}
    }
    # ...and one for Venus informational advice
    set pendingInformationHList [$Advice(pendingInformationFrame) subwidget frame]
    tixScrolledHList $pendingInformationHList.list -height 100
    pack $pendingInformationHList.list -side top \
	-padx 10 -pady 10 -expand yes -fill both 
    set Advice(pendingInformationHList) [$pendingInformationHList.list subwidget hlist]
    $Advice(pendingInformationHList) configure -command AskAdvice
    for {set i 0} {$i <= $Advice(IDnum)} {incr i} {
	if { $Advice(advice$i:State) == "pendingInformation" } then {
	    AdviceLine $Advice(pendingInformationHList) advice$i \
		$Advice(advice$i:Text) $Advice(advice$i:Urgency)
	    $Advice(pendingInformationHList) add advice$i \
    	        -itemtype window -window $Advice(pendingInformationHList).advice$i
	}
    }

    set Advice(Buttons) [frame $Advice(MainWindow).buttons -bd 2 -relief groove]
    button $Advice(Buttons).help -text "Help" -anchor w -relief groove -bd 2 \
	-command { AdviceHelpWindow }
    button $Advice(Buttons).ask -text "Request Hoard Advice" -anchor w \
	-command { AdviceRequestHoardAdvice }

    pack $Advice(Buttons).help -side left
    pack $Advice(Buttons).ask -side right
    pack $Advice(Buttons) -side bottom -fill x
}

proc AdviceRequestHoardAdvice { } {
    global ConsiderRemoving

    SendToAdviceMonitor "GetUsageStats $ConsiderRemoving(NumberRecent) $ConsiderRemoving(PercentAccess) $ConsiderRemoving(MinimumDatapoints)"

    SendToStdErr "GetUsageStats $ConsiderRemoving(NumberRecent) $ConsiderRemoving(PercentAccess) $ConsiderRemoving(MinimumDatapoints)"
    SendToStdErr "If not connect to Venus, please enter 1 2 1 2 1 2 <return>"
}

proc UsageStatisticsAvailable { venusfile missfile replacefile } {
    global Advice
    global Events
    global ConsiderAddingData
    global ConsiderRemovingData

    Consider_ProcessVenusInput $venusfile
    Consider_ProcessMissInput $missfile
    Consider_ProcessReplaceInput $replacefile

    set feedback 0
    if { $ConsiderAddingData(ID) > 0 } then {
	set text "Consider Adding"
	set id [AdviceGetID]
	set Advice(advice$id:Text)     $text
	set Advice(advice$id:State)    pendingAdvice
	set Advice(advice$id:Type)     ConsiderAddingAdvice
	set Advice(advice$id:Urgency)  $Events(ConsiderAddingAdvice:Urgency)
	set Advice(advice$id:Command)  [list ConsiderAdding]

	AdviceInsert $text information

	set Indicator(Advice:State) [AdviceDetermineState]
	NotifyEvent ConsiderAddingAdvice Advice [list OfferAdvice advice$id]
	set feedback 1
    }

    if { $ConsiderRemovingData(ID) > 0 } then {
	set text "Consider Removing"
	set id [AdviceGetID]
	set Advice(advice$id:Text)     $text
	set Advice(advice$id:State)    pendingAdvice
	set Advice(advice$id:Type)     ConsiderRemovingAdvice
	set Advice(advice$id:Urgency)  $Events(ConsiderRemovingAdvice:Urgency)
	set Advice(advice$id:Command)  [list ConsiderRemoving]

	AdviceInsert $text information

	set Indicator(Advice:State) [AdviceDetermineState]
	NotifyEvent ConsiderRemovingAdvice Advice [list OfferAdvice advice$id]
	set feedback 1
    }

    if { $feedback == 0 } then {
	set msgtext "The system has no advice to give."
	set titletext "Request Hoard Advice"
	PopupTransient $msgtext 
    }
}

proc AdviceLine {frame windowname textmsg urgency} {
    global Colors

    # Change background to highlight this entry
    frame $frame.$windowname  -background gray92 -highlightcolor gray92 

    set width 25
    set height 25
    canvas $frame.$windowname.canvas -width $width -height $height -background gray92 -borderwidth 0
    pack $frame.$windowname.canvas -side left 
    $frame.$windowname.canvas create oval 3 3 22 22 -fill $Colors($urgency) -width 2 -tags light

    label $frame.$windowname.label -text $textmsg -background gray92
    pack $frame.$windowname.label -side left -expand true -fill both -padx 2 -pady 2

    bind $frame.$windowname.label <Double-1> [list AskAdvice $windowname]
    bind $frame.$windowname.canvas <Double-1> [list AskAdvice $windowname]

    bind $frame.$windowname <Enter> \
	[list $frame.$windowname configure -background blue]
    bind $frame.$windowname.label <Enter> \
	[list $frame.$windowname configure -background blue]
    bind $frame.$windowname.canvas <Enter> \
	[list $frame.$windowname configure -background blue]
    bind $frame.$windowname <Leave> \
	[list $frame.$windowname configure -background gray92]
    bind $frame.$windowname.label <Leave> \
	[list $frame.$windowname configure -background gray92]
    bind $frame.$windowname.canvas <Leave> \
	[list $frame.$windowname configure -background gray92]
}

proc AdviceVisualEnd { } {
    global Advice

    LogAction "Advice Window: close"
    SetTime "AdviceWindow:close"
    set Advice(Geometry) [GeometryLocationOnly [wm geometry $Advice(MainWindow)]]
    destroy $Advice(MainWindow)
}

proc AdviceDetermineState { } {
    global Advice

    set state Normal
    for { set i 0 } { $i <= $Advice(IDnum) } { incr i } {
  	if { [string match "pending*" $Advice(advice$i:State)] == 1 } then {
  	    if { $Advice(advice$i:Urgency) == "Critical" } then {
	        set state "Critical"
	        break
	    }
	    if { $Advice(advice$i:Urgency) == "Warning" } then {
	        set state "Warning"
	    }
        }
    }
    return $state
}

proc AdviceBindings { } {
    global Window
    bind $Window.indicators.advice <Double-1> { 
	#
	# Code that is commented out below was for timing purposes.
	# Results are shown below.
	#
        # puts [time {
	    LogAction "Advice Indicator: double-click"
	    SetTime "AdviceIndicator:doubleclick"
	    %W configure -background $Indicator(SelectedBackground)
 	    AdviceVisualStart 
  	    update idletasks
	    SetTime "AdviceWindow:visible"
        # update idletasks
        # } 1]

	# 
	# Timing results:
	# 
	# Cold start (pmax_mach):
	# 1109365 microseconds per iteration
	# 1468740 microseconds per iteration (HIGH)
	# 1265617 microseconds per iteration
	# 1109373 microseconds per iteration
	# 1109375 microseconds per iteration
	# 1093749 microseconds per iteration (LOW)
	# 1109359 microseconds per iteration
	# 
	# Average (of 5) = 1140618 microseconds
	#
	# 
	# Warm start (pmax_mach):
	# 687498 microseconds per iteration
	# 671866 microseconds per iteration
	# 656249 microseconds per iteration
	# 656242 microseconds per iteration (LOW)
	# 687490 microseconds per iteration
	# 687490 microseconds per iteration
	# 687501 microseconds per iteration (HIGH)
	#
	# Average (of 5) = 678119 microseconds
	#
	#
	# Cold start (i486_mach):
	# 1170000
	# 1889996 (HIGH)
	# 1130003
	# 1280002
	# 1230000
	# 1259997
	# 1129998 (LOW)
	# 
	# Average (of 5) =  1214000 microseconds
	# 
	# 
	# Warm start (i486_mach):
	# 770000
	# 779996
	# 769996 (LOW)
	# 809996
	# 799997
	# 780003
	# 919999 (HIGH)
	# 
	# Average (of 5) = 787998 microseconds
    }
}

proc AdviceHelpWindow { } {
    global Advice

    LogAction "Advice Window: help"
    set helptext "The Advice window shows you a list of outstanding advice requests that from Venus.  To provide advice to Venus, double-click on the entry."
    set Advice(AdviceHelpWindow) .advicehelp
    PopupHelp "Advice Help" $Advice(AdviceHelpWindow) $helptext "5i"
}

proc AdviceWindowTextUpdate { } {
    global Advice
    global Colors
    global DisplayStyle

    if { [winfo exists $Advice(MainWindow)] == 1 } then {

        for {set i 0} {$i <= $Advice(IDnum)} {incr i} {
	    if { $Advice(advice$i:State) == "pendingAdvice" } then {
		$Advice(pendingAdviceHList).advice$i.canvas itemconfigure light \
		    -fill $Colors($Advice(advice$i:Urgency))
	    }
        }

        for {set i 0} {$i <= $Advice(IDnum)} {incr i} {
	    if { $Advice(advice$i:State) == "pendingInformation" } then {
		$Advice(pendingInformationHList).advice$i.canvas itemconfigure light \
		    -fill $Colors($Advice(advice$i:Urgency))
	    }
        }
    }
}

proc AskAdvice { foo } {
    global Advice

    LogAction "Advice Information Window: advice invokation"

    #
    # Code that is commented out below was for timing purposes.
    # Results for the WeakMiss command are shown below.  Note 
    # that I modified the WeakMiss command to return immediately
    # after displaying the query window and updating idle tasks.
    # Measurements taken on DecStation 5000/200 (verdi) and on
    # an i486 (planets).
    #
    # puts [time {
	SetTime "$Advice(${foo}:Type)${foo}:call"
        set result [eval $Advice(${foo}:Command)]
	SetTime "$Advice(${foo}:Type)${foo}:return"
    # } 1]    
    # 
    # Timing results:
    # 
    # Cold start (pmax):
    # 234369 microseconds per iteration
    # 390627 microseconds per iteration (HIGH)
    # 203127 microseconds per iteration (LOW)
    # 265619 microseconds per iteration
    # 250003 microseconds per iteration
    # 234375 microseconds per iteration
    # 218749 microseconds per iteration
    # 
    #  Average (of 5) = 240623 microseconds
    # 
    # Warm start (pmax):
    # 171876 microseconds per iteration
    # 156252 microseconds per iteration
    # 171876 microseconds per iteration
    # 171877 microseconds per iteration (HIGH)
    # 171876 microseconds per iteration
    # 156245 microseconds per iteration (LOW)
    # 156252 microseconds per iteration
    # 
    #  Average (of 5) = 165626 microseconds
    # 
    # 
    # Cold start (i486):
    # 190000 (LOW)
    # 190001 
    # 190001
    # 190000
    # 200000
    # 190001
    # 260001 (HIGH)
    # 
    #  Average (of 5) = 192001 microseconds
    # 
    # Warm start (i486):
    # 160000 (LOW)
    # 160001 
    # 160001
    # 160000
    # 170000
    # 170000
    # 170001 (HIGH)
    # 
    #  Average (of 5) = 164000 microseconds
    # 

    if { $result != -1 } then {

	if { $result == "" } then {
	    SendToStdErr "$Advice(${foo}:Command) returned NULL...exiting."
	    exit
	}

	set endOfWord [string wordend $Advice(${foo}:Command) 0]
	set msg [format "%s returns %d" [string range $Advice(${foo}:Command) 0 $endOfWord] $result]
        SendToAdviceMonitor $msg
        AdviceRemove $foo
	CancelPendingIndicatorEvents Advice

    } else {
	SendToStdErr "Can't remove advice because advice request returned error"
    }
}

proc AdviceRemove { foo } {
    global Advice
    global Indicator

    if { [winfo exists $Advice(MainWindow)] == 1 } {
        if { [$Advice(pendingAdviceHList) info exists $foo] == 1 } then {
	    $Advice(pendingAdviceHList) delete entry $foo
	}

        if { [$Advice(pendingInformationHList) info exists $foo] == 1 } then {
	    $Advice(pendingInformationHList) delete entry $foo
	}
    }

    set Advice(${foo}:State) complete
    set Advice(Count) [expr $Advice(Count) - 1]
    set Indicator(Advice:State) [AdviceDetermineState]
    IndicatorUpdate Advice
}

proc GetHoardWalkAdviceID { } {
    global Advice

    for { set n 0 } { $n <= $Advice(IDnum) } { incr n } {
	if { $Advice(advice$n:Type) == "HoardWalkPendingAdvice" } then {
	    if { $Advice(advice$n:State) == "pendingAdvice" } then { return $n }
	}
    }
    return -1
}

proc AdviceGetID {} {
    global Advice

    incr Advice(IDnum)
    set Advice(Count) [expr $Advice(Count) + 1]
    return $Advice(IDnum)
}

proc AdviceInsert { advicetext which } {
    global Advice

    if { [winfo exists $Advice(MainWindow)] == 1 } {
	if { $which == "advice" } {
	    AdviceLine $Advice(pendingAdviceHList) advice$Advice(IDnum) \
		$advicetext $Advice(advice$Advice(IDnum):Urgency)
	    $Advice(pendingAdviceHList) add advice$Advice(IDnum) \
		-itemtype window -window $Advice(pendingAdviceHList).advice$Advice(IDnum)
	} else {
	    AdviceLine $Advice(pendingInformationHList) advice$Advice(IDnum) \
		$advicetext $Advice(advice$Advice(IDnum):Urgency)
	    $Advice(pendingInformationHList) add advice$Advice(IDnum) \
		-itemtype window -window $Advice(pendingInformationHList).advice$Advice(IDnum)
	}
    }
}

proc DisconnectedCacheMissEvent { output pathname program } {
    global Advice
    global Events
    global Indicator

    SendToAdviceMonitor {Advice Received DisconnectedCacheMissEvent}
    SendToAdviceMonitor [pid]

    set text "Disconnected Cache Miss"
    set id [AdviceGetID]
    set Advice(advice$id:Text)    "$text: $pathname"
    set Advice(advice$id:State)   "pendingAdvice"
    set Advice(advice$id:Type)    DisconnectedCacheMissAdvice
    set Advice(advice$id:Urgency) $Events(DisconnectedCacheMissAdvice:Urgency)
    set Advice(advice$id:Command) [list DisconnectedCacheMissQuestionnaire $output $pathname $program]
    AdviceInsert "$text: $pathname" advice

    set Indicator(Advice:State) [AdviceDetermineState]

    NotifyEvent DisconnectedCacheMissAdvice Advice [list AskAdvice advice$id]
    SetTime "DisconnectedCacheMiss${id}:arrival"
#    if { PopupEvent? "Disconnected Cache Miss" }
}

proc HoardWalkAdviceEvent { } {
    global Advice
    global Events
    global Indicator

    set text "Hoard Walk Pending Advice"
    set id [AdviceGetID]

    set Advice(advice$id:Text) $text
    set Advice(advice$id:State) "pendingAdvice"
    set Advice(advice$id:Type) HoardWalkPendingAdvice
    set Advice(advice$id:Urgency) $Events(HoardWalkPendingAdvice:Urgency)
    set Advice(advice$id:Command) [list HoardWalkAdvice]
    AdviceInsert $text advice

    set Indicator(Advice:State) [AdviceDetermineState]

    NotifyEvent HoardWalkPendingAdvice Advice [list AskAdvice advice$id]
    SetTime "HoardWalk${id}:arrival"
}

proc WeakMissEvent { pathname program estimated_fetch_time timeout } {
    global Advice
    global Events
    global Indicator

    set text "Weak Miss"
    set id [AdviceGetID]

    set Advice(advice$id:Text) "$text: $pathname"
    set Advice(advice$id:State) "pendingAdvice"
    set Advice(advice$id:Type) WeaklyConnectedMiss
    set Advice(advice$id:Urgency) $Events(WeaklyConnectedCacheMissAdvice:Urgency)
    set Advice(advice$id:Command) [list WeakMissQuestionnaire $pathname $program $estimated_fetch_time $timeout]
    AdviceInsert "$text: $pathname" advice

    set Indicator(Advice:State) [AdviceDetermineState]

    NotifyEvent WeaklyConnectedCacheMissAdvice Advice [list AskAdvice advice$id]
    SetTime "WeakMiss${id}:arrival"
}

proc ReadDisconnectedCacheMissEvent { pathname program } {
    global Advice
    global Events
    global Indicator

    if { [regexp {.*\.coda\.cs\.cmu\.edu$} $pathname match prefix] == 1 } then {
	SendToAdviceMonitor "ReadMissQuestionnaire returns 0"
	return
    }

    set text "Read Disconnected Cache Miss"
    set id [AdviceGetID]

    set Advice(advice$id:Text) "$text: $pathname"
    set Advice(advice$id:State) "pendingAdvice"
    set Advice(advice$id:Type) ReadDisconnectedCacheMissAdvice
    set Advice(advice$id:Urgency) $Events(ReadDisconnectedCacheMissAdvice:Urgency)
    set Advice(advice$id:Command) [list ReadMissQuestionnaire $pathname $program]
    AdviceInsert "$text: $pathname" advice

    set Indicator(Advice:State) [AdviceDetermineState]

    NotifyEvent ReadDisconnectedCacheMissAdvice Advice [list AskAdvice advice$id]
    SetTime "ReadDisconnectedCacheMiss${id}:arrival"
}

proc ReconnectionEvent { outputfile date time length } {
    global Events
    global Advice
    global Indicator

    SendToAdviceMonitor {Advice Received ReconnectionEvent}

    set text "Reconnection Survey"
    set id [AdviceGetID]

    set Advice(advice$id:Text) "$text"
    set Advice(advice$id:State) "pendingAdvice"
    set Advice(advice$id:Type) ReconnectionSurvey
    set Advice(advice$id:Urgency) $Events(ReconnectionSurvey:Urgency)
    set Advice(advice$id:Command) [list ReconnectionQuestionnaire $outputfile $date $time $length]
    AdviceInsert "$text" advice

    set Indicator(Advice:State) [AdviceDetermineState]

    NotifyEvent ReconnectionSurvey Advice [list AskAdvice advice$id]
    SetTime "ReconnectionSurvey${id}:arrival"    
}
