##############################################################################
#
#
#  Reintegration Indicator
#
#
##############################################################################

proc ReintegrationInitData { } {
    global Reintegration

    set Reintegration(IDnum) -1
    set Reintegration(NumPending) 0
    set Reintegration(MainWindow) .reintegration
    set Reintegration(Geometry) +0+0
    set Reintegration(Active) 0
}

proc ReintegrationVisualStart { } {
    global DisplayStyle
    global Reintegration

    if { [winfo exists $Reintegration(MainWindow)] == 1 } then {
	raise $Reintegration(MainWindow)
	return
    }
    toplevel $Reintegration(MainWindow)
    wm title $Reintegration(MainWindow) "Reintegration Information"
    wm geometry $Reintegration(MainWindow) $Reintegration(Geometry)
    wm protocol $Reintegration(MainWindow) WM_DELETE_WINDOW {ReintegrationVisualEnd}

    set Reintegration(pendingReintegrationFrame) $Reintegration(MainWindow).f
    tixLabelFrame $Reintegration(pendingReintegrationFrame) \
	-label "Objects in the following trees need to be reintegrated:" \
	-options {
	label.padX 5
    }
    pack $Reintegration(pendingReintegrationFrame) -side top \
	-padx 10 -pady 10 -expand true -fill both

    set pendingReintegrationHList [$Reintegration(pendingReintegrationFrame) subwidget frame]
    tixScrolledHList $pendingReintegrationHList.list
    pack $pendingReintegrationHList.list -side top \
	-padx 10 -pady 10 -expand yes -fill both 
    set Reintegration(pendingReintegrationHList) [$pendingReintegrationHList.list subwidget hlist]
    $Reintegration(pendingReintegrationHList) configure -command { LogAction "Reintegration Window: Selected Subtree To Reintegrate"; DoReintegrate }
    for {set i 0} {$i <= $Reintegration(IDnum)} {incr i} {
	if { $Reintegration(reintegration$i:Valid) == 1 } then {
	    $Reintegration(pendingReintegrationHList) add reintegration$i \
    	        -itemtype text -text $Reintegration(reintegration$i:Text) \
		-style $DisplayStyle(Normal)
	}
    }

    set Reintegration(Buttons) [frame $Reintegration(MainWindow).buttons -bd 2 -relief groove]
    button $Reintegration(Buttons).reintegration -text "Reintegrate Now" -anchor w \
	-bd 2 -relief groove -command { LogAction "Reintegration Window: ReintegrateNow"; DoReintegrate ""}
    button $Reintegration(Buttons).help -text "Help" -anchor w \
	-bd 2 -relief groove -command { ReintegrationHelpWindow }
    pack $Reintegration(Buttons).help -side left -padx 2 -pady 2
    pack $Reintegration(Buttons).reintegration -side right -padx 2 -pady 2
    pack $Reintegration(Buttons) -side bottom -fill x
}

proc ReintegrationVisualEnd { } {
    global Reintegration

    LogAction "Reintegration Window: close"
    SetTime "ReintegrationWindow:close"
    set Reintegration(Geometry) [wm geometry $Reintegration(MainWindow)]
    destroy $Reintegration(MainWindow)
}

proc ReintegrationDetermineState { } {
    global Reintegration
 
    if { $Reintegration(NumPending) > 0 } then { 
	return Critical 
    } elseif { $Reintegration(Active) == 1 } then {
	return Warning
    } else { 
	return Normal
    }
}

proc ReintegrationCheckVisibilityAndUpdateScreen { } {
    global Reintegration

    if { [winfo exists $Reintegration(MainWindow)] == 1 } then {
        set Reintegration(Geometry) [wm geometry $Reintegration(MainWindow)]
	destroy $Reintegration(MainWindow)
        ReintegrationVisualStart
    }
}

proc ReintegrationBindings { } {
    global Window
    bind $Window.indicators.reintegration <Double-1> { 
	LogAction "Reintegration Indicator: double-click"
	SetTime "Reintegration:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	ReintegrationVisualStart 
	update idletasks
	SetTime "Reintegration:visible"
    }
}

