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

#
# Inform the user that a read miss has occurred.
#
# History :
#  96/02/13 : mre@cs.cmu.edu : code
#  96/12/10 : mre@cs.cmu.edu : make it work with CodaConsole
#

proc ReadMissQuestionnaire { pathname program} {
    global ReadMiss

    SendToAdviceMonitor {Received Request for Read Disconnected Cache Miss Questionnaire}

    set ReadMiss(MainWindow) .readmiss

    if { [winfo exists $ReadMiss(MainWindow)] } then {
        PopupHelp "Read Miss Questionnaire ERROR" .readmissduplicate \
	    "There is a read miss questionnaire running already.  Please try again later." "5i"
   	return -1
    }

    toplevel $ReadMiss(MainWindow)
    wm geometry $ReadMiss(MainWindow) $ReadMiss(Geometry)
    wm title $ReadMiss(MainWindow) "Read Miss"
    wm iconname $ReadMiss(MainWindow) Dialog

    set text [format "File: %s\nRequesting Program: %s" $pathname $program]
    set bitmap {}
    # The default is set to argument #0 (Fetch)
    set default 0
    set args [list Fetch {Coerce to Miss} {Add to HDB and then Fetch}]

    #
    # 1. Divide the main window into top and bottom.
    #
    set ReadMiss(Top) $ReadMiss(MainWindow).top
    set ReadMiss(Bottom) $ReadMiss(MainWindow).bottom

    frame $ReadMiss(Top) -relief raised -bd 1
    pack append  $ReadMiss(MainWindow) $ReadMiss(Top) {top fillx filly}
    frame $ReadMiss(Bottom) -relief raised -bd 1
    pack append $ReadMiss(MainWindow) $ReadMiss(Bottom) {bottom fillx filly}


    #
    # 2. Fill the top part with the bitmap and message.
    #

    message $ReadMiss(Top).msg -width 6i -text $text
    pack append $ReadMiss(Top) $ReadMiss(Top).msg \
	{right expand fillx filly padx 3m pady 3m}
    if {$bitmap != ""} {
	label $ReadMiss(Top).bitmap -bitmap $bitmap
	pack append $ReadMiss(Top) $ReadMiss(Top).bitmap {left padx 3m pady 3m}
    }
	

    #
    # 3. Create a row of buttons at the bottom of the dialog.
    #
	
    set i 0
    foreach b $args {
	if {$i == $default} {
	    button $ReadMiss(Bottom).button$i -text $b -command "set ReadMiss(button) $i" \
		 -bd 6 -relief groove 
	    frame $ReadMiss(Bottom).default -relief groove -bd 1
	    raise $ReadMiss(Bottom).button$i
	    pack append $ReadMiss(Bottom) $ReadMiss(Bottom).default \
		{right expand padx 3m pady 2m}
	    pack append $ReadMiss(Bottom) $ReadMiss(Bottom).button$i \
		{right padx 2m pady 2m}
	} else {
	    button $ReadMiss(Bottom).button$i -text $b -command "set ReadMiss(button) $i" -bd 2 -relief groove 
	    pack append $ReadMiss(Bottom) $ReadMiss(Bottom).button$i \
		{left expand padx 3m pady 3m}
	}
	incr i
    }

	
    #
    # 4. Set up a binding for <Return>, if there's a default.
    #    Set a grab and claim the focus, too.
    #

    if {$default >= 0} {
	bind $ReadMiss(MainWindow) <Return> \
	    "$ReadMiss(Bottom).button$default flash; set ReadMiss(button) $default"
    }

    #
    # 5. Wait for the user to respond, then restore the focus
    #    and return the index of the selected button.
    #

    tkwait variable ReadMiss(button)
    set ReadMiss(Geometry) [wm geometry $ReadMiss(MainWindow)]
    destroy $ReadMiss(MainWindow)

    return $ReadMiss(button)
}

