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
#  Network Indicator
#
#
##############################################################################


proc NetworkVisualStart { } {
    global Window
    global Network
    global Colors
    global Dimensions
    global Servers
    global Events

    if { [winfo exists $Network(MainWindow)] == 1 } then {
	raise $Network(MainWindow)
	return
    }
    toplevel $Network(MainWindow)
    wm title $Network(MainWindow) "Network Information"
    wm geometry $Network(MainWindow) $Network(Geometry)
    wm protocol $Network(MainWindow) WM_DELETE_WINDOW {NetworkVisualEnd}

    for { set i 0 } {$i < $Servers(numServers)} {incr i} {
	set s $Servers($i)
  	set bw [GetBandwidthEstimate $s]
	set color $Colors($Events([ServerDetermineState $s]:Urgency))

	if { $Servers(${s}:Mode) == "artificial" } {
	    set labelColor $Colors(disabled)
	} else {
	    set labelColor black
	}

	if { $bw < 0 } then { set bw 0 }
	tixCodaMeter $Network(MainWindow).bandwidth$s \
		-height $Dimensions(Meter:Height) \
		-width $Dimensions(Meter:Width) \
		-foreground $color \
		-labelWidth 10 \
		-label " $s " \
		-labelColor $labelColor \
		-rightLabel "10 Mb/s" \
		-percentFull $bw
        pack $Network(MainWindow).bandwidth$s -side top -expand true -fill x
    }

    button $Network(MainWindow).bandwidthhelp -text "Help" \
	-relief groove -bd 2 \
	-anchor w -command { NetworkHelpWindow }
    button $Network(MainWindow).bandwidthcontrol -text "Control Panel" \
	-relief groove -bd 2 \
	-anchor e \
	-command { LogAction "Network Window: control panel"; ControlPanelVisualStart; global ControlPanel; $ControlPanel(MainWindow:Notebook) raise physical }
#    button $Network(MainWindow).bandwidthclose -text "Close" -anchor e \
#	-command { NetworkVisualEnd }
    pack $Network(MainWindow).bandwidthhelp -side left
#    pack $Network(MainWindow).bandwidthclose -side right
    pack $Network(MainWindow).bandwidthcontrol -side right
}

proc NetworkUpdateMeterColor { server newColor } {
    global Network

    if { [winfo exists $Network(MainWindow).bandwidth$server] } then {
	$Network(MainWindow).bandwidth$server config -foreground $newColor
    }
}

proc NetworkUpdateBarLength { server newPercentage } {
    global Network

    if { [winfo exists $Network(MainWindow).bandwidth$server] } then {
	$Network(MainWindow).bandwidth$server config -percentFull $newPercentage
    }
}


proc NetworkUpdateServerBandwidth { server } {
    NetworkUpdateBarLength $server [GetBandwidthEstimate $server]
}

proc NetworkUpdateServerConnectivity { server connectivity } {
    global Colors
    global Events

    NetworkUpdateMeterColor $server $Colors($Events([ServerDetermineState $server]:Urgency))
}

proc NetworkUpdateWindow { } {
    global Servers

    for { set i 0 } {$i < $Servers(numServers)} {incr i} {

	set s $Servers($i)

        NetworkUpdateServerBandwidth $s
	NetworkUpdateServerConnectivity $s $Servers(${s}:Connectivity)
    }
}

proc NetworkVisualEnd { } {
    global Network

    LogAction "Network Window: close"
    SetTime "NetworkWindow:close"
    set Network(Geometry) [wm geometry $Network(MainWindow)]
    destroy $Network(MainWindow)
}

proc NetworkInitData { } {
    global Network

    set Network(MainWindow) .network
    set Network(Geometry) 386x356+0+0
}

proc ServerDetermineState { server } {
    global Servers

    if { $Servers(${server}:Connectivity) == "strong" } {
	return OperatingStronglyConnected
    } elseif { $Servers(${server}:Connectivity) == "weak" } {
	return OperatingWeaklyConnected
    } elseif { $Servers(${server}:Connectivity) == "none" } {
	return OperatingDisconnected
    } else {
	return Unknown
    }
}

proc NetworkDetermineState { } {
    global Servers

    set StronglyConnected "no"
    set WeaklyConnected "no"
    set Disconnected "no"

    for { set i 0 } {$i < $Servers(numServers)} {incr i} {
	set s $Servers($i)
	if { $Servers(${s}:Connectivity) == "strong" } {
	    set StronglyConnected "yes"
	}
	if { $Servers(${s}:Connectivity) == "weak" } {
	    set WeaklyConnected "yes"
	}
	if { $Servers(${s}:Connectivity) == "none" } {
	    SendToStdErr "NetworkDetermineState: $s is down"
	    set Disconnected "yes"
	}
    }

    if { $Disconnected == "yes" } then {
	return OperatingDisconnected
    } elseif { $WeaklyConnected == "yes" } then {
	return OperatingWeaklyConnected
    } else {
	return OperatingStronglyConnected
    }
}

