#
# A nice interface to the reconnection questionnaire
#
# History :
#  94/3/19 : mre@cs.cmu.edu : code
#  97/08/16 : mre@cs.cmu.edu : make it work with CodaConsole
#

#
#
#

# Changing colors...
#       -activebackground $pink2 \
#    set pink2 [format {#%2x%2x%2x} 238 169 184]
# or maybe
#    set pink2 [format {#eea9b8}]

proc quit {} {
    global Reconnection

    set Reconnection(Sanitized) 0

    SanityCheck
    if { $Reconnection(Sanitized) == 0 } { return }

    set outFile [open [lindex $Reconnection(outputfile) 0] a]

    puts $outFile [format "AwareOfDisconnection: %s" $Reconnection(AwareOfDisconnection)]

    case $Reconnection(AwareOfDisconnection) in {
        {Yes}
            {
                puts $outFile [format "VoluntaryDisconnection: %s" $Reconnection(VoluntaryDisconnection)]
                puts $outFile [format "PracticeDisconnection: %s" $Reconnection(PracticeDisconnection)]
                puts $outFile [format "Codacon: %s" $Reconnection(codacon)]
                puts $outFile [format "Cmon: %s" $Reconnection(cmon)]
                puts $outFile [format "Stoplight: %s" $Reconnection(stoplight)]
                puts $outFile [format "Sluggish: %s" $Reconnection(sluggishness)]
                puts $outFile [format "ObservedCacheMiss: %s" $Reconnection(ObservedCacheMiss)]
                puts $outFile [format "KnownOtherComments:\n%s\nEndComment.\n" [$Reconnection(known).other.comments.text get 1.0 end]]

                OutputPrepare $outFile
                OutputOverall $outFile
            }
        {No}
            {

            }
        {Suspected}
            {
                puts $outFile [format "Codacon: %s" $Reconnection(codacon)]
                puts $outFile [format "Cmon: %s" $Reconnection(cmon)]
                puts $outFile [format "Stoplight: %s" $Reconnection(stoplight)]
                puts $outFile [format "Sluggish: %s" $Reconnection(sluggishness)]
                puts $outFile [format "ObservedCacheMiss: %s" $Reconnection(ObservedCacheMiss)]
                puts $outFile [format "SuspectedOtherComments:\n%s\nEndComment.\n" [$Reconnection(suspicions).other.comments.text get 1.0 end]]
                OutputOverall $outFile

            }
    }

    puts $outFile [format "FinalComments:\n%s\nEndComment.\n" [$Reconnection(comments).label.text get 1.0 end]]

    close $outFile
    destroy $Reconnection(MainWindow)
}


