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
#  Control Panel
#
#
#
# The control panel "data structure" contains entries for each 
# panel of the control panel as well as some entries used in its
# implementation.  These include:
#
#	MainWindow --> contains the name of the main window's frame
#
#	The Events:<Event> fields contain the current (but not
#	committed) settings for event handling.
#
#	 	Events:<Event>:Message
#	 	Events:<Event>:Urgency
#	 	Events:<Event>:Notify
#	 	Events:<Event>:Notify:Popup
#	 	Events:<Event>:Notify:Beep
#	 	Events:<Event>:Notify:Flash
#
#
#	The Events:Panel fields contain the current values shown
#	in the Events panel's widgets.
#
#		Events:Panel:Message
#		Events:Panel:Urgency
#		Events:Panel:Notify
#		Events:Panel:Notify:Popup
#		Events:Panel:Notify:Beep
#		Events:Panel:Notify:Flash
#
#
# Each event entry contains the following information:
#	Name 			(char *)
#	Message			(char *)
#	Urgency 		(Critical, Warning, Normal)
#	Notify 			(boolean)
#	    Popup 		(boolean)
#	    Beep 		(boolean)
#	    Flash 		(boolean)
#
##############################################################################



proc ControlPanelVisualStart { } {
    global ControlPanel

    if { [winfo exists $ControlPanel(MainWindow)] == 1} then {
	raise $ControlPanel(MainWindow)
	return
    }
    toplevel $ControlPanel(MainWindow)
    wm title $ControlPanel(MainWindow) "Control Panel"
    wm geometry $ControlPanel(MainWindow) $ControlPanel(Geometry)
    wm protocol $ControlPanel(MainWindow) WM_DELETE_WINDOW {ControlPanelVisualEnd}

    set ControlPanel(MainWindow:Notebook) [MkNotebook $ControlPanel(MainWindow)]
#    set ControlPanel(MainWindow:Buttons) [MkControlButtons $ControlPanel(MainWindow)]

    pack $ControlPanel(MainWindow:Notebook) -side top -expand yes -fill both -padx 4 -pady 4
#    pack $ControlPanel(MainWindow:Buttons) -side bottom -fill x -padx 2 -pady 2
}

proc ControlPanelVisualEnd { } {
    global ControlPanel

    LogAction "Control Panel Window: close"
    SetTime "ControlPanelWindow:close"
    CommitAllEventConfigurations
    UrgencyCommitColors
    PhysicalCommitServerConnectivity
    LogicalCommitConnectivity
    BehaviorCommitChanges
    SaveConfigurationChanges
    set ControlPanel(Geometry) [wm geometry $ControlPanel(MainWindow)]    
    destroy $ControlPanel(MainWindow)
}

proc ControlPanelDetermineState { } {
    return Normal
}

proc ControlPanelInitData { } {
    global ControlPanel
    global Events
    global Colors
    global Behavior

    set ControlPanel(MainWindow) .cp
    set ControlPanel(Geometry) 700x475+0+0

    # Now modify the base configuration by any that have been user-specified.
    BehaviorInit 
    ReadAdviceConfiguration

    set ControlPanel(Events:DefaultMessage) "This is the default message"
    foreach event $Events(List) {
        set ControlPanel(Events:$event:Message) \
	    $Events($event:Message)
        set ControlPanel(Events:$event:Urgency) \
	    $Events($event:Urgency)
        set ControlPanel(Events:$event:Notify) \
	    $Events($event:Notify)
        set ControlPanel(Events:$event:Notify:Popup) \
	    $Events($event:Notify:Popup)
        set ControlPanel(Events:$event:Notify:Beep) \
	    $Events($event:Notify:Beep)
        set ControlPanel(Events:$event:Notify:Flash) \
	    $Events($event:Notify:Flash)
    }

    set ControlPanel(Colors:Unknown) $Colors(Unknown)
    set ControlPanel(Colors:Normal) $Colors(Normal)
    set ControlPanel(Colors:Warning) $Colors(Warning)
    set ControlPanel(Colors:Critical) $Colors(Critical)

    SendToStdErr "MARIA:  Determine logical connectivity"
    set ControlPanel(Logical:ReadConnectivity) "connected"
    set ControlPanel(Logical:ReadConnectivityCommitted) $ControlPanel(Logical:ReadConnectivity)
    set ControlPanel(Logical:WriteConnectivity) "connected"
    set ControlPanel(Logical:WriteConnectivityCommitted) $ControlPanel(Logical:WriteConnectivity)

    SendToStdErr "MARIA:  Determine connectivity"
    set ControlPanel(Physical:Connectivity) "connected"
    set ControlPanel(Physical:ConnectivityCommitted) $ControlPanel(Physical:Connectivity) 

    set ControlPanel(SafeDeletes:task) $Behavior(SafeDeletes:task)
    set ControlPanel(SafeDeletes:data) $Behavior(SafeDeletes:data)
    set ControlPanel(SafeDeletes:program) $Behavior(SafeDeletes:program)
}

proc ControlPanelBindings { } {
    global Window
    global Indicator

    bind $Window.indicators.controlpanel <Double-1> { 
	LogAction "Control Panel Indicator: double-click"
	SetTime "ControlPanelIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	ControlPanelVisualStart 
	update idletasks
	SetTime "ControlPanelWindow:visible"
    }
}

proc ControlPanelHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Window: help"
    set messagetext "The control panel allows you to configure the event notifications, physical connectivity to the servers, logical connectivity to the servers.  To switch to a different panel, click on its name."
    set ControlPanel(ControlPanelHelpWindow) .controlpanelhelp
    PopupHelp "Control Panel Help" $ControlPanel(ControlPanelHelpWindow) $messagetext "7i"
}

