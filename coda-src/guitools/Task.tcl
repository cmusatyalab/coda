##############################################################################
#
#
#  Task Indicator
#
#
##############################################################################

proc TaskGetColor { task } {
    global Statistics
    global Colors

    set usage [GetTaskAvailability $task]

    if { $usage == -1 } then {
	set meterColor $Colors(Unknown)
    } elseif { $usage == $Statistics(Full) } then {
        set meterColor $Colors(Normal)
    } else {
        set meterColor $Colors(Critical)
    }

    return $meterColor
}

proc TaskMakeRightLabel { maxSpace } {

    set righttext [format "%d MB" $maxSpace]
    if { $righttext == "-1 MB" } then {
	set righttext "?? MB"
    } elseif { $righttext == "0 MB" } then {
	set righttext "<1MB"
    }

    return $righttext
}

proc TaskUpdateMeterColor { task } {
    global Task

    regsub -all " " $task "*-*" funnytask    
    
    if { [winfo exists $Task(selectedlist).sel$funnytask] } then {
	$Task(selectedlist).sel$funnytask config -foreground [TaskGetColor $task]
    } 
}

proc TaskUpdateMeterAvailability { task } {
    global Task

    regsub -all " " $task "*-*" funnytask    
    
    if { [winfo exists $Task(selectedlist).sel$funnytask] } then {
	set usage [GetTaskAvailability $task]
	if { $usage == -1 } then {
	    set usage 0
	}

	$Task(selectedlist).sel$funnytask config -percentFull $usage
    }

    TaskUpdateMeterColor $task
}

proc TaskUpdateMeterMaxSize { task } {
    global Task
    global Dimensions
    global Statistics

    regsub -all " " $task "*-*" funnytask    

    if { [winfo exists $Task(selectedlist).sel$funnytask] } then {
	set max            $Statistics(Blocks:Allocated)
	set maxSpace       [GetTaskMaxSpaceMB $task]
	set allocated      [expr $maxSpace * 1024]
	set meterWidth     $Dimensions(Meter:Width)
	set relative_width [max 50 [expr round($meterWidth * $allocated / $max)] ]
	set rightLabel     [TaskMakeRightLabel $maxSpace]

	$Task(selectedlist).sel$funnytask config -rightLabel $rightLabel
	$Task(selectedlist).sel$funnytask config -width $relative_width
    }
}

proc TaskChangePriorityLabel { task priority } {
    global Task

    regsub -all " " $task "*-*" funnytask    

    if { [winfo exists $Task(MainWindow).sel$funnytask] } then {
        set labelText      [format "%d %s" $priority $task]

	$Task(MainWindow).sel$funnytask config -label $labelText
    }
}

proc TaskMeter { task priority } {
    global Task
    global Dimensions
    global Statistics

    regsub -all " " $task "*-*" funnytask    

    set framename      $Task(selectedlist).sel$funnytask
    set labelWidth     [expr $Task(selectedlist:maxlabellength) + 3]
    set labelText      [format "%d %s" $priority $task]
    set max            $Statistics(Blocks:Allocated)
    set maxSpace       [GetTaskMaxSpaceMB $task]
    set allocated      [expr $maxSpace * 1024]
    set meterWidth     $Dimensions(Meter:Width)
    set relative_width [max 50 [expr round($meterWidth * $allocated / $max)] ]
    set backColor      [$Task(selectedlist) cget -bg]
    set meterColor     [TaskGetColor $task]
    set righttext      [TaskMakeRightLabel $maxSpace]

    set usage [GetTaskAvailability $task]
    if { $usage == -1 } then {
	set usage 0
    }

    tixCodaMeter $framename \
	-background $backColor \
	-height $Dimensions(Meter:Height) \
	-width $relative_width \
	-labelWidth $labelWidth \
	-label $labelText \
	-rightLabel $righttext \
	-state normal \
	-percentFull $usage \
	-foreground $meterColor
    pack $framename -side left -padx 4 -pady 4 -anchor w
}

proc TaskInitData { } {
    global Task

    set Task(MainWindow) .task
    set Task(Geometry) 540x450+0+0

    InitUserDataData
    InitProgramData
    InitTaskData
    InitSelectedTaskList

    InitTaskAvailability
    set Task(AvailableTaskList) [list ]
    foreach task $Task(SelectedTaskList) {
	if { [GetTaskAvailability $task] == 100 } then {
	    set Task(AvailableTaskList) [lappend Task(AvailableTaskList) $task]
 	}
    }

    # Use Fibbonnacci sequence to determine number of each sized gap
    # e.g. 1 gap of 200; 1 gap of 100; 2 gaps of 50; 3 gaps of 25; 5 gaps of 20;
    #      8 gaps of 15; 13 gaps of 10; 21 gaps of 5;  34 gaps of 1
    # All tasks beyond this list will receive a priority of 30
    set Task(MinimumPriority) 30
    set Task(Priorities) [list 1000 \
	                       800 \
			       700 \
			       650 600 \
			       575 550 525 \
			       505 485 465 445 425 \
			       410 395 380 365 350 335 320 305 \
			       295 285 275 265 255 245 235 225 215 205 195 185 175 \
			       170 165 160 155 150 145 140 135 130 125 120 115 110 105 100 95 \
			                                                           90 85 80 75 70 \
			       69 68 67 66 65 64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 \
			                                   48 47 46 45 44 43 42 41 40 39 38 37 36 ]
}

proc TaskVisualStart { } {
    global Window
    global Task
    global TaskDefinition
    global Colors
    global DisplayStyle
    global Dimensions
    global Statistics

    CheckStatisticsCurrency $Statistics(Currency)

    TaskDefinitionDataInit

    if { [winfo exists $Task(MainWindow)] } then {
	raise $Task(MainWindow)
	return
    }

    toplevel $Task(MainWindow)
    wm title $Task(MainWindow) "Task Information"
    wm geometry $Task(MainWindow) $Task(Geometry)
    wm protocol $Task(MainWindow) WM_DELETE_WINDOW {TaskVisualEnd}

    set Task(CacheStatus) [frame $Task(MainWindow).cache -bd 1 -relief groove]
    SpaceMeter $Task(CacheStatus).cachemeter \
	"Cache Space:\n(in MB)" \
        [format "%d" [expr round($Statistics(Blocks:Allocated) / 1024)]] \
	$Dimensions(Meter:Width) \
	$Statistics(Blocks:Allocated) \
	$Statistics(Blocks:Allocated) \
	[GetCacheUsage]
	
#    tixCodaMeter $Task(CacheStatus).cachemeter -leftLabel "Empty" -rightLabel "Full" \
#	-label "Cache Space" -labelWidth 15 -foreground $Colors(meter) \
#	-width $Dimensions(Meter:Width) -height $Dimensions(Meter:Height) \
#	-percentFull [GetCacheUsage]
    pack $Task(CacheStatus).cachemeter -side top -expand true -fill both
    pack $Task(CacheStatus) -side top -fill x

    set Task(Panes) $Task(MainWindow).panes
    tixPanedWindow $Task(Panes)
    $Task(Panes) add allTasks -min 130 -size 150
    $Task(Panes) add selectedTasks -min 130 -size 150

    set Task(allTasks) [$Task(Panes) subwidget allTasks]
    set Task(allTasksLabelledFrame) $Task(allTasks).f
    tixLabelFrame $Task(allTasksLabelledFrame) -label "Tasks" -options {
	label.padX 5
    }
    pack $Task(allTasksLabelledFrame) -side left \
	-padx 10 -pady 10 -expand true -fill both

    set Task(allTasksListFrame) [$Task(allTasksLabelledFrame) subwidget frame]
    tixScrolledHList $Task(allTasksListFrame).tasklist -options {
	hlist.separator ";"
    }
    pack $Task(allTasksListFrame).tasklist -side right \
	-padx 10 -pady 10 -expand yes -fill both 

    set list [$Task(allTasksListFrame).tasklist subwidget hlist]
    for {set i 0; set maxLength 0} {$i < [llength $TaskDefinition(TaskList)]} {incr i} {
	set task [lindex $TaskDefinition(TaskList) $i]
	set length [string length $task]
	if { $length > $maxLength } then { set maxLength $length }
	regsub -all " " $task "*-*" funnytask
	$list add task$funnytask -itemtype text -text $task -style $DisplayStyle(Normal) 
    }
    $list entryconfigure taskNew... -style $DisplayStyle(Italic)
    $list configure -command SelectTask

    set Task(selectedTasks) [$Task(Panes) subwidget selectedTasks]
    set Task(selectedTasksLabelledFrame) $Task(selectedTasks).f
    tixLabelFrame $Task(selectedTasksLabelledFrame) -label "Hoarded Tasks" -options {
	label.padX 5
    }
    pack $Task(selectedTasksLabelledFrame) -side left \
	-padx 10 -pady 10 -expand true -fill both

    set Task(selectedTasksListFrame) [$Task(selectedTasksLabelledFrame) subwidget frame]

    tixScrolledHList $Task(selectedTasksListFrame).tasklist
    pack $Task(selectedTasksListFrame).tasklist -side right \
	-expand yes -fill both -padx 10 -pady 10 
    bind [$Task(selectedTasksListFrame).tasklist subwidget hlist] <ButtonRelease-3> { SelectedTaskDrop %W %y }
    bind [$Task(selectedTasksListFrame).tasklist subwidget hlist] <1> { SelectedTaskSelect %W %y }
    bind [$Task(selectedTasksListFrame).tasklist subwidget hlist] <Double-1> { SelectTaskHelper %W %y } 
    bind [$Task(selectedTasksListFrame).tasklist subwidget hlist] <Delete> { SelectedTaskDelete %W %x %y }
    bind [$Task(selectedTasksListFrame).tasklist subwidget hlist] <BackSpace> { SelectedTaskDelete %W %x %y }

    set Task(selectedlist) [$Task(selectedTasksListFrame).tasklist subwidget hlist]
    set Task(selectedlist:maxlabellength) $maxLength
    set Task(selectedlist:nextindex) 1
    for { set i 1} {$i <= [llength $Task(SelectedTaskList)]} {incr i} {
        set task [lindex $Task(SelectedTaskList) [expr $i - 1]]
	InputSelectedTask $task "" after
    }
    $Task(selectedlist) configure -command UnselectTask
    bind $Task(selectedlist) <ButtonRelease-3> {SelectedTaskDrop %W %y}
    bind $Task(selectedlist) <1> { SelectedTaskSelect %W %y }
    bind $Task(selectedlist) <Double-1> { SelectTaskHelper %W %y }
    bind $Task(selectedlist) <Delete> { SelectedTaskDelete %W %x %y }
    bind $Task(selectedlist) <BackSpace> { SelectedTaskDelete %W %x %y }

    pack $Task(Panes) -expand yes -fill both

    set Task(Buttons) [frame $Task(MainWindow).buttons -bd 2 -relief groove]
    button $Task(Buttons).help -text "Help" \
	-relief groove -bd 2 -anchor w \
	-command { TaskVisualHelpWindow }
    button $Task(Buttons).revert -text "Revert" \
	-relief groove -bd 2 -anchor w \
	-command { AbortTaskChanges; TaskVisualEnd}
    button $Task(Buttons).ok -text "OK" \
	-relief groove -bd 2 -anchor w \
	-command { CommitTaskChanges; TaskVisualEnd }
#    button $Task(Buttons).close -text "Close" -anchor w \
#	-command { TaskVisualEnd }
    pack $Task(Buttons).help -side left
#    pack $Task(Buttons).close -side right
#    pack $Task(Buttons).ok -side right
#    pack $Task(Buttons).revert -side right
    pack $Task(Buttons) -side bottom -fill x

}

proc TaskVisualHelpWindow { } {
    global Task

    LogAction "Task Window: help"
    set messagetext "The meter at the top of the screen shows the state of the cache.\n\nThe listing in the middle of the screen shows the tasks that have been defined.  Double clicking on any defined task will show that task's definition in a new window.\n\nThe listing at the bottom shows the tasks that have been hoarded.  The tasks are listed in priority order and also show what percentage of their associated data is currently available.  To hoard an existing task, click on the task's name and then use the right mouse button to drop this task into the list of hoarded tasks (a rough approximation to a drag and drop).  Note that the location in which you drop a task determines its priority relative to the other hoarded tasks.  To unselect a task, select the task with the mouse and hit the delete key."

    set Task(TopHelpWindow) .taskhelp

    PopupHelp "Task Information Help Window" $Task(TopHelpWindow) $messagetext "5i"
}

proc TaskVisualEnd { } {
    global Task

    LogAction "Task Window: close"
    SetTime "TaskInfoWindow:close"
    set Task(Geometry) [wm geometry $Task(MainWindow)]
    destroy $Task(MainWindow)
}

