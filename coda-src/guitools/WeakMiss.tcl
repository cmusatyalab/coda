#
# Inform the user that a weak miss has occurred.
#
# History :
#  95/10/20 : mre@cs.cmu.edu : code
#  96/12/10 : mre@cs.cmu.edu : make it work with CodaConsole
#
#

proc MkWindow { w pathname program fetchTime } {
    global WeakMiss

    wm geometry $w $WeakMiss(Geometry)
    wm title $w "Weak Miss"
    wm iconname $w Dialog

    set bitmap {}

    # Define button names and setup the default (to button 0 -- Fetch)
    set default 0
    set buttonList [list {Fetch} {Don't Fetch}]

    #
    # 1. Create the top-level window and divide it into top and bottom.
    #
    set WeakMiss(top) $w.top
    frame $WeakMiss(top) -relief raised -bd 1
    pack $WeakMiss(top) -side top -fill both

    set WeakMiss(bottom) $w.bottom
    frame $WeakMiss(bottom) -relief raised -bd 1
    pack $WeakMiss(bottom) -side bottom -fill both

    set WeakMiss(prod) $w.prod
    frame $WeakMiss(prod) -relief raised -bd 1


    #
    # 2. Fill the top part with the bitmap and message.
    #
    set WeakMiss(message) $WeakMiss(top).msg
    message $WeakMiss(message) \
	-width 6i \
	-text [format "File: %s\nRequesting Program: %s\nEstimated Fetch Time:  %s" \
		$pathname $program $fetchTime]

    pack $WeakMiss(message) -side top -fill both -padx 3m -pady 3m -expand true
    if {$bitmap != ""} {
    	label $WeakMiss(top).bitmap -bitmap $bitmap
	pack $WeakMiss(top).bitmap -side left -padx 3m -pady 3m
    }
	

    #
    # 3. Create a row of buttons at the bottom of the dialog.
    #
    MkButtons $WeakMiss(bottom) $buttonList $default


    #
    # 4. Set up a binding for <Return>, if there's a default.
    #    Set a grab and claim the focus, too.
    #
    if {$default >= 0} {
	bind $w <Return> "$WeakMiss(bottom).button$default flash; \
		set WeakMiss(result) $default"
    }

    #
    # 5. Set a timer for 15 seconds.  If the user doesn't respond
    #    in that timeframe, prod them...
    #
#    set WeakMiss(Prod) [after 15000 Prod]
    set WeakMiss(Prod) [after $WeakMiss(TimeOut) Prod]
}

proc MkButtons { frame buttonsList default } {
    global WeakMiss

    set i 0

    frame $frame.default -relief flat -bd 1
    pack $frame.default -side right -expand true -padx 3m -pady 2m

    foreach b $buttonsList {
	if {$i == $default} {
	    button $frame.default.button$i -text $b -command "LogAction WeakMissQuery:select$i; set WeakMiss(result) $i" -bd 6 -relief groove
	    pack $frame.default.button$i -side right -padx 2m -pady 2m
	} else {
	    button $frame.button$i -text $b -command "LogAction WeakMissQuery:select$i; set WeakMiss(result) $i" -bd 2 -relief groove
	    pack $frame.button$i -side right -expand true -padx 3m -pady 3m
	}
	incr i
    }
}

proc Prod { } {
    global WeakMiss

    if { ![winfo exists $WeakMiss(MainWindow)] } then { 
	puts "WeakMiss.tcl:Prod  Warning -- window is gone"
	return 
    }
    raise $WeakMiss(MainWindow)
    set newDefault 0

    message $WeakMiss(MainWindow).prodmsg -width 6i \
	-text "Do you need more time?" \
	-font *times-bold-r-*-*-18*
    pack $WeakMiss(MainWindow).prodmsg -after $WeakMiss(message) \
	-padx 3m -pady 3m -fill both -expand true

    pack forget $WeakMiss(bottom)
    
    MkButtons $WeakMiss(prod) [list {No, please fetch} {No, don't fetch} {Yes, please wait for my command}] $newDefault
    pack $WeakMiss(prod) -side bottom -fill both

#    set WeakMiss(FinalTimeout) [after 15000 [list set WeakMiss(result) 0]]
    set WeakMiss(FinalTimeout) [after $WeakMiss(TimeOut) [list set WeakMiss(result) 0]]
}

proc WaitForever { } {
    global WeakMiss

    $WeakMiss(MainWindow).prodmsg configure -text "Waiting for your command..."    
    pack forget $WeakMiss(prod)
    pack $WeakMiss(bottom) -side bottom -fill both
}

proc WeakMissQuestionnaire { pathname program fetchTime timeOut } {
    global WeakMiss

    set WeakMiss(MainWindow) .weakmiss

    if { [winfo exists $WeakMiss(MainWindow)] } then {
        PopupHelp "Weak Miss Questionnaire ERROR" .weakmissduplicate \
	    "There is a weak miss questionnaire running already.  Please try again later." "5i"
   	return -1
    }

    set WeakMiss(TimeOut) [expr $timeOut * 1000]

    toplevel $WeakMiss(MainWindow)
    MkWindow $WeakMiss(MainWindow) $pathname $program $fetchTime

    #
    # Now wait for the user to respond, then restore the focus
    # and return the index of the selected button.
    #
puts "tkwaiting in WeakMiss"
    tkwait variable WeakMiss(result)
    after cancel $WeakMiss(Prod)

    ## No, please fetch = 0
    ## No, don't fetch = 1
    ## Yes, please wait for my command = 2
    if { $WeakMiss(result) == 2 } then { 
        after cancel $WeakMiss(FinalTimeout)
        WaitForever 
        tkwait variable WeakMiss(result)
    }
 
    if { [info exists WeakMiss(FinalTimeout)] } then {
        after cancel $WeakMiss(FinalTimeout)
    }
    set WeakMiss(Geometry) [GeometryLocationOnly [wm geometry $WeakMiss(MainWindow)]]
    destroy $WeakMiss(MainWindow)
    return $WeakMiss(result)
}

