#!/bin/sh
# The next line restarts using a customized wish\
exec /coda/usr/mre/newbuild/src/coda-src/console/CodaConsole.out  "$0" "$@"

proc Quit { } {
    global Demo

    set result [exec kill $Demo(PID)]
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
    DemoLineTwoEntries $Demo(MainWindow).disconnectedNetwork "Disconnected:" \
	"ToOne" "OperatingDisconnectedToOne" \
	"ToAll" "OperatingDisconnectedToAll"
    DemoLineTwoEntries $Demo(MainWindow).weakNetwork "Weakly Connected:" \
	"ToOne" "OperatingWeaklyConnectedToOne" \
	"ToAll" "OperatingWeaklyConnectedToAll"
    DemoLineTwoEntries $Demo(MainWindow).strongNetwork "Strongly Connected:" \
	"ToOne" "OperatingStronglyConnectedToOne" \
	"ToAll" "OperatingStronglyConnectedToAll"
    DemoLineThreeEntries $Demo(MainWindow).advice "Advice Requests:" \
	"Disco Miss" "DisconnectedCacheMiss" \
	"Weak Miss" "WeakCacheMiss" \
	"Read Miss" "ReadDisconnectedCacheMiss"
    DemoLineThreeEntries $Demo(MainWindow).hoard "Hoard Walks:" \
	"Begin" "HoardWalkBegin" \
	"Advice Pending" "HoardWalkPendingAdvice" \
	"End" "HoardWalkEnd"
    DemoLineTwoEntries $Demo(MainWindow).reintegration "Reintegration:" \
	"Pending" "ReintegrationPending" \
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

proc ButtonActivated { event } {
    global Demo

    puts "$event ==> send CodaConsole $Demo($event:command)"
    send CodaConsole $Demo($event:command)
}

proc DemoReadMeWindow { framename windowname } {
    global Demo

    set width "5i"

    set READMEmessage "This is a demo of the CodaConsole interface.  The events are triggered by pressing the various buttons rather than by Venus.  Eventually, these events will be triggered by Venus, but this aspect of the interface has not yet been implemented"

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

    set Demo(ListOfEvents) [list TokensAcquired TokenExpiry ActivityPendingTokens FillRVM FixRVM FillCache FixCache OperatingDisconnectedToAll OperatingDisconnectedToOne OperatingWeaklyConnectedToAll OperatingWeaklyConnectedToOne OperatingStronglyConnectedToAll OperatingStrongConnectedToOne DisconnectedCacheMiss WeakCacheMiss ReadDisconnectedCacheMiss HoardWalkBegin HoardWalkPendingAdvice HoardWalkEnd InconsistencyDiscovered]

    set Demo(TokensAcquired:command) "TokensAcquiredEvent"
    set Demo(TokenExpiry:command) "TokenExpiryEvent"
    set Demo(ActivityPendingTokens:command) [list ActivityPendingTokensEvent reintegrate /coda/usr/mre/OBJS]

    set Demo(FillRVM:command) [list UpdateSpaceStatistics 2718 70125 19663872]
    set Demo(FixRVM:command) [list UpdateSpaceStatistics 2718 70125 663872]
    set Demo(FillCache:command) [list UpdateSpaceStatistics 2718 85125 663872]
    set Demo(FixCache:command) [list UpdateSpaceStatistics 2718 70125 663872]

    set Demo(OperatingDisconnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 0 rossini 0 puccini 0 grieg 0 haydn 0 wagner 0]
    set Demo(OperatingDisconnectedToOne:command) [list ServerBandwidthEstimateEvent scarlatti 0]
    set Demo(OperatingWeaklyConnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 59600 puccini 49600 rossini 65600 wagner 52600 haydn 41600 grieg 74600]
    set Demo(OperatingWeaklyConnectedToOne:command) [list ServerBandwidthEstimateEvent scarlatti 59600]
    set Demo(OperatingStronglyConnectedToAll:command) [list ServerBandwidthEstimateEvent scarlatti 224611 rossini 239908 puccini 224348 grieg 221331 haydn 243572 wagner 229957]
    set Demo(OperatingStronglyConnectedToOne:command) [list ServerBandwidthEstimateEvent scarlatti 224611]

    set Demo(DisconnectedCacheMiss:command) \
	[list DisconnectedCacheMissEvent /tmp/output2 \
					 /coda/usr/mre/thesis/dissertation/introduction.tex \
					 latex2e]
    set Demo(WeakCacheMiss:command) \
		[list WeakMissEvent /coda/usr/mre/thesis/dissertation/user_interface.tex latex2e 35 20]
    set Demo(ReadDisconnectedCacheMiss:command) \
		[list ReadDisconnectedCacheMissEvent /coda/usr/mre/newbuild/src/coda-src/advice/CodaConsole tixwish]

    set Demo(HoardWalkBegin:command) "HoardWalkBeginEvent"
    set Demo(HoardWalkPendingAdvice:command) "HoardWalkPendingAdviceEvent"
    set Demo(HoardWalkEnd:command) "HoardWalkEndEvent"

    set Demo(InconsistencyDiscovered:command) \
	[list NeedsRepair /coda/usr/mre/thesis/dissertation]

    set Demo(ReintegrationPending:command) \
	    [list ReintegrationPendingEvent /coda/usr/mre]
    set Demo(ReintegrationCompleted:command) \
	    [list ReintegrationCompleteEvent /coda/usr/mre]

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
set Demo(PID) [exec /coda/usr/mre/newbuild/src/coda-src/console/CodaConsole &]
after 5000 {send CodaConsole InitializeForExercises }

DemoReadMeWindow .readme "README"

set Demo(MainWindow) .demo

toplevel $Demo(MainWindow)
wm title $Demo(MainWindow) "Demonstrate CodaConsole"
wm geometry $Demo(MainWindow) +5-25

DemoInitialize
DemoVisualStart