proc TaskDetermineState { } {
    global Indicator
    global Task

    set AllAvailable 1
    foreach task $Task(SelectedTaskList) {
	set index [lsearch -exact $Task(AvailableTaskList) $task]
	if { $index == -1 } then {
	    set AllAvailable 0
	    break
	}
    }

    if { $AllAvailable == 0 } then {
	set NewState OneOrMoreTasksUnavailable
    } else {
	set NewState AllTasksAvailable
    }

    return $NewState
}

proc ChangeStatesAndUpdateIndicator { } {
    global Indicator

    set OriginalState $Indicator(Task:State)
    set Indicator(Task:State) [TaskDetermineState]

    if { $OriginalState != $Indicator(Task:State) } then {
	NotifyEvent $Indicator(Task:State) Task TaskVisualStart
	SetTime "TaskIndicator:$Indicator(Task:State)"
    }
}


proc TaskCheckVisibilityAndUpdateScreen { } {
    global Task
    global Colors
    global Statistics

    if { [winfo exists $Task(MainWindow)] } then {

	# For each selected task
	foreach task $Task(SelectedTaskList) {

	    # Determine name of widget
  	    regsub -all " " $task "*-*" funnytask

	    if { [winfo exists $Task(selectedlist).sel$funnytask] == 1 } then {

	        set taskAvailability [GetTaskAvailability $task]
		set size $Statistics(Blocks:Allocated)

	        # Update size
		TaskUpdateMeterMaxSize $task

		# Update availability
		TaskUpdateMeterAvailability $task

	        # Update color
		TaskUpdateMeterColor $task 
	    }
	}


    }
}

proc TaskBindings { } {
    global Window
    bind $Window.indicators.task <Double-1> { 
	LogAction "Task Indicator: double-click"
	SetTime "TaskIndicator:doubleclick"
	%W configure -background $Indicator(SelectedBackground)
	TaskVisualStart 
	update idletasks
	SetTime "TaskInfoWindow:visible"
    }
}

proc InputSelectedTask { task which relative_location } {
    global Task
    global Dimensions
    global Statistics
    global Colors

    regsub -all " " $task "*-*" funnytask    

    CheckStatisticsCurrency $Statistics(Currency)

    set index $Task(selectedlist:nextindex)
    incr Task(selectedlist:nextindex)

    TaskMeter $task $index
        
    if { $which == "" } then {
        $Task(selectedlist) add sel$funnytask \
	    -itemtype window \
	    -window $Task(selectedlist).sel$funnytask 
    } else {
	if { $relative_location == "after" } then {
            $Task(selectedlist) add sel$funnytask \
	        -itemtype window \
	        -window $Task(selectedlist).sel$funnytask \
   	        -after $which
	} elseif { $relative_location == "before" } then {
            $Task(selectedlist) add sel$funnytask \
	        -itemtype window \
	        -window $Task(selectedlist).sel$funnytask \
   	        -before $which
	} else {
	    SendToStdErr "Error:  InputSelectedTask unknown relative location = $relative_location"
	    SendToStdErr "        Element not inserted"
	    exit -1
	}
    }

    bind $Task(selectedlist).sel$funnytask.canvas <ButtonRelease-3> { SelectedTaskDrop %W %y }
    bind $Task(selectedlist).sel$funnytask.label <ButtonRelease-3> { SelectedTaskDrop %W %y }
    bind $Task(selectedlist).sel$funnytask <ButtonRelease-3> { SelectedTaskDrop %W %y }

    bind $Task(selectedlist).sel$funnytask.canvas <1> { SelectedTaskSelect %W %y }
    bind $Task(selectedlist).sel$funnytask.label <1> { SelectedTaskSelect %W %y }
    bind $Task(selectedlist).sel$funnytask <1> { SelectedTaskSelect %W %y }

    bind $Task(selectedlist).sel$funnytask.canvas <Double-1> { SelectTaskHelper %W %y }
    bind $Task(selectedlist).sel$funnytask.label <Double-1> { SelectTaskHelper %W %y }
    bind $Task(selectedlist).sel$funnytask <Double-1> { SelectTaskHelper %W %y }

    bind $Task(selectedlist).sel$funnytask.canvas <Delete> { SelectedTaskDelete %W %x %y }
    bind $Task(selectedlist).sel$funnytask.label <Delete> { SelectedTaskDelete %W %x %y }
    bind $Task(selectedlist).sel$funnytask <Delete> { SelectedTaskDelete %W %x %y }

    bind $Task(selectedlist).sel$funnytask.canvas <BackSpace> { SelectedTaskDelete %W %x %y }
    bind $Task(selectedlist).sel$funnytask.label <BackSpace> { SelectedTaskDelete %W %x %y }
    bind $Task(selectedlist).sel$funnytask <BackSpace> { SelectedTaskDelete %W %x %y }

    ReprioritizeSelectedTasks
    Hoard 
}

proc SelectedTaskSelect { window y } {
    global Task

    set Task(SelectedTaskSelect:window) $window
    set Task(SelectedTaskSelect:y) $y

    set hlistWindow [string range $window 0 57]
    set lastDot [string last "." $window]
    if { $lastDot < 58 } then {
	set element [[string range $window 0 57] nearest $y]
    } elseif { $lastDot == 58 } then {
	set element [string range $window 59 end]
    } else {
	set element [string range $window 59 [expr $lastDot - 1]]
    }

    focus $hlistWindow
    update idletasks
    $hlistWindow anchor set $element 
    LogAction "Task Information Window: Select $element"
}

proc SelectedTaskDrop { window y } {
    global Indicator
    global Task

    # Determine which tasks have been selected
    set list [$Task(allTasksListFrame).tasklist subwidget hlist]
    set selectedList [list ]
    foreach task $Task(TaskList) {
	regsub -all " " $task "*-*" funnytask
	if { [$list selection includes task$funnytask] } then { lappend selectedList $task }
    }

    # Determine where they should be dropped
    set location [string range $window 59 end]
    if { [string match *canvas* $window] == 1 } then {
	set canvasHeight [lindex [$window configure -height] 4]
	set element [string range $location 0 [expr [string last "." $location]-1]]
	if { $y < [expr ($canvasHeight + 10) / 2] } then {
	    set relativeOrder before
	} else {
	    set relativeOrder after
	}
    } elseif { [string match *label* $window] == 1 } then {
	set element [string range $location 0 [expr [string last "." $location]-1]]
        set relativeOrder after
    } elseif { [string compare $window $Task(selectedlist)] == 0 } then {
        set relativeOrder after
	set element [$window nearest [expr $y - 10]]
    } else {
	set canvasHeight [lindex [$window configure -height] 4]
	set element [string range $window 59 end]
	if { $y < [expr ($canvasHeight + 10) / 2] } then {
		set relativeOrder before
	} else {
		set relativeOrder after
	}
    }

    foreach task $selectedList {
	regsub -all " " $task "*-*" funnytask

	if { [string compare [string range $element 3 end] $funnytask] == 0 } then {
	    # This element is alreaady in this position -- don't delete and reinsert...
	    continue
	}

        if { [winfo exists $Task(selectedlist).sel$funnytask] == 1 } then {
	    # This element is already selected
    	    SelectedTaskDeleteElement $task
        }
    }

    regsub -all {\*-\*} $element " " realtask
    set realtask [string range $realtask 3 end]
    set index [lsearch -exact $Task(SelectedTaskList) $realtask]

    foreach task $selectedList {
	regsub -all " " $task "*-*" funnytask

	LogAction "Task Information Window: Hoarding $task"

	if { [string compare [string range $element 3 end] $funnytask] == 0 } then {
	    # This element is alreaady in this position -- don't delete and reinsert...
	    continue
	}

	if { $relativeOrder == "before" } then {
            set Task(SelectedTaskList) [linsert $Task(SelectedTaskList) $index $task] 
	} else {
	    set Task(SelectedTaskList) [linsert $Task(SelectedTaskList) [expr $index + 1] $task] 
	}
	InputSelectedTask $task $element $relativeOrder
    }   

    SelectedTaskSaveToFile
    ReprioritizeSelectedTasks
    ChangeStatesAndUpdateIndicator
}

proc ReprioritizeSelectedTasks { } {
    global Task

    set index 1
    foreach sel $Task(SelectedTaskList) {
	set newLabel [format "%d %s" $index $sel]
	regsub -all " " $sel "*-*" funnysel
	if { [winfo exists $Task(selectedlist).sel$funnysel] == 1 } then {
 	    $Task(selectedlist).sel$funnysel config -label $newLabel
  	}
	incr index
    }
}

proc SelectTask { entry } {
    global Task
    global TaskDefinition


    if { [string match sel* $entry] == 1 } then {
        set task [string range $entry 3 end]
    } else {
        set task [string range $entry 4 end]
    }
    regsub -all {\*-\*} $task " " realTask
    TaskDefinitionWindow $realTask
}

proc SelectTaskHelper { window y } {
    set hlistWindow [string range $window 0 57]
    set lastDot [string last "." $window]
    if { $lastDot < 58 } then {
	set element [[string range $window 0 57] nearest $y]
    } elseif { $lastDot == 58 } then {
	set element [string range $window 59 end]
    } else {
	set element [string range $window 59 [expr $lastDot - 1]]
    }
    SelectTask $element
}

# Delete the selected task
proc SelectedTaskDelete { window x y } {
    global Task


    # Tcl doesn't give us useful information here, so we've squirreled it away
    # in the SelectedTaskSelect procedure and we'll use it now.  (The x value is
    # unimportant.
    set window $Task(SelectedTaskSelect:window)
    set y $Task(SelectedTaskSelect:y)

    set hlistWindow [string range $window 0 57]
    set lastDot [string last "." $window]
    if { $lastDot < 58 } then {
	set element [[string range $window 0 57] nearest $y]
    } elseif { $lastDot == 58 } then {
	set element [string range $window 59 end]
    } else {
	set element [string range $window 59 [expr $lastDot - 1]]
    }

    regsub -all {\*-\*} $element " " realtask 
    set task [string range $realtask 3 end]

    LogAction "Task Information Window: Un-hoarding $task"
    SelectedTaskDeleteElement $task
    Hoard 
    ChangeStatesAndUpdateIndicator
}

proc SelectedTaskDeleteElement { task } {
    global Task

    set selectedIndex [lsearch -exact $Task(SelectedTaskList) $task]
    if { $selectedIndex >= 0 } then {
	# Remove task from SelectedTaskList
        set Task(SelectedTaskList) [lreplace $Task(SelectedTaskList) $selectedIndex $selectedIndex]

        # Remove task from TaskInformation window's list of selected tasks
        regsub -all " " $task "*-*" funnytask
        [$Task(selectedTasksListFrame).tasklist subwidget hlist] delete entry sel$funnytask

	# Destroy the selected task window
	destroy [$Task(selectedTasksListFrame).tasklist subwidget hlist].sel$funnytask

	# Decrement next index
	set Task(selectedlist:nextindex) [expr $Task(selectedlist:nextindex) - 1]
    }

    SelectedTaskSaveToFile
    ReprioritizeSelectedTasks
}

proc SelectedTaskSaveToFile { } {
    global Pathnames
    global Task

    set hoardDir $Pathnames(newHoarding)
    set SelectedFILE [MyRenameAndOpen ${hoardDir}/SelectedTasks {WRONLY CREAT TRUNC}]

    foreach task $Task(SelectedTaskList) {
	puts $SelectedFILE  $task
    }
    close $SelectedFILE
}

proc UnselectTask { entry } {
    SendToStdErr "MARIA:  Implement UnselectTask($entry)"
}

proc AbortTaskChanges { } {
    global Task

    SendToStdErr "MARIA:  Inplement AbortTaskChanges"

#    for {set i 1} {$i <= [llength $Task(NewlySelectedTaskList)]} {incr i} {
#	set index [expr $Task(selectedlist:nextindex) - $i]
#	set task [lindex $Task(NewlySelectedTaskList) [expr $i - 1]]
#	regsub -all " " $task "*-*" funnytask
#	$Task(selectedlist) delete entry sel$funnytask
#    }
#    set Task(selectedlist:nextindex) [expr $Task(selectedlist:nextindex) - $i + 1]
#    if { [llength $Task(NewlySelectedTaskList)] > 0 } then {
#        set Task(NewlySelectedTaskList) [lreplace $Task(NewlySelectedTaskList) 0 end]
#    }
}

proc CommitTaskChanges { } {
    global Task

    SendToStdErr "MARIA:  Implement CommitTaskChanges"

#    if { [llength $Task(NewlySelectedTaskList)] > 0 } then {
#        set Task(SelectedTaskList) \
#	    [concat $Task(SelectedTaskList) $Task(NewlySelectedTaskList)]
#
#        set Task(NewlySelectedTaskList) \
#	    [lreplace $Task(NewlySelectedTaskList) 0 end]
#
#	SelectedTaskSaveToFile
#    }
}


