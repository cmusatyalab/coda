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
# Suggest that user consider not hoarding certain objects
#
# History:
#  97/11/24: mre@cs.cmu.edu : created
#

#
# Data Structures:
#    ConsiderRemoving records data associated with the window and its contents
#    ConsiderRemovingData records data obtained from Venus
#


proc CR_InitPersistent { } {
    global ConsiderRemoving

    set ConsiderRemoving(MainWindow) .considerremoving
    set ConsiderRemoving(HelpWindow) .considerremovinghelp

    set ConsiderRemoving(Geometry) "600x275"
    regsub x $ConsiderRemoving(Geometry) " " ConsiderRemoving(MinSize)

    set ConsiderRemoving(NumberColumns) 5
    set ConsiderRemoving(Column0Header) "Pathname"
    set ConsiderRemoving(Column1Header) "Not Accessed\n  Recently"
    set ConsiderRemoving(Column2Header) "Not Frequently\n   Accessed"
    set ConsiderRemoving(Column3Header) "Ignore?" 
    set ConsiderRemoving(Column4Header) "" 

    set ConsiderRemoving(NumberRecent) 2
    set ConsiderRemoving(PercentAccess) 50
    set ConsiderRemoving(MinimumDatapoints) 2


    set ConsiderRemoving(ID) 0
    CR_InitTransient
}

proc CR_InitTransient { } {
    global ConsiderRemovingData
    global ConsiderRemoving

    if { [info exists ConsiderRemovingData] } then {
	unset ConsiderRemovingData
    }
    set ConsiderRemovingData(ID) 0

    for { set i 0 } { $i < $ConsiderRemoving(ID) } { incr i } {
	unset ConsiderRemoving(Path:$i)
	unset ConsiderRemoving(Ignore:$i)
    }
    set ConsiderRemoving(ID) 0
}


proc CR_InitDataElement { id } {
    global ConsiderRemovingData

    set ConsiderRemovingData(NotRecent:$id) 0
    set ConsiderRemovingData(NotFrequent:$id) 0
}

proc CR_Set { path what } {
    global ConsiderRemovingData

    if { [info exists ConsiderRemovingData($path)] } then {
	set id $ConsiderRemovingData($path)
    } else {
	set id $ConsiderRemovingData(ID)
	set ConsiderRemovingData($path) $id
	set ConsiderRemovingData($id) $path
	incr ConsiderRemovingData(ID)
        CR_InitDataElement $id
    }

    set ConsiderRemovingData(${what}:${id}) 1
}


proc ConsiderRemoving { } {
    global ConsiderRemoving

    if { [winfo exists $ConsiderRemoving(MainWindow)] } then {
        raise $ConsiderRemoving(MainWindow)
        return 
    }

    toplevel $ConsiderRemoving(MainWindow)
    wm title $ConsiderRemoving(MainWindow) "Consider Removing"
    wm geometry $ConsiderRemoving(MainWindow) $ConsiderRemoving(Geometry)
    wm minsize $ConsiderRemoving(MainWindow) \
	[lindex $ConsiderRemoving(MinSize) 0] [lindex $ConsiderRemoving(MinSize) 1]
    wm protocol $ConsiderRemoving(MainWindow) WM_DELETE_WINDOW {CR_Complete}

    CR_MkWindow

    # Now wait for the user to respond...
    tkwait variable ConsiderRemoving(Completed)

    destroy $ConsiderRemoving(MainWindow)
    return 0
}

proc CR_Complete { } {
    global ConsiderRemoving
    global ConsiderIgnore

    set ConsiderRemoving(Geometry) [GeometrySizeOnly [wm geometry $ConsiderRemoving(MainWindow)]]

    for { set i 0 } { $i < $ConsiderRemoving(ID) } { incr i } {
	if { $ConsiderRemoving(Ignore:$i) == 1 } then {
	    Consider_AddIgnore $ConsiderRemoving(Path:$i)
	}
    }
    flush $ConsiderIgnore(IgnoreFILE)

    # Empty out the data structures in preparation for next time
    CR_InitTransient

    set ConsiderRemoving(Completed) 1
}

