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
#!/usr/bin/tixwish-tk4.1

source ../tixCodaMeter.tcl

label .label -text "foo"
pack .label

tixCodaMeter .meter \
   -height 20 \
   -width 100 \
   -label "banana" \
   -state normal \
   -percentFull 50 \
   -foreground purple \
   -labelWidth 12 \
   -rightLabel "hello" 
pack .meter

