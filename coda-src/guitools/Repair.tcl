##############################################################################
#
#
#  Repair Indicator
#
#
##############################################################################


proc RepairInitData { } {
    global Repair

    set Repair(IDnum) -1
    set Repair(NumPending) 0
    set Repair(MainWindow) .repair
    set Repair(Geometry) +0+0
}

proc RepairVisualStart { } {
    global DisplayStyle
    global Repair

    if { [winfo exists $Repair(MainWindow)] == 1 } then {
	raise $Repair(MainWindow)
	return
    }
    toplevel $Repair(MainWindow)
    wm title $Repair(MainWindow) "Repair Information"
    wm geometry $Repair(MainWindow) $Repair(Geometry)
    wm protocol $Repair(MainWindow) WM_DELETE_WINDOW {RepairVisualEnd}

    set Repair(pendingRepairFrame) $Repair(MainWindow).f
    tixLabelFrame $Repair(pendingRepairFrame) \
	-label "The following objects need to be repaired:" \
	-options {
	label.padX 5
    }
    pack $Repair(pendingRepairFrame) -side top \
	-padx 10 -pady 10 -expand true -fill both

    set pendingRepairHList [$Repair(pendingRepairFrame) subwidget frame]
    tixScrolledHList $pendingRepairHList.list
    pack $pendingRepairHList.list -side top \
	-padx 10 -pady 10 -expand yes -fill both 
    set Repair(pendingRepairHList) [$pendingRepairHList.list subwidget hlist]
    $Repair(pendingRepairHList) configure -command { LogAction "Repair Window: Selected object to repair"; DoRepair }
    for {set i 0} {$i <= $Repair(IDnum)} {incr i} {
        if { $Repair(repair$i:Valid) == 1 } then {
 	    $Repair(pendingRepairHList) add repair$i \
    	        -itemtype text -text $Repair(repair$i:Text) -style $DisplayStyle(Normal)
	}
    }

    set Repair(Buttons) [frame $Repair(MainWindow).buttons -bd 2 -relief groove]
    button $Repair(Buttons).repair -text "Repair Now" -anchor w \
	-bd 2 -relief groove -command { LogAction "Repair Window: RepairNow"; DoRepair ""}
    button $Repair(Buttons).help -text "Help" -anchor w \
	-bd 2 -relief groove -command { RepairHelpWindow }
#    button $Repair(Buttons).close -text "Close" -anchor w \
#	-bd 2 -relief groove -command { RepairVisualEnd }
    pack $Repair(Buttons).help -side left -padx 2 -pady 2
    pack $Repair(Buttons).repair -side right -padx 2 -pady 2
#    pack $Repair(Buttons).close -side right -padx 2 -pady 2
    pack $Repair(Buttons) -side bottom -fill x

}

proc RepairVisualEnd { } {
    global Repair

    LogAction "Repair Window: close"
    SetTime "RepairWindow:close"
    set Repair(Geometry) [wm geometry $Repair(MainWindow)]
    destroy $Repair(MainWindow)
}

proc RepairDetermineState { } {
    global Repair
 
    if { $Repair(NumPending) > 0 } then { return Critical } else { return Normal}
}

proc RepairCheckVisibilityAndUpdateScreen { } {
    global Repair

    if { [winfo exists $Repair(MainWindow)] == 1 } then {
        set Repair(Geometry) [wm geometry $Repair(MainWindow)]
	destroy $Repair(MainWindow)
        RepairVisualStart
    }
}

proc RepairBindings { } {
    global Window
    bind $Window.indicators.repair <Double-1> { 
	LogAction "Repair Indicator: double-click"
	SetTime "RepairIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	RepairVisualStart 
	update idletasks
	SetTime "RepairWindow:visible"
    }
}

proc RepairHelpWindow { } {
    global Repair

    LogAction "Repair Window: help"
    set helptext "The Repair window shows a list of objects that need repaired.  In the future, you will be able to invoke repair by double-clicking on the object's name."
    set Repair(RepairHelpWindow) .repairhelp
    PopupHelp "Repair Help" $Repair(RepairHelpWindow) $helptext "5i"
}

proc RepairInsert { path fid } {
    global Repair
    global DisplayStyle
    global Indicator

    incr Repair(IDnum)
    incr Repair(NumPending)
    if { [winfo exists $Repair(MainWindow)] == 1 } {
	$Repair(pendingRepairHList) add repair$Repair(IDnum) \
	    -itemtype text -text $path -style $DisplayStyle(Normal)
    }
    set Repair(repair$Repair(IDnum):Valid) 1
    set Repair(repair$Repair(IDnum):Text) $path
    set Repair(repair$Repair(IDnum):Fid) $fid
    set Indicator(Repair:State) [RepairDetermineState]
    return $Repair(IDnum)
}

proc RepairRemove { path fid } {
    global Repair
    global Indicator

    for { set i 0 } { $i <= $Repair(IDnum) } { incr i } {
	if { $fid == $Repair(repair$i:Fid) } then {
	    break
	}
    }
    if { $i > $Repair(IDnum) } then {
	SendToAdviceMonitor "$path does not have a repair pending"
	return
    }

    set Repair(NumPending) [expr $Repair(NumPending) - 1]
    if { [winfo exists $Repair(MainWindow)] == 1 } {
	$Repair(pendingRepairHList) delete entry repair$i 
    }
    set Repair(repair$i:Valid) 0
    set Repair(repair$i:Text) "bogus"
    set Repair(repair$i:Fid) "bogus"
    set Indicator(Repair:State) [RepairDetermineState]
}

proc IsRepairPending { path fid } {
    global Repair

    for { set i 0 } { $i <= $Repair(IDnum) } { incr i } {
	if { $fid == $Repair(repair$i:Fid) } then {
	    return 1
	}
    }
    return 0
}

proc RepairPendingEvent { pathname fidString } {
    global Repair
    global Indicator

    SendToStdErr "RepairPendingEvent $pathname $fidString"

    if { [IsRepairPending $pathname $fidString] == 1 } then {
	return
    }

    set id [RepairInsert $pathname $fidString]

    SetTime "Repair$id:insert"
    NotifyEvent RepairPending Repair RepairVisualStart
    RepairCheckVisibilityAndUpdateScreen
}

proc RepairCompleteEvent { pathname fidString } {
    global Repair
    global Indicator

    SendToStdErr "RepairCompleteEvent $pathname $fidString"

    set id [RepairRemove $pathname $fidString]

    SetTime "Repair$id:completed"
    NotifyEvent RepairCompleted Repair RepairVisualStart
    RepairCheckVisibilityAndUpdateScreen
}

proc DoRepair { idstring } {
    global Repair

    if { $idstring == "" } then {
	return
    }

    SetTime "$idstring:beginrepair"
    SendToAdviceMonitor [format "Repairing: %s" $Repair($idstring:Text)]
    RepairCompleteEvent $Repair($idstring:Text)
}