proc TaskDefinitionDataInit { } {
    global TaskDefinition
    global Task
    global UserData
    global Program

    set TaskDefinition(UserDataList) $UserData(List)
    set TaskDefinition(ProgramList) $Program(List)
    set TaskDefinition(TaskList) $Task(TaskList)

    foreach d $TaskDefinition(UserDataList) {
	set TaskDefinition(${d}:Definition) $UserData(${d}:Definition)
    }

    foreach p $TaskDefinition(ProgramList) {
	set TaskDefinition(${p}:Definition) $Program(${p}:Definition)
    }

    foreach t $TaskDefinition(TaskList) {
        set TaskDefinition(${t}:userDataList) $Task(${t}:userDataList) 
        set TaskDefinition(${t}:programsList) $Task(${t}:programsList) 
        set TaskDefinition(${t}:subtasksList) $Task(${t}:subtasksList) 
    }
}

proc TaskDefinitionWindow { thisTask } {
    global Task
    global TaskDefinition
    global TaskList

    incr Task(WindowID)
    LogAction "Task Definition Window (ID=$Task(WindowID)): open"
    SetTime "TaskDefinitionWindow$Task(WindowID):select"
    set Task(TaskDefinitionWindow) .taskdefine$Task(WindowID)
    toplevel $Task(TaskDefinitionWindow)
    wm title $Task(TaskDefinitionWindow) "Task Definition"
    wm geometry $Task(TaskDefinitionWindow) +0+0
    wm protocol $Task(TaskDefinitionWindow) WM_DELETE_WINDOW "LogAction TaskDefinitionWindow(ID=$Task(WindowID)):beginclose; TaskDefinitionWindowSave $Task(WindowID); destroy $Task(TaskDefinitionWindow); LogAction TaskDefinitionWindow(ID=$Task(WindowID)):endclose; SetTime TaskDefinitionWindow$Task(WindowID):close"


    set Task(TaskDefinition:MainFrame) $Task(TaskDefinitionWindow).main
    frame $Task(TaskDefinition:MainFrame) -bd 2 -relief groove
    pack  $Task(TaskDefinition:MainFrame) -side top -expand true -fill both

    set Task(TaskDefinition:NameList) $Task(TaskDefinition:MainFrame).names
    tixComboBox $Task(TaskDefinition:NameList) -label "Task Names: " -editable true \
	-command [list TaskDefinitionShowTask $Task(WindowID)]
    ComboBoxFixEntryBindings [$Task(TaskDefinition:NameList) subwidget entry]
    pack $Task(TaskDefinition:NameList) -side top -expand true -fill both
    for {set i 0} {$i < [llength $TaskDefinition(TaskList)]} {incr i} {
	$Task(TaskDefinition:NameList) insert end [lindex $TaskDefinition(TaskList) $i]
    }
    set Task(TaskDefinition:NameList:entrywidget) \
	[$Task(TaskDefinition:NameList) subwidget entry] 
    update idletasks
    focus $Task(TaskDefinition:NameList:entrywidget) 
    update idletasks
    TaskDefinitionWindowBindings

    set Task(TaskDefinition:Panes) $Task(TaskDefinition:MainFrame).panes
    tixPanedWindow $Task(TaskDefinition:Panes)
    pack $Task(TaskDefinition:Panes)
    $Task(TaskDefinition:Panes) add subtasks -min 100 -size 125
    $Task(TaskDefinition:Panes) add programs -min 100 -size 125
    $Task(TaskDefinition:Panes) add userData -min 100 -size 125

    set Task(TaskDefinition:Panes:userData) \
	[$Task(TaskDefinition:Panes) subwidget userData]
    set Task(TaskDefinition:Panes:programs) \
	[$Task(TaskDefinition:Panes) subwidget programs]
    set Task(TaskDefinition:Panes:subtasks) \
	[$Task(TaskDefinition:Panes) subwidget subtasks]

    set Task(TaskDefinition:Panes:userData:Frame) $Task(TaskDefinition:Panes:userData).f
    set Task(TaskDefinition:Panes:programs:Frame) $Task(TaskDefinition:Panes:programs).f
    set Task(TaskDefinition:Panes:subtasks:Frame) $Task(TaskDefinition:Panes:subtasks).f

    tixLabelFrame $Task(TaskDefinition:Panes:userData:Frame) -label "User Data:" \
	-options { label.padX 5 }
    pack $Task(TaskDefinition:Panes:userData:Frame) -side left -padx 10 \
	-expand true -fill both
    set Task(TaskDefinition:Panes:userData:Frame:Picked) \
	[ListSelect $Task(TaskDefinition:Panes:userData:Frame) \
	 "Predefined:" "\"$thisTask\" contains:" \
	 $TaskDefinition(UserDataList) $Task(${thisTask}:userDataList) 0 \
	 DataDefinitionWindow]

    tixLabelFrame $Task(TaskDefinition:Panes:programs:Frame) -label "Programs:" \
	-options { label.padX 5 }
    pack $Task(TaskDefinition:Panes:programs:Frame) -side left -padx 10\
	-expand true -fill both
    set Task(TaskDefinition:Panes:programs:Frame:Picked) \
	[ListSelect $Task(TaskDefinition:Panes:programs:Frame) \
   	 "Predefined:" "\"$thisTask\" contains:" \
	 $TaskDefinition(ProgramList) $Task(${thisTask}:programsList) 0 \
	 ProgramDefinitionWindow]

    tixLabelFrame $Task(TaskDefinition:Panes:subtasks:Frame) -label "Subtasks:" \
	-options { label.padX 5 }
    pack $Task(TaskDefinition:Panes:subtasks:Frame) -side left -padx 10 \
	-expand true -fill both
    set Task(TaskDefinition:Panes:subtasks:Frame:Picked) \
        [ListSelect $Task(TaskDefinition:Panes:subtasks:Frame) \
	 "Predefined:" "\"$thisTask\" contains:" \
	 $TaskDefinition(TaskList) $Task(${thisTask}:subtasksList) 0\
	 TaskDefinitionWindow]

    pack [TaskDefinitionWindowButtons] -side bottom -expand true -fill both

    set index [lsearch -exact $Task(TaskList) $thisTask]
    $Task(TaskDefinition:NameList) pick $index
    update idletasks
    $Task(TaskDefinition:NameList:entrywidget) select range 0 end
    SetTime "TaskDefinitionWindow$Task(WindowID):visible"
}

proc TaskDefinitionWindowButtons { } {
    global Task

    set frame [frame $Task(TaskDefinition:MainFrame).buttons -bd 2 -relief groove]

    button $frame.save -text "Save" -relief groove -bd 2 \
	-command [list TaskDefinitionWindowSave $Task(WindowID)]
    button $frame.saveas -text "Save as..." -relief groove -bd 2 \
	-command [list TaskDefinitionWindowSaveAs $Task(WindowID)]
    button $frame.revert -text "Revert" -relief groove -bd 2 \
	-command [list TaskDefinitionWindowRevert $Task(WindowID)]
    button $frame.delete -text "Delete Task" -relief groove -bd 2 \
	-command [list TaskDefinitionWindowDelete $Task(WindowID)] 
    button $frame.help -text "Help" -relief groove -bd 2 \
	-command { TaskDefinitionWindowHelp }

    pack $frame.help -side left
    pack $frame.save -side right
    pack $frame.saveas -side right
    pack $frame.revert -side right -padx 20
    pack $frame.delete -side right

    return $frame
}


proc TDW-GetWindow { WindowID } { 
    return .taskdefine${WindowID} 
}

proc TDW-GetMainWindow { WindowID } { 
    return .taskdefine${WindowID}.main 
}

proc TDW-GetNameList { WindowID } { 
    return .taskdefine${WindowID}.main.names 
}

proc TDW-GetEntryWidget { WindowID } { 
    return [.taskdefine${WindowID}.main.names subwidget entry] 
}

proc TDW-GetPane { WindowID Pane } {
    return [.taskdefine${WindowID}.main.panes subwidget $Pane]
}

proc TDW-GetChoiceList { WindowID Pane } {
    return [[[[.taskdefine${WindowID}.main.panes subwidget $Pane].f subwidget frame].choices subwidget frame].list subwidget listbox]
}

proc TDW-GetPickedList { WindowID Pane } {
    return [[[[.taskdefine${WindowID}.main.panes subwidget $Pane].f subwidget frame].picked subwidget frame].list subwidget listbox]
}

proc TDW-GetPicked { WindowID Pane } {
    return [[.taskdefine${WindowID}.main.panes subwidget $Pane].f subwidget frame].picked
}

proc TDW-GetHList { WindowID } { 
    return [.taskdefine${WindowID}.main.definitionlist subwidget hlist] 
}

proc TDW-AddElement { WindowID type element } {
    global DisplayStyle
    global UserData
    global Program
    global Task
    global TaskDefinition

    # Determine this element's index
    switch -exact $type {
        userData { 
	    set index [lsearch -exact $UserData(List) $element] 
	    lappend TaskDefinition(UserDataList) $element
	    set TaskDefinition(UserDataList) [lsort -command MyCompare $TaskDefinition(UserDataList)]
	}
	programs { 
	    set index [lsearch -exact $Program(List) $element] 
	    lappend TaskDefinition(ProgramList) $element
	    set TaskDefinition(ProgramList) [lsort -command MyCompare $TaskDefinition(ProgramList)]
	}
	subtasks { 
	    set index [lsearch -exact $Task(TaskList) $element] 
#	    lappend TaskDefinition(TaskList) $element
#	    set TaskDefinition(TaskList) [lsort -command MyCompare $TaskDefinition(TaskList)]
	}
    }

    if { $type == "subtasks" } then {
	# Add this to the Task List on the TaskInformation window
	regsub -all " " $element "*-*" funnytask
        [$Task(allTasksListFrame).tasklist subwidget hlist] \
	    add task$funnytask \
	    -at $index \
	    -itemtype text \
	    -text $element \
	    -style $DisplayStyle(Normal)
    }

    for { set i 0 } { $i <= $Task(WindowID) } { incr i } {
	# Only worry about those windows that still exist
	if { [winfo exists [TDW-GetWindow $i]] } then {
	    switch -exact $type {
	 	userData {
     	            # Add this element to this window's $type choices list
	            [TDW-GetChoiceList $i $type] insert $index $element
		}

		programs {
     	            # Add this element to this window's $type choices list
	            [TDW-GetChoiceList $i $type] insert $index $element
		}

		subtasks {
	            # Add this element to this window's NameList
	            [TDW-GetNameList $i] insert $index $element

   	            # Add this element to this window's $type choices list
	            [TDW-GetChoiceList $i $type] insert $index $element
	        } 
	    }
        }
    }
}

proc TDW-DeleteElement { WindowID type element } {
    global UserData
    global Program
    global Task

    # Determine this element's index
    switch -exact $type {
        userData { set index [lsearch -exact $UserData(List) $element] }
	programs { set index [lsearch -exact $Program(List) $element] }
	subtasks { set index [lsearch -exact $Task(TaskList) $element] }
    }

    if { $type == "subtasks" } then {
        # Remove task from TaskInformation window's list of tasks 
        regsub -all " " $element "*-*" funnytask
        [$Task(allTasksListFrame).tasklist subwidget hlist] delete entry task$funnytask

	# Unselect this task from TaskInformation window (if appropriate)
	SelectedTaskDeleteElement $element
    }

    for { set i 0 } { $i <= $Task(WindowID) } { incr i } {
	# Only worry about those windows that still exist
	if { [winfo exists [TDW-GetWindow $i]] } then {

	    if { $type == "subtasks" } then {
	        # Delete this element from this window's NameList
	        [[TDW-GetNameList $i] subwidget listbox] delete $index 

	        # If this task is the visible one, then delete the defn too
	        if { [[TDW-GetNameList $i] cget -value] == $element } then {
	 	    [TDW-GetNameList $i] pick 0
	        }
	    }

            # Delete this element from this window's $type choices list
	    [TDW-GetChoiceList $i $type] delete $index

	    # Delete this element from this window's $type picked list
	    set pickedList [TDW-GetPickedList $i $type]
	    set listboxContains [$pickedList get 0 end]
    	    set listIndex [lsearch -exact $listboxContains $element]
    	    if { $listIndex >= 0 } then { $pickedList delete $listIndex }
	}
    }
}

