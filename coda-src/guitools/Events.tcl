#
# The events "data structure" contains entries for each event.
# Each event entry contains the following information:
#	Name 			(char *)
#	Message			(char *)
#	Urgency 		(Critical, Warning, Normal)
#	Notify 			(boolean)
#	    Popup 		(boolean)
#	    Beep 		(boolean)
#	    Flash 		(boolean)
#
proc InitEventsArray { } {
    global Pathnames
    global Events

    set Events(List) "TokensAcquired TokenExpiry ActivityPendingTokens SpaceNormal SpaceWarning SpaceCritical OperatingStronglyConnected OperatingWeaklyConnected OperatingDisconnected ConsiderAddingAdvice ConsiderRemovingAdvice WeaklyConnectedCacheMissAdvice ReadDisconnectedCacheMissAdvice DisconnectedCacheMissAdvice ReconnectionSurvey HoardWalkBegin HoardWalkPendingAdvice HoardWalkEnd HoardWalkOff HoardWalkOn ReintegrationPending ReintegrationEnabled ReintegrationActive ReintegrationCompleted RepairPending RepairCompleted AllTasksAvailable OneOrMoreTasksUnavailable"

    set Events(TokensAcquired:Name) "Tokens Acquired"
    set Events(TokenExpiry:Name) "Token Expiry"
    set Events(ActivityPendingTokens:Name) "Activity Pending Tokens"
    set Events(SpaceNormal:Name) "Space Normal (<= 90%)"
    set Events(SpaceWarning:Name) "Space Warning (> 90%)"
    set Events(SpaceCritical:Name) "Space Critical (>95%)"
    set Events(OperatingStronglyConnected:Name) "Operating Strongly Connected"
    set Events(OperatingWeaklyConnected:Name) "Operating Weakly Connected"
    set Events(OperatingDisconnected:Name) "Operating Disconnected"
#    set Events(NetworkConnectivityDecrease:Name) "Network Connectivity Decrease"
#    set Events(NetworkConnectivityIncrease:Name) "Network Connectivity Increase"
    set Events(ConsiderAddingAdvice:Name) "Consider Adding Advice"
    set Events(ConsiderRemovingAdvice:Name) "Consider Removing Advice"
    set Events(WeaklyConnectedCacheMissAdvice:Name) "Weakly Connected Cache Miss Advice"
    set Events(ReadDisconnectedCacheMissAdvice:Name) "Read Disconnected Cache Miss Advice"
    set Events(DisconnectedCacheMissAdvice:Name) "Disconnected Cache Miss Advice"
    set Events(ReconnectionSurvey:Name) "Reconnection Survey"
    set Events(HoardWalkBegin:Name) "Hoard Walk Begin"
    set Events(HoardWalkPendingAdvice:Name) "Hoard Walk Pending Advice"
    set Events(HoardWalkEnd:Name) "Hoard Walk End"
    set Events(HoardWalkOff:Name) "Hoard Walk Off"
    set Events(HoardWalkOn:Name) "Hoard Walk On"
    set Events(ReintegrationPending:Name) "Reintegration Pending"
    set Events(ReintegrationEnabled:Name) "Reintegration Enabled"
    set Events(ReintegrationActive:Name) "Reintegration Active"
    set Events(ReintegrationCompleted:Name) "Reintegration Completed"
    set Events(RepairPending:Name) "Repair Pending"
    set Events(RepairCompleted:Name) "Repair Completed"
    set Events(AllTasksAvailable:Name) "All Tasks Available"
    set Events(OneOrMoreTasksUnavailable:Name) "One Or More Tasks Unavailable"

    set Events(TokensAcquired:Indicator) Tokens
    set Events(TokenExpiry:Indicator) Tokens
    set Events(ActivityPendingTokens:Indicator) Tokens
    set Events(SpaceNormal:Indicator) Space
    set Events(SpaceWarning:Indicator) Space
    set Events(SpaceCritical:Indicator) Space
    set Events(OperatingStronglyConnected:Indicator) Network
    set Events(OperatingWeaklyConnected:Indicator) Network
    set Events(OperatingDisconnected:Indicator) Network
#    set Events(NetworkConnectivityDecrease:Indicator) Network
#    set Events(NetworkConnectivityIncrease:Indicator) Network
    set Events(ConsiderAddingAdvice:Indicator) Advice
    set Events(ConsiderRemovingAdvice:Indicator) Advice
    set Events(WeaklyConnectedCacheMissAdvice:Indicator) Advice
    set Events(ReadDisconnectedCacheMissAdvice:Indicator) Advice
    set Events(DisconnectedCacheMissAdvice:Indicator) Advice
    set Events(ReconnectionSurvey:Indicator) Advice
    set Events(HoardWalkBegin:Indicator) HoardWalk
    set Events(HoardWalkPendingAdvice:Indicator) HoardWalk
    set Events(HoardWalkEnd:Indicator) HoardWalk
    set Events(HoardWalkOff:Indicator) HoardWalk
    set Events(HoardWalkOn:Indicator) HoardWalk
    set Events(ReintegrationPending:Indicator) Reintegration
    set Events(ReintegrationEnabled:Indicator) Reintegration
    set Events(ReintegrationActive:Indicator) Reintegration
    set Events(ReintegrationCompleted:Indicator) Reintegration
    set Events(RepairPending:Indicator) Repair
    set Events(RepairCompleted:Indicator) Repair
    set Events(AllTasksAvailable:Indicator) Task
    set Events(OneOrMoreTasksUnavailable:Indicator) Task

    set Events(TokensAcquired:Message) "This event is triggered when you obtain tokens."
    set Events(TokenExpiry:Message) "This event is triggered when your tokens expire."
    set Events(ActivityPendingTokens:Message) "This event is triggered when Venus activity is waiting until you obtain tokens."
    set Events(SpaceNormal:Message) "This event is triggered when the available cache space, disk partition space or RVM space rises above 10%."
    set Events(SpaceWarning:Message) "This event is triggered when the available cache space, disk partition space or RVM space drops below 10%."
    set Events(SpaceCritical:Message) "This event is triggered when the available cache space, disk partition space or RVM space drops below 5%."
    set Events(OperatingStronglyConnected:Message) "This event is triggered when Venus begins operating strongly connected with respect to all the subtrees of the file system."
    set Events(OperatingWeaklyConnected:Message) "This event is triggered when Venus begins operating weakly connected with respect to at least one subtree of the file system."
    set Events(OperatingDisconnected:Message) "This event is triggered when Venus begins operating disconnected with respect to at least one subtree of the file system."
#    set Events(NetworkConnectivityDecrease:Message) "This event is triggered when the network connectivity to a server decreases substantially (10% or more)."
#    set Events(NetworkConnectivityIncrease:Message) "This event is triggered when the network connectivity to a server increases substantially (10% or more)."
    set Events(ConsiderAddingAdvice:Message) "This event occurs when the system identifies objects that it believes you should consider hoarding.  It is triggered on demand."
    set Events(ConsiderRemovingAdvice:Message) "This event occurs when the system identifies objects that it believes you should not be hoarding.  It is triggered on demand."
    set Events(WeaklyConnectedCacheMissAdvice:Message) "This event is triggered by a cache miss while operating weakly connected.  (Operating weakly connected means that your machine is communicating with the Coda servers over a low bandwidth, high latency, or intermittent network connection.)"
    set Events(ReadDisconnectedCacheMissAdvice:Message) "This event is triggered by a cache miss while operating read disconnected.  (Operating read disconnected means that your machine is pretending to be disconnected when servicing cache misses.  Read disconnected mode is primarily used to test whether or not you have hoarded everything you expect to need prior to disconnecting from the network.)"
    set Events(DisconnectedCacheMissAdvice:Message) "This event is triggered by a cache miss while operating disconnected.  (Operating disconnected means that your machine is unable to communicate with the Coda servers.)"
    set Events(ReconnectionSurvey:Message) "This event is triggered upon reconnection to the Coda servers after a period of disconnected operation."
    set Events(HoardWalkBegin:Message) "This event is triggered when Venus begins a hoard walk."
    set Events(HoardWalkPendingAdvice:Message) "This event is triggered when Venus has a hoard walk temporarily waiting for advice."
    set Events(HoardWalkEnd:Message) "This event is triggered when Venus completes a hoard walk."
    set Events(HoardWalkOff:Message) "This event is triggered when Venus (at user request) turns periodic hoard walks off."
    set Events(HoardWalkOn:Message) "This event is triggered when Venus (at user request) turns periodic hoard walks on."
    set Events(ReintegrationPending:Message) "This event is triggered when there is a reintegration waiting until you obtain tokens or a network connection is reestablished."
    set Events(ReintegrationEnabled:Message) "This event is triggered when the user obtains tokens while a reintegration is pending."
    set Events(ReintegrationActive:Message) "This event is triggered when a reintegration is in progress."
    set Events(ReintegrationCompleted:Message) "This event is triggered when a reintegration has completed."
    set Events(RepairPending:Message) "This event is triggered when there is an object that needs to be repaired."
    set Events(RepairCompleted:Message) "This event is triggered when a repair has completed."
    set Events(AllTasksAvailable:Message) "This event is triggered when all tasks that have been selected for hoarding become available."
    set Events(OneOrMoreTasksUnavailable:Message) "This event is triggered when one or more tasks that have been selected for hoarding become unavailable."

    #
    # Initialize the Events list to some basic values so that any events
    # not directly configured by the user will have a reasonable starting
    # value.
    #
    foreach event $Events(List) {
	set Events($event:Urgency) Critical
	set Events($event:Notify) Yes
	set Events($event:Notify:Popup) No
	set Events($event:Notify:Beep) No
	set Events($event:Notify:Flash) No
    }

    # Special case for unknown event.  We need to know its urgency for choosing a color
    set Events(Unknown:Urgency) Unknown

    set Events(PopupActive) No
}

