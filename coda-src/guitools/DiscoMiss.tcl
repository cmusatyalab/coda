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
# A nice interface to the disconnected cache miss questionnaire
#
# History :
#  94/4/6 : mre@cs.cmu.edu : code
#  96/12/10 : mre@cs.cmu.edu : make it work with CodaConsole
#

#
#
#


proc Quit {} {
	global DiscoMiss

	if { [SanityCheck [$DiscoMiss(affect).scale get] $DiscoMiss(Determined)] == 0 } then {
		return
	}
        set outFile [open $DiscoMiss(outputfile) a]
	puts $outFile [format "Practice: %d" $DiscoMiss(Practice)]
	OutputAffect $outFile
	OutputComments $outFile
	flush $outFile
	close $outFile
	set DiscoMiss(Geometry) [wm geometry $DiscoMiss(MainWindow)]
	destroy $DiscoMiss(MainWindow)
        LogAction "Disco Miss: done"
  	return
}

proc PopUpDialog {} {
        set panel .discopanel

	LogAction "Disco Miss: please choose a number"
	toplevel $panel
	message $panel.msg \
		-text "\nPlease choose a value from 1 to 6 by clicking the mouse\nover your selection." \
		-justify left \
		-width "5i"
	button $panel.ok \
		-text "OK" \
		-relief groove \
		-bd 6 \
		-command "LogAction DiscoMiss:ok; destroy $panel"
	pack $panel.msg -side top -fill both
	pack $panel.ok -side bottom -pady 10 -padx 10
        bind $panel <Return> [list destroy $panel]
	grab set $panel
	tkwait window $panel
}

proc DiscoMissHelp { } {
	LogAction "Disco Miss: help"
        set helpmsg "\nA cache miss has occurred while disconnected.  It cannot be filled because 
there is no connection to the servers.  The purpose of this questionnaire is to determine how\
important this file is to your work.  By answering this questionnaire, you will help us to\
evaluate the Coda file system.  There are no wrong answers to these questions.\n\
\nThe most important question is how much you believe this cache miss will affect your current work.\
We ask that you choose a value between 1 (the cache miss will not affect you at all -- except for\
having to answer these questions) and 6 (the cache miss will seriously impede your progress).\
We realize that in some situations this question cannot be answered so you can check the 'It cannot\
be determined' box in this case.  We also realize that some disconnected sessions are really\
practice disconnections.  In this case, we ask that you pretend this had been a real disconnected\
session in order to answer the question and that you check the 'This is a practice disconnection'\
box.  If you have other comments, you can write them in the box provided.\n\n
Keyboard accelerators are provided for your convenience:\n
\t1-6\tto select that answer\n\
\td\tto check the 'It cannot be determined' box \n\
\tp\tto check the 'This is a practice disconnection' box \n\
\tc\tto move the focus to the comment entry area \n\
\t^D\tto move the focus out of the comments entry area \n\
\th or ?\tto see this message \n\
\t<Return>\tto click the 'Done' button \n"

	PopupHelp "Disconnected Cache Miss Help Window" .discohelp $helpmsg "7i"
}

proc SanityCheck {scale_value be_determined} {
	if { $scale_value == 0 } {
		if { $be_determined == 0 } {
			PopUpDialog
			return 0
		}
	}
	return 1
}

proc labeltext {path text length} {
    frame $path 
    label $path.label -text $text
	
    text $path.text -relief sunken -bd 2 -height 3 -width 60\
	-yscrollcommand "$path.scroll set"
    scrollbar $path.scroll -command "$path.text yview"

    bind $path.text <BackSpace> { }
    bind Text <Delete> { }
    bind $path.text <Delete> { 
	if {[%W tag nextrange sel 1.0 end] != "" } then {
	    %W delete sel.first sel.last
	} else {
	    %W delete insert-1c
	}
    }
    bind $path.text <Any-KeyPress> {
	LogAction "Disco Miss: keystroke"
    }
    bind $path.text <1> {
	LogAction "Disco Miss: click in comments"
    }

    pack $path.label -side left -anchor nw 
    pack $path.text -side left  -fill y -expand true
    pack $path.scroll -side left -fill y
}