proc ListSelect { frame leftLabel rightLabel leftVariable rightVariable start DoubleCommand} {
    global Task

    set Choices [$frame subwidget frame].choices
    set Picked [$frame subwidget frame].picked

    tixLabelFrame $Choices -label $leftLabel -options { label.padX 5}
    pack $Choices -side left -padx 10 -expand true -fill both
    set ChoicesList [$Choices subwidget frame].list
    tixScrolledListBox $ChoicesList -width 200
    pack $ChoicesList -side top -expand true -fill both
    for {set i $start} {$i < [llength $leftVariable]} {incr i} {
	set element [lindex $leftVariable $i]
        $ChoicesList subwidget listbox insert end $element
    }

    tixLabelFrame $Picked -label $rightLabel -options { label.padX 5 }
    pack $Picked -side left -padx 10 -expand true -fill both
    set PickedList [$Picked subwidget frame].list
    tixScrolledListBox $PickedList -width 200
    pack $PickedList -side top -expand true -fill both
    for {set i 0} {$i < [llength $rightVariable]} {incr i} {
        $PickedList subwidget listbox insert end [lindex $rightVariable $i]
    }

    # Actions in choices list
    bind [$ChoicesList subwidget listbox] <Double-1> \
	    [list ListItemDisplay %W %y $DoubleCommand]
    bind [$ChoicesList subwidget listbox] <1> \
	    [list ListItemSelect %W %y $PickedList]

    # Actions in picked list
    bind [$PickedList subwidget listbox] <1> \
            [list ListHighlight %W %y $DoubleCommand]
    bind [$PickedList subwidget listbox] <Delete> \
            {ListDeleteEnd2 %W}
    bind [$PickedList subwidget listbox] <BackSpace> \
            {ListDeleteEnd2 %W}
    bind [$PickedList subwidget listbox] <Double-1> \
	    [list ListItemDisplay %W %y $DoubleCommand]

    return $Picked
}


proc ListSelectStart { w y } {
        $w select anchor [$w nearest $y]
}
proc ListSelectExtend { w y } {
        $w select set anchor [$w nearest $y]
}
proc ListSelectEnd {w y list} {
     global Task

        $w select set anchor [$w nearest $y]
        foreach i [$w curselection] {
		set taskname [$w get $i]
		if { $taskname == "New..." } then {
		    if { [string match *userData* $w] == 1 } then {
			DataDefinitionWindow $taskname
		    } elseif { [string match *programs* $w] == 1 } then {
			ProgramDefinitionWindow $taskname
		    } elseif { [string match *subtasks* $w] == 1 } then {
			$Task(TaskDefinition:NameList) pick 0
		    } else {
		 	SendToStdErr  "ListSelectEnd:  This should never happen."
		    }
		} else {
                    $list subwidget listbox insert end [$w get $i]
		}
        }

}

proc ListSelectEnd2 {w y} {
    $w select set anchor [$w nearest $y]
}

proc ListHighlight {w y DoubleCommand} {
    global List

    LogAction "Task Definition Window: Selected element from Contains Listbox"
    $w selection set [$w nearest $y]

    if { [info exists List(screwedupID)] } then {
	after cancel $List(screwedupID)
	unset List(screwedupID)
	ListItemDisplay $w $y $DoubleCommand
    } else {
        set List(screwedupID) [after 500 [list TclScrewedUpDoItYourself]]
    }
}

proc ListDeleteEnd {w y} {
    global Task

        $w select set anchor [$w nearest $y]
#315        foreach i [lsort -decreasing [$w curselection]] {}
        foreach i [lsort -decreasing [$w curselection]] {
		set taskname [$w get $i]
                $w delete $i
        }

	# Make the most recently deleted item visible in the Choices List
	regsub "picked" $w "choices" ChoiceList
	set Choices [$ChoiceList get 0 end]
	set choice [lsearch -exact $Choices $taskname]
	after 50 [list $ChoiceList see $choice]
}


proc ListDeleteEnd2 {w} {
    global Task

        if { [$w curselection] == "" } then {
   	    return
        }


        foreach i [lsort -decreasing [$w curselection]] {
		set taskname [$w get $i]
                $w delete $i
		LogAction "Task Definition Window: Delete $taskname from Contains Listbox"
        }

	# Make the most recently deleted item visible in the Choices List
	regsub "picked" $w "choices" ChoiceList
	set Choices [$ChoiceList get 0 end]
	set choice [lsearch -exact $Choices $taskname]
	after 50 [list $ChoiceList see $choice]
}

proc ListItemSelectFromPicked { window Yval list } {
   set List(afterID) [after 500 [list $list subwidget listbox insert end $taskname]]
}

proc ListItemSelect { window Yval list } {
    global List

    set taskname [$window get [$window nearest $Yval]]
    LogAction "Task Definition Window: Select $taskname from Choices Listbox"
    
    if { $taskname == "New..." } then {
	SendToStdErr "MARIA:  Should we popup an error box?  Or just display the item?(HARD)  Or what?"
	return
    }

    # Check for duplicates and bail if necessary
    set pickedIndex [lsearch -exact [$list subwidget listbox get 0 end] $taskname]
    if { $pickedIndex >= 0 } then {
	$list subwidget listbox yview $pickedIndex
	return
    }

    regexp {^[0-9]*} [string range $window 11 end] WindowID
    set displayedTask [[TDW-GetEntryWidget $WindowID] get]
    if { (($displayedTask != $taskname) ||
	 ([string first ".main.panes.subtasks" $window] == -1)) } then {
       set List(afterID) [after 500 [list $list subwidget listbox insert end $taskname]]
    }
}

proc TclScrewedUpDoItYourself { } {
    global List

    unset List(screwedupID)
}

proc ListItemDisplay { window Yval command } {
    global List


    if { [info exists List(afterID)] } then { after cancel $List(afterID) }
    set taskname [$window get [$window nearest $Yval]]
    update idletasks
    # Although the "after 0" looks like a no-op, do NOT (under penalty of death)
    # take it out or you'll get a cute little jitterBUG thing.
    after 0 [list $command $taskname]
}

proc TaskDefinitionShowTask { WindowID task } {
    global Task
    global TaskDefinition

    LogAction "Task Definition Window: Show $task"

    # Search for this task on the TaskList
    set existingTask [lsearch -exact $TaskDefinition(TaskList) $task]

    # Set the labels for the "picked" (righthand) lists
    foreach f {userData programs subtasks} {
        set pickedLabel [[TDW-GetPicked $WindowID $f] subwidget label]
        $pickedLabel config -text "\"$task\" contains:"

        if { $existingTask >= 0 } then {
            set pickedList [TDW-GetPickedList $WindowID $f]
            $pickedList delete 0 end

	    for {set i 0} {$i < [llength $Task(${task}:${f}List)]} {incr i} {
	        $pickedList insert end [lindex $Task(${task}:${f}List) $i]
            }
        }
    }


    # Highlight task name if it is "New..."
    if { $task == "New..." } then {
	# HACK ALERT!
	# Wait 50 ms for activity to die down before highlighting the "New..." entry
        after 50 [list [TDW-GetEntryWidget $WindowID] select range 0 end]
    }
}

proc TaskDefinitionWindowSave { WindowID } { 
    global Task
    global TaskDefinition
    global DisplayStyle

    LogAction "Task Definition Window: save"
    set task [[TDW-GetEntryWidget $WindowID] get]
    if { $task != "New..." } then {
        set index [lsearch -exact $TaskDefinition(TaskList) $task] 
        if { $index == -1 } then {
	    TaskDefinitionWindowSaveAs $WindowID
    	return
        } else {
            foreach f {userData programs subtasks} {
	        set pickedList [TDW-GetPickedList $WindowID $f] 
                set Task(${task}:${f}List) [$pickedList get 0 end]
   	        set TaskDefinition(${task}:${f}List) [$pickedList get 0 end]
            }
        }
    }
    TaskDefinitionSaveToFile
}

proc TaskDefinitionWindowSaveAs { WindowID } {
    global Task
    global TaskDefinition
    global DisplayStyle

    LogAction "Task Definition Window: save as"
    set task [[TDW-GetEntryWidget $WindowID] get]
    while { [lsearch -exact $TaskDefinition(TaskList) $task] != -1 } {
	if { $task == "New..." } then {
	    LogAction "Task Definition Window: Enter name instead of New..."
	    set messagetext "Please enter a new name:  "
	} else {
	    LogAction "Task Definition Window: Enter name instead of $task"
	    set messagetext "A task named $task is already defined.  Please enter a new name:  "
	}
	set TaskDefinitionPickName [TDW-GetWindow $WindowID].pickname
	PopupNewName "Task Definition Save As..." $TaskDefinitionPickName $messagetext Task(newName) "5i" $TaskDefinition(TaskList)
	if { $Task(newName) == "" } then { return } else { set task $Task(newName) }

	[TDW-GetEntryWidget $WindowID] delete 0 end
	[TDW-GetEntryWidget $WindowID] insert 0 $task
	unset Task(newName)
    }

    lappend TaskDefinition(TaskList) $task
    set TaskDefinition(TaskList) [lsort -command MyCompare $TaskDefinition(TaskList)]
    lappend Task(TaskList) $task
    set Task(TaskList) [lsort -command MyCompare $Task(TaskList)]
    set index [lsearch $TaskDefinition(TaskList) $task]
    if { $index == 0 } then { set index 1 }

    # Add new task to the TaskDefinitionWindow subtasks area
    TDW-AddElement $WindowID subtasks $task
	
    TaskDefinitionWindowSave $WindowID
    [TDW-GetNameList $WindowID] configure -value $task
}

proc TaskDefinitionSaveToFile { } {
    global Task
    global TaskDefinition
    global Pathnames

    set hoardDir $Pathnames(newHoarding)
    set TaskFILE [MyRenameAndOpen ${hoardDir}/Tasks {WRONLY CREAT TRUNC}]

    foreach element $TaskDefinition(TaskList) {
	if { $element == "New..." } then { continue }
	set datalist [join $Task(${element}:userDataList) ","]
	set programlist [join $Task(${element}:programsList) ","]
	set subtasklist [join $Task(${element}:subtasksList) ","]

	puts $TaskFILE "${element}: D:($datalist) P:($programlist) S:($subtasklist)"
    }
    close $TaskFILE
}

proc TaskDefinitionWindowRevert { WindowID } {
    global Task

    set task [[TDW-GetEntryWidget $WindowID] get]
    set index [lsearch -exact $Task(TaskList) $task]
    [TDW-GetNameList $WindowID] pick $index
    focus [TDW-GetEntryWidget $WindowID]
    update idletasks
}

proc TaskDefinitionWindowDelete { WindowID } {
    global Task
    global TaskDefinition
    global ControlPanel

    LogAction "Task Definition Window: delete task"
    set task [[TDW-GetNameList $WindowID] cget -value]

    if { $task == "New..." } then {
	LogAction "Task Definition Window: delete New..."
	set messagetext "The \"New...\" task is not really a task definition.  Its only purpose is to allow you to define a new task.  Therefore, you cannot delete it."
	set TaskDefinitionDeleteNewError) [TDW-GetWindow $WindowID].deletenew
	PopupHelp "Task Definition Error" $TaskDefinitionDeleteNewError $messagetext "5i"
	return
    }

    if { $ControlPanel(SafeDeletes:task) == "on" } then {
	set messagetext [list "This action will delete the entire $task definition."]

	set containingTasks [ReturnContainingTaskList $task subtasksList]
	set containingTasks [SortAndRemoveDuplicates $containingTasks]
	
	if { [llength $containingTasks] > 0 } then {
	    lappend messagetext [format "Further, the \"%s\" task definition is contained in %d other task(s).  These include:" $task [llength $containingTasks]]
	    foreach t $containingTasks {
		lappend messagetext [format "\t%s" $t]
	    }
        }

        lappend messagetext "Do you want to delete $task?"
	set ReallyDeleteYesNoFrame [TDW-GetWindow $WindowID].reallydeleteyesno
	if { [info exists Task(ReallyDeleteYesNoVariable:$WindowID)] } then {
  	    unset Task(ReallyDeleteYesNoVariable:$WindowID)
	}
	LogAction "Task Definition Window: Safe Delete Query"
	PopupSafeDelete "Really delete?" $ReallyDeleteYesNoFrame [join $messagetext "\n"] Task(ReallyDeleteYesNoVariable:$WindowID) 
        tkwait variable Task(ReallyDeleteYesNoVariable:$WindowID)
        destroy $ReallyDeleteYesNoFrame
        if { $Task(ReallyDeleteYesNoVariable:$WindowID) == "no" } then {
	    return
	}
    }

    ## Now, really delete it!
    TDW-DeleteElement $WindowID subtasks $task

    set index [lsearch -exact $Task(TaskList) $task]

    # Remove data from individual tasks
    RemoveTaskFromTaskDefinitions $task subtasksList

    set Task(TaskList) [lreplace $Task(TaskList) $index $index]
    set TaskDefinition(TaskList) [lreplace $TaskDefinition(TaskList) $index $index]

    TaskDefinitionSaveToFile
}

proc TaskDefinitionWindowHelp { } {
    global Task

    LogAction "Task Definition Window: help"
    set messagetext "To examine a task definition, use the pulldown list to select the task.\n\nTo define a new task, first change the name from \"New...\", then click on the names of user data, programs and subtasks that should be part of this new task.  (To delete an element from a task definition, select it from the \"contains\" list and then hit the \"Delete\" key.)  To define a new piece of user data or a new program, double-click on the \"New...\" element of the appropriate list.  To view an existing piece of user data, an existing program definition or an existing (sub)task, double-click on its name in the appropriate listing.  To delete an existing task definition, select it to be viewed and then hit the delete key."

    set Task(TaskDefHelpWindow) .taskdefhelp
    PopupHelp "Task Definition Help Window" $Task(TaskDefHelpWindow) $messagetext "5i"
}