#
# Sanity Checks
#
proc SanityCheck { } {
    global Reconnection

    set Reconnection(Sanitized) 0
    case $Reconnection(AwareOfDisconnection) in {
        {Yes}
        {
            set Reconnection(Sanitized) 0
            set OtherKnowledge 0

            if { [string length [$Reconnection(known).other.comments.text get 1.0 end]] > 0 } then {
            set OtherKnowledge 1 
            }

            if {($Reconnection(VoluntaryDisconnection)) || ($Reconnection(PracticeDisconnection)) || 
                ($Reconnection(codacon)) || ($Reconnection(cmon)) || ($Reconnection(stoplight)) || 
	        ($Reconnection(sluggishness)) || ($Reconnection(ObservedCacheMiss)) || 
                ($OtherKnowledge)} then { 
                set Reconnection(Sanitized) 1 
            }

            if {$Reconnection(Sanitized) != 1} then { 
            PopUpDialog "\nPlease tell us how you knew you were operating disconnected.  \Feel free to use the `Other' text entry field if necessary." 
            return 
            }


            if {$Reconnection(VoluntaryDisconnection)} then {
            set Reconnection(Sanitized) 0
            set NumberPracticeSessions 0
            set OtherPreparation 0

            if {$Reconnection(PracticeDisconnectionPreparation)} then {
                set value [$Reconnection(prepare).le.practicedisconnection.scale get]
                set IsInteger [regexp {^[0-9]*$} $value]
                if {$IsInteger} {set NumberPracticeSessions $value}
            }
            if { [string length [$Reconnection(prepare).le.comments.text get 1.0 end]] > 0 } then {
                 set OtherPreparation 1 
            }

            if {($Reconnection(NoPreparation)) || ($Reconnection(HoardWalk)) || ($Reconnection(PseudoDisconnection)) || 
                ($NumberPracticeSessions) || ($OtherPreparation) } { set Reconnection(Sanitized) 1 }

            if {$Reconnection(Sanitized) != 1} then { 
                PopUpDialog "\nPlease tell us what, if anything, you did to \
                    prepare for this disconnected session." 
                return 
            }
            }
        }
        {No}
        {
            set Reconnection(Sanitized) 1
            return
        }
        {Suspected} 
        {
            set Reconnection(Sanitized) 0
            set OtherSuspicion 0

            if { [string length [$Reconnection(suspicions).other.comments.text get 1.0 end]] > 0 } then { 
            set OtherSuspicion 1
            }

            if {($Reconnection(codacon)) || ($Reconnection(cmon)) || ($Reconnection(stoplight)) || 
	        ($Reconnection(sluggishness)) || ($Reconnection(ObservedCacheMiss)) || 
                ($OtherSuspicion)} then { 

                set Reconnection(Sanitized) 1 

            }

            if {$Reconnection(Sanitized) != 1} then { 

                PopUpDialog "\nPlease tell us why you suspected you were operating\
		             disconnected.  Feel free to use the `Other' text entry\
			     field if necessary." 
                return 

            }
        }
    }

    set Reconnection(Sanitized) 0
    if {[$Reconnection(overall).scale get] == 0} {
        if { $Reconnection(PracticeDisconnection) == 0} {

            PopUpDialog "\nPlease tell us how successful you believe this disconnected session was by choose a value from 1 to 9." 
            return
        }
    }
    set Reconnection(Sanitized) 1
} 

proc PopUpDialog {msg} {
    global Reconnection

    set Reconnection(panel) .panel

    toplevel $Reconnection(panel)
    message $Reconnection(panel).msg \
        -text $msg \
        -justify left \
        -width "5i"
    button $Reconnection(panel).ok \
        -text "OK" \
        -command "destroy $Reconnection(panel)"
    pack append $Reconnection(panel) \
        $Reconnection(panel).msg    {top fill} \
        $Reconnection(panel).ok    {pady 10 padx 10}
    grab set $Reconnection(panel)
    tkwait window $Reconnection(panel)
}


#
# Helpful Routines
#

proc cbentry {path text length var command} {

    frame $path
    checkbutton $path.cb \
        -text $text \
        -variable $var \
        -relief flat \
        -activebackground gray \
        -command $command
    entry $path.entry -width $length -relief sunken
    pack append $path $path.cb {left expand} $path.entry {right expand}
}

proc cbtext {path text length var command} {

    frame $path
    checkbutton $path.cb \
        -text $text \
        -variable $var \
        -relief flat \
        -activebackground gray \
        -command $command
    text $path.text \
        -relief raised \
        -bd 2 \
        -height 2 \
        -width $length \
        -yscrollcommand "$path.scroll set" \
        -state disabled
    scrollbar $path.scroll -command "$path.text yview"
    pack append $path $path.cb {left frame nw} $path.text {left} $path.scroll {left filly}
}

proc labeltext {path text length} {
    frame $path
    label $path.label -text $text
    text $path.text \
        -relief raised \
        -bd 2 \
        -height 3 \
        -width $length \
        -yscrollcommand "$path.scroll set"
    scrollbar $path.scroll -command "$path.text yview"
    pack append $path $path.label {left frame nw} $path.text {left} $path.scroll {left filly}
}

proc labelentry {path text length} {
    frame $path
    label $path.label -text $text
    entry $path.entry -width $length -relief sunken
    pack append $path $path.label {left expand} $path.entry {right expand}
}

proc Transition { newState } {
    global Reconnection

    set Reconnection(State) $newState
}

proc Unpack {} {
    global Reconnection

    pack unpack $Reconnection(suspicions)
    pack unpack $Reconnection(known)
    pack unpack $Reconnection(prepare)
    pack unpack $Reconnection(overall)
    pack unpack $Reconnection(comments)
}

