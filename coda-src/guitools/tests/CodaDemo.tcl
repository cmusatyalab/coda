#!/bin/sh
# The next line restarts using a customized wish\
#exec /coda/usr/mre/src/coda-src/console/CodaConsole.out  "$0" "$@"

proc Quit { } {
    global Demo

    puts LostConnection
    exit
}

proc DemoVisualStart { } {
    global Demo

    foreach line { tokens rvmSpace cacheSpace disconnectedNetwork weakNetwork strongNetwork advice hoard reintegration repair tasks quit } {
        frame $Demo(MainWindow).$line -relief flat
        pack $Demo(MainWindow).$line -side top -expand true -fill x
    }

    DemoLineThreeEntries $Demo(MainWindow).tokens "Tokens:" \
	"Acquired" "TokensAcquired" \
	"Expired" "TokenExpiry" \
	"Action Pending" "ActivityPendingTokens"
    DemoLineTwoEntries $Demo(MainWindow).rvmSpace "RVM Space:" \
	"Fill" "FillRVM" \
	"Fix" "FixRVM"
    DemoLineTwoEntries $Demo(MainWindow).cacheSpace "Cache Space:" \
	"Fill" "FillCache" \
	"Fix" "FixCache"

    DemoLineTwoEntries $Demo(MainWindow).strongNetwork "Strongly Connected:" \
	"ToOne" "OperatingStronglyConnectedToOne" \
	"ToAll" "OperatingStronglyConnectedToAll"
    DemoLineTwoEntries $Demo(MainWindow).weakNetwork "Weakly Connected:" \
	"ToOne" "OperatingWeaklyConnectedToOne" \
	"ToAll" "OperatingWeaklyConnectedToAll"
    DemoLineTwoEntries $Demo(MainWindow).disconnectedNetwork "Disconnected:" \
	"ToOne" "OperatingDisconnectedToOne" \
	"ToAll" "OperatingDisconnectedToAll"

    DemoLineFourEntries $Demo(MainWindow).advice "Advice Requests:" \
	"Disco Miss" "DisconnectedCacheMiss" \
	"Weak Miss" "WeakCacheMiss" \
	"Read Miss" "ReadDisconnectedCacheMiss" \
	"Reconnection Survey" "ReconnectionSurvey"
    DemoLineFiveEntries $Demo(MainWindow).hoard "Hoard Walks:" \
	"Begin" "HoardWalkBegin" \
	"Advice Pending" "HoardWalkPendingAdvice" \
	"End" "HoardWalkEnd" \
        "PeriodicOn" "HoardWalkPeriodicOn"\
	"PeriodicOff" "HoardWalkPeriodicOff"
    DemoLineFourEntries $Demo(MainWindow).reintegration "Reintegration:" \
	"Enabled" "ReintegrationEnabled" \
	"Pending" "ReintegrationPending" \
	"Active" "ReintegrationActive" \
	"Completed" "ReintegrationCompleted"
    DemoLineTwoEntries $Demo(MainWindow).repair "Repair:" \
	"In Conflict" "RepairPending" \
	"Fixed" "RepairCompleted"
    DemoLineTwoEntries $Demo(MainWindow).tasks "Tasks:" \
	"Available" "AllTasksAvailable" \
	"Unavailable" "AllTasksUnavailable"

    button $Demo(MainWindow).quit.button -text "Quit" -relief groove -bd 2 \
	-command "Quit" -anchor w
    pack $Demo(MainWindow).quit.button -side bottom 
}

proc DemoLineTwoEntries { framename label oneText oneCommand twoText twoCommand } {
    label $framename.label -text $label -width 20
    button $framename.button1 -text $oneText -relief groove -bd 2 \
	-command [list ButtonActivated $oneCommand] -anchor w
    button $framename.button2 -text $twoText -relief groove -bd 2 \
	-command [list ButtonActivated $twoCommand] -anchor w
    pack $framename.label -side left
    pack $framename.button2 -side right -expand true -fill x 
    pack $framename.button1 -side right -expand true -fill x
}

proc DemoLineThreeEntries { framename label oneText oneCommand twoText twoCommand threeText threeCommand } {
    label $framename.label -text $label -width 20
    button $framename.button1 -text $oneText -relief groove -bd 2 \
	-command [list ButtonActivated $oneCommand] -anchor w
    button $framename.button2 -text $twoText -relief groove -bd 2 \
	-command [list ButtonActivated $twoCommand] -anchor w
    button $framename.button3 -text $threeText -relief groove -bd 2 \
	-command [list ButtonActivated $threeCommand] -anchor w
    pack $framename.label -side left
    pack $framename.button3 -side right -expand true -fill x 
    pack $framename.button2 -side right -expand true -fill x 
    pack $framename.button1 -side right -expand true -fill x 
}

