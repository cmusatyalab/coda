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
#  Tokens Indicator
#
#  States:  Unknown, Valid, Expired, Expired&Pending
#
##############################################################################


proc TokensVisualStart { } {
    global Window
    global Tokens
    global Colors
    global Indicator
    global password

    if { [winfo exists $Tokens(MainWindow)] == 1 } then {
	raise $Tokens(MainWindow)
	return
    }
    toplevel $Tokens(MainWindow)
    wm title $Tokens(MainWindow) "Authentication Information"
    wm geometry $Tokens(MainWindow) $Tokens(Geometry)
    wm protocol $Tokens(MainWindow) WM_DELETE_WINDOW {TokensVisualEnd}

    switch -exact $Indicator(Tokens:State) {
	Valid { ShowTokenExpirationDate } 
        Expired { GetTokens }
	Expired&Pending { ShowPendingGetTokens }
    }
}

proc TokensVisualEnd { } {
    global Tokens

    LogAction "Tokens Window: close"
    SetTime "TokensWindow:close"
    destroy $Tokens(MainWindow)
}

proc TokensInitData { } {
    global Tokens

    set Tokens(MainWindow) .tokens
    set Tokens(Geometry) +0+0
    set Tokens(PendingFetchList) [list ]
    set Tokens(PendingReintegrationList) [list ]
}

proc TokensDetermineState { } {
    global Pathnames

    set rc [catch {exec $Pathnames(ctokens)} result]

    set UID [string range $result [string wordstart $result 40] \
				  [string wordend $result 40]]

    if { [string first "Expires" $result] >= 0 } then {
        set Expires [string trimright [string range $result \
		           [expr [string last "Expires" $result] + 8] end] "]"]
        return Valid
    } elseif { [string first "Not Authenticated" $result] >= 0 } then {
        return Expired
    } else {
	return Unknown
    }
}

proc TokenBindings { } {
    global Window    
    bind $Window.indicators.tokens <Double-1> { 
	LogAction "Tokens Indicator: double-click"
	SetTime "TokensIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	TokensVisualStart 
	update idletasks
	SetTime "TokensWindow:visible"
    }
}

proc TokensHelpWindow { } {
    global Indicator

    LogAction "Tokens Window: help"
    switch -exact $Indicator(Tokens:State) {
	Valid { set messagetext "This window shows when your current tokens will expire.  In addition, you can destroy your tokens or reauthenticate to get new tokens." }
	Expired { set messagetext "This window allows you to authenticate to obtain tokens." }
	Expired&Pending { set messagetext "This window allows you to authenticate to obtain tokens.  It also shows which subtrees have reintegrations pending." }
    }

    PopupHelp "Tokens Help Window" .help $messagetext 5i
}

proc TokensCheckVisibilityAndUpdateScreen { } {
    global Tokens

    if { [winfo exists $Tokens(MainWindow)] == 1 } then {
        set Tokens(Geometry) [GeometryLocationOnly [wm geometry $Tokens(MainWindow)]]
	destroy $Tokens(MainWindow)
        TokensVisualStart
    }
}

proc MkAuthenticateMessage { top message } {
    global Tokens

    set w [frame $top.message -bd 2 -relief groove]
    message $w.msg \
	-text $message \
	-justify left \
	-width "5i"
    pack $w.msg -side top -expand true -fill x
    set Tokens(AuthenticateMessage) $w.msg
    return $w
}

proc MkAuthenticate { top user } {
    set w [frame $top.auth -bd 2 -relief groove]

    CommandEntry $w.username "Username: " 10 {} -textvar username
    $w.username.entry config -bg gray92
    bind $w.username.entry <Delete> { tkEntryBackspace %W }
    bind $w.username.entry <BackSpace> { tkEntryBackspace %W }
    bind $w.username.entry <1> { LogAction "Tokens Window: Click on Username entry" }
    bind $w.username.entry <Any-KeyPress> { LogAction "Tokens Window: Typing in Username entry" }
    bind $w.username.entry <Return> "LogAction TokensWindowUsernameEntry:<Return>; focus $w.password.entry"
    $w.username.entry delete 0 end
    $w.username.entry insert 0 $user

    CommandEntry $w.password "Password: " 10 Authenticate -textvar password
    # Hide the password by making the letters the same color as the background!
    $w.password.entry config -fg gray92 -bg gray92
    bind $w.password.entry <Delete> { tkEntryBackspace %W }
    bind $w.password.entry <1> { LogAction "Tokens Window: Click on Password entry" }
    bind $w.password.entry <Any-KeyPress> { LogAction "Tokens Window: Typing in Password entry" }
    bind $w.password.entry <Return> { LogAction "Tokens Window: <Return> in Password entry"; Authenticate }

    pack $w.username -side top  -expand true
    pack $w.password -side top  -expand true
    return $w
}