proc InitializeGlobals {} {
    global Reconnection


    set Reconnection(MainWindow)  .reconnection
    toplevel $Reconnection(MainWindow)

    set Reconnection(info) $Reconnection(MainWindow).info
    set Reconnection(aware) $Reconnection(MainWindow).aware
    set Reconnection(prepare) $Reconnection(MainWindow).prepare
    set Reconnection(known) $Reconnection(MainWindow).known
    set Reconnection(suspicions) $Reconnection(MainWindow).suspicions
    set Reconnection(overall) $Reconnection(MainWindow).overall
    set Reconnection(comments) $Reconnection(MainWindow).comments

    # Use xfontsel to select an appropriate font.
    # set Reconnection(fontinfo) "-Adobe-helvetica-bold-r-normal-*-*-160-*-*-*-*-*-*"

    set Reconnection(Sanitized) 0

    set Reconnection(State) "Aware"
    set Reconnection(AwareOfDisconnection) "Unknown"
    set Reconnection(VoluntaryDisconnection) 0
    set Reconnection(PracticeDisconnection) 0
    set Reconnection(codacon) 0
    set Reconnection(cmon) 0
    set Reconnection(stoplight) 0
    set Reconnection(sluggishness) 0
    set Reconnection(ObservedCacheMiss) 0
    set Reconnection(KnownOtherComments) 0
    set Reconnection(SuspiciousOtherComments) 0
    set Reconnection(PrepOtherComments) 0
    set Reconnection(NoPreparation) 0
    set Reconnection(HoardWalk) 0
    set Reconnection(PseudoDisconnection) 0

}

proc packwindows {} {
    global Reconnection

    wm geometry $Reconnection(MainWindow) +0+0
    wm title $Reconnection(MainWindow) "Reconnection Questionnaire"

    set f $Reconnection(MainWindow).c.main

    case $Reconnection(State) in {
        {Aware}
            {
                pack append $f \
                    $Reconnection(info) {top fill}\
                    $Reconnection(aware)  {top fill pady 10}
                set child $Reconnection(aware)
            }
        {Knows}
            {
                pack unpack $Reconnection(prepare)
                pack after $Reconnection(aware) $Reconnection(known) {top fill pady 10}
                pack append $f $Reconnection(overall) {top fill pady 10}
                pack append $f $Reconnection(comments) {top fill pady 10}
                set child $Reconnection(comments)
            }
        {Suspicions}
            {
                pack after $Reconnection(aware) $Reconnection(suspicions) {top fill pady 10}
                pack append $f $Reconnection(overall) {top fill pady 10}
                pack append $f $Reconnection(comments) {top fill pady 10}
                set child $Reconnection(comments)
            }
        {Prepare}
            {
                pack append $f $Reconnection(prepare) {top fill pady 10}
                pack append $f $Reconnection(overall) {top fill pady 10}
                pack append $f $Reconnection(comments) {top fill pady 10}
                set child $Reconnection(comments)
            }
        {FinalComments}
            {
                pack append $f $Reconnection(comments) {top fill pady 10}
                set child $Reconnection(comments)
            }
    }

    #
    # Let the user resize this window
    #
    wm minsize $Reconnection(MainWindow) 10 10

    #
    # Wait for the packer to add the frame, then fix up the
    # size of the canvas

    # Brent originally suggested that I use:
    #    tkwait visibility $child
    # but this triggers a bug because the $child window becomes visible
    # before packing is finished.  Thus the size of the frame is not yet
    # set which causes the size of the scrollregion to get set incorrectly.
    # Note that this bug only occurs on the pmax when the screenheight is
    # artifically set to allow us to test the scrollbar on large screen 
    # monitors.  Henry Rowley (har@cs) suggested that I use:
    #    update idletasks
    # This causes the bug to disappear on the pmax but creates an even
    # worse bug on the 386 (the questionnaire window .is about 20x40).
    # I'm going back to the tkwait command since that appears to work well
    # on the pmax and the 386 so long as we use the real screenheights.
    tkwait visibility $child

    set w [winfo width $f]
    set h [winfo height $f]
    $Reconnection(MainWindow).c config -scrollregion "0 0 $w $h"
    set sh [winfo screenheight $f]
#    set sh 300 ; # hack to test on large screen monitors
    if {$h > $sh} {
        # Subtract off 25 to account for the title bar (I think)
        set h [expr $sh-25]
    }
    $Reconnection(MainWindow).c config -width $w -height $h
}


#
# Questions
#