proc MkNotebook { top } {

    option add *TixNoteBook.tagPadX 6
    option add *TixNoteBook.tagPadY 4
    option add *TixNoteBook.borderWidth 2
    option add *TixNoteBook.font\
        -*-helvetica-bold-o-normal-*-14-*-*-*-*-*-*-*

    set w [tixNoteBook $top.f2 -ipadx 5 -ipady 5]

    $w add events -createcmd "CreatePage MkEvents $w events" \
	-label "Event Configuration" \
	-under 0
    $w add urgency -createcmd "CreatePage MkUrgency $w urgency" \
	-label "Urgency Colors" \
	-under 0
    $w add physical -createcmd "CreatePage MkPhysical $w physical" \
	-label "Physical\nConnectivity" \
	-under 0
    $w add logical -createcmd "CreatePage MkLogical $w logical" \
	-label "Logical\nConnectivity" \
	-under 0
    $w add behavior -createcmd "CreatePage MkBehavior $w behavior" \
	-label "Behavior" \
	-under 0

    return $w
}

proc MkControlButtons { top } {
    set w [frame $top.f1 -bd 2 -relief groove]

    button $w.ok -text "Commit All Panels" -relief groove -bd 6 \
	-command {CommitAllEventConfigurations; UrgencyCommitColors; PhysicalCommitServerConnectivity; LogicalCommitConnectivity; ControlPanelVisualEnd} 
    button $w.revert -text "Revert All Panels" -relief groove -bd 2 \
	-command {UrgencyRevertColors; PhysicalRevertServerConnectivity; LogicalRevertConnectivity; ControlPanelVisualEnd}
    button $w.help -text "Help" -relief groove -bd 2 \
	-command ControlPanelHelpWindow 

    pack $w.ok -side right -padx 4 -pady 4
    pack $w.revert -side right -padx 4 -pady 4
    pack $w.help -side left -padx 2 -pady 2

    return $w
}

proc MkEvents { nb page } {
    global ControlPanel

    LogAction "Control Panel: Click on Events tab"
    SetTime "ControlPanelMkEvents:begin"
    set w [$nb subwidget $page]

    set buttons [MkEventButtons $w]
    pack $buttons -side bottom -fill x -padx 4 -pady 4

    set leftFrame [frame $w.left -bd 0 -relief flat]

    tixLabelFrame $leftFrame.top -label "Select an indicator..."
    set leftTopFrame [$leftFrame.top subwidget frame]
    tixLabelFrame $leftFrame.bottom -label "and one of its events..."
    set leftBottomFrame [$leftFrame.bottom subwidget frame]

    set eventlist [MkEventList $leftBottomFrame Tokens]
    set eventmsg [MkEventMessage $leftBottomFrame]
    set indicatorlist [MkIndicatorList $leftTopFrame]
    pack $indicatorlist -side top -fill both -padx 4 -pady 4
    pack $eventlist -side top -fill both -padx 4 -pady 4
    pack $eventmsg -side top -padx 4 -pady 4

    tixLabelFrame $w.right -label "...to change its configuration"
    set rightFrame [$w.right subwidget frame]

    set eventpanel [MkEventPanel $rightFrame]
    pack $eventpanel -side top -expand true -fill both -padx 2 -pady 2

    pack $w.left -side left -expand true -fill both -padx 2 -pady 2
	pack $leftFrame.top -side top  -expand true -fill both -padx 2 -pady 2
	pack $leftFrame.bottom -side bottom -expand true -fill both -padx 2 -pady 2
    pack $w.right -side right -expand true -fill both -padx 2 -pady 2

#4/2/97
    SelectEvent TokensAcquired
    update idletasks
    SetTime "ControlPanelMkEvents:visible"
}

proc MkIndicatorList { top } {
    global Indicator
    global Events

    set w [frame $top.indicatorlist -bd 2 -relief groove]

    # Create and configure the events combobox
    tixComboBox $w.cbx1 -command "SelectIndicator" -options {
	    entry.width 35
	    listbox.height 8
  	}
    set ControlPanelData(indicatorlist) $w.cbx1

    # Text in quotes below must be unique prior to the first '('
    foreach indicator $Indicator(List) {
	if { $indicator != "Control Panel" } then {
   	    $w.cbx1 insert end $indicator
	}
    }

    $w.cbx1 pick 0

    pack $w.cbx1 -side left -padx 4 -pady 4

    return $w
}

proc SelectIndicator { indicator } {
    global ControlPanelData

    LogAction "Control Panel Events Tab: Selecting $indicator indicator"
    regsub -all " " $indicator "" Indicator
    set w $ControlPanelData(eventlist)
    pack forget $w$ControlPanelData(CurrentIndicator)
    set ControlPanelData(CurrentIndicator) $Indicator
    pack $w$Indicator -side left -padx 4 -pady 4
    $w$Indicator pick 0
}


proc MkEventList { top indicator} {
    global ControlPanelData
    global Indicator
    global Events

    regsub -all " " $indicator "" ThisIndicator

    set w [frame $top.eventlist -bd 2 -relief groove]

    # Create and configure the events combobox for each indicator
    foreach indicator $Indicator(List) {
        regsub -all " " $indicator "" IndicatorName
        tixComboBox $w.cbx${IndicatorName} -command "SelectEvent" -options {
	    entry.width 35
	    listbox.height 5
  	}

        foreach event $Events(List) {
	    if { $Events($event:Indicator) == $IndicatorName } then {
	        $w.cbx${IndicatorName} insert end $Events($event:Name)
	    }
	}
    }

    set ControlPanelData(CurrentIndicator) ${ThisIndicator}
    pack $w.cbx${ThisIndicator} -side left -padx 4 -pady 4
#4/2/97
    $w.cbx${ThisIndicator} pick 0

    set ControlPanelData(eventlist) $w.cbx
    return $w
}