proc TaskDefinitionWindowBindings { } {
    global Task

    bind [$Task(TaskDefinition:NameList) subwidget entry] <Return> { \
	global Task
	set task [[$Task(TaskDefinition:NameList) subwidget entry] get]
    }
}

proc DataDefinitionWindow { element } {
    global UserData
    global UserDataDefinition

    incr UserData(WindowID)
    LogAction "Data Definition Window (ID=$UserData(WindowID)): open"
    SetTime "DataDefinitionWindow$UserData(WindowID):select"
    set UserData(DataDefinitionWindow) .datadefine$UserData(WindowID)
    toplevel $UserData(DataDefinitionWindow)
    wm title $UserData(DataDefinitionWindow) "User Data Definition"
    wm geometry $UserData(DataDefinitionWindow) 445x300+0+0
    wm protocol $UserData(DataDefinitionWindow) WM_DELETE_WINDOW "LogAction DataDefinitionWindow(ID=$UserData(WindowID)):beginclose; DataDefinitionWindowSave $UserData(WindowID); destroy $UserData(DataDefinitionWindow); LogAction DataDefinitionWindow(ID=$UserData(WindowID)):endclose; SetTime DataDefinitionWindow$UserData(WindowID):close"

    set UserData(DataDefinition:MainFrame) $UserData(DataDefinitionWindow).main
    frame $UserData(DataDefinition:MainFrame) -bd 2 -relief groove 
    pack  $UserData(DataDefinition:MainFrame) -side top -expand true -fill both

    set UserData(DataDefinition:NameList) $UserData(DataDefinition:MainFrame).names
    tixComboBox $UserData(DataDefinition:NameList) \
	-label "User Data Name: " \
	-editable true \
	-command [list DataDefinitionShowData $UserData(WindowID)]
    ComboBoxFixEntryBindings [$UserData(DataDefinition:NameList) subwidget entry]
    pack $UserData(DataDefinition:NameList) -side top -expand true -fill both -pady 5
    for {set i 0} {$i < [llength $UserData(List)]} {incr i} {
	$UserData(DataDefinition:NameList) insert end [lindex $UserData(List) $i]
    }
    set UserData(DataDefinition:NameList:entrywidget) \
	    [$UserData(DataDefinition:NameList) subwidget entry] 

    set UserData(DataDefinition:Label) $UserData(DataDefinition:MainFrame).label
    label $UserData(DataDefinition:Label) \
	-text "  contains the following pathnames:"
    pack $UserData(DataDefinition:Label) -side top -expand true -fill both -pady 5

    set UserData(DataDefinition:DefinitionList) \
	    $UserData(DataDefinition:MainFrame).definitionlist
    tixScrolledHList $UserData(DataDefinition:DefinitionList)
    pack $UserData(DataDefinition:DefinitionList) -side top \
	-expand true -fill both -padx 10 -pady 5
    set UserData(DataDefinition:DefinitionList:hlist) \
	    [$UserData(DataDefinition:DefinitionList) subwidget hlist]

    pack [DataDefinitionWindowButtons] -side bottom -expand true -fill both

    tkwait visibility $UserData(DataDefinition:NameList:entrywidget) 
    focus $UserData(DataDefinition:NameList:entrywidget) 
    update idletasks

    set index [lsearch -exact $UserData(List) $element]
    $UserData(DataDefinition:NameList) pick $index

    SetTime "DataDefinitionWindow$UserData(WindowID):visible"
}

proc DDW-GetWindow { WindowID } { 
    return .datadefine${WindowID} 
}

proc DDW-GetWindowID { frame } {
    scan $frame {.datadefine%d} windowID
    return $windowID
}

proc DDW-GetMainWindow { WindowID } { 
    return .datadefine${WindowID}.main 
}

proc DDW-GetNameList { WindowID } { 
    return .datadefine${WindowID}.main.names 
}

proc DDW-GetEntryWidget { WindowID } { 
    return [.datadefine${WindowID}.main.names subwidget entry] 
}

proc DDW-GetHList { WindowID } { 
    return [.datadefine${WindowID}.main.definitionlist subwidget hlist] 
}

proc DDW-AddElement { WindowID } {
    global UserData

    set thisData [[DDW-GetEntryWidget $WindowID] get]
    set index [lsearch -exact $UserData(List) $thisData]
    for { set i 0 } { $i <= $UserData(WindowID) } { incr i } {
	if { [winfo exists [DDW-GetWindow $i]] } then {
	    [DDW-GetNameList $i] insert $index $thisData
	}
    }
}

proc DDW-DeleteElement { WindowID } {
    global UserData

    set thisData [[DDW-GetNameList $WindowID] cget -value]
    set index [lsearch -exact $UserData(List) $thisData]
    for { set i 0 } { $i <= $UserData(WindowID) } { incr i } {
	if { [winfo exists [DDW-GetWindow $i]] } then {
	    [[DDW-GetNameList $i] subwidget listbox] delete $index

	    # If this task is the visible task, then delete the elements too
	    if { [[DDW-GetNameList $i] cget -value] == $thisData } then {
	 	[DDW-GetNameList $i] pick 0
	    }
	}
    }
}

proc DataDefinitionDataElement { frame datadefinition } {
    global UserData

    set WindowID [DDW-GetWindowID $frame]
    incr UserData(HighElement:$WindowID)

    set def [split $datadefinition :]
    set pathname [lindex $def 0]
    set meta [lindex $def 1]

    if { [winfo exists $frame] == 0 } then {
        frame $frame -highlightcolor "blue" -highlightthickness 2

        # Make the pathname entry
        entry $frame.pathname -relief sunken -width 27
        pack $frame.pathname -side left -expand true -fill x

        # Make the radiobuttons
        frame $frame.buttons
        set UserData(Variable:${frame}) "none"
        radiobutton $frame.buttons.n -text "none" \
	    -variable UserData(Variable:${frame}) \
	    -state disabled \
	    -value "none" \
	    -command {LogAction "Data Definition Window: Meta-Information Select None"} \
	    -anchor w
        radiobutton $frame.buttons.c -text "immediate children" \
	    -variable UserData(Variable:${frame}) \
	    -state disabled \
	    -value "children" \
	    -command {LogAction "Data Definition Window: Meta-Information Select Children"} \
	    -anchor w 
        radiobutton $frame.buttons.d -text "all descendants" \
	    -variable UserData(Variable:${frame}) \
	    -state disabled \
	    -value "descendants" \
	    -command {LogAction "Data Definition Window: Meta-Information Select Descendants"} \
	    -anchor w
        pack $frame.buttons.n -side top -expand true -fill both
        pack $frame.buttons.c -side top -expand true -fill both
        pack $frame.buttons.d -side top -expand true -fill both
        pack $frame.buttons -side right -expand true -fill both

	DataDefinitionDataElementBindings $frame
    }

    $frame.pathname delete 0 end
    $frame.pathname insert 0 $pathname

    if { ![file exists $pathname] } then {
	SendToStdErr "Yoohoo.... $pathname DOES NOT EXIST!"
    }
    if { ($pathname != "") && ([file isdirectory $pathname]) } then {
	EnableMetaInformation $frame
    } else {
	DisableMetaInformation $frame
    }

    if { $meta == "c" } then {
        set UserData(Variable:${frame}) "children"
    } elseif { $meta == "d" } then {
        set UserData(Variable:${frame}) "descendants"
    } else {
        set UserData(Variable:${frame}) "none"
    }

    return $frame
}

proc EnableMetaInformation { frame } {
    $frame.buttons.n configure -state active
    $frame.buttons.c configure -state active
    $frame.buttons.d configure -state active
}


proc DisableMetaInformation { frame } {
    $frame.buttons.n configure -state disabled
    $frame.buttons.c configure -state disabled
    $frame.buttons.d configure -state disabled
}

proc DataDefinitionDataElementDelete { frame } {
    global UserData

    set framePrefix [string range $frame 0 [expr [string length $frame] - 10]]
    set element [string range $framePrefix [expr [string last "." $framePrefix] + 1] end]
    scan $element "element%d" elementID
    
    $framePrefix.pathname delete 0 end
    set UserData(Variable:${framePrefix}) "none"
    if { $elementID != [expr $UserData(HighElement:[DDW-GetWindowID $frame]) - 1] } then {
	set WindowID [DDW-GetWindowID $frame]
        [DDW-GetHList $WindowID] delete entry $element 	
	incr elementID
	set nextElement "element$elementID"
        while { ![[DDW-GetHList $WindowID] info exists $nextElement] } {
	    incr elementID
	    set nextElement "element$elementID"
	}
	regsub {element[0-9]*} $frame $nextElement nextFrame
	focus $nextFrame
        update idletasks
    }
}

proc DataDefinitionWindowButtons { } {
    global UserData

    set frame [frame $UserData(DataDefinition:MainFrame).buttons -bd 2 -relief groove]

    button $frame.save -text "Save" -relief groove -bd 2 \
	-command [list DataDefinitionWindowSave $UserData(WindowID)]
    button $frame.saveas -text "Save as..." -relief groove -bd 2 \
	-command [list DataDefinitionWindowSaveAs $UserData(WindowID)]
    button $frame.revert -text "Revert" -relief groove -bd 2 \
	-command [list DataDefinitionWindowRevert $UserData(WindowID)]
    button $frame.delete -text "Delete Data" -relief groove -bd 2 \
	-command [list DataDefinitionWindowDelete $UserData(WindowID)] 
    button $frame.help -text "Help" -relief groove -bd 2 \
	-command { DataDefinitionWindowHelp }

    pack $frame.help -side left
    pack $frame.save -side right
    pack $frame.saveas -side right
    pack $frame.revert -side right -padx 20
    pack $frame.delete -side right

    return $frame
}

proc DataDefinitionWindowHelp { } {
    global UserData

    
    LogAction "Data Definition Window: help"    
    set messagetext "The User Data Definition window allows you to define a new data set or to view an existing data set definition.  To select an existing data set definition, you may either type the name of the set and then hit return, or click on the down arrow button and use the popdown listing.  To define a new data set, you can type the new name and hit return.\n\nThe listing below the name shows the pathnames of the elements in the data set and the meta-information associated with each pathname.  If the pathname is a directory, you can choose to include only the immediate children of that directory or all of that directory's descendants."
    set UserData(DataDefinitionHelp) $UserData(DataDefinitionWindow).help

    PopupHelp "User Data Definition Help" $UserData(DataDefinitionHelp) $messagetext "5i"
}

proc DataDefinitionWindowRevert { WindowID } {
    global UserData

    set data [[DDW-GetEntryWidget $WindowID] get]
    set index [lsearch -exact $UserData(List) $data]
    [DDW-GetNameList $WindowID] pick $index
    focus [DDW-GetEntryWidget $WindowID]
    update idletasks
}

proc ReturnContainingTaskList { element whichList } {
    global Task

    set containingList ""
    foreach task $Task(TaskList) {
	# Search for this element in the task's whichList definition
	if { [lsearch -exact $Task($task:$whichList) $element] >= 0 } then {
	    # If you find it... 

	    # 1) Add this task to the containing list
	    lappend containingList "$task"
	
	    # 2) Look for this task in the subtask list of other tasks
	    set recursiveContainingList [ReturnContainingTaskList $task subtasksList]
	    set containingList [concat $containingList $recursiveContainingList]
	}
    }
    return $containingList
}

proc RemoveTaskFromTaskDefinitions { element whichList } {
    global Task
    global TaskDefinition

    foreach task $Task(TaskList) {
	if { $task == "New..." } then { continue }
	set index [lsearch -exact $Task($task:$whichList) $element] 
	if { $index >= 0 } then {
	    set Task($task:$whichList) [lreplace $Task($task:$whichList) $index $index]
	}

	set index2 [lsearch -exact $TaskDefinition($task:$whichList) $element]
	if { $index2 >= 0 } then {
	    set TaskDefinition($task:$whichList) [lreplace $TaskDefinition($task:$whichList) $index2 $index2]
	}
	if { $index != $index2 } then {
	    SendToStdErr "Warning:  Indices on Task and TaskDefinition differ for $task's $whichList list"
	}
    }
}

