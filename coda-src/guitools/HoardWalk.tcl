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

##############################################################################
#
#
#  HoardWalk Indicator
#
#
##############################################################################

proc HoardWalkInitData { } {
    global HoardWalk

    set HoardWalk(MainWindow) .hoardwalk
    set HoardWalk(Geometry) +0+0

    set HoardWalk(State) "Inactive"
    set HoardWalk(State:secondary) ""
    set HoardWalk(Progress) 0

    set HoardWalk(PeriodicMessage:off) "The hoard daemon is not set to run periodically.  The next hoard walk will not occur until it is run manually (by you) or until it is setup to run periodically again."

    set HoardWalk(PeriodicMessage:on) "The next hoard walk is expected to occur in approximately %s minutes."
}

proc HoardWalkVisualStart { } {
    global Window
    global HoardWalk
    global Colors
    global Dimensions
    global DisplayStyle
    global Indicator
    global Pathnames
    global Bandwidth
    global Statistics
    global Servers
    global Events

    if { [winfo exists $HoardWalk(MainWindow)] == 1 } then {
	raise $HoardWalk(MainWindow)
	return
    }
    toplevel $HoardWalk(MainWindow)
    wm title $HoardWalk(MainWindow) "Hoard Walk Information"
    wm geometry $HoardWalk(MainWindow) $HoardWalk(Geometry)
    wm protocol $HoardWalk(MainWindow) WM_DELETE_WINDOW {HoardWalkVisualEnd}

    switch -exact $Indicator(HoardWalk:State) {
	Unknown {
	    set expected  "This is a temporary message."
	    set HoardWalk(NextMessage) $HoardWalk(MainWindow).expected 
	    message $HoardWalk(NextMessage) \
		-text $expected \
		-justify left \
		-width "4i"
 	    button $HoardWalk(MainWindow).hoardwalkhelp -text "Help" -anchor w \
		-relief groove -bd 2\
		-command "HoardWalkNextHelpWindow"
	    set HoardWalk(PeriodicButton) $HoardWalk(MainWindow).hoardwalkperiodic
	    button $HoardWalk(PeriodicButton) -text "Periodic" -anchor e \
		-relief groove -bd 2\
		-command "HoardWalkPeriodic"
	    button $HoardWalk(MainWindow).hoardwalknow -text "Walk Now" -anchor e \
		-relief groove -bd 2\
		-command "LogAction HoardWalkUnknown:walknow; DemandHoardWalk"

	    pack $HoardWalk(NextMessage) -side top -expand true -fill x
	    pack $HoardWalk(MainWindow).hoardwalknow -side right
	    pack $HoardWalk(MainWindow).hoardwalkhelp -side left
	    pack $HoardWalk(PeriodicButton) -side right

	    HoardWalkVisualConfigure
	}
        Active {
	    if { $HoardWalk(State:secondary) == "PendingAdvice" } then {
		set urgency [GetEventUrgency HoardWalkPendingAdvice]
		set color [GetColorFromUrgency $urgency]
	    } else {
		set color $Colors(meter)
	    }
	    
	    tixCodaMeter $HoardWalk(MainWindow).progress \
		-width $Dimensions(ProgressBar:Width) \
		-label "Hoard Walk Progress (in %)" \
		-leftLabel "0" \
		-rightLabel "100" \
		-foreground $color \
	        -percentFull [GetHoardWalkProgress] 

 	    button $HoardWalk(MainWindow).hoardwalkhelp \
		-text "Help" \
		-anchor w \
		-relief groove -bd 2 \
		-command "HoardWalkProgressHelpWindow"

	    pack $HoardWalk(MainWindow).progress -side top -expand true -fill x
	    pack $HoardWalk(MainWindow).hoardwalkhelp -side left 

	    update idletasks

	    HoardWalkProgressUpdate $HoardWalk(Progress)
        }
        Inactive { 
	    set expected  "This is a temporary message."
	    set HoardWalk(NextMessage) $HoardWalk(MainWindow).expected 
	    message $HoardWalk(NextMessage) \
		-text $expected \
		-justify left \
		-width "4i"
 	    button $HoardWalk(MainWindow).hoardwalkhelp -text "Help" -anchor w \
		-relief groove -bd 2\
		-command "HoardWalkNextHelpWindow"
	    set HoardWalk(PeriodicButton) $HoardWalk(MainWindow).hoardwalkperiodic
	    button $HoardWalk(PeriodicButton) -text "Periodic" -anchor e \
		-relief groove -bd 2\
		-command "HoardWalkPeriodic"
	    button $HoardWalk(MainWindow).hoardwalknow -text "Walk Now" -anchor e \
		-relief groove -bd 2\
		-command "LogAction HoardWalkInactive:walknow; DemandHoardWalk"

	    pack $HoardWalk(NextMessage) -side top -expand true -fill x
	    pack $HoardWalk(MainWindow).hoardwalknow -side right
	    pack $HoardWalk(MainWindow).hoardwalkhelp -side left
	    pack $HoardWalk(PeriodicButton) -side right

	    HoardWalkVisualConfigure
	}
    }   
}

proc HoardWalkCheckVisibilityAndUpdateScreen { } {
    global HoardWalk

    if { [winfo exists $HoardWalk(MainWindow)] == 1 } then {
	set HoardWalk(Geometry) [GeometryLocationOnly [wm geometry $HoardWalk(MainWindow)]]
	destroy $HoardWalk(MainWindow)
        HoardWalkVisualStart
    }
}