proc QuestionAware {} {
    global Reconnection

    #
    # Aware of disconnection?
    #

    frame $Reconnection(aware) -relief raised -borderwidth 2
    message $Reconnection(aware).msg -text "Were you aware you were operating disconnected?" \
        -justify left \
        -width "8i"

    frame $Reconnection(aware).rb
    radiobutton $Reconnection(aware).rb.yes \
        -text "Yes" \
        -variable Reconnection(AwareOfDisconnection) \
        -value "Yes" \
        -relief flat \
        -activebackground gray \
        -command "Unpack; Transition Knows; packwindows"
    radiobutton $Reconnection(aware).rb.no \
        -text "No" \
        -variable Reconnection(AwareOfDisconnection) \
        -value "No" \
        -relief flat \
        -activebackground gray \
        -command "Unpack; Transition FinalComments; packwindows"
    radiobutton $Reconnection(aware).rb.suspected \
        -text "I suspected, but wasn't sure" \
        -variable Reconnection(AwareOfDisconnection) \
        -value "Suspected" \
        -relief flat \
        -activebackground gray \
        -command "Unpack; Transition Suspicions; packwindows"
    pack append $Reconnection(aware).rb \
        $Reconnection(aware).rb.yes       {left padx 40} \
        $Reconnection(aware).rb.no        {left padx 40} \
        $Reconnection(aware).rb.suspected     {left padx 40}


    pack append $Reconnection(aware) \
        $Reconnection(aware).msg      {top frame nw} \
        $Reconnection(aware).rb  {top expand}
}

proc QuestionComments {} {
    global Reconnection

    #
    # Final comments
    #

    frame $Reconnection(comments) -relief raised -borderwidth 2
    labeltext $Reconnection(comments).label "Final comments:" 40
    button $Reconnection(comments).done \
        -command quit\
        -text "Done" \
        -state active

    pack append $Reconnection(comments) \
        $Reconnection(comments).label {left frame nw} \
        $Reconnection(comments).done {right padx 10}
}

proc TransitionFromKnows {} {
    global Reconnection

    if {$Reconnection(VoluntaryDisconnection)} then {
    Transition Prepare
    } else {
    Transition Knows
    }
}

proc QuestionKnown {} {
    global Reconnection

    #
    # What gives us away?
    #

    frame $Reconnection(known) -relief raised -borderwidth 2
    message $Reconnection(known).msg \
        -text "How did you know that you were disconnected?  Choose \
all that apply." \
        -justify left \
        -width "8i"

    #
    # Possible suspicious activity
    #

    frame $Reconnection(known).cbfirstline

    checkbutton $Reconnection(known).cbfirstline.voluntary \
        -text "Voluntary Disconnection" \
        -relief flat \
        -variable Reconnection(VoluntaryDisconnection) \
        -command "TransitionFromKnows ; packwindows"
    checkbutton $Reconnection(known).cbfirstline.practice \
        -text "Practice Disconnection" \
        -relief flat \
        -variable Reconnection(PracticeDisconnection) \
        -command "TransitionFromKnows; packwindows"

    pack append $Reconnection(known).cbfirstline \
        $Reconnection(known).cbfirstline.voluntary    {left expand fill} \
        $Reconnection(known).cbfirstline.practice     {left expand fill}

    frame $Reconnection(known).cbsecondline

    checkbutton $Reconnection(known).cbsecondline.codacon    \
        -text "codacon" \
        -relief flat \
        -variable Reconnection(codacon)
    checkbutton $Reconnection(known).cbsecondline.cmon    \
        -text "cmon" \
        -relief flat \
        -variable Reconnection(cmon)
    checkbutton $Reconnection(known).cbsecondline.stoplight    \
        -text "Stoplight" \
        -relief flat \
        -variable Reconnection(stoplight)

    pack append $Reconnection(known).cbsecondline \
        $Reconnection(known).cbsecondline.codacon     {left expand fill} \
        $Reconnection(known).cbsecondline.cmon    {left expand fill} \
        $Reconnection(known).cbsecondline.stoplight   {left expand fill}

    frame $Reconnection(known).cbthirdline 

    checkbutton $Reconnection(known).cbthirdline.sluggish \
        -text "sluggishness at point of disconnection" \
        -relief flat \
        -variable "Reconnection(sluggishness)"

    checkbutton $Reconnection(known).cbthirdline.cachemiss \
        -text "Observed a cache miss" \
        -relief flat \
        -variable "Reconnection(ObservedCacheMiss)"

    pack append $Reconnection(known).cbthirdline \
        $Reconnection(known).cbthirdline.sluggish    {left expand fill} \
        $Reconnection(known).cbthirdline.cachemiss    {left expand fill}

    frame $Reconnection(known).other
    cbtext $Reconnection(known).other.comments "Other (please specify):" 40 Reconnection(KnownOtherComments) "ResetKnownOtherComments"
    pack append $Reconnection(known).other \
        $Reconnection(known).other.comments {left}

    pack append $Reconnection(known) \
        $Reconnection(known).msg {top frame nw} \
        $Reconnection(known).cbfirstline {top frame nw} \
        $Reconnection(known).cbsecondline {top frame nw} \
        $Reconnection(known).cbthirdline {top frame nw} \
        $Reconnection(known).other {top frame nw}

}

