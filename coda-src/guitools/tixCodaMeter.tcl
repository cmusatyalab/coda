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

tixWidgetClass tixCodaMeter {
	-classname tixCodaMeter
	-superclass tixPrimitive
	-method {
	}
	-flag {
		-background -foreground -disabledForeground -height -width -labelWidth -label -labelColor -leftLabel -rightLabel -state -percentFull -color -highlightcolor 
	}
	-configspec {
		{-background background Background lightgray}
		{-foreground foreground Foreground black}
		{-disabledForeground disabledForeground DisabledForeground gray}
		{-height height Height 50}
		{-width width Width 200}
		{-labelWidth labelWidth LabelWidth 0}
		{-label label Label ""}
		{-labelColor labelColor LabelColor black}
		{-leftLabel leftLabel LeftLabel ""}
		{-rightLabel rightLabel RightLabel ""}
		{-state state State normal tixCodaMeter:CheckState}
		{-percentFull percentFull PercentFull 50 tixCodaMeter:CheckPercentFull}
		{-color color Color purple}
		{-hightlightcolor highlightcolor HighlightColor lightblue}
	}
	-alias {
		{-bg -background}
		{-fg -foreground}
	}
	-default {
		{*Meter.padX 5}
	}
}

proc tixCodaMeter:InitWidgetRec {w} {
    upvar #0 $w data

    tixChainMethod $w InitWidgetRec

    tixCodaMeter:InitWidgetRecHelper $w
}

proc tixCodaMeter:InitWidgetRecHelper {w} {
    upvar #0 $w data

    set data(nwX)  10
    set data(nwY) [expr 0.20 * $data(-height)]
    set data(seX) [expr $data(-width) - 10]
    set data(seY) [expr 0.60 * $data(-height)]
}

proc tixCodaMeter:ConstructWidget {w} {
    upvar #0 $w data

    tixChainMethod $w ConstructWidget

    set data(w:label) $w.label
    label $data(w:label) -anchor w -text $data(-label) \
	-foreground $data(-labelColor) -background $data(-background) \
	-width $data(-labelWidth) -highlightcolor $data(-highlightcolor)
    pack $data(w:label) -side left

    set data(w:canvas) $w.canvas
    canvas $data(w:canvas) \
	-height $data(-height) \
	-width $data(-width) \
	-background $data(-background) \
	-highlightcolor $data(-highlightcolor) \
	-relief raised 
    pack $data(w:canvas) -expand yes -fill both

    tixCodaMeter:ConstructWidgetHelper $w
}

proc tixCodaMeter:ConstructWidgetHelper {w} {
    upvar #0 $w data

    $data(w:canvas) delete leftLabel
    $data(w:canvas) create text $data(nwX) [expr $data(seY) + 5] \
	-text $data(-leftLabel) \
	-anchor nw \
	-justify left \
	-fill black \
	-tags leftLabel

    $data(w:canvas) delete rightLabel
    $data(w:canvas) create text $data(seX) [expr $data(seY) + 5] \
	-text $data(-rightLabel) \
	-anchor ne \
	-justify left \
	-fill black \
	-tags rightLabel

    $data(w:canvas) delete meter
    $data(w:canvas) create rectangle $data(nwX) $data(nwY) $data(seX) $data(seY) \
	-outline black \
	-tags meter

    set subwidth [expr [expr [expr $data(seX) - $data(nwX)] * $data(-percentFull)] / 100]

    $data(w:canvas) delete usage
    $data(w:canvas) create rectangle \
	[expr $data(nwX) + 1] [expr $data(nwY) + 1] \
	[expr $data(nwX) + $subwidth - 1] [expr $data(seY) - 1] \
	-outline black \
	-fill $data(-foreground) \
 	-tags usage

    $data(w:canvas) delete percentText
    if { $data(-percentFull) <= 50 } then {
        $data(w:canvas) create text \
	    [expr $data(nwX) + $subwidth + 13] \
	    [expr $data(nwY) + (($data(seY) - $data(nwY))/2)] \
	    -text [format "%d%%" $data(-percentFull)] \
	    -tags percentText
    } else {
        $data(w:canvas) create text \
	    [expr $data(nwX) + $subwidth - 13] \
	    [expr $data(nwY) + (($data(seY) - $data(nwY))/2)] \
	    -text [format "%d%%" $data(-percentFull)] \
	    -tags percentText
    }

    tixCodaMeter:config-state $w $data(-state)
}