proc MkAuthenticateButtons { top } {
    global Tokens
    global Indicator

    set w [frame $top.buttons -bd 2 -relief groove]
    button $w.tokenshelp -text "Help" -anchor w -relief groove -bd 2 \
	-command { TokensHelpWindow }
#    button $w.tokensclose -text "Close" -anchor e -relief groove -bd 2 \
#	-command { TokensVisualEnd }
    button $w.tokenssubmit -text "Submit" -anchor e -relief groove -bd 6 \
	-command { LogAction "Tokens Window: submit"; Authenticate }
    set Tokens(AuthenticateButton) $w.tokenssubmit
    if { $Indicator(Tokens:State) == "Valid" } then { 
        $Tokens(AuthenticateButton) configure -text "New Tokens"
        $Tokens(AuthenticateButton) configure -command { LogAction "Tokens Window: newtokens"; ObtainTokens}
    }
    button $w.tokensdestroy -text "Destroy Tokens" -anchor e -relief groove -bd 2 \
	-command { LogAction "Tokens Window: destroytokens"; DestroyTokens } -state disabled
    set Tokens(DestroyButton) $w.tokensdestroy
    if { $Indicator(Tokens:State) == "Valid" } then { 
	$Tokens(DestroyButton) configure -state normal
    }
    
    pack $w.tokenshelp -side left -expand true
#    pack $w.tokensclose -side right
    pack $w.tokenssubmit -side right -expand true
    pack $w.tokensdestroy -side right -expand true
    return $w
}

proc GetTokens { } {
    global Tokens
    global user

    set message "Tokens for user $user have expired."
    pack [MkAuthenticateMessage $Tokens(MainWindow) $message] -side top \
	-expand true -fill both
    pack [MkAuthenticate $Tokens(MainWindow) $user] -side top \
	-expand true -fill x
    pack [MkAuthenticateButtons $Tokens(MainWindow)] -side bottom \
	-expand true -fill x \
	-padx 2 -pady 2
}

proc MakePendingString { list object actioned } {
    set Length [llength $list]
    if { $Length == 0 } then {
	return ""
    }

    if { $Length == 1 } then { 
	set plural "" 
	set singular "s"
    } else { 
	set plural "s" 
	set singular ""
    }

    set message "The following $object$plural need$singular to be $actioned:\n"
    for {set i 0} {$i < $Length} {incr i} {
        set sublist [split [lindex $list $i] :]
	set message [format "%s\t%s\n" $message [lindex $sublist 1]]
    } 
    return $message
}

proc ShowPendingGetTokens { } {
    global Tokens
    global Events
    global user

    set message [format "%s\n\n" "Tokens for user $user have expired."]
    set message [format "%s\n%s" $message [MakePendingString $Tokens(PendingFetchList) "object" "fetched"]]
    set message [format "%s\n%s" $message [MakePendingString $Tokens(PendingReintegrationList) "subtree" "reintegrated"]]

    pack [MkAuthenticateMessage $Tokens(MainWindow) $message] -side top \
	-expand true -fill both
    pack [MkAuthenticate $Tokens(MainWindow) $user] -side top \
	-expand true -fill x
    pack [MkAuthenticateButtons $Tokens(MainWindow)] -side bottom \
	-expand true -fill x \
	-padx 2 -pady 2
}