proc ResetKnownOtherComments {} {
    global Reconnection

    if { $Reconnection(KnownOtherComments) == 0 } then {
        $Reconnection(known).other.comments.text delete 1.0 end
        $Reconnection(known).other.comments.text config -state disabled
    } else {
        $Reconnection(known).other.comments.text config -state normal
    }
}

proc QuestionSuspicious {} {
    global Reconnection


    #
    # What gives us away?
    #

    frame $Reconnection(suspicions) -relief raised -borderwidth 2
    message $Reconnection(suspicions).msg \
        -text "What made you suspect that you were disconnected? Choose all that apply." \
        -justify left \
        -width "8i"

    #
    # Possible suspicious activity
    #

    frame $Reconnection(suspicions).cbfirstline

    checkbutton $Reconnection(suspicions).cbfirstline.codacon \
        -text "codacon" \
        -relief flat \
        -variable Reconnection(codacon)

    checkbutton $Reconnection(suspicions).cbfirstline.cmon \
        -text "cmon" \
        -relief flat \
        -variable Reconnection(cmon)

    checkbutton $Reconnection(suspicions).cbfirstline.stoplight \
        -text "stoplight" \
        -relief flat \
        -variable Reconnection(stoplight)

    pack append $Reconnection(suspicions).cbfirstline \
        $Reconnection(suspicions).cbfirstline.codacon    {left expand fill} \
        $Reconnection(suspicions).cbfirstline.cmon       {left expand fill} \
        $Reconnection(suspicions).cbfirstline.stoplight  {left expand fill} 

    frame $Reconnection(suspicions).cbsecondline

    checkbutton $Reconnection(suspicions).cbsecondline.sluggish \
        -text "sluggishness at point of disconnection" \
        -relief flat \
        -variable "Reconnection(sluggishness)"

    checkbutton $Reconnection(suspicions).cbsecondline.cachemiss \
        -text "Observed a cache miss" \
        -relief flat \
        -variable "Reconnection(ObservedCacheMiss)"

    pack append $Reconnection(suspicions).cbsecondline \
        $Reconnection(suspicions).cbsecondline.sluggish  {left expand fill} \
        $Reconnection(suspicions).cbsecondline.cachemiss {left expand fill}

    frame $Reconnection(suspicions).other
    cbtext $Reconnection(suspicions).other.comments "Other (please specify):" 40 Reconnection(SuspiciousOtherComments) "ResetSuspiciousOtherComments"
    pack append $Reconnection(suspicions).other \
        $Reconnection(suspicions).other.comments {left}

    pack append $Reconnection(suspicions) \
        $Reconnection(suspicions).msg {top frame nw} \
        $Reconnection(suspicions).cbfirstline {top frame nw} \
        $Reconnection(suspicions).cbsecondline {top frame nw} \
        $Reconnection(suspicions).other {top frame nw}
}

proc ResetSuspiciousOtherComments {} {
    global Reconnection

    if {$Reconnection(SuspiciousOtherComments) == 0 } then {
        $Reconnection(suspicions).other.comments.text delete 1.0 end
        $Reconnection(suspicions).other.comments.text config -state disabled
    } else {
        $Reconnection(suspicions).other.comments.text config -state normal
    }
}

proc ResetPractice {} {
    global Reconnection

    if { $Reconnection(PracticeDisconnectionPreparation) == 0 } then {
        SetPracticeScale
    }
}