proc PresentInfo {} {
	global DiscoMiss

	#
        # Informational Dialog
        #
	set DiscoMiss(info) $DiscoMiss(MainWindow).info

        frame $DiscoMiss(info) -relief raised -borderwidth 2
        message $DiscoMiss(info).msg \
		-text "\nA disconnected cache miss has occurred on the object `$DiscoMiss(pathname)'.\nThe object was referenced by `$DiscoMiss(program)'.\n" \
                -justify left \
                -width "6i"

        pack $DiscoMiss(info).msg -side top -fill both
}

proc SetAffectScale {} {
	global DiscoMiss

	if { $DiscoMiss(Determined) } {
		$DiscoMiss(affect).scale set 0
	}
}

proc ResetDetermined {value} {
	global DiscoMiss

	if { $value > 0 } {
		set DiscoMiss(Determined) 0
	}
}

proc QuestionAffect {} {
 	global DiscoMiss
	#
	# Affect of cache miss?
	#

        set DiscoMiss(affect) $DiscoMiss(MainWindow).affect

	frame $DiscoMiss(affect) -relief raised -borderwidth 2
	message $DiscoMiss(affect).msg \
		-text "How much do you believe this cache miss will affect your current work?" \
		-justify left \
		-width "6i"
	scale $DiscoMiss(affect).scale -from 0 -to 6 -length 400 -tickinterval 1 \
		-label "Invalid | Not at all!                                    Seriously Impede!" \
		-orient horizontal \
		-activebackground Gray \
		-command "LogAction DiscoMiss:scale; ResetDetermined"


	frame $DiscoMiss(affect).cbs -relief flat -borderwidth 2

	set DiscoMiss(Determined) 0
	checkbutton $DiscoMiss(affect).cbs.determined \
		-text "It cannot be determined." \
		-variable DiscoMiss(Determined) \
		-command "LogAction DiscoMiss:cannotdetermine; SetAffectScale"

	set DiscoMiss(Practice) 0
	checkbutton $DiscoMiss(affect).cbs.practice \
		-text "This is a practice disconnection." \
		-command "LogAction DiscoMiss:practicedisconnection" \
		-variable DiscoMiss(Practice)

	pack $DiscoMiss(affect).cbs.determined -side left -padx 100
	pack $DiscoMiss(affect).cbs.practice -side right -padx 10

	pack $DiscoMiss(affect).msg -side top -anchor nw
	pack $DiscoMiss(affect).scale -side top
	pack $DiscoMiss(affect).cbs -side top -anchor nw
}

proc OutputAffect {outFile} {
 	global DiscoMiss
	puts $outFile [format "ExpectedAffect: %s" [$DiscoMiss(affect).scale get]]
}

proc PresentComments {} {
 	global DiscoMiss
	set DiscoMiss(comments) $DiscoMiss(MainWindow).comments
	frame $DiscoMiss(comments) -relief raised -borderwidth 2
	labeltext $DiscoMiss(comments).label "Other comments:" 50

	pack $DiscoMiss(comments).label -side top -anchor nw -expand true
}

proc OutputComments {outFile} {
 	global DiscoMiss
	puts $outFile [format "OtherComments:\n%s\nEndComment.\n" [$DiscoMiss(comments).label.text get 1.0 end]]
}

