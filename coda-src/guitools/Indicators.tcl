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
#  Create the indicator panel
#
#
##############################################################################

proc CreateIndicators { } {
    global Indicator
    global Colors

    toplevel .indicators
    wm geometry .indicators -0+240

    # Give the indicator lights window a name so that the user can specify in their
    # X resources file that windows of this name should not have a title or borders.
    # Because the window will be reparented by the window manager, the window manager
    # will retain control of the window -- allowing moves and destroys.
    wm title .indicators  "Indicators"

    # Prevent window manager from adding a title and borders, and from controling the widget.
    # Window manager's inability to control the widget means that it cannot move or destroy it.
    # wm overrideredirect .indicators 1

    foreach indicator $Indicator(List) {
	regsub -all { } $indicator "" squishname
	set framename [string tolower $squishname]
	label .indicators.$framename \
	    -anchor w \
	    -text $indicator \
	    -font *times-bold-r-*-*-14* \
	    -background $Indicator(Background) \
	    -foreground $Colors($Indicator($squishname:Urgency))
        CommonIndicatorBindings .indicators.$framename
	pack .indicators.$framename -side top -expand true -fill x
    }

    SpecificIndicatorBindings
}

proc CommonIndicatorBindings { indicator } {
    global Indicator

    bind $indicator <Enter> \
	    [list %W configure -background $Indicator(HighlightBackground)]
    bind $indicator <Leave> \
	    [list %W configure -background $Indicator(Background)]
}

proc SpecificIndicatorBindings { } {
    ControlPanelBindings
    TokenBindings
    SpaceBindings
    NetworkBindings
    AdviceBindings
    HoardWalkBindings
    ReintegrationBindings
    RepairBindings
    TaskBindings
}

proc IndicatorOff { indicator } {
    global Window
    global Colors

    $Window.indicators.[string tolower $indicator] configure \
	-foreground $Colors(Unknown)
    update idletasks
}

proc IndicatorOn { indicator } {
    global Window
    global Indicator
    global Colors

    $Window.indicators.[string tolower $indicator] configure \
	-foreground $Colors($Indicator(${indicator}:Urgency))
    update idletasks
}

proc IndicatorColor { indicator color } {
    global Window

    $Window.indicators.[string tolower $indicator] configure \
	-foreground $color
    update idletasks
}

proc IndicatorFlash { times indicator oldColor } {
    global Colors
    global Indicator
    global Window

    set newColor $Colors($Indicator(${indicator}:Urgency))

    set FutureTime 0
    for { set i 1 } { $i <= $times } { incr i } {
	set FutureTime [expr $FutureTime + 500]
	lappend Indicator(${indicator}:PendingEvents) [after $FutureTime [list IndicatorColor $indicator $newColor]]
	set FutureTime [expr $FutureTime + 500]
	lappend Indicator(${indicator}:PendingEvents) [after $FutureTime [list IndicatorColor $indicator $oldColor]]
    }
    set FutureTime [expr $FutureTime + 500]
    lappend Indicator(${indicator}:PendingEvents) [after $FutureTime [list IndicatorColor $indicator $newColor]]
}

proc CancelPendingIndicatorEvents { indicator } {
    global Indicator

    if { [info exists Indicator(${indicator}:PendingEvents)] == 0 } then { return }

    foreach pendingEvent $Indicator(${indicator}:PendingEvents) {
        after cancel $pendingEvent
    }
    set Indicator(${indicator}:PendingEvents) [list ]
}

proc IndicatorUpdate { indicator } {
    global Indicator
    global Colors

    set Indicator(${indicator}:Urgency) [StateToUrgency ${indicator}]

    set framename [string tolower $indicator]
    .indicators.$framename configure \
	-foreground $Colors($Indicator(${indicator}:Urgency))
    SetTime "${indicator}Indicator:update"
}

proc IndicatorsUpdate { } {
    global Indicator

    foreach indicator $Indicator(List) {
	regsub -all { } $indicator "" squishname
        IndicatorUpdate $squishname 
    }
}

proc InitIndicatorArray { } {
    global Indicator

    set Indicator(List) [list "Control Panel" Tokens Space Network Advice "Hoard Walk" Reintegration Repair Task]

    set Indicator(Background) black
    set Indicator(HighlightBackground) gray15
    set Indicator(SelectedBackground) gray92

    set Indicator(ControlPanel:State)  [ControlPanelDetermineState]
    set Indicator(Tokens:State)        [TokensDetermineState]
    set Indicator(Space:State)         [SpaceDetermineState]
    set Indicator(Network:State)       [NetworkDetermineState]
    set Indicator(Advice:State)        [AdviceDetermineState]
    set Indicator(HoardWalk:State)     [HoardWalkDetermineState]
    set Indicator(Reintegration:State) [ReintegrationDetermineState]
    set Indicator(Repair:State)        [RepairDetermineState]
    set Indicator(Task:State)          [TaskDetermineState]

    set Indicator(ControlPanel:Urgency)  [StateToUrgency ControlPanel]
    set Indicator(Tokens:Urgency)        [StateToUrgency Tokens]
    set Indicator(Space:Urgency)         [StateToUrgency Space]
    set Indicator(Network:Urgency)       [StateToUrgency Network]
    set Indicator(Advice:Urgency)        [StateToUrgency Advice]
    set Indicator(HoardWalk:Urgency)     [StateToUrgency HoardWalk]
    set Indicator(Reintegration:Urgency) [StateToUrgency Reintegration]
    set Indicator(Repair:Urgency)        [StateToUrgency Repair]
    set Indicator(Task:Urgency)          [StateToUrgency Task]
} 

proc StateToUrgency { indicator } {
    global Indicator
    global Events


    switch -exact $indicator {
        ControlPanel { 
	    return Normal
	}

	Tokens { 
	    switch -exact $Indicator(Tokens:State) {
	        Unknown         { return Unknown }
	        Valid           { return $Events(TokensAcquired:Urgency) }
	        Expired          { return $Events(TokenExpiry:Urgency) }
		Expired&Pending { return $Events(ActivityPendingTokens:Urgency) }
	    }
	}

	Space { 
            return $Indicator(Space:State)
        }

	Network { 
            return $Events($Indicator(Network:State):Urgency)
        }

	Advice {
	    return $Indicator(Advice:State)
	}

	HoardWalk { 
	    switch -exact $Indicator(HoardWalk:State) {
	        Unknown		{ return $Events(HoardWalkOff:Urgency) }
		Active 		{ return $Events(HoardWalkBegin:Urgency) }
		Inactive 	{ return $Events(HoardWalkEnd:Urgency) }
		PendingAdvice 	{ return $Events(HoardWalkPendingAdvice:Urgency) }
	    }
        }

	Reintegration { 
            return $Indicator(Reintegration:State)
        }

	Repair {
	    return $Indicator(Repair:State)
	}

	Task { 
	    return $Events($Indicator(Task:State):Urgency)
        }
    }
}
