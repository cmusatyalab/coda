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
# Suggest that user consider hoarding additional objects
#
# History:
#  97/11/23: mre@cs.cmu.edu : created
#

#
# Data Structures:
#    ConsiderAdding records data associated with the window and its contents
#    ConsiderAddingData records data obtained from Venus
#


proc CA_InitPersistent { } {
    global ConsiderAdding

    set ConsiderAdding(MainWindow) .consideradding
    set ConsiderAdding(HelpWindow) .consideraddinghelp

    set ConsiderAdding(Geometry) "600x275"
    regsub x $ConsiderAdding(Geometry) " " ConsiderAdding(MinSize)

    set ConsiderAdding(NumberColumns) 6
    set ConsiderAdding(Column0Header) "Pathname"
    set ConsiderAdding(Column1Header) "  Infrequent\nbut Consistent"
    set ConsiderAdding(Column2Header) "Accessed\n (lucky)" 
    set ConsiderAdding(Column3Header) " Missed\n(unlucky)" 
    set ConsiderAdding(Column4Header) "Ignore?" 
    set ConsiderAdding(Column5Header) "" 

    set ConsiderAdding(ID) 0
    CA_InitTransient
}

proc CA_InitTransient { } {
    global ConsiderAddingData
    global ConsiderAdding

    if { [info exists ConsiderAddingData] } then {
	unset ConsiderAddingData
    }
    set ConsiderAddingData(ID) 0

    for { set i 0 } { $i < $ConsiderAdding(ID) } { incr i } {
	unset ConsiderAdding(Path:$i)
	unset ConsiderAdding(Ignore:$i)
    }
    set ConsiderAdding(ID) 0
}


proc CA_InitDataElement { id } {
    global ConsiderAddingData

    set ConsiderAddingData(Refetch:$id) 0
    set ConsiderAddingData(Lucky:$id) 0
    set ConsiderAddingData(Unlucky:$id) 0
}

proc CA_Set { path what } {
    global ConsiderAddingData

    if { [info exists ConsiderAddingData($path)] } then {
	set id $ConsiderAddingData($path)
    } else {
	set id $ConsiderAddingData(ID)
	set ConsiderAddingData($path) $id
	set ConsiderAddingData($id) $path
	incr ConsiderAddingData(ID)
        CA_InitDataElement $id
    }

    set ConsiderAddingData(${what}:${id}) 1
}


proc ConsiderAdding { } {
    global ConsiderAdding

    if { [winfo exists $ConsiderAdding(MainWindow)] } then {
        raise $ConsiderAdding(MainWindow)
        return 
    }

    toplevel $ConsiderAdding(MainWindow)
    wm title $ConsiderAdding(MainWindow) "Consider Adding"
    wm geometry $ConsiderAdding(MainWindow) $ConsiderAdding(Geometry)
    wm minsize $ConsiderAdding(MainWindow) \
	[lindex $ConsiderAdding(MinSize) 0] [lindex $ConsiderAdding(MinSize) 1]
    wm protocol $ConsiderAdding(MainWindow) WM_DELETE_WINDOW {CA_Complete}

    CA_MkWindow

    # Now wait for the user to respond...
    tkwait variable ConsiderAdding(Completed)

    destroy $ConsiderAdding(MainWindow)
    return 0
}

proc CA_Complete { } {
    global ConsiderAdding
    global ConsiderIgnore

    set ConsiderAdding(Geometry) [GeometrySizeOnly [wm geometry $ConsiderAdding(MainWindow)]]

    for { set i 0 } { $i < $ConsiderAdding(ID) } { incr i } {
	if { $ConsiderAdding(Ignore:$i) == 1 } then {
	    Consider_AddIgnore $ConsiderAdding(Path:$i)
	}
    }
    flush $ConsiderIgnore(IgnoreFILE)

    # Empty out the data structures in preparation for next time
    CA_InitTransient

    set ConsiderAdding(Completed) 1
}