proc DataDefinitionWindowDelete { WindowID } {
    global Task
    global UserData
    global ControlPanel

    LogAction "Data Definition Window: delete data"
    set data [ [DDW-GetNameList $WindowID] cget -value]
    if { $data == "New..." } then {
	[DDW-GetNameList $WindowID] pick 0
	focus [DDW-GetNameList $WindowID]
        update idletasks
	return
    }
    set index [lsearch -exact $UserData(List) $data]
    if { $index == -1 } then { 
	[DDW-GetNameList $WindowID] pick 0
	focus [DDW-GetEntryWidget $WindowID]
        update idletasks
	return 
    }

    if { $ControlPanel(SafeDeletes:data) == "on" } then {
	set messagetext [list "This action will delete the entire $data definition."]

	set containingTasks [ReturnContainingTaskList $data userDataList]
	set containingTasks [SortAndRemoveDuplicates $containingTasks]
	
	if { [llength $containingTasks] > 0 } then {
	    lappend messagetext [format "Further, the \"%s\" data set is contained in %d other task(s).  These include:" $data [llength $containingTasks]]
	    foreach t $containingTasks {
		lappend messagetext [format "\t%s" $t]
	    }
        }

        lappend messagetext "Do you want to delete $data?"
	set ReallyDeleteYesNoFrame [DDW-GetWindow $WindowID].reallydeleteyesno
	if { [info exists UserData(ReallyDeleteYesNoVariable:$WindowID)] } then {
  	    unset UserData(ReallyDeleteYesNoVariable:$WindowID)
	}
	LogAction "Data Definition Window: Safe Delete Query"
	PopupSafeDelete "Really delete?" $ReallyDeleteYesNoFrame [join $messagetext "\n"] UserData(ReallyDeleteYesNoVariable:$WindowID)
        tkwait variable UserData(ReallyDeleteYesNoVariable:$WindowID)
        destroy $ReallyDeleteYesNoFrame
        if { $UserData(ReallyDeleteYesNoVariable:$WindowID) == "no" } then {
	    return
        }
    }

    ## Now, really delete it!
    DDW-DeleteElement $WindowID

    # Remove data from individual tasks
    RemoveTaskFromTaskDefinitions $data userDataList
    TDW-DeleteElement $WindowID userData $data

    # Delete the definition
    unset UserData($data:Definition)

    set UserData(List) [lreplace $UserData(List) $index $index]
    [DDW-GetNameList $WindowID] pick 0
    focus [DDW-GetEntryWidget $WindowID]
    update idletasks

    DataDefinitionSaveToFile
}

proc DataDefinitionWindowSaveAs { WindowID } {
    global UserData
    global Task

    LogAction "Data Definition Window: save as"
    set data [[DDW-GetEntryWidget $WindowID] get]
    while { [lsearch -exact $UserData(List) $data] != -1 } {
	if { $data == "New..." } then {
	    LogAction "Data Definition Window: Enter name instead of New..."
	    set messagetext "Please enter a new name:  "
 	} else {
	    LogAction "Data Definition Window: Enter name instead of $data"
   	    set messagetext "A data set named $data is already defined.  Please enter a new name:  "
	}
	set DataDefinitionPickName [DDW-GetWindow $WindowID].pickname
	PopupNewName "User Data Definition Save As..." $DataDefinitionPickName $messagetext UserData(newName) "5i" $UserData(List)
	if { $UserData(newName) == "" } then { return }

	set data $UserData(newName) 
	[DDW-GetEntryWidget $WindowID] delete 0 end
	[DDW-GetEntryWidget $WindowID] insert 0 $data
	unset UserData(newName)
    }

    lappend UserData(List) $data
    set UserData(List) [lsort -command MyCompare $UserData(List)]
    set index [lsearch $UserData(List) $data]
    if { $index == 0 } then { set index 1 }

    # Add new data to correct location in the NameLists
    DDW-AddElement $WindowID

    # Add new data to the TaskDefinitionWindow data area
    TDW-AddElement $WindowID userData $data

    DataDefinitionWindowSave $WindowID
    [DDW-GetNameList $WindowID] configure -value $data
}

proc DataDefinitionWindowSave { WindowID } {
    global UserData

    LogAction "Data Definition Window: save"
    set data [[DDW-GetEntryWidget $WindowID] get]
    if { $data != "New..." } then {
        if { [lsearch -exact $UserData(List) $data] == -1 } then {
   	    DataDefinitionWindowSaveAs $WindowID
	    return
        } else {
	    set frame [DDW-GetHList $WindowID].element
            set i 0
	    set aok 1
	    set UserData($data:Definition) [list ]
 	    while { $aok } {
	        if { [winfo exists $frame$i.pathname] == 1 } then {
		    set pathname [$frame$i.pathname get]
		    if { ($pathname != "") && ($pathname != "New...") } then {

   		        switch -exact $UserData(Variable:$frame$i) {
		            descendants { set meta "d" }
		            children { set meta "c" }
		            default { set meta "n" }
		        }
		        set definition [format "%s:%s" $pathname $meta]
		        lappend UserData($data:Definition) $definition
		    } 
	            incr i
	        } else { set aok 0 } 
	    }
        }
    }
    DataDefinitionSaveToFile
}

proc DataDefinitionInsertNewElement { index } {

}

proc DataDefinitionDataElementBindings { frame } {
    global UserData

    bind $frame <1> {
	LogAction "Data Element: Select in %W"
	focus %W
        update idletasks
	%W.pathname xview end
	focus %W.pathname
        update idletasks
	SendToStdErr "MARIA:  We should double-check all meta-informaiton."
    }

    bind $frame.pathname <1> {
	LogAction "Data Element: Select pathname in %W"
	SendToStdErr "MARIA:  We should double-check all meta-informaiton."
    }

    # Same as <BackSpace> below
    bind $frame.pathname <Delete> {
	if { [string length [%W get]] == 0 } then {
	    LogAction "Data Element: Delete in %W"
	    DataDefinitionDataElementDelete %W
	} else {
	    if { [%W select present] == 0 } then {
	        tkEntryBackspace %W 
	        break
	    }
	}
    }

    # Same as <Delete> above
    bind $frame.pathname <BackSpace> {
	if { [string length [%W get]] == 0 } then {
	    LogAction "Data Element: Delete in %W"
	    DataDefinitionDataElementDelete %W
	} else {
	    if { [%W select present] == 0 } then {
	        tkEntryBackspace %W 
	        break
	    }
	}
    }

    bind $frame.pathname <Tab> {
        regexp {^[0-9]*} [string range %W 11 end] WindowID
	set pathname [%W get]
	set frame [string range %W 0 [expr [string length %W] - 10]]
        if { ($pathname != "") && ([file isdirectory $pathname]) } then {
    	    LogAction "Data Element: <Tab> enables meta-information"
	    EnableMetaInformation $frame
        } else {
    	    LogAction "Data Element: <Tab> enables meta-information"
	    DisableMetaInformation $frame
	}
    }

    bind $frame.buttons.d <Return> {
        regexp {^[0-9]*} [string range %W 11 end] WindowID
	regsub "buttons.d" %W "pathname" fudgedW
	if { [$fudgedW get] == "" } then { return }
 	regexp {element[0-9]*} %W thisElement
	CreateNextDataElement $fudgedW $WindowID $thisElement 
    }

    bind $frame.buttons.d <Tab> {
        regexp {^[0-9]*} [string range %W 11 end] WindowID
	regsub "buttons.d" %W "pathname" fudgedW
	if { [$fudgedW get] == "" } then { return }
 	regexp {element[0-9]*} %W thisElement
	CreateNextDataElement $fudgedW $WindowID $thisElement 
    }


    bind $frame.pathname <Return> { 
        regexp {^[0-9]*} [string range %W 11 end] WindowID
	set pathname [%W get]
	if { $pathname == "" } then { return }
	set frame [string range %W 0 [expr [string length %W] - 10]]
        if { ($pathname != "") && ([file isdirectory $pathname]) } then {
	    EnableMetaInformation $frame
        } else {
	    DisableMetaInformation $frame
 	}
 	regexp {element[0-9]*} %W thisElement

	LogAction "Data Element: <Return> in pathname"	
	CreateNextDataElement %W $WindowID $thisElement 
    }
}

proc CreateNextDataElement { W WindowID thisElement } {
    global UserData

	# First, check to see if this pathname is a duplicate...  If so, delete it.
	set newPathname [[DDW-GetHList $WindowID].$thisElement.pathname get]
  	set thisFrame [DDW-GetHList $WindowID].element
	for {set i 0} {$i < [expr $UserData(HighElement:$WindowID) - 1]} {incr i} {
	    if { [winfo exists $thisFrame$i.pathname] == 1 } then {
		set pathname [$thisFrame$i.pathname get]
		if { [string compare $pathname $newPathname] == 0 } then {
		    if { [string range $thisElement 7 end] != $i } then {
 		        [DDW-GetHList $WindowID].$thisElement.pathname delete 0 end
			set UserData(Variable:[DDW-GetHList $WindowID].$thisElement) "none"
			DisableMetaInformation [DDW-GetHList $WindowID].$thisElement
		        focus $thisFrame$i.pathname
		        return
		    }
		}
	    }
	}

	set element [string range $thisElement 7 end]
        set nextelement [expr $element + 1]
        set nextElement "element$nextelement"
	regsub {element[0-9]*} $W $nextElement nextFrame
	set nextFrame [string range $nextFrame 0 [expr [string length $nextFrame] - 10]]

        while { ![[DDW-GetHList $WindowID] info exists $nextElement] } {
	    if { $nextelement ==  $UserData(HighElement:$WindowID)} then {
	        # This is the high element of the definition so insert a new
	        # element at the end of the list...
	        DataDefinitionDataElement $nextFrame ":"
	        [DDW-GetHList $WindowID] add $nextElement \
		    -itemtype window \
	  	    -window $nextFrame	
	    } else {
	        # This is just a hole in the element list so keep looking
	        incr nextelement
	        set nextElement "element$nextelement"
	        regsub {element[0-9]*} $W $nextElement nextFrame
	        set nextFrame [string range $nextFrame 0 [expr [string length $nextFrame] - 10]]
	    }
        }

	# Move the focus to the next element.
	update idletasks
	[DDW-GetHList $WindowID] yview $nextElement
	update idletasks
	focus $nextFrame.pathname
}

proc DataDefinitionShowData { WindowID data } {
    global UserData

    LogAction "Data Definition Window: Show $data"

    # Search for this task on the Data List
    set existingData [lsearch -exact $UserData(List) $data]

    # Clear out the SHList and replace it with this user data definition
    set HList [DDW-GetHList $WindowID]
    set i 0
    if { $existingData >= 0 } then {
        $HList delete all
        set UserData(HighElement:$WindowID) 0

        if { [ array names UserData ${data}:Definition ] != "" } then {
            for { set i 0 } { $i < [llength $UserData(${data}:Definition)] } { incr i } {
                set frame $HList.element${i} 
                DataDefinitionDataElement $frame [lindex $UserData(${data}:Definition) $i]
	        $HList add element$i \
		    -itemtype window \
		    -window $frame
            }
        }

        set frame $HList.element${i} 
        DataDefinitionDataElement $frame ":"
        $HList add element$i \
	    -itemtype window \
	    -window $frame

        incr i
        while { [winfo exists $HList.element${i}] == 1 } {
	    destroy $HList.element${i}
	    incr i
        }
    } else {
	while { [winfo exists $HList.element${i}] == 1 } {incr i}
	set i [expr $i-1]
	set frame $HList.element${i}
    }

    # Highlight data name if it is "New..."
    update idletasks
    if { $data == "New..." } then {
	# HACK ALERT!
	# Wait 50 ms for activity to die down before highlighting the entry
        after 50 [list [DDW-GetEntryWidget $WindowID] select range 0 end]
    } else {
        focus $frame.pathname
        update idletasks
    }
}

proc DataDefinitionSaveToFile { } {
    global UserData
    global Pathnames

    set hoardDir $Pathnames(newHoarding)
    set DataFILE [MyRenameAndOpen ${hoardDir}/Data {WRONLY CREAT TRUNC}]

    foreach element $UserData(List) {
	if { $element == "New..." } then { continue }
	puts $DataFILE "${element}:"
	foreach def $UserData(${element}:Definition) {
	    set definition [split $def :]
	    if { [lindex $definition 1] == "n" } then {
	        puts $DataFILE [lindex $definition 0]
	    } else {
	        puts $DataFILE [format "%s %s" [lindex $definition 0] [lindex $definition 1]]
	    }
	}
	puts $DataFILE ""
    }
    close $DataFILE
}