proc ShowTokenExpirationDate { } {
    global Tokens
    global Pathnames

    set rc [catch {exec $Pathnames(ctokens)} ctokens]

    set UIDindex [string first "UID" $ctokens]
    regexp {[0-9]*} [string range $ctokens [expr $UIDindex + 4] end] userid

    set results [eval exec "grep :[string trim $userid]: $Pathnames(passwd)"]
    set username [string range $results 0 [expr [string first ":" $results] - 1]]

    set ExpiresIndex [string first "Expires" $ctokens]
    set ExpirationDate [string range $ctokens [expr $ExpiresIndex + 8] \
					      [expr $ExpiresIndex + 19]]

    set msg "Tokens for user $username expire at $ExpirationDate"
    pack [MkAuthenticateMessage $Tokens(MainWindow) $msg] -side top \
	-expand true -fill both
    pack [MkAuthenticateButtons $Tokens(MainWindow)] -side bottom \
	-expand true -fill x \
	-padx 2 -pady 2

    global Window
}

proc Authenticate { } {
    global Pathnames
    global Indicator
    global Tokens
    global user
    global password

    set errormsg ""

    if { $password == "" } then { 
	set errormsg "No password provided" 
    } else {
	# Do NOT make this else an elseif with the catch clause as the test.
	# It does not work that way.  I don't know why.  I don't particularly
	# care at this point.  I hate tcl/tk!

        if [catch {exec $Pathnames(clog) $user $password} result]  then {
  	    if {[string compare "Invalid login (RPC2_NOTAUTHENTICATED (F))." $result] == 0} then {
	        # Invalid password entered for this user
	        set errormsg "Incorrect Coda password entered for $user"
   	    } else {
	        # Some other error happened
	        set errormsg "An error occurred while authenticating $user"
	    }
        } else {
	    TokensVisualEnd
	}
    }

    # Destroy the password
    set password ""

    # Put up an error message
    if { $errormsg != "" } then {
   	toplevel .error
	wm title .error "Authentication Error"
	message .error.msg \
	    -text $errormsg \
	    -justify left \
	    -width "4i"
	pack .error.msg -side top -expand true -fill x
	bell; bell; bell
	Submit 5 {destroy .error} ""
    }

    IndicatorUpdate Tokens
}

proc DestroyTokens { } {
    global Pathnames
    global Tokens
    global user

    exec $Pathnames(cunlog)
    $Tokens(AuthenticateMessage) configure -text "Tokens for user $user have expired."
    $Tokens(DestroyButton) configure -state disabled
    IndicatorUpdate Tokens
    ObtainTokens
}

proc ObtainTokens { } {
    global Tokens
    global user

    $Tokens(AuthenticateButton) configure -text "Submit"
    $Tokens(AuthenticateButton) configure -command {LogAction "Tokens Window: submit"; Authenticate}
    pack [MkAuthenticate $Tokens(MainWindow) $user] -side top -expand true -fill x
}

proc TokensAcquiredEvent { } {
    global Indicator
    global Tokens

    set Indicator(Tokens:State) Valid
    set Tokens(PendingFetchList) [list ]
    set Tokens(PendingReintegrationList) [list ]

    NotifyEvent TokensAcquired Tokens TokensVisualStart
    SetTime "TokensIndicator:acquired"
    TokensCheckVisibilityAndUpdateScreen 
}

proc TokenExpiryEvent { } {
    global Indicator

    set Indicator(Tokens:State) Expired

    NotifyEvent TokenExpiry Tokens TokensVisualStart
    SetTime "TokensIndicator:expiry"
    TokensCheckVisibilityAndUpdateScreen 
}

proc ActivityPendingTokensEvent { activity object } {
    global Indicator
    global Tokens

    set Indicator(Tokens:State) Expired&Pending
    if { ($activity == "reintegration") || ($activity == "reintegrate") } then {
        lappend Tokens(PendingReintegrationList) \
	    [format "%s: %s" $activity $object]
    } 
    if { $activity == "fetch" } then {
        lappend Tokens(PendingFetchList) \
	    [format "%s: %s" $activity $object]
    }	

    NotifyEvent ActivityPendingTokens Tokens TokensVisualStart
    SetTime "TokensIndicator:pending"
    TokensCheckVisibilityAndUpdateScreen 
}