proc MkEventMessage { top } {
    global ControlPanel

    set w [frame $top.eventmsg -bd 2 -relief groove]
    message $w.msg -width 300 -bd 2 -anchor n  \
	-text $ControlPanel(Events:DefaultMessage) -aspect 10000
    pack $w.msg -fill both -expand false -padx 6 -pady 6
    set ControlPanel(Events:Panel:Message) $w.msg
    return $w
}

proc SelectEvent { event } {
    global ControlPanel

    LogAction "Control Panel Events Tab: Selecting $event event"
    set eventname [format "%s()" [join [split $event] ""]]
    set ControlPanel(Events:SelectedEvent) \
	   [string range $eventname 0 [expr [string first "(" $eventname] - 1]]
    SetEventMessage $ControlPanel(Events:SelectedEvent)
    SetEventConfiguration 
}

proc SetEventConfiguration { } {
    global ControlPanel

    if { [array names ControlPanel Events:Panel:Notify ] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Notify)] == 1 } then {
	    set ControlPanel(Events:PanelVariable:Notify) \
	        $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify)
	}
    }

    if { [array names ControlPanel Events:Panel:Urgency ] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Urgency)] == 1 } then {
	    set ControlPanel(Events:PanelVariable:Urgency) \
		$ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Urgency)

	    if { $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify) == "No" } then {
		UrgencyDisable $ControlPanel(Events:Panel:Urgency)
	    } else { 
		UrgencyEnable $ControlPanel(Events:Panel:Urgency)
	    }
	}
    }

    if { [array names ControlPanel Events:Panel:Notify:Popup ] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Notify:Popup)] == 1 } then {
	    set ControlPanel(Events:PanelVariable:Notify:Popup) \
		$ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify:Popup)

	    if { $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify) == "No" } then {
	 	PanelDisable Notify:Popup
	    } else {
	 	PanelEnable Notify:Popup
	    }
	}
    }

    if { [array names ControlPanel Events:Panel:Notify:Beep ] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Notify:Beep)] == 1 } then {
	    set ControlPanel(Events:PanelVariable:Notify:Beep) \
	        $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify:Beep)

	    if { $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify) == "No" } then {
 	        PanelDisable Notify:Beep
	    } else {
 	        PanelEnable Notify:Beep
	    }
	}
    }

    if { [array names ControlPanel Events:Panel:Notify:Flash ] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Notify:Flash)] == 1 } then {
	    set ControlPanel(Events:PanelVariable:Notify:Flash) \
	        $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify:Flash)

 	    if { $ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Notify) == "No" } then {
 	        PanelDisable Notify:Flash
	    } else {
 	        PanelEnable Notify:Flash
	    }
	}
    }
}

proc SetEventMessage { event } {
    global ControlPanel

    if { [array names ControlPanel Events:Panel:Message] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Message)] == 1 } then {
            $ControlPanel(Events:Panel:Message) configure \
	        -text $ControlPanel(Events:$event:Message)
	}
    }
}

proc SetEventConfigurationParameter { parameter argument } {
    global ControlPanel

    LogAction "Control Panel Events Tab: Setting Notification for $parameter to $argument"
    set ControlPanel(Events:$ControlPanel(Events:SelectedEvent):$parameter) $argument

    if { $parameter == "Notify" } then {
	if { $argument == "No" } then {
	    UrgencyDisable $ControlPanel(Events:Panel:Urgency)
	    PanelDisable Notify:Popup
	    PanelDisable Notify:Beep
	    PanelDisable Notify:Flash
        } else {
	    UrgencyEnable $ControlPanel(Events:Panel:Urgency)
	    PanelEnable Notify:Popup
	    PanelEnable Notify:Beep
	    PanelEnable Notify:Flash
	}
    }
}

proc PanelEnable { ID } {
    global ControlPanel

    set frame $ControlPanel(Events:Panel:$ID)
    $frame.yes config -state normal
    $frame.no config -state normal
    $frame.label config -foreground [$frame.yes cget -foreground]

    set ControlPanel(Events:PanelVariable:$ID) \
	$ControlPanel(Events:$ControlPanel(Events:SelectedEvent):$ID)
}

proc PanelDisable { ID } {
    global ControlPanel

    set frame $ControlPanel(Events:Panel:$ID)
    $frame.yes config -state disabled
    $frame.no config -state disabled
    $frame.label config -foreground [$frame.yes cget -disabledforeground]

    set ControlPanel(Events:PanelVariable:$ID) "Unknown"
}

proc UrgencyEnable { w } {
    global ControlPanel

    $w.right.critical config -state normal
    $w.right.warning config -state normal
    $w.right.normal config -state normal
    $w.left.label config -foreground [$w.right.critical cget -foreground]
    set ControlPanel(Events:PanelVariable:Urgency) \
	$ControlPanel(Events:$ControlPanel(Events:SelectedEvent):Urgency)
}

proc UrgencyDisable { w } {
    global ControlPanel

    $w.right.critical config -state disabled
    $w.right.warning config -state disabled
    $w.right.normal config -state disabled
    $w.left.label config -foreground [$w.right.critical cget -disabledforeground]
    set ControlPanel(Events:PanelVariable:Urgency) "Unknown"
}

proc RevertThisEventConfiguration { } {
    global ControlPanel
    global Events

    LogAction "Control Panel Events Tab:  Click on Revert Button"
    set event $ControlPanel(Events:SelectedEvent)
    set ControlPanel(Events:$event:Notify) $Events($event:Notify)
    set ControlPanel(Events:$event:Urgency) $Events($event:Urgency)
    set ControlPanel(Events:$event:Notify:Popup) $Events($event:Notify:Popup)
    set ControlPanel(Events:$event:Notify:Beep) $Events($event:Notify:Beep)
    set ControlPanel(Events:$event:Notify:Flash) $Events($event:Notify:Flash)

    SetEventConfiguration
}