proc CA_MkWindow { } {
    global ConsiderAdding

    set f $ConsiderAdding(MainWindow)

    tixTree $f.list -options "\
	hlist.columns $ConsiderAdding(NumberColumns)\
	hlist.header true \
	hlist.separator {!@\#$%^&*()}\
    "
    pack $f.list -side top -expand true -fill both

    set ConsiderAdding(List) [$f.list subwidget hlist]

    CA_AddHeaders $ConsiderAdding(List)
    CA_InsertObjects $ConsiderAdding(List)

    # Create Help Button
    frame $f.buttons -relief flat
    pack $f.buttons -side bottom -fill x

    button $f.buttons.help -text "Help" \
	-relief groove -bd 2 -command {CA_HelpWindow}
    pack $f.buttons.help -side left -padx 4 -pady 4
}

proc CA_AddHeaders { f } {
    global DisplayStyle
    global ConsiderAdding

    $f header create 0 \
	-itemtype text \
	-text $ConsiderAdding(Column0Header) \
	-style $DisplayStyle(Header)

    $f header create 1 \
	-itemtype text \
	-text $ConsiderAdding(Column1Header) \
	-style $DisplayStyle(Header)

    $f header create 2 \
	-itemtype text \
	-text $ConsiderAdding(Column2Header) \
	-style $DisplayStyle(Header)

    $f header create 3 \
	-itemtype text \
	-text $ConsiderAdding(Column3Header) \
	-style $DisplayStyle(Header)

    $f header create 4 \
	-itemtype text \
	-text $ConsiderAdding(Column4Header) \
	-style $DisplayStyle(Header)

    $f header create 5 \
	-itemtype text \
	-text $ConsiderAdding(Column5Header) \
	-style $DisplayStyle(Header)

    $f column width 0 {}
    $f column width 1 "1.5i"
    $f column width 2 "1.25i"
    $f column width 3 "1.25i"
    $f column width 4 "1i"
    $f column width 5 0
}


proc CA_InsertObject { list path refetched lucky unlucky } {
    global ConsiderAdding
    global DisplayStyle

    set id $ConsiderAdding(ID)
    incr ConsiderAdding(ID)

    set ConsiderAdding(Path:$id) $path

    $list add $path -itemtype text -text $path

    if { $refetched } then {
	$list item create $path 1 \
	    -itemtype text -text "X" -style $DisplayStyle(Centered)
    }

    if { $lucky } then {
	$list item create $path 2 \
	    -itemtype text -text "X" -style $DisplayStyle(Centered)
    }

    if { $unlucky } then {
	$list item create $path 3 \
	    -itemtype text -text "X" -style $DisplayStyle(Centered)
    }

    checkbutton $list.ignore$id \
	-variable ConsiderAdding(Ignore:$id) \
	-background gray92 \
	-anchor c 
    $list item create $path 4 \
	-itemtype window -window $list.ignore$id

}

proc CA_InsertObjects { f } {
    global ConsiderAddingData

    for { set id 0 } { $id < $ConsiderAddingData(ID) } { incr id } {
	CA_InsertObject $f $ConsiderAddingData($id) \
	    $ConsiderAddingData(Refetch:$id) \
	    $ConsiderAddingData(Lucky:$id) \
	    $ConsiderAddingData(Unlucky:$id)
    }
}


proc CA_HelpWindow { } {
    global ConsiderAdding

    regsub "\n" $ConsiderAdding(Column1Header) " " header1
    regsub "\n" $ConsiderAdding(Column2Header) " " header2
    regsub "\n" $ConsiderAdding(Column3Header) " " header3

    set helptext "The Consider Adding window identifies file and directories that you may have forgotten to hoard.  It uses three heuristics to identify these objects:

    $header1:
        identifies objects that you access infrequently, but
        consistently

    $header2:
        identifies objects that you have accessed while operating
        disconnected, but for which you have not assigned a hoard
        priority -- thus the only reason you were able to access
        them successfully is because they happened to be cached 
        (you were lucky)

    $header3:
        identifies objects that you have attempted to access while
        operating disconnected but have been unsuccessfuly because 
        they have not been cached (you were unlucky)

If the system identifies an object you would like to hoard, you must manually add it to one of your task definitions.  In the future, you may be able to add it from this screen.  If the system identifies an object you do not want to hoard and for which you do not to receive advice in the future, you may click on the object's checkbox at the far right."

    PopupHelp "Consider Adding Advice Help Window" $ConsiderAdding(HelpWindow) $helptext "7i"
}