proc HoardWalkVisualConfigure { } {
    global HoardWalk

    if { $HoardWalk(State) == "Unknown" } then {
        $HoardWalk(NextMessage) configure \
	    -text $HoardWalk(PeriodicMessage:off)
	$HoardWalk(PeriodicButton) configure \
	    -text "Periodic on"
    } else {
        $HoardWalk(NextMessage) configure \
	    -text [format $HoardWalk(PeriodicMessage:on) [GetNextHoardWalk]]
	$HoardWalk(PeriodicButton) configure \
	    -text "Periodic off"
    }
}

proc HoardWalkVisualEnd { } {
    global HoardWalk

    LogAction "Hoard Walk Window: close"
    SetTime "HoardWalkWindow:close"
    set HoardWalk(Geometry) [GeometryLocationOnly [wm geometry $HoardWalk(MainWindow)]]
    destroy $HoardWalk(MainWindow)
}

proc HoardWalkDetermineState { } {
    global HoardWalk

    return $HoardWalk(State)
}

proc HoardWalkBindings { } {
    global Window
    bind $Window.indicators.hoardwalk <Double-1> { 
	LogAction "Hoard Walk Indicator: double-click"
	SetTime "HoardWalkIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	HoardWalkVisualStart
	update idletasks
	SetTime "HoardWalkWindow:visible"
    }
}

proc HoardWalkBeginEvent { } {
    global HoardWalk
    global Indicator

    set HoardWalk(State) "Active"
    set HoardWalk(Progress) 0
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    NotifyEvent HoardWalkBegin HoardWalk HoardWalkVisualStart
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:begin"
}

proc HoardWalkContinueEvent { } {
    global HoardWalk
    global Indicator

    set HoardWalk(State) "Active"
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    NotifyEvent HoardWalkBegin HoardWalk HoardWalkVisualStart
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:begin"
}

proc HoardWalkPendingAdviceEvent { input output } {
    global HoardWalk
    global Indicator

    set HoardWalk(State:secondary) "PendingAdvice"
    HoardWalkAdviceInit
    set HoardWalk(AdviceInput) $input
    set HoardWalk(AdviceOutput) $output
    HoardWalkAdviceEvent
    NotifyEvent HoardWalkPendingAdvice Advice [list HoardWalkAdvice]
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:pending"
}

proc HoardWalkProgressUpdate { percentDone } {
    global HoardWalk

    set HoardWalk(Progress) $percentDone
    if { [winfo exists $HoardWalk(MainWindow).progress] == 1 } then {
        $HoardWalk(MainWindow).progress config -percentFull $percentDone
    }
}

proc HoardWalkEndEvent { } {
    global HoardWalk
    global Indicator

    set HoardWalk(State) "Inactive"
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    NotifyEvent HoardWalkEnd HoardWalk HoardWalkVisualStart
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:end"
}

proc HoardWalkOnEvent { } {
    global HoardWalk
    global Indicator

    set HoardWalk(State) "Inactive"
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    NotifyEvent HoardWalkOn HoardWalk HoardWalkVisualStart
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:on"
}

proc HoardWalkOffEvent { } {
    global HoardWalk
    global Indicator

    set HoardWalk(State) "Unknown"
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    NotifyEvent HoardWalkOff HoardWalk HoardWalkVisualStart
    HoardWalkCheckVisibilityAndUpdateScreen
    SetTime "HoardWalkEvent:off"
}

proc HoardWalkPeriodic { } {
    global HoardWalk
    global Pathnames
    global Indicator

    LogAction "Hoard Walk Window: periodic"
    if { $HoardWalk(State) == "Unknown" } then {
        set HoardWalk(State) "Inactive"
	Lock Output
	SendToAdviceMonitor "HoardPeriodicOn"
	UnLock Output
    } else {
        set HoardWalk(State) "Unknown"
	Lock Output
	SendToAdviceMonitor "HoardPeriodicOff"
	UnLock Output
    }
    set Indicator(HoardWalk:State) [HoardWalkDetermineState]
    IndicatorUpdate HoardWalk
    HoardWalkVisualConfigure
    HoardWalkCheckVisibilityAndUpdateScreen
}

proc HoardWalkProgressHelpWindow { } {
    global HoardWalk

    LogAction "Hoard Walk Progress Window: help"
    set HoardWalk(ProgressHelpWindow) .progresshelp
    set helptext "This is the progress help text"
    PopupHelp "Progress Help Window" $HoardWalk(ProgressHelpWindow) $helptext "5i"
}

proc HoardWalkNextHelpWindow { } {
    global HoardWalk

    LogAction "Hoard Walk NextWalk Window: help"
    set HoardWalk(NextHelpWindow) .nexthelp
    set helptext "This window tells you when the next automatic hoard walk is expected to run.  It also allows you to trigger a hoard walk immediately and to turn periodic hoard walks off and on."
    PopupHelp "Next Hoard Walk Help Window" $HoardWalk(NextHelpWindow) $helptext "5i"
}


proc DemandHoardWalk { } {
    global Pathnames

    set hoardpid [exec $Pathnames(hoard) walk &]
}