proc EventConfigurationHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Events Tab: help"
    set messagetext "This control panel allows you to configure event notification.  The left half of the screen allows you to select which event you would like to configure and describes that event.  The right half of the screen allows you to configure the selected event.  You may decide to have the system notify you of the event or you may decide that you would prefer not to know about the event.  If you want to be notified of the event, you can select the urgency level, whether or not the event notification will popup a window on your screen, whether or not the event will cause your machine to beep, and whether or not the event will cause your screen to flash.\nWhen you finish configuring one or more events, you can commit your changes by clicking on the \"Commit\" button.  If you decide you do not want to keep your changes, you can click on the \"Revert\" button"

    set ControlPanel(EventConfigurationHelpWindow) .eventconfighelp
    PopupHelp "Event Configuration Help Window" $ControlPanel(EventConfigurationHelpWindow) $messagetext "5i"
}

proc CommitThisEventConfiguration { } {
    global ControlPanel
    global Events

    LogAction "Control Panel Events Tab:  Click on Commit Button"
    set event $ControlPanel(Events:SelectedEvent)
    CommitEventConfiguration $event
}

proc CommitEventConfiguration { event } {
    global ControlPanel
    global Events

    set Events($event:Notify) $ControlPanel(Events:$event:Notify) 
    set Events($event:Urgency) $ControlPanel(Events:$event:Urgency) 
    set Events($event:Notify:Popup) $ControlPanel(Events:$event:Notify:Popup) 
    set Events($event:Notify:Beep) $ControlPanel(Events:$event:Notify:Beep) 
    set Events($event:Notify:Flash) $ControlPanel(Events:$event:Notify:Flash) 
}

proc CommitAllEventConfigurations { } {
    global Events

    set Length [llength $Events(List)]
    for {set i 0} {$i < $Length} {incr i} {
        CommitEventConfiguration [lindex $Events(List) $i]
    } 
}

# This routine makes the "Notify?" radiobuttons
proc MkEventNotify { top } {
    global ControlPanel

    set w [frame $top.eventnotify -bd 2 -relief groove]

    label $w.label \
	-anchor w \
	-text "Notify?" 
    radiobutton $w.yes \
	-variable ControlPanel(Events:PanelVariable:Notify) \
	-value "Yes" \
	-text "Yes" \
	-command "SetEventConfigurationParameter Notify Yes"
    radiobutton $w.no \
	-variable ControlPanel(Events:PanelVariable:Notify) \
	-value "No" \
	-text "No" \
	-command "SetEventConfigurationParameter Notify No"

    pack $w.label -side left 
    pack $w.no -side right -padx 5
    pack $w.yes -side right -padx 5

    set ControlPanel(Events:Panel:Notify) $w
    return $w
}

# This routine makes the "Urgency?" combobox
proc MkEventUrgency { top } {
    global ControlPanel
    global Colors

    set w [frame $top.colorcontrol -bd 2 -relief groove]
    frame $w.left -bd 0 -relief flat
    frame $w.right -bd 0 -relief flat
    label $w.left.label \
	-anchor w \
	-text "Urgency?"
    radiobutton $w.right.critical \
	-variable ControlPanel(Events:PanelVariable:Urgency) \
	-anchor w \
	-value "Critical" \
	-text "Critical" \
	-selectcolor $Colors(Critical) \
	-command "SetEventConfigurationParameter Urgency Critical" 
    radiobutton $w.right.warning \
	-variable ControlPanel(Events:PanelVariable:Urgency) \
	-anchor w \
	-value "Warning" \
	-text "Warning" \
	-selectcolor $Colors(Warning) \
	-command "SetEventConfigurationParameter Urgency Warning" 
    radiobutton $w.right.normal \
	-variable ControlPanel(Events:PanelVariable:Urgency) \
	-anchor w \
	-value "Normal" \
	-text "Normal" \
	-selectcolor $Colors(Normal) \
	-command "SetEventConfigurationParameter Urgency Normal" 
 
    pack $w.left.label -side left
    pack $w.right.critical -side top -padx 10
    pack $w.right.warning -side top -padx 10
    pack $w.right.normal -side top -padx 10
    pack $w.left -side left
    pack $w.right -side right

    set ControlPanel(Events:Panel:Urgency) $w
    return $w
}

proc UpdateEventUrgencyColors { } {
    global ControlPanel
    global Colors

    if { [array names ControlPanel Events:Panel:Urgency] != "" } then {
        if { [winfo exists $ControlPanel(Events:Panel:Urgency)] == 1 } then {
            set w $ControlPanel(Events:Panel:Urgency)

            $w.right.critical config -selectcolor $Colors(Critical)
            $w.right.warning config -selectcolor $Colors(Warning)
            $w.right.normal config -selectcolor $Colors(Normal)
	}
    }
}

# This routine makes the "Popup?" radiobuttons
proc MkEventPopup { top } {
    global ControlPanel

    set w [frame $top.popupcontrol -bd 2 -relief groove]

    label $w.label \
	-anchor w \
	-text "Popup?" 
    radiobutton $w.yes \
	-variable ControlPanel(Events:PanelVariable:Notify:Popup) \
	-value "Yes" \
	-text "Yes" \
	-command "SetEventConfigurationParameter Notify:Popup Yes"
    radiobutton $w.no \
	-variable ControlPanel(Events:PanelVariable:Notify:Popup) \
	-value "No" \
	-text "No" \
	-command "SetEventConfigurationParameter Notify:Popup No"

    pack $w.label -side left 
    pack $w.no -side right -padx 5
    pack $w.yes -side right -padx 5

    set ControlPanel(Events:Panel:Notify:Popup) $w
    return $w
}