proc RegisterEvents { } {
    global Events

    # Get output lock
    Lock Output

    # Print Initial String, followed by each active event
    puts "RegisterEvents"
    
    # Token Related
    if { [RegisterEvent TokensAcquired] == 1 } then {
	puts " TokensAcquired"
    }
    if { [RegisterEvent TokenExpiry] == 1 } then {
	puts " TokensExpired"
    } 
    if { [RegisterEvent ActivityPendingTokens] == 1 } then {
	puts " ActivityPendingTokens"
    }

    # Space Related
    if { ([RegisterEvent SpaceNormal] == 1) || 
         ([RegisterEvent SpaceWarning] == 1) || 
         ([RegisterEvent SpaceCritical] == 1) } then {
	     puts " SpaceInformation"
    }

    # Network Related
    if { ([RegisterEvent OperatingStronglyConnected] == 1) } then {
	puts " ServerAccessible"
	puts " ServerConnectionStrongEvent"
    }
    if { [RegisterEvent OperatingWeaklyConnected] == 1 } then {
	puts " NetworkQualityEstimate"
	puts " ServerConnectionWeakEvent"
    }
    if { [RegisterEvent OperatingDisconnected] == 1 } then {
	puts " ServerInaccessible"
    }

    # Advice Related
    #
    # Note the absence of ConsiderAddingAdvice and ConsiderRemovingAdvice registration.
    # Venus maintains the statistics necessary for these advice requests regardless of
    # interest, since long-term data important.  Further, Venus does not trigger any
    # communication associated with these events so it need not avoid that communication.
    #
    if { [RegisterEvent ReadDisconnectedCacheMissAdvice] == 1 } then {
	puts " ReadDisconnectedCacheMissEvent"
    }
    if { [RegisterEvent WeaklyConnectedCacheMissAdvice] == 1 } then {
	puts " WeaklyConnectedCacheMissEvent"
    }
    if { [RegisterEvent DisconnectedCacheMissAdvice] == 1 } then {
	puts " DisconnectedCacheMissEvent"
    }
    if { [RegisterEvent ReconnectionSurvey] == 1 } then {
	puts " Reconnection"
    }

    # HoardWalk Related
    if { [RegisterEvent HoardWalkOn] == 1 } then {
	puts " HoardWalkPeriodicOn"
    }
    if { [RegisterEvent HoardWalkOff] == 1 } then {
	puts " HoardWalkPeriodicOff"
    }
    if { [RegisterEvent HoardWalkBegin] == 1 } then {
	puts " HoardWalkBegin"
    }
    if { [RegisterEvent HoardWalkEnd] == 1 } then {
	puts " HoardWalkEnd"
    }
    if { ([RegisterEvent HoardWalkBegin] == 1) && ([RegisterEvent HoardWalkEnd] == 1) } then {
	puts " HoardWalkStatus"
    }
    if { [RegisterEvent HoardWalkPendingAdvice] == 1 } then {
	puts " HoardWalkAdviceRequest"
    }

    # Reintegration Related
    if { [RegisterEvent ReintegrationPending] == 1 } then {
	puts " ReintegrationPendingTokens"
    }
    if { [RegisterEvent ReintegrationEnabled] == 1 } then {
	puts " ReintegrationEnabled"
    }
    if { [RegisterEvent ReintegrationActive] == 1 } then {
	puts " ReintegrationActive"
    }
    if { [RegisterEvent ReintegrationCompleted] == 1 } then {
	puts " ReintegrationCompleted"
    }

    # Repair Related
    if { [RegisterEvent RepairPending] == 1 } then {
	puts " ObjectInConflict"
    }
    if { [RegisterEvent RepairCompleted] == 1 } then {
	puts " ObjectConsistent"
    }

    # Task Related
    if { [RegisterEvent AllTasksAvailable] == 1 } then {
	puts " TaskAvailability"
    }
    if { [RegisterEvent OneOrMoreTasksUnavailable] == 1 } then {
	puts " TaskUnavailable"
    }

    # Print final string
    puts "END"
    puts ""
    flush stdout

    # Release output lock
    UnLock Output
}