proc ProgramDefinitionWindow { element } {
    global Program
    global ProgramDefinition
    global DisplayStyle

    incr Program(WindowID)
    LogAction "Program Definition Window (ID=$Program(WindowID)): open"
    SetTime "ProgramDefinitionWindow$Program(WindowID):select"
    set Program(ProgramDefinitionWindow) .programdefine$Program(WindowID)
    toplevel $Program(ProgramDefinitionWindow)
    wm title $Program(ProgramDefinitionWindow) "Program Definition"
    wm geometry $Program(ProgramDefinitionWindow) 450x300+0+0
    wm protocol $Program(ProgramDefinitionWindow) WM_DELETE_WINDOW "LogAction ProgramDefinitionWindow(ID=$Program(WindowID)):beginclose; ProgramDefinitionWindowSave $Program(WindowID); destroy $Program(ProgramDefinitionWindow); LogAction ProgramDefinitionWindow($Program(WindowID)):endclose; SetTime ProgramDefinitionWindow$Program(WindowID):close"

    set Program(ProgramDefinition:MainFrame) $Program(ProgramDefinitionWindow).main
    frame $Program(ProgramDefinition:MainFrame) -bd 2 -relief groove 
    pack  $Program(ProgramDefinition:MainFrame) -side top -expand true -fill both

    set Program(ProgramDefinition:NameList) $Program(ProgramDefinition:MainFrame).names
    tixComboBox $Program(ProgramDefinition:NameList) \
	-label "Program Name: " \
	-editable true \
	-command [list ProgramDefinitionShowProgram $Program(WindowID)]
    ComboBoxFixEntryBindings [$Program(ProgramDefinition:NameList) subwidget entry]
    pack $Program(ProgramDefinition:NameList) -side top -expand true -fill both -pady 5
    for {set i 0} {$i < [llength $Program(List)]} {incr i} {
	$Program(ProgramDefinition:NameList) insert end [lindex $Program(List) $i]
    }

    set Program(ProgramDefinition:Label) $Program(ProgramDefinition:MainFrame).label
    label $Program(ProgramDefinition:Label) \
	-text "  contains the following executables:"
    pack $Program(ProgramDefinition:Label) -side top -expand true -fill both -pady 5

    set Program(ProgramDefinition:NameList:entrywidget) \
	[$Program(ProgramDefinition:NameList) subwidget entry] 

    set Program(ProgramDefinition:DefinitionList) \
	$Program(ProgramDefinition:MainFrame).definitionlist
    tixScrolledHList $Program(ProgramDefinition:DefinitionList)
    pack $Program(ProgramDefinition:DefinitionList) -side top \
	-expand true -fill both -padx 10 -pady 5
    set Program(ProgramDefinition:DefinitionList:hlist) \
	[$Program(ProgramDefinition:DefinitionList) subwidget hlist]

    pack [ProgramDefinitionWindowButtons] -side bottom -expand true -fill both

    tkwait visibility $Program(ProgramDefinition:NameList:entrywidget) 
    focus $Program(ProgramDefinition:NameList:entrywidget) 
    update idletasks

    set index [lsearch -exact $Program(List) $element]
    $Program(ProgramDefinition:NameList) pick $index

    SetTime "ProgramDefinitionWindow$Program(WindowID):visible"
}

proc ProgramDefinitionProgramElement { frame ProgramDefinition } {
    global Program

    set WindowID [PDW-GetWindowID $frame]
    incr Program(HighElement:$WindowID)

    if { [winfo exists $frame] == 0 } then {
        frame $frame -highlightcolor "blue" -highlightthickness 2

        # Make the pathname entry
        entry $frame.pathname -relief sunken -width 40
	bind $frame.pathname <1> { LogAction "Program Definition Element: click" }
        pack $frame.pathname -side left -expand true -fill x

	ProgramDefinitionProgramElementBindings $frame
    }

    set def [split $ProgramDefinition :]
    $frame.pathname delete 0 end
    $frame.pathname insert 0 [lindex $def 0]

    return $frame
}

proc ProgramDefinitionProgramElementDelete { frame } {
    global Program

    set framePrefix [string range $frame 0 [expr [string length $frame] - 10]]
    set element [string range $framePrefix [expr [string last "." $framePrefix] + 1] end]
    scan $element "element%d" elementID

    $framePrefix.pathname delete 0 end
    if { $elementID != [expr $Program(HighElement:[PDW-GetWindowID $frame]) - 1] } then {
	set WindowID [PDW-GetWindowID $frame]
        [PDW-GetHList $WindowID] delete entry $element 	
	incr elementID
	set nextElement "element$elementID"
        while { ![[PDW-GetHList $WindowID] info exists $nextElement] } {
	    incr elementID
	    set nextElement "element$elementID"
	}
	regsub {element[0-9]*} $frame $nextElement nextFrame
	focus $nextFrame
        update idletasks
    }
}

proc PDW-GetWindow { WindowID } {
    return .programdefine${WindowID}
}

proc PDW-GetWindowID { frame } {
    scan $frame {.programdefine%d} windowID
    return $windowID
}

proc PDW-GetMainWindow { WindowID } { 
    return .programdefine${WindowID}.main 
}

proc PDW-GetNameList { WindowID } { 
    return .programdefine${WindowID}.main.names 
}

proc PDW-GetEntryWidget { WindowID } { 
    return [.programdefine${WindowID}.main.names subwidget entry] 
}

proc PDW-GetHList { WindowID } { 
    return [.programdefine${WindowID}.main.definitionlist subwidget hlist] 
}

proc PDW-AddElement { WindowID } {
    global Program

    set thisProgram [[PDW-GetEntryWidget $WindowID] get]
    set index [lsearch -exact $Program(List) $thisProgram]
    for { set i 0 } { $i <= $Program(WindowID) } { incr i } {
	if { [winfo exists [PDW-GetWindow $i]] } then {
	    [PDW-GetNameList $i] insert $index $thisProgram
	}
    }
}

proc PDW-DeleteElement { WindowID } {
    global Program

    set thisProgram [[PDW-GetNameList $WindowID] cget -value]
    set index [lsearch -exact $Program(List) $thisProgram]
    for { set i 0 } { $i <= $Program(WindowID) } { incr i } {
	if { [winfo exists [PDW-GetWindow $i]] } then {
	    [[PDW-GetNameList $i] subwidget listbox] delete $index

	    # If this task is the visible task, then delete the elements too
	    if { [[PDW-GetNameList $i] cget -value] == $thisProgram } then {
	 	[PDW-GetNameList $i] pick 0
	    }
	}
    }
}


proc ProgramDefinitionProgramElementBindings { frame } {
    global Program

    bind $frame <1> {
	focus %W
        update idletasks
	%W.pathname xview end
	focus %W.pathname
        update idletasks

	LogAction "Program Element: Select in %W"
    }

    # Same as <BackSpace> below
    bind $frame.pathname <Delete> {
	if { [string length [%W get]] == 0 } then {
	    LogAction "Program Element: Delete in %W"
	    ProgramDefinitionProgramElementDelete %W
	} else {
	    if { [%W select present] == 0 } then {
	        tkEntryBackspace %W 
	        break
	    }
	}
    }

    # Same as <Delete> above
    bind $frame.pathname <BackSpace> {
	if { [string length [%W get]] == 0 } then {
	    LogAction "Program Element: Delete in %W"
	    ProgramDefinitionProgramElementDelete %W
	} else {
	    if { [%W select present] == 0 } then {
	        tkEntryBackspace %W 
	        break
	    }
	}
    }

    bind $frame.pathname <Return> {
	LogAction "Program Element: <Return> begin"
        regexp {^[0-9]*} [string range %W 14 end] WindowID

	if { [%W get] == "" } then { return }
 	regexp {element[0-9]*} %W thisElement

	# First, check to see if this pathname is a duplicate...  If so, delete it.
	set newPathname [[PDW-GetHList $WindowID].$thisElement.pathname get]
  	set thisFrame [PDW-GetHList $WindowID].element
	for {set i 0} {$i < [expr $Program(HighElement:$WindowID) - 1]} {incr i} {
	    if { [winfo exists $thisFrame$i.pathname] == 1 } then {
		set pathname [$thisFrame$i.pathname get]
		if { [string compare $pathname $newPathname] == 0 } then {
		    [PDW-GetHList $WindowID].$thisElement.pathname delete 0 end
		    focus $thisFrame$i.pathname
		    return
		}
	    }
	}

	set element [string range $thisElement 7 end]
        set nextelement [expr $element + 1]
        set nextElement "element$nextelement"
	regsub {element[0-9]*} %W $nextElement nextFrame
	set nextFrame [string range $nextFrame 0 [expr [string length $nextFrame] - 10]]

        while { ![[PDW-GetHList $WindowID] info exists $nextElement] } {
	    if { $nextelement ==  $Program(HighElement:$WindowID)} then {
	        # This is the high element of the definition so insert a new
	        # element at the end of the list...
	        ProgramDefinitionProgramElement $nextFrame ""
	        [PDW-GetHList $WindowID] add $nextElement \
		    -itemtype window \
	  	    -window $nextFrame	
	    } else {
	        # This is just a hole in the element list so keep looking
	        incr nextelement
	        set nextElement "element$nextelement"
	        regsub {element[0-9]*} %W $nextElement nextFrame
	        set nextFrame [string range $nextFrame 0 [expr [string length $nextFrame] - 10]]
	    }
        }

	# Move the focus to the next element.
	update idletasks
	[PDW-GetHList $WindowID] yview $nextElement
	update idletasks
	focus $nextFrame.pathname

	LogAction "Program Element: <Return> end"
    }
}

proc ProgramDefinitionWindowButtons { } {
    global Program

    set frame [frame $Program(ProgramDefinition:MainFrame).buttons -bd 2 -relief groove]

    button $frame.save -text "Save" -relief groove -bd 2 \
	-command [list ProgramDefinitionWindowSave $Program(WindowID)] 
    button $frame.saveas -text "Save as..." -relief groove -bd 2 \
	-command [list ProgramDefinitionWindowSaveAs $Program(WindowID)] 
    button $frame.revert -text "Revert" -relief groove -bd 2 \
	-command [list ProgramDefinitionWindowRevert $Program(WindowID)] 
    button $frame.delete -text "Delete Program" -relief groove -bd 2 \
	-command [list ProgramDefinitionWindowDelete $Program(WindowID)] 
    button $frame.help -text "Help" -relief groove -bd 2 \
	-command { ProgramDefinitionWindowHelp }

    pack $frame.help -side left
    pack $frame.save -side right
    pack $frame.saveas -side right
    pack $frame.revert -side right -padx 10
    pack $frame.delete -side right

    return $frame
}

proc ProgramDefinitionWindowBindings { } {

}

proc ProgramDefinitionWindowSave { WindowID } {
    global Program

    LogAction "Program Definition Window: save"
    set prog [[PDW-GetEntryWidget $WindowID] get]
    if { $prog != "New..." } then {
        if { [lsearch -exact $Program(List) $prog] == -1 } then {
	    ProgramDefinitionWindowSaveAs $WindowID
	    return
        } else {
	    set frame [PDW-GetHList $WindowID].element
            set i 0
	    set aok 1
	    set Program($prog:Definition) [list ]
 	    while { $aok } {
	        if { [winfo exists $frame$i.pathname] == 1 } then {
		    set pathname [$frame$i.pathname get]
		    if { ($pathname != "") && ($pathname != "New...") } then {
		        lappend Program($prog:Definition) $pathname
		    } 
		    incr i
	        } else { set aok 0 } 
	    }
        }
    }
    ProgramDefinitionSaveToFile
}

proc ProgramDefinitionWindowSaveAs { WindowID} {
    global Program
    global Task

    LogAction "Program Definition Window: save as"
    set prog [[PDW-GetEntryWidget $WindowID] get]
    while { [lsearch -exact $Program(List) $prog] != -1 } {
	if { $prog == "New..." } then {
	    LogAction "Program Definition Window: Enter name instead of New..."
	    set messagetext "Please enter a new name:  "
	} else {
	    LogAction "Program Definition Window: Enter name instead of $prog"
	    set messagetext "A program set named $prog is already defined.  Please enter a new name:  "
	}
	set ProgramDefinitionPickName [PDW-GetWindow $WindowID].pickname
	PopupNewName "Program Definition Save As..." $ProgramDefinitionPickName $messagetext Program(newName) "5i" $Program(List)
	if { $Program(newName) == "" } then { return } else { set prog $Program(newName) }
  	[PDW-GetEntryWidget $WindowID] delete 0 end
	[PDW-GetEntryWidget $WindowID] insert 0 $prog
	unset Program(newName)
    } 

    lappend Program(List) $prog
    set Program(List) [lsort -command MyCompare $Program(List)]
    set index [lsearch $Program(List) $prog]
    if { $index == 0 } then { set index 1 }

    # Add new proram to correct location in the NameLists
    PDW-AddElement $WindowID

    # Add new program to the TaskDefinitionWindow program area
    TDW-AddElement $WindowID programs $prog

    ProgramDefinitionWindowSave $WindowID
    [PDW-GetNameList $WindowID] configure -value $prog
}

proc ProgramDefinitionWindowRevert { WindowID } {
    global Program

    set program [[PDW-GetEntryWidget $WindowID] get]
    set index [lsearch -exact $Program(List) $program]
    [PDW-GetNameList $WindowID] pick $index
    focus [PDW-GetEntryWidget $WindowID]
    update idletasks
}