# This routine makes the "Beep" radiobuttons
proc MkEventBeep { top } {
    global ControlPanel

    set w [frame $top.beepcontrol -bd 2 -relief groove]
    label $w.label \
	-anchor w \
	-text "Beep?" 
    radiobutton $w.yes \
	-variable ControlPanel(Events:PanelVariable:Notify:Beep) \
	-value "Yes" \
	-text "Yes" \
	-command "SetEventConfigurationParameter Notify:Beep Yes"
    radiobutton $w.no \
	-variable ControlPanel(Events:PanelVariable:Notify:Beep) \
	-value "No" \
	-text "No" \
	-command "SetEventConfigurationParameter Notify:Beep No"

    pack $w.label -side left 
    pack $w.no -side right -padx 5
    pack $w.yes -side right -padx 5

    set ControlPanel(Events:Panel:Notify:Beep) $w
    return $w
}

# This routine makes the "Flash" radiobuttons
proc MkEventFlash { top } {
    global ControlPanel

    set w [frame $top.flashcontrol -bd 2 -relief groove]
    label $w.label \
	-anchor w \
	-text "Flash?" 
    radiobutton $w.yes \
	-variable ControlPanel(Events:PanelVariable:Notify:Flash) \
	-value "Yes" \
	-text "Yes" \
	-command "SetEventConfigurationParameter Notify:Flash Yes"
    radiobutton $w.no \
	-variable ControlPanel(Events:PanelVariable:Notify:Flash) \
	-value "No" \
	-text "No" \
	-command "SetEventConfigurationParameter Notify:Flash No"

    pack $w.label -side left 
    pack $w.no -side right -padx 5
    pack $w.yes -side right -padx 5

    set ControlPanel(Events:Panel:Notify:Flash) $w
    return $w
}

proc MkEventPanel { top } {
    global ControlPanel

    set w [frame $top.eventpanel]

    set notify [MkEventNotify $w]
    set color [MkEventUrgency $w]
    set popup [MkEventPopup $w]
    set beep [MkEventBeep $w]
    set flash [MkEventFlash $w]

    pack $notify -side top -padx 4 -pady 4 -fill x -expand true
    pack $color -side top -padx 4 -pady 4 -fill x -expand true
    pack $popup -side top -padx 4 -pady 4 -fill x -expand true
    pack $beep -side top -padx 4 -pady 4 -fill x -expand true
    pack $flash -side top -padx 4 -pady 4 -fill x -expand true

    pack $w -side bottom

    return $w
}

proc MkEventButtons { top } {
    global ControlPanel

    set w [frame $top.eventbuttons -relief groove -bd 2]

    button $w.help -text "Help" -relief groove -bd 2 \
	-command EventConfigurationHelpWindow
    button $w.ok -text "Commit" -relief groove -bd 2 \
	-command CommitThisEventConfiguration
    button $w.revert -text "Revert" -relief groove -bd 2 \
	-command RevertThisEventConfiguration 
    pack $w.help -side left -padx 4 -pady 4
    pack $w.ok -side right -padx 4 -pady 4
    pack $w.revert -side right -padx 4 -pady 4
    set ControlPanel(Events:Panel:Buttons) $w
    return $w
}

proc MkUrgency { nb page } {
    global ControlPanel
    global Indicator
    global Urgency
    global Colors

    LogAction "Control Panel: Click on Urgency tab"
    SetTime "ControlPanelMkUrgency:begin"
    set w [$nb subwidget $page]

    set ControlPanel(Urgency:Comboboxes) [frame $w.f1 -bd 2 -relief groove]
    set ControlPanel(Urgency:Indicators) [frame $w.f2 -bd 2 -relief groove -bg $Indicator(Background)]
    set ControlPanel(Urgency:ColorSamples) [frame $w.f3 -bd 2 -relief groove -bg $Indicator(Background)]

    foreach color $Colors(List) {
        set window $ControlPanel(Urgency:ColorSamples).[string tolower $color] 
        label $window \
	    -anchor w \
	    -text $color \
	    -font *times-bold-r-*-*-14* \
	    -background $Indicator(Background) \
	    -foreground $color
	pack $window -side top
    }

    foreach urgency {Normal Warning Critical} {
        set window $ControlPanel(Urgency:Indicators).[string tolower $urgency] 
        label $window \
	    -anchor w \
	    -text $urgency \
	    -font *times-bold-r-*-*-14* \
	    -background $Indicator(Background) \
	    -foreground $Colors($urgency)
        $window configure -foreground purple
	pack $window -side top 
    }

    foreach level {Normal Warning Critical} {
  	set window $ControlPanel(Urgency:Comboboxes).[string tolower $level]
        tixComboBox $window \
	    -command "UrgencyChangeColor $level" \
	    -label $level \
	    -options {
	        label.width 10
   	        entry.width 10
	        listbox.height 7
            }
        foreach color $Colors(List) {
            $window insert end $color
        }
        pack $window -side top -padx 4 -pady 4
        set ControlPanel(Urgency:$level) $window

	$window pick [lsearch -exact $Colors(List) $Colors($level)]
    }

    set buttons [frame $w.buttons -relief groove -bd 2]
    button $buttons.help -text "Help" -relief groove -bd 2 \
	-command "UrgencyHelpWindow"
    button $buttons.ok -text "Commit" -relief groove -bd 2 \
	-command "UrgencyCommitColors"
    button $buttons.revert -text "Revert" -relief groove -bd 2 \
	-command "UrgencyRevertColors"
    pack $buttons.help -side left -padx 4 -pady 4
    pack $buttons.ok -side right -padx 4 -pady 4
    pack $buttons.revert -side right -padx 4 -pady 4
    set ControlPanel(Urgency:Panel:Buttons) $buttons

    pack $buttons -side bottom -padx 4 -pady 4  -fill x
    pack $ControlPanel(Urgency:ColorSamples) -side left -padx 40 -pady 10
    pack $ControlPanel(Urgency:Indicators) -side right -padx 40 -pady 10 
    pack $ControlPanel(Urgency:Comboboxes) -side left -padx 50 -pady 10
    update idletasks
    SetTime "ControlPanelMkUrgency:visible"
}