proc ResetPrepOtherComments {} {
    global Reconnection

    if { $Reconnection(PrepOtherComments) == 0 } then {
        $Reconnection(prepare).le.comments.text delete 1.0 end
        $Reconnection(prepare).le.comments.text config -state disabled
    } else {
        $Reconnection(prepare).le.comments.text config -state normal
    }
}

proc ResetNoPreparation {} {
    global Reconnection

    if { ($Reconnection(NoPreparation)) && (($Reconnection(HoardWalk)) || ($Reconnection(PseudoDisconnection)) || ($Reconnection(PracticeDisconnectionPreparation)) || ($Reconnection(PrepOtherComments))) } then {
        set Reconnection(NoPreparation) 0
    }
}

proc ResetOtherPreps {} {
    global Reconnection

    set Reconnection(HoardWalk) 0
    set Reconnection(PseudoDisconnection) 0
    set Reconnection(PracticeDisconnectionPreparation) 0
    set Reconnection(PrepOtherComments) 0
    ResetPractice
    ResetPrepOtherComments
}

proc QuestionPrepare {} {
    global Reconnection

    #
    # What Preparation?
    #

    frame $Reconnection(prepare) -relief raised -borderwidth 2
    message $Reconnection(prepare).msg -text "What did you do to prepare for this disconnection?  Choose all that apply." \
        -justify left \
        -width "8i"

    frame $Reconnection(prepare).cb
    checkbutton $Reconnection(prepare).cb.nothing \
        -text "Nothing" \
        -variable Reconnection(NoPreparation) \
        -relief flat \
        -activebackground gray \
        -command "ResetOtherPreps"
    checkbutton $Reconnection(prepare).cb.hoardwalk \
        -text "Hoard Walk" \
        -variable Reconnection(HoardWalk) \
        -relief flat \
        -activebackground gray \
        -command "ResetNoPreparation"
    checkbutton $Reconnection(prepare).cb.pseudodisconnection \
        -text "Pseudo Disconnection" \
        -variable Reconnection(PseudoDisconnection) \
        -relief flat \
        -activebackground gray \
        -command "ResetNoPreparation"
    pack append $Reconnection(prepare).cb \
        $Reconnection(prepare).cb.nothing     {left expand fill} \
        $Reconnection(prepare).cb.hoardwalk       {left expand fill} \
        $Reconnection(prepare).cb.pseudodisconnection {left expand fill}

    frame $Reconnection(prepare).le
    frame $Reconnection(prepare).le.practicedisconnection
    checkbutton $Reconnection(prepare).le.practicedisconnection.cb \
        -text "Practice Disconnection(s), how many?" \
        -variable Reconnection(PracticeDisconnectionPreparation) \
        -relief flat \
        -activebackground gray \
        -command "SetPracticeScale"
    scale $Reconnection(prepare).le.practicedisconnection.scale -from 0 -to 5 -length 175 \
        -tickinterval 1 \
        -label "None          (5 or more)"  \
        -orient horizontal \
        -activebackground Gray \
        -command "SetPracticeCB"

    pack append $Reconnection(prepare).le.practicedisconnection \
        $Reconnection(prepare).le.practicedisconnection.cb {left frame nw padx 25} \
        $Reconnection(prepare).le.practicedisconnection.scale {left}

    cbtext $Reconnection(prepare).le.comments "Other (please specify):" 40 Reconnection(PrepOtherComments) "ResetPrepOtherComments; ResetNoPreparation"
    pack append $Reconnection(prepare).le \
        $Reconnection(prepare).le.practicedisconnection {top frame nw} \
        $Reconnection(prepare).le.comments {top frame nw}

    pack append $Reconnection(prepare) \
        $Reconnection(prepare).msg    {top frame nw} \
        $Reconnection(prepare).cb     {top frame nw} \
        $Reconnection(prepare).le     {top frame nw}
}

proc SetPracticeScale {} {
    global Reconnection

    if { $Reconnection(PracticeDisconnectionPreparation) } then {
        $Reconnection(prepare).le.practicedisconnection.scale set 1
    } else {
        $Reconnection(prepare).le.practicedisconnection.scale set 0
    }
}

proc SetPracticeCB {value} {
    global Reconnection

    if { $value == 0 } then {
        set Reconnection(PracticeDisconnectionPreparation) 0
    } else {
        set Reconnection(PracticeDisconnectionPreparation) 1
    }
}

