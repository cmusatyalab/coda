
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