proc UrgencyHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Urgency Colors Tab: help"
    set messagetext "This control panel allows you to configuration the colors used to indicate urgency.  By default, red indicates an event is critical; yellow indicates an event is simply a warning; and, green indicates the event is normal.  As you select new colors, the color will show up in the small \"indicator\" panel on the right.  Once you are pleased with your color selections, click on the \"Commit\" window to make your changes take effect on the actual indicator lights.  Clicking on the \"Revert\" button will cause the colors to revert back to their last commited values."

    set ControlPanel(UrgencyHelpWindow) .urgencyhelp
    PopupHelp "Urgency Help Window" $ControlPanel(UrgencyHelpWindow) $messagetext "5i"

}

proc UrgencyChangeColor { level newColor } {
    global ControlPanel

    LogAction "Control Panel Urgency Colors Tab:  Changing $level to $newColor"
    $ControlPanel(Urgency:Indicators).[string tolower $level] configure \
	-foreground $newColor
}

proc UrgencyCommitColors { } {
    global ControlPanel
    global Colors

    LogAction "Control Panel Urgency Tab:  Click on Commit Button"
    if { ([info exists ControlPanel(Urgency:Comboboxes)]) && 
	 ([winfo exists $ControlPanel(Urgency:Comboboxes)]) } then {
        foreach urgency {Normal Warning Critical} {
            set Colors($urgency) [$ControlPanel(Urgency:Comboboxes).[string tolower $urgency] cget -value]
	}
    }

    IndicatorsUpdate
    AdviceWindowTextUpdate
    UpdateEventUrgencyColors
}

proc UrgencyRevertColors { } {
    global ControlPanel
    global Colors

    LogAction "Control Panel Urgency Tab:  Click on Revert Button"
    if { [info exists ControlPanel(Urgency:Comboboxes)] } then {
        foreach urgency {Normal Warning Critical} {
	    $ControlPanel(Urgency:Comboboxes).[string tolower $urgency] pick \
	        [lsearch -exact $Colors(List) $Colors($urgency)]
	}
    }
}

proc MkPhysical { nb page } {
    global ControlPanel
    global Physical

    LogAction "Control Panel: Click on Physical tab"
    SetTime "ControlPanelMkPhysical:begin"
    set w [$nb subwidget $page]

    set leftframe [frame $w.left -relief flat]
    set centerframe [frame $w.center -relief flat]
    set rightframe [frame $w.right -relief groove -bd 2]
    set buttonframe [frame $w.buttons -relief groove -bd 2]

    foreach s { scarlatti puccini rossini } {
        pack [MkServer $leftframe.$s $s] -side top -padx 10 -pady 10
    }

    foreach s { grieg haydn wagner } {
        pack [MkServer $centerframe.$s $s] -side top -padx 10 -pady 10
    }

    radiobutton $rightframe.disconnect \
	-variable ControlPanel(Physical:Connectivity) \
	-value "disconnected" \
	-text "Disconnect from ALL" \
	-command { LogAction "Control Panel Physical Tab: DisconnectFromALL"; DisconnectFromALL}
    radiobutton $rightframe.reconnect \
	-variable ControlPanel(Physical:Connectivity) \
	-value "connected" \
	-text "Connect to ALL" \
	-command { LogAction "Control Panel Physical Tab: ReconnectToALL"; ReconnectToALL}
    pack $rightframe.disconnect -side top -padx 10 -pady 5 -anchor w
    pack $rightframe.reconnect -side top -padx 10 -pady 5 -anchor w

    button $buttonframe.help -text "Help" -relief groove -bd 2 \
	-command "PhysicalHelpWindow"
    button $buttonframe.ok -text "Commit" -relief groove -bd 2 \
	-command "PhysicalCommitServerConnectivity"
    button $buttonframe.revert -text "Revert" -relief groove -bd 2 \
	-command "PhysicalRevertServerConnectivity"
    pack $buttonframe.help -side left -padx 4 -pady 4
    pack $buttonframe.ok -side right -padx 4 -pady 4
    pack $buttonframe.revert -side right -padx 4 -pady 4

    pack $buttonframe -side bottom -padx 4 -pady 4 -fill x
    pack $leftframe -side left -padx 20
    pack $centerframe -side left -padx 20
    pack $rightframe -side left -padx 20
    update idletasks
    SetTime "ControlPanelMkPhysical:visible"
}

proc PhysicalHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Physical Connectivity Tab: help"
    set messagetext "This control panel allows you to control connectivity to individual Coda servers as well as to the Coda servers as a group.  Click on the \"on\" or \"off\" buttons to set an individual server's connectivity.  Click on the \"Connect from ALL\" or the \"Disconnect to ALL\" buttons to connect or disconnect from all of the servers at once."
    set ControlPanel(PhysicalHelpWindow) .physicalhelp
    PopupHelp "Physical Help Window" $ControlPanel(PhysicalHelpWindow) $messagetext "5i"
}

proc MkServer { framename server } {
    global Servers
    global ControlPanel

    set maxServerName 0
    foreach index [array names Servers] { 
	set thisLength [string length $Servers($index)]
        if { $thisLength > $maxServerName } then { 
	    set maxServerName $thisLength
	}
    }

    frame $framename -bd 2 -relief groove
 
    label $framename.label \
	-width [expr $maxServerName + 2] \
	-anchor w \
	-text $server \
	-font fixed 
    pack $framename.label -side left


    frame $framename.buttons -relief flat
    set ControlPanel(Physical:$server:State) $Servers($server:State)
    radiobutton $framename.buttons.on -text "on" \
	-variable ControlPanel(Physical:$server:State) -value "on" -anchor w \
	-command [list LogAction "Control Panel Physical Tab: Connect to $server"]
    radiobutton $framename.buttons.off -text "off" \
	-variable ControlPanel(Physical:$server:State) -value "off" -anchor w \
	-command [list LogAction "Control Panel Physical Tab: Disconnect from $server"]
    pack $framename.buttons.on -side top
    pack $framename.buttons.off -side top
    pack $framename.buttons -side right

    return $framename
}

