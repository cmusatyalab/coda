
lappend auto_path /usr/mre/russianSave/coda-src/guitools
wm withdraw .
set Pathnames(newHoarding) /coda/usr/mre/newHoarding

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


Consider_ProcessVenusInput /tmp/usage3
#ConsiderAdding
#tkwait window $ConsiderAdding(MainWindow)
#update idletasks
#Consider_ProcessVenusInput /tmp/usage4 
ConsiderRemoving

