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

source ../OutsideWorld.tcl
source ../Helper.tcl
source ../ConsiderAdding.tcl
source ../ConsiderRemoving.tcl
source ../Consider.tcl



wm withdraw .
set Pathnames(newHoarding) /coda/usr/mre/newHoarding

set DisplayStyle(Centered) \
    [tixDisplayStyle text -font *times-medium-r-*-*-14* -background gray92 -anchor c]


set DisplayStyle(Header) \
    [tixDisplayStyle text \
	 -fg black -anchor c \
	 -padx 8 -pady 2 \
	 -font *times-bold-r-*-*-14* \
	 -background gray92]

set DisplayStyle(Normal) \
    [tixDisplayStyle text \
	 -fg black -anchor c \
	 -padx 8 -pady 2 \
	 -font *times-medium-r-*-*-14* \
	 -background gray92]



Consider_Init

Consider_ProcessVenusInput ./usage_statistics
ConsiderAdding
update idletasks
ConsiderRemoving

exit