proc DisconnectFromALL { } {
    global Pathnames
    global ControlPanel
    
    # Set the control panel radiobuttons appropriately
    foreach server { scarlatti rossini puccini grieg haydn wagner } {
	set ControlPanel(Physical:$server:State) "off"
    }
}

proc ReconnectToALL { } {
    global ControlPanel

    # Set the control panel radiobuttons appropriately
    foreach server { scarlatti rossini puccini grieg haydn wagner } {
	set ControlPanel(Physical:$server:State) "on"
    }
}

proc PhysicalCommitServerConnectivity { } {
    global ControlPanel
    global Pathnames
    global Physical

    LogAction "Control Panel Physical Tab:  Click on Commit Button"
    if { ($ControlPanel(Physical:Connectivity) == "connected") && 
	 ($ControlPanel(Physical:ConnectivityCommitted) != "connected") } then {
	exec $Pathnames(cfs) reconnect
	set ControlPanel(Physical:ConnectivityCommitted) "connected"
    }

    if { ($ControlPanel(Physical:Connectivity) == "disconnected") && 
	 ($ControlPanel(Physical:ConnectivityCommitted) != "disconnected") } then {
	exec $Pathnames(cfs) disconnect
	set ControlPanel(Physical:ConnectivityCommitted) "disconnected"
    }

    if { ($ControlPanel(Physical:Connectivity) == "connected") } then {
        foreach server { scarlatti rossini puccini grieg haydn wagner } {
	    if { [info exists ControlPanel(Physical:$server:State)] } then {
   	        if { $ControlPanel(Physical:$server:State) == "off" } then {
	  	    exec $Pathnames(cfs) disconnect $server
                }
	    }
	}
    }

    if { ($ControlPanel(Physical:Connectivity) == "disconnected") } then {
        foreach server { scarlatti rossini puccini grieg haydn wagner } {
	    if { [info exists ControlPanel(Physical:$server:State)] } then {
   	        if { $ControlPanel(Physical:$server:State) == "on" } then {
 	  	    exec $Pathnames(cfs) reconnect $server
                }
	    }
	}
    }
}


proc PhysicalRevertServerConnectivity { } {
    global ControlPanel
    global Servers

    LogAction "Control Panel Physical Tab:  Click on Revert Button"
    set ControlPanel(Physical:Connectivity) $ControlPanel(Physical:ConnectivityCommitted)
    foreach server { scarlatti rossini puccini grieg haydn wagner } {
	set  ControlPanel(Physical:$server:State) $Servers($server:State)
    }
}

proc MkLogical { nb page } {
    global ControlPanel
    global Logical

    LogAction "Control Panel: Click on Logical tab"
    SetTime "ControlPanelMkLogical:begin"
    set w [$nb subwidget $page]

    tixLabelFrame $w.read -label "Read Connectivity" -options {
	label.padX 5
    }
    tixLabelFrame $w.write -label "Write Connectivity" -options {
	label.padX 5
    }

    set readframe [$w.read subwidget frame]
    set writeframe [$w.write subwidget frame]
    set buttonframe [frame $w.buttons -relief groove -bd 2]
    
    radiobutton $readframe.readconnected \
	-variable ControlPanel(Logical:ReadConnectivity) \
	-value "connected" \
	-command [list LogAction "Control Panel Logical Tab: Setting ReadConnectivity to connected" ]\
	-text "Fetch missing objects IMMEDIATELY"
    radiobutton $readframe.readdisconnected \
	-variable ControlPanel(Logical:ReadConnectivity) \
	-value "disconnected" \
	-command [list LogAction "Control Panel Logical Tab: Setting ReadConnectivity to disconnected" ]\
	-text "Query me before fetching missing objects"
    pack $readframe.readconnected -side top -padx 10 -pady 10 -anchor w
    pack $readframe.readdisconnected -side top -padx 10 -pady 10 -anchor w

    radiobutton $writeframe.writeconnected \
	-variable ControlPanel(Logical:WriteConnectivity) \
	-value "connected" \
	-command [list LogAction "Control Panel Logical Tab: Setting WriteConnectivity to connected" ]\
	-text "Propagate updates IMMEDIATELY"
    radiobutton $writeframe.writedisconnected \
	-variable ControlPanel(Logical:WriteConnectivity) \
	-value "disconnected" \
	-command [list LogAction "Control Panel Logical Tab: Setting WriteConnectivity to disconnected" ]\
	-text "Hold updates until further notice"
    pack $writeframe.writeconnected -side top -padx 10 -pady 10 -anchor w
    pack $writeframe.writedisconnected -side top -padx 10 -pady 10 -anchor w

    button $buttonframe.help -text "Help" -relief groove -bd 2 \
	-command "LogicalHelpWindow"
    button $buttonframe.ok -text "Commit" -relief groove -bd 2 \
	-command "LogicalCommitConnectivity"
    button $buttonframe.revert -text "Revert" -relief groove -bd 2 \
	-command "LogicalRevertConnectivity"
    button $buttonframe.wizard -text "Wizard" -relief groove -bd 2 \
	-command "ToggleWizard" -state disabled

    pack $buttonframe.help -side left -padx 4 -pady 4
    pack $buttonframe.ok -side right -padx 4 -pady 4
    pack $buttonframe.revert -side right -padx 4 -pady 4
    pack $buttonframe.wizard -side left -padx 4 -pady 4

    pack $buttonframe -side bottom -padx 4 -pady 4 -fill x
    pack $w.read -side top -pady 20 
    pack $w.write -side top -pady 20
    update idletasks
    SetTime "ControlPanelMkLogical:visible"
}