proc ReintegrationHelpWindow { } {
    global Reintegration

    LogAction "Reintegration Window: help"
    set helptext "The Reintegration Information window shows a list of subtrees that need to be reintegrated.  In the future, you will be able to control what subtrees and how much of each subtree will be reintegrated."
    set Reintegration(ReintegrationHelpWindow) .reintegrationhelp
    PopupHelp "Reintegration Help" $Reintegration(ReintegrationHelpWindow) $helptext "5i"
}

proc ReintegrationPendingEvent { pathname } {
    global Reintegration
    global Indicator

    set id [ReintegrationInsert $pathname]
    set Indicator(Reintegration:State) [ReintegrationDetermineState]
#    set Reintegration(reintegration$id:Text) $pathname

    NotifyEvent ReintegrationPending Reintegration ReintegrationVisualStart
    SetTime "Reintegration$id:insert"
    ReintegrationCheckVisibilityAndUpdateScreen
    ActivityPendingTokensEvent reintegration $pathname
}

proc ReintegrationEnabledEvent { pathname } {
    global Reintegration
    global Indicator

    set id [ReintegrationRemove $pathname]
    set Indicator(Reintegration:State) [ReintegrationDetermineState]

    NotifyEvent ReintegrationEnabled Reintegration ReintegrationVisualStart
    ReintegrationCheckVisibilityAndUpdateScreen
}

proc ReintegrationActiveEvent { pathname } {
    global Reintegration
    global Indicator

    set Reintegration(Active) 1
    set Indicator(Reintegration:State) [ReintegrationDetermineState]

    NotifyEvent ReintegrationActive Reintegration ReintegrationVisualStart
    ReintegrationCheckVisibilityAndUpdateScreen
}

proc ReintegrationCompletedEvent { pathname } {
    global Reintegration
    global Indicator

    set Reintegration(Active) 0
    set Indicator(Reintegration:State) [ReintegrationDetermineState]

    NotifyEvent ReintegrationCompleted Reintegration ReintegrationVisualStart
    ReintegrationCheckVisibilityAndUpdateScreen
}

proc DoReintegrate { idstring } {
    global Reintegration

    if { $idstring == "" } then {
	return 
    }

    SetTime "$idstring:beginreintegration"
    SendToAdviceMonitor [format "Reintegrating: %s" $Reintegration($idstring:Text)]
    ReintegrationCompletedEvent  $Reintegration($idstring:Text)
}

proc ReintegrationInsert { text } {
    global Reintegration
    global DisplayStyle
    global Indicator

    incr Reintegration(IDnum)
    incr Reintegration(NumPending)
    if { [winfo exists $Reintegration(MainWindow)] == 1 } {
	$Reintegration(pendingReintegrationHList) add reintegration$Reintegration(IDnum) \
	    -itemtype text -text $text -style $DisplayStyle(Normal)
    }
    set Reintegration(reintegration$Reintegration(IDnum):Valid) 1
    set Reintegration(reintegration$Reintegration(IDnum):Text) $text
    return $Reintegration(IDnum)
}

proc ReintegrationRemove { text } {
    global Reintegration
    global Indicator

    for { set i 0 } { $i <= $Reintegration(IDnum) } { incr i } {
	if { $text == $Reintegration(reintegration$i:Text) } then {
	    break
	}
    }
    if { $i > $Reintegration(IDnum) } then {
	SendToAdviceMonitor "$text does not have a reintegration pending"
	return
    }

    set Reintegration(NumPending) [expr $Reintegration(NumPending) - 1]
    if { [winfo exists $Reintegration(MainWindow)] == 1 } {
	$Reintegration(pendingReintegrationHList) delete entry reintegration$i 
    }
    set Reintegration(reintegration$i:Valid) 0
    set Reintegration(reintegration$i:Text) "bogus"
    set Indicator(Reintegration:State) [ReintegrationDetermineState]
    return $i
}