proc ProgramDefinitionWindowDelete { WindowID } {
    global Program
    global Task
    global ControlPanel

    LogAction "Program Definition Window: delete program"
    set prog [[PDW-GetNameList $WindowID] cget -value]
    if { $prog == "New..." } then {
 	[PDW-GetNameList $WindowID] pick 0
	focus [PDW-GetNameList $WindowID]
        update idletasks
	return
    }

    set index [lsearch -exact $Program(List) $prog]
    if { $index == -1 } then {
	[PDW-GetNameList $WindowID] pick 0
        focus [PDW-GetEntryWidget $WindowID]
        update idletasks
	return 
    }

    if { $ControlPanel(SafeDeletes:program) == "on" } then {
	set messagetext [list "This action will delete the entire $prog definition."]

	set containingTasks [ReturnContainingTaskList $prog programsList]
	set containingTasks [SortAndRemoveDuplicates $containingTasks]
	
	if { [llength $containingTasks] > 0 } then {
	    lappend messagetext [format "Further, the \"%s\" program definition is contained in %d other task(s).  These include:" $prog [llength $containingTasks]]
	    foreach t $containingTasks {
		lappend messagetext [format "\t%s" $t]
	    }
        }

	lappend messagetext "Do you want to delete $prog?"
	set ReallyDeleteYesNoFrame [PDW-GetWindow $WindowID].reallydeleteyesno
	if { [info exists Program(ReallyDeleteYesNoVariable:$WindowID)] } then {
  	    unset Program(ReallyDeleteYesNoVariable:$WindowID)
	}
	LogAction "Program Definition Window: Safe Delete Query"
	PopupSafeDelete "Really delete?" $ReallyDeleteYesNoFrame [join $messagetext "\n"] Program(ReallyDeleteYesNoVariable:$WindowID) 
        tkwait variable Program(ReallyDeleteYesNoVariable:$WindowID)
        destroy $ReallyDeleteYesNoFrame
        if { $Program(ReallyDeleteYesNoVariable:$WindowID) == "no" } then {
	    return
        }
    }

    ## Now, really delete it!
    PDW-DeleteElement $WindowID

    # Remove data from individual tasks
    RemoveTaskFromTaskDefinitions $prog programsList
    TDW-DeleteElement $WindowID programs $prog

    # Delete the definition
    unset Program($prog:Definition)

    set Program(List) [lreplace $Program(List) $index $index]
    [PDW-GetNameList $WindowID] pick 0
    focus [PDW-GetEntryWidget $WindowID]
    update idletasks

    ProgramDefinitionSaveToFile
}

proc ProgramDefinitionWindowHelp { } {
    global Program

    LogAction "Program Definition Window: help"
    set messagetext "The Program Definition window allows you to define a new program or to view an existing program definition.  To select an existing program definition, you may either type the name of the program and then hit return, or click on the down arrow button and use the popdown listing.  To define a new program, you can type the new name and hit return.\n\nThe listing below the program name shows the executables that are part of this program definition."

    set Program(ProgramDefinitionHelp) $Program(ProgramDefinitionWindow).help

    PopupHelp "Program Definition Help" $Program(ProgramDefinitionHelp) $messagetext "5i"
}

proc ProgramDefinitionShowProgram { WindowID program } {
    global Program

    LogAction "Program Definition Window: Show $program"

    # Search for this task on the Program List
    set existingProgram [lsearch -exact $Program(List) $program]

    # Clear out the SHList and replace it with this program definition
    set HList [PDW-GetHList $WindowID]
    set i 0
    if { $existingProgram >= 0 } then { 
	$HList delete all 
        set Program(HighElement:$WindowID) 0

	if { [ array names Program ${program}:Definition ] != "" } then {
	    for { set i 0 } { $i < [llength $Program(${program}:Definition)] } { incr i } {
	        set frame $HList.element${i} 
	        ProgramDefinitionProgramElement $frame \
		     [lindex $Program(${program}:Definition) $i]
		$HList add element$i \
		     -itemtype window \
		     -window $frame
	    }
    	}

        set frame $HList.element${i} 
        ProgramDefinitionProgramElement $frame ":"
        $HList add element$i \
	    -itemtype window \
	    -window $frame

        incr i
        while { [winfo exists $HList.element${i}] == 1 } {
	    destroy $HList.element${i}
	    incr i
        }
    } else {
	while { [winfo exists $HList.element${i}] == 1 } { incr i }
	set i [expr $i-1]
	set frame $HList.element${i}
    }


    # Highlight program name if it is "New..."
    update idletasks
    if { $program == "New..." } then {
	# HACK ALERT!
	# Wait 50 ms for activity to die down before highlighting the "New..." entry
        after 50 [list [PDW-GetEntryWidget $WindowID] select range 0 end]
    } else {
	focus $frame.pathname
        update idletasks
    }
}

proc ProgramDefinitionSaveToFile { } {
    global Program
    global Pathnames

    set hoardDir $Pathnames(newHoarding)
    set ProgramFILE [MyRenameAndOpen ${hoardDir}/Programs {WRONLY CREAT TRUNC}]

    foreach element $Program(List) {
	if { $element == "New..." } then { continue }
	puts $ProgramFILE "${element}:"
	foreach def $Program(${element}:Definition) {
	    puts $ProgramFILE $def
	}
	puts $ProgramFILE ""
    }

    close $ProgramFILE
}

proc TaskAvailable { args } {
    global Indicator
    global Task

    set listOfTasks [split [lindex $args 0] :]
    for { set i 0 } { $i < [llength $listOfTasks] } { incr i } {
	set TaskName [lindex $listOfTasks $i]
        set Task(AvailableTaskList) [lappend Task(AvailableTaskList) $TaskName]
        SetTaskAvailability $TaskName 100
    }

    ChangeStatesAndUpdateIndicator
    TaskCheckVisibilityAndUpdateScreen
    update idletasks
}

proc TaskAvailabilityProc { priority available unavailable incomplete } {

    set TaskName [GetTaskNameFromPriority $priority]
    set availability [expr $available * 100 / ($available+$unavailable)]

    SetTaskMaxSpace $TaskName [expr ($available + $unavailable) / 1024] [expr $available + $unavailable]
    SetTaskCurrentSizeKB $TaskName $available
    SetTaskAvailability $TaskName $availability

    if { $unavailable == 0 } then {
	TaskAvailable $TaskName
    }

    if { $unavailable != 0 } then {
	TaskUnavailable $TaskName
    }

    if { $incomplete == 1 } then {
	SendToStdErr "MARIA:  TaskAvailabilityProc ignores incomplete"
    }
}

proc MakeAllHoardedTasksAvailable { } {
    global Task

    TaskAvailable [join $Task(SelectedTaskList) :]
}

proc MakeAllHoardedTasksUnavailable { } {
    global Task

    TaskUnavailable [join $Task(SelectedTaskList) :]
}

proc MakeSecondHoardedTaskUnavailable { } {
    global Task

    set NukedTask [lindex $Task(SelectedTaskList) 1]
    if { $NukedTask != "" } then {
        TaskUnavailable $NukedTask
    }
}

proc GetTaskIndex { priority } {
    global Task

    set index [lsearch -exact $Task(Priorities) $priority]
    return $index
}

proc GetTaskNameFromPriority { priority } {
    global Task

    set index [GetTaskIndex $priority]
    return [lindex $Task(SelectedTaskList) $index]
}

proc TaskUnavailable { priority size } {
    global Indicator
    global Task

    set TaskName  [GetTaskNameFromPriority $priority]
    set TaskCurrentSize [GetTaskCurrentSizeKB $TaskName]
    set TaskNewSize [expr $TaskCurrentSize - $size]
    set TaskMaxSize [GetTaskMaxSpaceKB $TaskName]
    SetTaskCurrentSizeKB $TaskName $TaskNewSize
    set NewAvailability [expr $TaskNewSize*100/$TaskMaxSize]
    SetTaskAvailability $TaskName $NewAvailability

    set index [lsearch -exact $Task(AvailableTaskList) $TaskName]
    if { $index == -1 } then { 
	return 
    } else { 
	set Task(AvailableTaskList) [lreplace $Task(AvailableTaskList) $index $index]
    }

    ChangeStatesAndUpdateIndicator
    TaskCheckVisibilityAndUpdateScreen
    update idletasks
}

proc MyCompare { a b } {
    if { [string compare $a "New..."] == 0 } then {
	return -1
    }
    if { [string compare $a "New..."] == 0 } then {
	return 1
    }
    return [ string compare $a $b ]
}

###
###  Support for hoarding!
###

#
# The priority of a task is calculated by determining its index in the list of
# selected tasks and using that index to lookup in the list of priorities.
# If we exceed the number of unique task priorities, things are going to start
# acting strange!!!
#
proc TaskGetPriority { task } {
    global Task

    # Get the index from the list of hoarded tasks
    set index [lsearch -exact $Task(SelectedTaskList) $task]

    # Lookup the index in the list of Priorities
    if { $index >= [llength $Task(Priorities)] } then {
	SendToStdErr "ERROR: Too many tasks hoarded!!!!"
	set priority $Task(MinimumPriority)
    } else {
        set priority [lindex $Task(Priorities) $index]
    }

    return $priority
}

proc Hoard { } {
    global Task
    global Hoard
    global HoardList

    # Check if order is the same
    if { [info exist Hoard(LastPriorityOrder)] } then {
	if { $Hoard(LastPriorityOrder) == $Task(SelectedTaskList) } then {
	    # Suppress hoard -- same as last one
	    return
	}
    }

    if { [info exists HoardList] } then {
	unset HoardList
    }
    set HoardList(linenum) 0

    # Clear the hoard database
    SendToAdviceMonitor "ClearHoardDatabase"

    # Get output lock
    Lock Output

    # Print initial string
    puts "Hoard"

    # Printing hoarding commands
    foreach task $Task(SelectedTaskList) {
        HoardTask $task [TaskGetPriority $task] 1 0 $task
    }
    set Hoard(LastPriorityOrder) $Task(SelectedTaskList)

    # Print final string
    puts "END"
    puts ""
    flush stdout

    # Release output lock
    UnLock Output
}

proc HoardTask { task priority arg depth taskpath } {
    global Task
    global TaskDefinition

    foreach f {userData programs subtasks} {
	for {set i 0} {$i < [llength $Task(${task}:${f}List)]} {incr i} {
	    set element [lindex $Task(${task}:${f}List) $i]

	    # Recurse
	    if { $f == "userData" } then {
		HoardData $element $priority $arg [expr $depth+1] $taskpath\\$element
	    }
	    if { $f == "programs" } then {
		HoardProgram $element $priority $arg [expr $depth+1] $taskpath\\$element
	    }
	    if { $f == "subtasks" } {
		HoardTask $element $priority $arg [expr $depth+2] "$taskpath\\$element"
	    }
	}
    }
}

proc HoardData { data priority arg depth taskpath } {
    global UserData
    global HoardList

    for {set i 0} {$i < [llength $UserData(${data}:Definition)]} {incr i} {

	set def [split [lindex $UserData(${data}:Definition) $i] :]
	set pathname [lindex $def 0]

	if { ![file exists $pathname] } then {
	    SendToStdErr "Output ERROR: $pathname does not exist"
	}
	set meta [lindex $def 1]

	if { [file isdirectory $pathname] } then {
	    set HoardList(Line$HoardList(linenum)) \
		[format "%s %s %s %s" $pathname $meta $priority $taskpath]
	    set HoardList(Path$HoardList(linenum)) $pathname
	    set HoardList(Priority$HoardList(linenum)) $priority
	    set HoardList(Meta$HoardList(linenum)) $meta
	    set HoardList(TaskPath$HoardList(linenum)) $taskpath
	    incr HoardList(linenum)
  	    puts [format " %s %s %s" $priority $pathname $meta]
	    flush stdout
	} else {
	    set HoardList(Line$HoardList(linenum)) \
		[format "%s %s %s" $pathname $priority $taskpath]
	    set HoardList(Path$HoardList(linenum)) $pathname
	    set HoardList(Priority$HoardList(linenum)) $priority
	    set HoardList(Meta$HoardList(linenum)) ""
	    set HoardList(TaskPath$HoardList(linenum)) $taskpath
	    incr HoardList(linenum)
  	    puts [format " %s %s" $priority $pathname]
	    flush stdout
	}
    }
}

proc HoardProgram { program priority arg depth taskpath } {
    global Program
    global Pathnames
    global HoardList

    for {set i 0} {$i < [llength $Program(${program}:Definition)]} {incr i} {
	set element [lindex $Program(${program}:Definition) $i]
	regsub -all {/} $element {\\} profileName
	if { [file exists $Pathnames(newHoarding)/ProgramProfiles/${profileName}] } then {
	    set PROFILE [open $Pathnames(newHoarding)/ProgramProfiles/${profileName} "r"]
	    while {[gets $PROFILE line] >= 0} {
		set HoardList(Line$HoardList(linenum)) "$line $priority $taskpath"
		set HoardList(Path$HoardList(linenum)) [lindex $line 0]
		set HoardList(Priority$HoardList(linenum)) $priority
		if { [llength $line] == 2 } then {
		    set HoardList(Meta$HoardList(linenum)) [lindex $line 1]
		} else {
		    set HoardList(Meta$HoardList(linenum)) ""
		}
		set HoardList(TaskPath$HoardList(linenum)) $taskpath
		incr HoardList(linenum)
	    }
	    close $PROFILE
	} else {
	    SendToStdErr "ERROR:  Hoard profile (for $program) does not exist! (yet?)"
	}
    }
}


