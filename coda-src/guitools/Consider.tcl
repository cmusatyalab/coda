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

proc Consider_Init { } {
    global ConsiderIgnore
    global Pathnames

    CA_InitPersistent
    CR_InitPersistent

    Consider_ReadIgnoreData

    set hoardDir $Pathnames(newHoarding)
    set ConsiderIgnore(IgnoreFILE) [MyRenameAndOpen ${hoardDir}/Ignore {CREAT WRONLY APPEND}]
}


######################################################################
#                                                                    #
#  Routines related to the pathnames which should be ignored         #
#                                                                    #
######################################################################


#
#  Read the Ignore data file
#
proc Consider_ReadIgnoreData { } {
    global Pathnames
    global ConsiderIgnore

    set hoardDir $Pathnames(newHoarding)
    set IgnoreFileName ${hoardDir}/Ignore

    set ConsiderIgnore(Count) 0
    if { [file exists $IgnoreFileName] } then {
	set IgnoreFILE [open $IgnoreFileName {RDONLY}]
	foreach line [split [read $IgnoreFILE] \n] {
	    if { $line == "" } then {
		continue
	    } else {
		set ConsiderIgnore($ConsiderIgnore(Count)) $line
		incr ConsiderIgnore(Count)
	    }
	}
	close $IgnoreFILE
    }
}

#
#  Adds a pathname to the ignore data and append it to the ignore data file
#
proc Consider_AddIgnore { path } {
    global ConsiderIgnore

    puts $ConsiderIgnore(IgnoreFILE) $path
    set ConsiderIgnore($ConsiderIgnore(Count)) $path
    incr ConsiderIgnore(Count)

    flush $ConsiderIgnore(IgnoreFILE)
}

#
#  Determine if a pathname should be ignored
#
#     Returns:
#         1 if path should be ignored
#         0 otherwise.
#
proc Consider_IgnoreElement { path } {
    global ConsiderIgnore

    for { set i 0 } { $i < $ConsiderIgnore(Count) } { incr i } {
	if { [string compare $path $ConsiderIgnore($i)] == 0 } then {
	    return 1
	}
    }
    return 0
}


######################################################################
#                                                                    #
#  Routines related to processing input from Venus                   #
#                                                                    #
######################################################################

proc Consider_GetPath { fidString } {
    set fid [split $fidString .]
    set volume [lindex $fid 0]
    set mountPath [string range [string trim [exec cfs getmountpoint $volume]] 11 end]
    set relativePath [string trimleft [string trim [exec cfs getpath $fidString]] .]
    return "${mountPath}${relativePath}"
}


# N.B. Although it would be more elegant to check the path to see if we 
# should ignore it once, it is way too inefficient to get the path for 
# every singleobject in the usage_statistics file.  For this reason, we 
# only get the path and check to see if we should ignore it once we know 
# an object is interesting.
proc Consider_ProcessVenusInputLine { line } {
    global ConsiderRemoving

    set lineList               [split [string trim $line]]

    set hoardPriority          [lindex $lineList 1]
    set disconnectionsSinceUse [lindex $lineList 2]
    set disconnectionsUsed     [lindex $lineList 3]
    set disconnectionsUnused   [lindex $lineList 4]

 
    if { ($disconnectionsUsed > 0) && ($hoardPriority == 0) } then {
	set path [Consider_GetPath [string trim [lindex $lineList 0] <>]]
	if { [Consider_IgnoreElement $path] } then { return }
	CA_Set $path Lucky
    }

    if { $disconnectionsSinceUse > $ConsiderRemoving(NumberRecent) } then {
	set path [Consider_GetPath [string trim [lindex $lineList 0] <>]]
	if { [Consider_IgnoreElement $path] } then { return }
	CR_Set $path NotRecent
    }

    set totalDisconnections [expr $disconnectionsUsed + $disconnectionsUnused]
    if { $totalDisconnections > 0 } then {
	set percent [expr 100 * $disconnectionsUsed / $totalDisconnections]
    } else {
	set percent 0
    }
    if { ($percent < $ConsiderRemoving(PercentAccess)) && 
	 ($totalDisconnections >= $ConsiderRemoving(MinimumDatapoints)) } then {
	set path [Consider_GetPath [string trim [lindex $lineList 0] <>]]
	if { [Consider_IgnoreElement $path] } then { return }
	CR_Set $path NotFrequent
    }
    
}

proc Consider_ProcessVenusInput { filename } {

    SendToStdErr "Process VenusInput $filename"

    if { ![file exists $filename] } then { return }

    set headerLine "<FID> priority discosSinceLastUse discosUsed discosUnused "

    set VenusFILE [open $filename {RDONLY}]
    foreach line [split [read $VenusFILE] \n] {
	if { [string compare $line $headerLine] == 0 } then { continue }
	if { $line == "" } then { continue }
	if { [catch {Consider_ProcessVenusInputLine $line} result] } then {
	    SendToStdErr "while in Consider_ProcessVenusInputLine $line"
	    SendToStdErr "  $result"
	}
    }
    close $VenusFILE

    SendToStdErr "ProcessVenusInput $filename"
}


#
# N.B. For now, we will ignore the program and instance data.
# We may want to present this information to the user in the future,
# but for now I'd like to keep the interface in a clean tabular
# format.  We'll have to see if users suggest adding this information
# in practice.
#
proc Consider_ProcessMissInputLine { line } {
    global ConsiderAdding

    set lineList [split [string trim $line]]
    set path [lindex $lineList 0]
    set program [lindex $lineList 1]
    set instances [lindex $lineList 2]

    if { [Consider_IgnoreElement $path] } then { return }
    CA_Set $path Unlucky
}

proc Consider_ProcessMissInput { filename } {

    SendToStdErr "ProcessMissInput $filename"

    if { ![file exists $filename] } then { return }

    set MissFILE [open $filename {RDONLY}]
    foreach line [split [read $MissFILE] \n] {
	if { $line == "" } then { continue }
	Consider_ProcessMissInputLine $line
    }
    close $MissFILE

    SendToStdErr "ProcessMissList $filename"
}


proc Consider_ProcessReplaceInput { filename } {
    
    if { ![file exists $filename] } then { return }

    SendToStdErr "ProcessReplaceInput $filename"

    set ReplaceFILE [open $filename {RDONLY}]
    foreach line [split [read $ReplaceFILE] \n] {
	if { $line == "" } then { continue }
	if { [Consider_IgnoreElement $line] } then { return }
	CA_Set $line Refetch
    }
    close $ReplaceFILE

    SendToStdErr "ProcessReplaceInput $filename"
}