proc OutputPrepare {outFile} {
    global Reconnection

    puts $outFile [format "NoPreparation: %s" $Reconnection(NoPreparation)]
    puts $outFile [format "HoardWalk: %s" $Reconnection(HoardWalk)]
    puts $outFile [format "PseudoDisconnection: %s" $Reconnection(PseudoDisconnection)]
    puts $outFile [format "PracticeDisconnection(s): %s" $Reconnection(PracticeDisconnectionPreparation)]
    if { $Reconnection(PracticeDisconnectionPreparation) } then {
        set value [$Reconnection(prepare).le.practicedisconnection.scale get]
        set IsInteger [regexp {^[0-9]*$} $value]

        puts $outFile [format "NumberPracticeDisconnection(s): %s" [$Reconnection(prepare).le.practicedisconnection.scale get]]
    }
    puts $outFile [format "PreparationComments:\n %s\nEndComment.\n" [$Reconnection(prepare).le.comments.text get 1.0 end]]
}

proc QuestionOverall {} {
    global Reconnection

    #
    # Overall Impression
    #

    frame $Reconnection(overall) -relief raised -borderwidth 2
    message $Reconnection(overall).msg \
        -text "Overall, how successful would you consider this disconnected session?\n"\
        -justify left \
        -width "8i"
    scale $Reconnection(overall).scale -from 0 -to 9 -length 400 -tickinterval 1 \
        -label "Invalid | Not at all!    Not very     Hard To Say Somewhat      Very!"\
        -orient horizontal \
        -activebackground Gray \
        -showvalue False


    pack append $Reconnection(overall) \
        $Reconnection(overall).msg    {top frame nw} \
        $Reconnection(overall).scale  {top}
}


proc OutputOverall {outFile} {
    global Reconnection
    
    puts $outFile [format "OveralImpression: %s" [$Reconnection(overall).scale get]]
}

proc PresentInfo { date time length } {
    global Reconnection

    #
    # Informational Dialog
    #

    if {$date == "12/31/69"} then {
        set msg "A network connection has been established.\nDisconnection Time:  prior to Venus start-up"
    } else {
        set msg "A network connection has been reestablished.\nDisconnection Time:  $date at $time.\nDisconnection Length:  $length."
    }

    frame $Reconnection(info) -relief raised -borderwidth 2
    message $Reconnection(info).msg -text $msg \
        -justify left \
        -width "8i"

    pack append $Reconnection(info) \
        $Reconnection(info).msg       {top fill}
}

proc CreateMainScrollBar {} {
    global Reconnection

    # The canvas size gets fixed up in packwindows
    canvas $Reconnection(MainWindow).c \
	    -yscrollcommand "$Reconnection(MainWindow).scrollbar set" \
	    -width 200 \
	    -height 100
    scrollbar $Reconnection(MainWindow).scrollbar -command "$Reconnection(MainWindow).c yview"

    pack append $Reconnection(MainWindow) $Reconnection(MainWindow).scrollbar {right filly}
    pack append $Reconnection(MainWindow) $Reconnection(MainWindow).c {left fillx filly expand}

    frame $Reconnection(MainWindow).c.main

    $Reconnection(MainWindow).c create window 0 0 -anchor nw -window $Reconnection(MainWindow).c.main
}


#  outfile is the output file name
#  date is the day the disconnection occurred (e.g. 4/8/94)
#  time is the time the disconnection occurred (e.g. 4:52pm)
#  length is the length of the disconnection (e.g. 1 day)
proc ReconnectionQuestionnaire { outputfile date time length } {
    global Reconnection


    #
    # Global Variables
    #
    set Reconnection(outputfile) $outputfile
    set Reconnection(date) $date
    set Reconnection(time) $time
    set Reconnection(length) $length

    InitializeGlobals

    #
    # Create all windows
    #
    CreateMainScrollBar
    PresentInfo $Reconnection(date) $Reconnection(time) $Reconnection(length)
    QuestionAware
    QuestionComments
    QuestionPrepare
    QuestionSuspicious
    QuestionKnown
    QuestionOverall

    packwindows

    #
    # Now, wait for user actions...
    #
    tkwait window $Reconnection(MainWindow)
    return 0
}