proc tixCodaMeter:SetBindings {w} {
    upvar #0 $w data

    tixChainMethod $w SetBindings
}

#
# Configuration routines
#
proc tixCodaMeter:config-background {w value} {
    upvar #0 $w data

    $data(w:canvas) config -background $value
}

proc tixCodaMeter:config-foreground {w value} {
    upvar #0 $w data

    set data(-foreground) $value
    $data(w:canvas) itemconfigure usage -fill $value
    tixCodaMeter:config-state $w $data(-state)
}

proc tixCodaMeter:config-height {w value} {
    upvar #0 $w data

    set data(-height) $value
    $data(w:canvas) config -height $value
    tixCodaMeter:InitWidgetRecHelper $w
    tixCodaMeter:ConstructWidgetHelper $w
}

proc tixCodaMeter:config-width {w value} {
    upvar #0 $w data

    set data(-width) $value
    $data(w:canvas) config -width $value
    tixCodaMeter:InitWidgetRecHelper $w
    tixCodaMeter:ConstructWidgetHelper $w
}

proc tixCodaMeter:config-labelWidth {w value} {
    upvar #0 $w data

    $data(w:label) config -width $value
}

proc tixCodaMeter:config-label {w value} {
    upvar #0 $w data

    $data(w:label) config -text $value
}

proc tixCodaMeter:config-labelColor {w value} {
    upvar #0 $w data

    $data(w:label) config -foreground $value
}

proc tixCodaMeter:config-leftLabel {w value} {
    upvar #0 $w data

    $data(w:canvas) itemconfigure leftLabel -text $value
}

proc tixCodaMeter:config-rightLabel {w value} {
    upvar #0 $w data

    $data(w:canvas) itemconfigure rightLabel -text $value
}

proc tixCodaMeter:config-state {w value} {
    upvar #0 $w data

    if { $value == "normal" } then {
	$data(w:label) config -foreground $data(-labelColor)
        $data(w:canvas) itemconfigure rightLabel -fill black
	$data(w:canvas) itemconfigure leftLabel -fill black
        $data(w:canvas) itemconfigure usage -fill $data(-foreground)
	$data(w:canvas) itemconfigure usage -outline black
	$data(w:canvas) itemconfigure meter -outline black
    } else {
	$data(w:label) config -foreground $data(-disabledForeground)
        $data(w:canvas) itemconfigure rightLabel -fill $data(-disabledForeground)
	$data(w:canvas) itemconfigure leftLabel -fill $data(-disabledForeground)
	$data(w:canvas) itemconfigure usage -fill $data(-disabledForeground)
	$data(w:canvas) itemconfigure usage -outline $data(-disabledForeground)
	$data(w:canvas) itemconfigure meter -outline $data(-disabledForeground)
    }
}

proc tixCodaMeter:config-percentFull {w value} {
    upvar #0 $w data

    set data(-percentFull) $value
    tixCodaMeter:ConstructWidgetHelper $w
}

proc tixCodaMeter:config-command {w value} {
    upvar #0 $w data

    set data(-command) $value
    tixCodaMeter:ConstructWidgetHelper $w
}


proc tixCodaMeter:config-frequency {w value} {
    upvar #0 $w data

    set data(-frequency) $value
}


proc tixCodaMeter:CheckState {state} {

    if {[lsearch {normal disabled} $state] != -1} {
	return $state
    } else {
	error "bad state value \"$state\" (should be either \"normal\" or \"disabled\")"
    }
    return $state
}

proc tixCodaMeter:CheckPercentFull { percentFull } {
    if { [scan $percentFull "%d" value] != 1 } then {
        error "percentFull value must be an integer value"
    }

    if { $value < 0 } then {
	set returnvalue 0
    } elseif { $value > 100} {
	set returnvalue 100
    } else {
	set returnvalue $value
    }
    return $returnvalue
}

proc tixCodaMeter:IncrCount {w} {
    upvar #0 $w data

    incr data(count)
}

