##############################################################################
#
#
#  Helper Routines for tcl/tk/tix
#
#
##############################################################################

proc Submit { seconds command message} {
    after [expr $seconds*1000] $command
    after [expr $seconds*1000] $message
}

proc CreatePage {command w name} {
    # Stolen from tix widget demo
    tixBusy $w on
    set id [after 10000 tixBusy $w off]
    $command $w $name
    after cancel $id
    after 0 tixBusy $w off
}

proc addLeadingZero { time } {
    regsub {^[0-9]$} $time 0& newtime
    return $newtime
}


proc watch {framename labelname varname} {
    frame $framename -relief flat -borderwidth 2
    label $framename.label -text "$labelname = "
    label $framename.value -textvariable $varname
    pack $framename.label -side left
    pack $framename.value -side right
}


proc PopupHelpNoGrab { windowname framename messagetext width } {
    toplevel $framename
    wm title $framename  $windowname
    wm geometry $framename -5-25
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width $width
    button $framename.ok \
	-text "OK" \
	-relief groove \
	-bd 6 \
	-command "LogAction HelpWindow:close; destroy $framename"
    bind $framename.ok <Return> { destroy $framename}
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.ok -side top -fill both -expand true -padx 10 -pady 10
    tkwait visibility $framename
    tkwait window $framename
}

proc PopupHelp { windowname framename messagetext width } {
    toplevel $framename
    wm title $framename  $windowname
    wm geometry $framename -5-25
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width $width
    button $framename.ok \
	-text "OK" \
	-relief groove \
	-bd 6 \
	-command "LogAction HelpWindow:close; destroy $framename"
    bind $framename.ok <Return> { destroy $framename}
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.ok -side top -fill both -expand true -padx 10 -pady 10
    tkwait visibility $framename
    grab set $framename
    tkwait window $framename
}

proc PopupNewName { windowname framename messagetext variable width OldNames} {
    toplevel $framename
    wm title $framename  $windowname
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width $width
    entry $framename.entry -width 32 -relief sunken -textvariable $variable

    frame $framename.buttons -relief flat
    button $framename.buttons.ok \
	-text "OK" \
	-relief groove \
	-bd 6 \
	-command "destroy $framename"
    button $framename.buttons.cancel \
	-text "cancel" \
	-relief groove \
	-bd 2 \
	-command "$framename.entry delete 0 end; destroy $framename"

    bind $framename.entry <Return> [list destroy $framename]
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.entry -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.buttons.ok -side right -padx 10  -pady 10
    pack $framename.buttons.cancel -side left  -padx 10  -pady 10
    pack $framename.buttons -side top -expand true -fill both
    focus $framename.entry
    update idletasks
    grab set $framename
    tkwait window $framename
}

proc PopupYesNo { windowname framename messagetext variable } {
    toplevel $framename
    wm title $framename  $windowname
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width "5i"
    frame $framename.rb -relief flat
    radiobutton $framename.rb.yes \
	-text "Yes" \
	-variable $variable \
	-value "yes" \
	-relief groove \
	-command { LogAction "PopupYesNo: yes" } \
	-bd 2 
    radiobutton $framename.rb.no \
	-text "No" \
	-variable $variable \
	-value "no" \
	-relief groove \
	-command { LogAction "PopupYesNo: no" } \
	-bd 2 
    bind $framename.rb.no <Return> { destroy $framename }
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.rb.no -side left -fill both -expand true -padx 10 -pady 10
    pack $framename.rb.yes -side right -fill both -expand true -padx 10 -pady 10
    pack $framename.rb -side top -fill both -expand true -padx 10 -pady 10
    update idletasks
    grab set $framename
}

proc PopupTransient { messagetext title } {
    global Transient

    if { ![info exists Transient(ID)] } then { set Transient(ID) 0 }

    set framename .transcient$Transient(ID)
    incr Transient(ID)

    toplevel $framename
    wm title $framename $title
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width "4i"
    pack $framename.msg -side top -expand true -fill x
    bell; bell; bell
    Submit 5 "destroy $framename" ""
}

proc PopupSafeDelete { windowname framename messagetext variable } {
    toplevel $framename
    wm title $framename  $windowname
    message $framename.msg \
	-text $messagetext \
	-justify left \
	-width "5i"
    frame $framename.buttons -relief flat
    radiobutton $framename.buttons.yes \
	-text "Yes " \
	-variable $variable \
	-value "yes" \
	-relief groove \
	-bd 2 
    radiobutton $framename.buttons.no \
	-text "No  " \
	-variable $variable \
	-value "no" \
	-relief groove \
	-bd 2 
    button $framename.buttons.cancelsafe \
	-text "Control Panel" \
	-command { ControlPanelVisualStart; global ControlPanel; $ControlPanel(MainWindow:Notebook) raise behavior } \
	-relief groove \
	-bd 2
    bind $framename.buttons.no <Return> { destroy $framename }
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.buttons.cancelsafe -side left -padx 10 -pady 10
    pack $framename.buttons.yes -side right -padx 20 -pady 10
    pack $framename.buttons.no -side right -padx 20 -pady 10
    pack $framename.buttons -side top -fill both -expand true -padx 10 -pady 10
    update idletasks
#    grab set $framename
}

proc SortAndRemoveDuplicates { list } {
    if { [llength $list] == 0 } { return $list }
    set sortedList [lsort $list]
    set lastElement [lindex $sortedList 0]
    set newList [list $lastElement]
    for { set i 1 } { $i < [llength $sortedList] } { incr i } {
	set element [lindex $sortedList $i]
	if { $element != $lastElement } then {
	    lappend newList $element
	}
	set lastElement $element
    }
    return $newList
}

proc ComboBoxFixEntryBindings {widget} {
    # Same as <BackSpace> below
    bind $widget <Delete> {
	if { [string length [%W get]] != 0 } then {
	    if { [%W select present] == 0 } then {
		tkEntryBackspace %W
		break
	    }
	}
    }

    # Same as <Delete> above
    bind $widget <BackSpace> {
	if { [string length [%W get]] != 0 } then {
	    if { [%W select present] == 0 } then {
		tkEntryBackspace %W
		break
	    }
	}
    }
}


#
# The following routines are taken from the examples in Brent Welch's book
# _Practical Programming in Tcl and Tk_.
#

proc CommandEntry { name label width command args } {
	frame $name
	label $name.label -text $label -width $width -anchor w
	eval {entry $name.entry -relief sunken} $args
	pack $name.label -side left
	pack $name.entry -side right -fill x -expand true
	bind $name.entry <Return> $command
	return $name.entry
}

proc MinimumNonNegative { a b } {
    if { ($a == -1) && ($b == -1) } then {
	return 0
    } 

    if { $a == -1 } then {
	return $b
    }
    if { $b == -1 } then {
	return $a
    }

    if { $a < $b } then {
	return $a
    } else {
	return $b
    }
}

proc GeometryLocationOnly { GeometryString } {
    set firstPlus [string first "+" $GeometryString]
    set firstMinus [string first "-" $GeometryString]
    return [string range $GeometryString [MinimumNonNegative $firstPlus $firstMinus] end]
}

proc GeometrySizeOnly { GeometryString } {
    set firstPlus [string first "+" $GeometryString]
    set firstMinus [string first "-" $GeometryString]
    return [string range $GeometryString 0 [expr [MinimumNonNegative $firstPlus $firstMinus]-1]]
}