proc DemoLineFourEntries { framename label oneText oneCommand twoText twoCommand threeText threeCommand fourText fourCommand } {
    label $framename.label -text $label -width 20
    button $framename.button1 -text $oneText -relief groove -bd 2 \
	-command [list ButtonActivated $oneCommand] -anchor w
    button $framename.button2 -text $twoText -relief groove -bd 2 \
	-command [list ButtonActivated $twoCommand] -anchor w
    button $framename.button3 -text $threeText -relief groove -bd 2 \
	-command [list ButtonActivated $threeCommand] -anchor w
    button $framename.button4 -text $fourText -relief groove -bd 2 \
	-command [list ButtonActivated $fourCommand] -anchor w
    pack $framename.label -side left
    pack $framename.button4 -side right -expand true -fill x 
    pack $framename.button3 -side right -expand true -fill x 
    pack $framename.button2 -side right -expand true -fill x 
    pack $framename.button1 -side right -expand true -fill x 
}

proc DemoLineFiveEntries { framename label oneText oneCommand twoText twoCommand threeText threeCommand fourText fourCommand fiveText fiveCommand } {
    label $framename.label -text $label -width 20
    button $framename.button1 -text $oneText -relief groove -bd 2 \
	-command [list ButtonActivated $oneCommand] -anchor w
    button $framename.button2 -text $twoText -relief groove -bd 2 \
	-command [list ButtonActivated $twoCommand] -anchor w
    button $framename.button3 -text $threeText -relief groove -bd 2 \
	-command [list ButtonActivated $threeCommand] -anchor w
    button $framename.button4 -text $fourText -relief groove -bd 2 \
	-command [list ButtonActivated $fourCommand] -anchor w
    button $framename.button5 -text $fiveText -relief groove -bd 2 \
	-command [list ButtonActivated $fiveCommand] -anchor w
    pack $framename.label -side left
    pack $framename.button5 -side right -expand true -fill x 
    pack $framename.button4 -side right -expand true -fill x 
    pack $framename.button3 -side right -expand true -fill x 
    pack $framename.button2 -side right -expand true -fill x 
    pack $framename.button1 -side right -expand true -fill x 
}



proc ButtonActivated { event } {
    global Demo

    puts stderr "$event: $Demo($event:command)"
    if { [string match "OperatingDisconnected*" $event] == 1 } then {
	for { set i 1 } { $i < [llength $Demo($event:command)] } { incr i; incr i } {
	    puts "ServerConnectionGoneEvent [lindex $Demo($event:command) $i]"
	}
    }
    if { [string match "OperatingWeaklyConnected*" $event] == 1 } then {
	for { set i 1 } { $i < [llength $Demo($event:command)] } { incr i; incr i } {
	    puts "ServerConnectionWeakEvent [lindex $Demo($event:command) $i]"
	}
    }
    if { [string match "OperatingStronglyConnected*" $event] == 1 } then {
	for { set i 1 } { $i < [llength $Demo($event:command)] } { incr i; incr i } {
	    puts "ServerConnectionStrongEvent [lindex $Demo($event:command) $i]"
	}
    }
    puts $Demo($event:command)

    flush stdout
}

proc DemoReadMeWindow { framename windowname } {
    global Demo

    set width "5i"

    set READMEmessage "This is a demo of the CodaConsole interface.  During the demonstration, the advice monitor is running as one process and a fake Venus is running with this process.  Pressing the buttons that will appear on the following screen causes the fake Venus to trigger the specified event.  It triggers this event by sending the appropriate RPC message to the advice monitor.  Eventually, Venus itself will call these RPCs when it detects the various events.  The purpose of this demo is to show the advice monitor's functionality."

    toplevel $framename
    wm title $framename  $windowname
    wm geometry $framename +5-25
    message $framename.msg \
	-text $READMEmessage \
	-justify left \
	-width $width
    button $framename.ok \
	-text "OK" \
	-relief groove \
	-bd 6 \
	-command "destroy $framename"
    bind $framename.ok <Return> { destroy $framename}
    pack $framename.msg -side top -fill both -expand true -padx 10 -pady 10
    pack $framename.ok -side top -fill both -expand true -padx 10 -pady 10
    tkwait visibility $framename
    grab set $framename
    tkwait window $framename
}