proc RegisterEvent { event } {
    global Events

    if { $Events($event:Notify) == "Yes" } then {
        return 1
    } else {
        return 0
    }
}

proc NotifyEvent { event indicator popupCommand } {
    global Events
    global Indicator
    global Window
    global Colors

    if { $Events($event:Notify) == "Yes" } then {

 	CancelPendingIndicatorEvents $indicator

        set oldColor [$Window.indicators.[string tolower $indicator] cget -foreground]
	IndicatorUpdate $indicator
	update idletasks

  	if { $Events($event:Notify:Beep) == "Yes" } then {
	    bell
	    after 1000 { bell }
	    after 2000 { bell }
	}

	if { $Events($event:Notify:Popup) == "Yes" } then {
		LogAction "Advice Invokation: automatic"
		after 50 $popupCommand
	        update idletasks
	        # If the popupCommand was ${indicator}VisualStart, then the idletasks above
	        # is necessary so that we get the thing fully visible before we do the
	        # ${indicator}CheckVisibilityAndUpdateScreen upon return.  Otherwise,
	        # the window will only be partially visible and will have the incorrect
	        # geometry and when {$indicator}CheckVisibilityAndUpdateScreen redraws the
	        # window, it will have size 1x1.  As it is, this causes the window to come
	        # up, immediately disappear, and then reappear.  Not good, but at least it
	        # looks right eventually.
	}

	if { $Events($event:Notify:Flash) == "Yes" } then {
	    IndicatorFlash 10 $indicator $Colors(UnFlash)
	}
    } else {
	puts stderr "Event $event for indicator $indicator is not configured to notify the user"
	flush stderr
    }
}

proc ConfigureEvent { event notify urgency popup beep flash} {
    global Events
    global ControlPanel

    set Events($event:Urgency) $urgency
    set Events($event:Notify) $notify
    set Events($event:Notify:Popup) $popup
    set Events($event:Notify:Beep) $beep
    set Events($event:Notify:Flash) $flash

    set ControlPanel(Events:$event:Urgency) $urgency
    set ControlPanel(Events:$event:Notify) $notify
    set ControlPanel(Events:$event:Notify:Popup) $popup
    set ControlPanel(Events:$event:Notify:Beep) $beep
    set ControlPanel(Events:$event:Notify:Flash) $flash
}

proc ConfigEventsForStudy { } {
    global Events

    foreach event [list WeaklyConnectedCacheMissAdvice DisconnectedCacheMissAdvice] {
	ConfigureEvent $event Yes Critical Yes No Yes
    }

    ConfigureEvent OneOrMoreTasksUnavailable Yes Critical No Yes Yes
}

proc GetEventUrgency { event } {
    global Events

    return $Events(${event}:Urgency)
}



