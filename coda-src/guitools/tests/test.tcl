#!/usr/bin/tixwish-tk4.1

lappend auto_path /coda/usr/mre/russians/coda/coda-src/guitools

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