proc NetworkCheckVisibilityAndUpdateScreen { } {
    global Network

    if { [winfo exists $Network(MainWindow)] == 1 } then {
#	update idletasks
#        set Network(Geometry) [wm geometry $Network(MainWindow)]
#	destroy $Network(MainWindow)
#        NetworkVisualStart
	NetworkUpdateWindow
    }
}

proc NetworkBindings { } {
    global Window
    bind $Window.indicators.network <Double-1> { 
	LogAction "Network Indicator: double-click"
	SetTime "NetworkIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	NetworkVisualStart 
	update idletasks
	SetTime "NetworkWindow:visible"
    }
}

proc ServerConnectionStrongEvent { Server } {
    global Servers
    global Colors
    global Events

    set server [ParseServerName $Server]

    set Servers(${server}:Connectivity) "strong"
    NetworkUpdateMeterColor $server $Colors($Events([ServerDetermineState $server]:Urgency))
}

proc ServerConnectionWeakEvent { Server } {
    global Servers
    global Colors
    global Events

    set server [ParseServerName $Server]
  SendToStdErr "ServerConnectionWeakEvent $server"

    set Servers(${server}:Connectivity) "weak"
    NetworkUpdateMeterColor $server $Colors($Events([ServerDetermineState $server]:Urgency))
}

proc ServerConnectionGoneEvent { Server } {
    global Servers
    global Colors
    global Events

    set server [ParseServerName $Server]
  SendToStdErr "ServerConnectionGoneEvent $server"

    set Servers(${server}:Connectivity) "none"
    NetworkUpdateMeterColor $server $Colors($Events([ServerDetermineState $server]:Urgency))
}


proc ServerAccessibleEvent { Server } {
    global Bandwidth

    set server [ParseServerName $Server]
  SendToStdErr "ServerAccessibleEvent $server"

    ServerConnectionStrongEvent $server
    ServerBandwidthEstimateEvent $server $Bandwidth(Ethernet)
}

proc ServerInaccessibleEvent { Server } {

    set server [ParseServerName $Server]
  SendToStdErr "ServerInaccessibleEvent $server"

    ServerConnectionGoneEvent $server
    ServerBandwidthEstimateEvent $server 0
}


proc ServerBandwidthEstimateEvent { args } {
    global Bandwidth
    global Indicator

    set OriginalState $Indicator(Network:State)

    for { set i 0 } { $i < [llength $args] } { incr i } {
	set server [ParseServerName [lindex $args $i]]
	incr i
	set Bandwidth($server) [lindex $args $i]
    }

    # This is sleazy.  If one server was already at either a warning or error, and a 
    # different one drops, we will not notify the user.  Minor, but probably something
    # to fix eventually.  Shouldn't be difficult, just more bookkeeping.
    set NewState [NetworkDetermineState]
    if { $OriginalState != $NewState } then { 
	set Indicator(Network:State) $NewState
	NotifyEvent $NewState Network NetworkVisualStart
        SetTime "NetworkIndicator:$NewState"
    }

    NetworkCheckVisibilityAndUpdateScreen   
}

proc NetworkOpportunityEvent { } {
    SendToStdErr "NetworkOpportunityEvent not implemented"
}

proc NetworkScarcityEvent { } {
    SendToStdErr "NetworkScarcityEvent not implemented"
}

proc ArtificialNetwork { server } {
    global Network
    global Servers
    global Colors

    set Servers(${server}:Mode) "artificial"

    if { [winfo exists $Network(MainWindow)] == 1 } then {
        # Change the color of the server's name
	$Network(MainWindow).bandwidth$server config -labelColor $Colors(disabled)
    }
}

proc NaturalNetwork { server } {
    global Network
    global Servers

    set Servers(${server}:Mode) "natural"

    if { [winfo exists $Network(MainWindow)] == 1 } then {
        # Change the color of the server's name
	$Network(MainWindow).bandwidth$server config -labelColor black
    }
}

proc NetworkHelpWindow { } {
    global Network

    LogAction "Network Window: help"
    set Network(HelpWindow) $Network(MainWindow)help
    set messagetext "The bandwidth to each server is expressed as a percentage of maximum Ethernet speed.  Servers whose names are written in light gray have had their network connections artificially manipulated through the control panel.  Servers whose bandwidth estimate is unknown have a bar shown in dark gray."
    PopupHelp "Network Help Window" $Network(HelpWindow) $messagetext 5i
}


proc ParseServerName { Server } {

    set index [string first "." $Server]
    if { $index != -1 } then {
        set server [string range $Server 0 [expr $index - 1]]
    } else {
	set server $Server
    }

    return $server
}