proc PresentOtherOptions {} {
 	global DiscoMiss
	set DiscoMiss(other) $DiscoMiss(MainWindow).other
	frame $DiscoMiss(other) -relief raised -borderwidth 2
	button $DiscoMiss(other).donebutton \
		-command Quit \
		-relief groove \
		-bd 6 \
		-text "Done" \
		-state active
	button $DiscoMiss(other).helpbutton \
		-command DiscoMissHelp \
		-relief groove \
		-bd 2 \
		-text "Help" \
		-state active

	pack $DiscoMiss(other).donebutton -side right -pady 10 -padx 10
	pack $DiscoMiss(other).helpbutton -side left -pady 10 -padx 10
}

proc KeyboardAccelerators { } {
    global DiscoMiss

    bind $DiscoMiss(MainWindow) <Any-KeyPress> { Accelerate %W %K }
    bind $DiscoMiss(comments).label.text <Control-d> {focus $DiscoMiss(MainWindow)}
    bindtags $DiscoMiss(affect).scale [list $DiscoMiss(affect).scale $DiscoMiss(MainWindow) all]

    bind $DiscoMiss(affect).scale <Any-KeyPress> { 
	SendToStdErr "DiscoMiss: keypress on scale widget" 
    }

    bind $DiscoMiss(affect).scale <ButtonPress-1> {
	global DiscoMiss
	set DiscoMiss(ButtonPressed) 1
    }

    bind $DiscoMiss(affect).scale <Motion> {
	global DiscoMiss
	if { $DiscoMiss(ButtonPressed) == 1 } then {
	    set newValue [$DiscoMiss(affect).scale get %x %y]
            $DiscoMiss(affect).scale set $newValue
	    LogAction "Disco Miss: scale motion"
	}	
    }

    bind $DiscoMiss(affect).scale <ButtonRelease-1> { 
	global DiscoMiss
	set DiscoMiss(ButtonPressed) 0
        set newValue [$DiscoMiss(affect).scale get %x %y]
        $DiscoMiss(affect).scale set $newValue
    }
}

proc Accelerate { window keysym } {
 	global DiscoMiss
    if { $window == "$DiscoMiss(comments).label.text" } then {
	return
    }

    switch -regexp $keysym {
	[0-6]       { $DiscoMiss(affect).scale set $keysym }
	p	    { Toggle DiscoMiss(Practice) }
	d	    { Toggle DiscoMiss(Determined) }
	c	    { focus $DiscoMiss(comments).label.text }

	h	    { PopUpHelp }
	"question"  { PopUpHelp }

 	"Return"    { Quit }

	default     { } # IGNORE
    }
}

proc Toggle { variablename } {
    upvar #0 $variablename var

    if { $var == 0 } then { set var 1 } else { set var 0 }
}

proc DisconnectedCacheMissQuestionnaire { outputfile pathname program } {
    global DiscoMiss

    SendToStdErr {Received Request for Disconnected Cache Miss Questionnaire}

    set DiscoMiss(outputfile) $outputfile
    set DiscoMiss(pathname) $pathname
    set DiscoMiss(program) $program
    
    set DiscoMiss(MainWindow) .discomiss

    if { [winfo exists $DiscoMiss(MainWindow)] } then {
        PopupHelp "Disconnected Miss Questionnaire ERROR" .discomissduplicate \
	    "There is a disconnected miss questionnaire running already.  Please try again later." "5i"
   	return -1
    }

    toplevel $DiscoMiss(MainWindow)
    wm geometry $DiscoMiss(MainWindow) $DiscoMiss(Geometry)
    wm title $DiscoMiss(MainWindow) "Disconnected Cache Miss Questionnaire"

    #
    # Create all windows
    #
    PresentInfo
    QuestionAffect
    PresentComments
    PresentOtherOptions

    set DiscoMiss(ButtonPressed) 0


    pack $DiscoMiss(info) -side top -fill x 
    pack $DiscoMiss(affect) -side top -fill x
    pack $DiscoMiss(comments) -side top -fill x
    pack $DiscoMiss(other) -side top -fill x

    KeyboardAccelerators
    #
    # Now, wait for user actions...
    #
    tkwait window $DiscoMiss(MainWindow)
    return 0
}