proc LogicalHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Logical Connectivity Tab: help"
    set messagetext "This control panel allows you to control connectivity to subtrees of the Coda File System.  Read disconnected means that cache misses become non-transparent even if Venus is connected to the network.  Write disconnected means that updates are sent to the server only after they have aged (default: 10 minutes).\nClick on the \"Read Connected to ALL subtrees\" or \"Read disconnected from ALL subtrees\" buttons to read connect or read disconnect from all subtrees of the Coda File System.  Similarly, for write connectivity.  Once the \"Wizard\" button is implemented, you will be able to control connectivity to individual subtrees."
    set ControlPanel(LogicalHelpWindow) .logicalhelp
    PopupHelp "Logical Help Window" $ControlPanel(LogicalHelpWindow) $messagetext "5i"
}

proc LogicalCommitConnectivity { } {
    global ControlPanel
    global Pathnames

    LogAction "Control Panel Logical Tab:  Click on Commit Button"
    if { $ControlPanel(Logical:ReadConnectivity) == "connected" } then {
	SendToStdErr "MARIA:  Implement read reconnect"
    } elseif { $ControlPanel(Logical:ReadConnectivity) == "disconnected" } then {
	SendToStdErr "MARIA:  Implement read disconnect"
    }

    if { $ControlPanel(Logical:WriteConnectivity) == "connected" } then {
	exec $Pathnames(cfs) wr	
    } elseif { $ControlPanel(Logical:WriteConnectivity) == "disconnected" } then {
	exec $Pathnames(cfs) wd
    }

    set ControlPanel(Logical:ReadConnectivityCommitted) $ControlPanel(Logical:ReadConnectivity)
    set ControlPanel(Logical:WriteConnectivityCommitted) $ControlPanel(Logical:WriteConnectivity)
}

proc LogicalRevertConnectivity { } {
    global ControlPanel

    LogAction "Control Panel Logical Tab:  Click on Revert Button"
    set ControlPanel(Logical:ReadConnectivity) $ControlPanel(Logical:ReadConnectivityCommitted)
    set ControlPanel(Logical:WriteConnectivity) $ControlPanel(Logical:WriteConnectivityCommitted)
}

proc MkBehavior { nb page } {
    global ControlPanel
    global Behavior

    LogAction "Control Panel: Click on Behavior tab"
    SetTime "ControlPanelMkBehavior:begin"
    set w [$nb subwidget $page]

    set buttonframe [frame $w.buttons -relief groove -bd 2]
    button $buttonframe.help -text "Help" -relief groove -bd 2 \
	-command "BehaviorHelpWindow"
    button $buttonframe.ok -text "Commit" -relief groove -bd 2 \
	-command "BehaviorCommitChanges"
    button $buttonframe.revert -text "Revert" -relief groove -bd 2 \
	-command "BehaviorRevertChanges"

    pack $buttonframe.help -side left -padx 4 -pady 4
    pack $buttonframe.ok -side right -padx 4 -pady 4
    pack $buttonframe.revert -side right -padx 4 -pady 4
    pack $buttonframe -side bottom -padx 4 -pady 4 -fill x

    message $w.msg \
	-text "Delete confirmation required for..." \
	-font *times-bold-r-*-*-18* \
	-anchor w \
	-justify left \
	-width "5i"
    pack $w.msg -side top -padx 4 -pady 4

    foreach element {data program task} {
	set elementFrame [frame $w.$element -relief groove -bd 2]
	label $elementFrame.label -text "...$element definitions?" -width 20
	radiobutton $elementFrame.safe \
	    -variable ControlPanel(SafeDeletes:$element) \
	    -value "on" \
	    -command [list LogAction "Control Panel Behavior Tab: Setting $element to safe"] \
	    -text "Yes"
	radiobutton $elementFrame.unsafe \
	    -variable ControlPanel(SafeDeletes:$element) \
	    -value "off" \
	    -command [list LogAction "Control Panel Behavior Tab: Setting $element to unsafe"] \
	    -text "No"
	pack $elementFrame.label -side left -padx 5 -pady 5 -anchor w
 	pack $elementFrame.safe -side top -anchor w
 	pack $elementFrame.unsafe -side top -anchor w
	pack $elementFrame -pady 15
    }
    update idletasks
    SetTime "ControlPanelMkBehavior:visible"
}

proc BehaviorInit { } {
    global Behavior
    global ControlPanel

    foreach element {data program task} {
        set Behavior(SafeDeletes:$element) "on"
    }
}

proc BehaviorHelpWindow { } {
    global ControlPanel

    LogAction "Control Panel Behavior Tab: help"
    set messagetext "This control panel allows you to control the behavior of the CodaConsole interface.  In particular, you can control whether or not the system queries you before deleting a data, program, or task definition."
    set ControlPanel(BehaviorHelpWindow) .behaviorhelp
    PopupHelp "Behavior Help Window" $ControlPanel(BehaviorHelpWindow) $messagetext "5i"
}

proc BehaviorCommitChanges { } {
    global Behavior
    global ControlPanel

    LogAction "Control Panel Behavior Tab:  Click on Commit Button"
    foreach element {data program task} {
        set Behavior(SafeDeletes:$element) $ControlPanel(SafeDeletes:$element)
    }
}

proc BehaviorRevertChanges { } {
    global Behavior
    global ControlPanel

    LogAction "Control Panel Behavior Tab:  Click on Revert Button"
    foreach element {data program task} {
        set ControlPanel(SafeDeletes:$element) $Behavior(SafeDeletes:$element)
    }
}

proc SaveConfigurationChanges { } {
    global Pathnames

    set AdviceRC [MyRenameAndOpen $Pathnames(advicerc) {WRONLY CREAT TRUNC}] 

    OutputEventConfiguration $AdviceRC
    OutputColorConfiguration $AdviceRC
    OutputBehaviorConfiguration $AdviceRC

    flush $AdviceRC
    close $AdviceRC
}