proc DemoInitialize { } {
    global Demo

    set Demo(ListOfEvents) [list TokensAcquired TokenExpiry ActivityPendingTokens FillRVM FixRVM FillCache FixCache OperatingDisconnectedToAll OperatingDisconnectedToOne OperatingWeaklyConnectedToAll OperatingWeaklyConnectedToOne OperatingStronglyConnectedToAll OperatingStrongConnectedToOne DisconnectedCacheMiss WeakCacheMiss ReadDisconnectedCacheMiss ReconnectionSurvey HoardWalkBegin HoardWalkPendingAdvice HoardWalkEnd InconsistencyDiscovered]

    set Demo(TokensAcquired:command) "TokensAcquiredEvent"
    set Demo(TokenExpiry:command) "TokenExpiryEvent"
    set Demo(ActivityPendingTokens:command) [list ActivityPendingTokensEvent reintegrate /coda/usr/mre/OBJS]

    set Demo(FillRVM:command) [list UpdateSpaceStatistics 2718 70125 19663872]
    set Demo(FixRVM:command) [list UpdateSpaceStatistics 2718 70125 663872]
    set Demo(FillCache:command) [list UpdateSpaceStatistics 2718 85125 663872]
    set Demo(FixCache:command) [list UpdateSpaceStatistics 2718 70125 663872]

    set Demo(OperatingDisconnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 0 rossini 0 puccini 0 grieg 0 haydn 0 wagner 0]
    set Demo(OperatingDisconnectedToOne:command) [list ServerInaccessible scarlatti]

puts stderr $Demo(OperatingDisconnectedToAll:command)
puts stderr $Demo(OperatingDisconnectedToOne:command)

    set Demo(OperatingWeaklyConnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 59600 puccini 49600 rossini 65600 wagner 52600 haydn 41600 grieg 74600]
    set Demo(OperatingWeaklyConnectedToOne:command) [list ServerBandwidthEstimateEvent scarlatti 59600]
    set Demo(OperatingStronglyConnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 224611 rossini 239908 puccini 224348 grieg 221331 haydn 243572 wagner 229957]
    set Demo(OperatingStronglyConnectedToOne:command) [list ServerAccessible scarlatti]

    set Demo(DisconnectedCacheMiss:command) \
	[list DisconnectedCacheMissEvent /tmp/output2 \
					 /coda/usr/mre/thesis/dissertation/introduction.tex \
					 latex2e]
    set Demo(WeakCacheMiss:command) \
	[list WeakMissEvent /coda/usr/mre/thesis/dissertation/user_interface.tex latex2e 35 20]
    set Demo(ReadDisconnectedCacheMiss:command) \
	[list ReadDisconnectedCacheMissEvent /coda/usr/mre/src/coda-src/advice/CodaConsole tixwish]
    set Demo(ReconnectionSurvey:command) "ReconnectionSurvey"

    set Demo(HoardWalkBegin:command) "HoardWalkBeginEvent"
    set Demo(HoardWalkPendingAdvice:command) "HoardWalkPendingAdviceEvent"
    set Demo(HoardWalkEnd:command) "HoardWalkEndEvent"
    set Demo(HoardWalkPeriodicOn:command) "HoardWalkPeriodicOnEvent"
    set Demo(HoardWalkPeriodicOff:command) "HoardWalkPeriodicOffEvent"

    set Demo(InconsistencyDiscovered:command) \
	[list NeedsRepair /coda/usr/mre/thesis/dissertation]

    set Demo(ReintegrationPending:command) \
	    [list ReintegrationPendingEvent /coda/usr/mre]
    set Demo(ReintegrationCompleted:command) \
	    [list ReintegrationCompleteEvent /coda/usr/mre]
    set Demo(ReintegrationActive:command) \
	    [list ReintegrationActiveEvent /coda/usr/mre]
    set Demo(ReintegrationEnabled:command) \
	    [list ReintegrationEnabledEvent /coda/usr/mre]

    set Demo(RepairPending:command) \
	    [list RepairPendingEvent /coda/usr/mre/thesis/dissertation]
    set Demo(RepairCompleted:command) \
	    [list RepairCompleteEvent /coda/usr/mre/thesis/dissertation]

    set Demo(AllTasksAvailable:command) "MakeAllHoardedTasksAvailable"
    set Demo(AllTasksUnavailable:command) "MakeAllHoardedTasksUnavailable"
}

proc RepairPending { } {
    Re
}

wm withdraw .

set SetupRC [catch {exec csh /coda/usr/mre/thesis/UserStudy/CodaConsoleSetup/DemoSetup} result]

DemoReadMeWindow .readme "README"

set Demo(MainWindow) .demo

toplevel $Demo(MainWindow)
wm title $Demo(MainWindow) "Demonstrate CodaConsole"
wm geometry $Demo(MainWindow) +5-25

DemoInitialize
DemoVisualStart