proc CR_MkWindow { } {
    global ConsiderRemoving

    set f $ConsiderRemoving(MainWindow)

    tixTree $f.list -options "
	hlist.columns $ConsiderRemoving(NumberColumns)\
	hlist.header true \
	hlist.separator {!@\#$%^&*()}\
    "
    pack $f.list -side top -expand true -fill both

    set ConsiderRemoving(List) [$f.list subwidget hlist]

    CR_AddHeaders $ConsiderRemoving(List)
    CR_InsertObjects $ConsiderRemoving(List)

    # Create Help Button
    frame $f.buttons -relief flat
    pack $f.buttons -side bottom -fill x

    button $f.buttons.help -text "Help" \
	-relief groove -bd 2 -command {CR_HelpWindow}
    pack $f.buttons.help -side left -padx 4 -pady 4
}

proc CR_AddHeaders { f } {
    global DisplayStyle
    global ConsiderRemoving

    $f header create 0 \
	-itemtype text \
	-text $ConsiderRemoving(Column0Header) \
	-style $DisplayStyle(Header)

    $f header create 1 \
	-itemtype text \
	-text $ConsiderRemoving(Column1Header) \
	-style $DisplayStyle(Header)

    $f header create 2 \
	-itemtype text \
	-text $ConsiderRemoving(Column2Header) \
	-style $DisplayStyle(Header)

    $f header create 3 \
	-itemtype text \
	-text $ConsiderRemoving(Column3Header) \
	-style $DisplayStyle(Header)

    $f header create 4 \
	-itemtype text \
	-text $ConsiderRemoving(Column4Header) \
	-style $DisplayStyle(Header)

    $f column width 0 {}
    $f column width 1 "1.5i"
    $f column width 2 "1.5i"
    $f column width 3 "1i"
    $f column width 4 0
}


proc CR_InsertObject { list path recency frequency } {
    global ConsiderRemoving
    global DisplayStyle

    set id $ConsiderRemoving(ID)
    incr ConsiderRemoving(ID)

    set ConsiderRemoving(Path:$id) $path

    $list add $path -itemtype text -text $path

    if { $recency } then {
	$list item create $path 1 \
	    -itemtype text -text "X" -style $DisplayStyle(Centered)
    }

    if { $frequency } then {
	$list item create $path 2 \
	    -itemtype text -text "X" -style $DisplayStyle(Centered)
    }

    checkbutton $list.ignore$id \
	-variable ConsiderRemoving(Ignore:$id) \
	-background gray92 \
	-anchor c 
    $list item create $path 3 \
	-itemtype window -window $list.ignore$id

}

proc CR_InsertObjects { f } {
    global ConsiderRemovingData

    for { set id 0 } { $id < $ConsiderRemovingData(ID) } { incr id } {
	CR_InsertObject $f $ConsiderRemovingData($id) \
	    $ConsiderRemovingData(NotRecent:$id) \
	    $ConsiderRemovingData(NotFrequent:$id) \
    }
}


proc CR_HelpWindow { } {
    global ConsiderRemoving

    regsub "\n" $ConsiderRemoving(Column1Header) " " header1
    regsub "\n" $ConsiderRemoving(Column2Header) " " header2
    regsub "\n" $ConsiderRemoving(Column3Header) " " header3

    set helptext "The Consider Removing window identifies file and directories that you may have forgotten are hoarded.  It uses two heuristics to identify these objects:

    $header1:
        identifies objects that you have not accessed in the 
        $ConsiderRemoving(NumberRecent) most recent disconnected sessions

    $header2:
        identifies objects that you have accessed in fewer than
        $ConsiderRemoving(PercentAccess)% of disconnected sessions

If the system identifies an object you would prefer not to hoard, you must manually remove it from your task definitions.  In the future, you may be able to remove it from this screen.  If the system identifies an object you do want to hoard and for which you do not wish to receive advice in the future, you may click on the object's checkbox at the far right."

    PopupHelp "Consider Removing Advice Help Window" $ConsiderRemoving(HelpWindow) $helptext "7i"
}

