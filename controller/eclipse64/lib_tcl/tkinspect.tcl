#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
#exec wish $0 ${1+"$@"}

# BEGIN LICENSE BLOCK
# Version: CMPL 1.1
#
# The contents of this file are subject to the Cisco-style Mozilla Public
# License Version 1.1 (the "License"); you may not use this file except
# in compliance with the License.  You may obtain a copy of the License
# at www.eclipse-clp.org/license.
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
# the License for the specific language governing rights and limitations
# under the License. 
# 
# The Original Code is  The ECLiPSe Constraint Logic Programming System. 
# The Initial Developer of the Original Code is  Cisco Systems, Inc. 
# Portions created by the Initial Developer are
# Copyright (C) 1999 - 2006 Cisco Systems, Inc.  All Rights Reserved.
# 
# Contributor(s): Kish Shen
# 
# END LICENSE BLOCK

## Inspect subterm Tcl/Tk interface to Eclipse
##
##
## By Kish Shen
##
## Last Update: 23 Oct 2000
##
## Notes on path: The path to a subterm is a list of individual argument
## positions. If the position is not a number, it is a specialised position
## that will is handled by the Prolog side for its special meaning, examples
## include attributes and structure arguments with fieldnames. It is also
## designed to allow lists to be handled specially. The Prolog side provides
## routine to handle the movement around (left, right, down) these special
## positions, as well as interpreting their meaning. Currently the special
## positions are:
## Pos=FieldName    Structure arguments with fieldnames
## Pos-Attribute    Attributes
## list(Nth)        Nth element of a list
## tail(Nth)        Tail of a list (at Nth position)
##
## Added support for follow-node y scroll
## Speeded up moving down structures in navigation panel by use of the new
## expandbranch hierarchy method (no intermediate updates). 
## Sept-Oct'00:
## Changed communications with ECLiPSe to be by ec_rpc goals only
## Display flat lists + number normal args

package provide tkinspect 1.0

#set tkinspectvalues(prdepth)    5
set tkinspectvalues(invocnum)  -1
set tkinspectvalues(twinhdef)   3
set tkinspectvalues(listdepth)  20
#set tkinspectvalues(symbols) 1

font create tkeclsmall -family helvetica -weight bold -size 10

proc tkinspect:Inspect_term_init {source} {
    global tkinspectvalues tcl_platform tkecl

    set iw .ec_tools.inspect
    if {[winfo exists $iw]} {
	tkinspect:RaiseWindow $iw
	return
    }

    set tkinspectvalues($iw,source) $source
    set tkinspectvalues($iw,obslabel) " "
    set initstatus [tkinspect:select_command $iw $source]
    if {![string match "ok" $initstatus]} {
	tk_messageBox -type ok -message "Invalid term: unable to start Inspector..."
	return
    }

    toplevel $iw ;#-height 160m -width 190m
    wm title $iw "ECLiPSe Inspector"
    wm minsize $iw 27 3

#    ec_inspect_init

#-----------------------------------------------------------------------
# Menu Bar setup
#-----------------------------------------------------------------------
    set mbar [menu $iw.menubar]
    $iw config -menu $mbar
    $mbar add cascade -label "Windows" -menu $mbar.win -underline 0
    $mbar add cascade -label "Options" -menu $mbar.opt -underline 0
    $mbar add cascade -label "Select" -menu $mbar.select -underline 0
    #    $mbar add command -label "Help" -underline 0 -command "tkecl:Get_helpfileinfo insp $iw"
#-----------------------------------------------------------------------
# Images setup
#-----------------------------------------------------------------------
    set atomimage [tkinspect:CreateImage atom gif]
    set varimage [tkinspect:CreateImage var gif]
    set attrimage [tkinspect:CreateImage attr gif]
    set numimage [tkinspect:CreateImage number ppm]
    set stringimage [tkinspect:CreateImage string gif]
    set structureimage [tkinspect:CreateImage struct gif]

    set upimage [tkinspect:CreateImage uparrow ppm]
    set downimage [tkinspect:CreateImage downarrow ppm]
    set leftimage [tkinspect:CreateImage leftarrow ppm]
    set rightimage [tkinspect:CreateImage rightarrow ppm]
    set alertimage [tkinspect:CreateImage alert gif]
    set suspimage [tkinspect:CreateImage suspended gif]
    set schedimage [tkinspect:CreateImage scheduled gif]
    set deadimage [tkinspect:CreateImage  dead gif]
    set blankimage [tkinspect:CreateImage blank gif]

    
    set tkinspectvalues(atomimage) $atomimage
    set tkinspectvalues(varimage) $varimage
    set tkinspectvalues(attrimage) $attrimage
    set tkinspectvalues(numimage) $numimage
    set tkinspectvalues(structureimage) $structureimage
    set tkinspectvalues(stringimage) $stringimage
    set tkinspectvalues(deadimage) $deadimage
    set tkinspectvalues(schedimage) $schedimage
    set tkinspectvalues(suspimage) $suspimage

    set tkinspectvalues(upimage) $upimage
    set tkinspectvalues(downimage) $downimage
    set tkinspectvalues(leftimage) $leftimage
    set tkinspectvalues(rightimage) $rightimage
    set tkinspectvalues(alertimage) $alertimage
    set tkinspectvalues(blankimage) $blankimage


#-------------------------------------------------------------------------
# Frames setup
#-------------------------------------------------------------------------

    set top [frame $iw.top -height 300]
#        set nav [toplevel $top.n]
#        wm resizable $nav 0 0
#          set cf [frame $nav.control]
#          set info [frame $nav.info]

       set tkinspectvalues($top.m,iw) $iw

       ;# $top.m should be $h. Need to set var. before widget is called :-(
       set h [hierarchy $top.m -root 1 -browsecmd tkinspect:Get_subterms \
	    -nodelook tkinspect:Look_term -expand 1 -selectmode single \
	    -paddepth 40 -padstack 6 -selectcmd tkinspect:Display \
	    -command tkinspect:MayCentre -background white \
	    -selectbackground gray]
       set tf [frame $top.tf] 
    set bot [frame $iw.bot]
       set alf [frame $bot.alert]


#------------------------------------------------
# Frame packings
#------------------------------------------------
    pack $top -side top -expand true -fill both

        pack $h -expand true -fill both -side left
        pack $tf -expand true -fill both
#        pack $nav -side right 
#         wm title $nav "Navigation control"
#            pack $cf -side top
    pack $bot -side bottom -fill x
        pack [button $bot.exit -text "Close" -command "tkinspect:Exit $iw"] \
		-expand true -side bottom -fill x
        pack $alf -side bottom -expand true -fill x





#------------------------------------------------------------------------
# Details
#------------------------------------------------------------------------


# Alert frame---------------------------------
    set alert_text [text $alf.t -height 2 -relief groove -background lightgray]
    bind $alert_text <Any-Key> {break} ;# read only
    bind $alert_text <Any-Button> {break} ;# really read only!
    bind $alert_text <Any-Motion> {break}
    bind $alert_text <Any-ButtonRelease> {break} 
    bind $alert_text <Leave> {break}
    $alert_text tag configure bold -font tkecllabel -justify left

    ;#grid [label $alf.l -image $alertimage] -row 0 -column 0 -sticky w
    grid $alert_text -row 0 -column 1 -sticky news 
    grid rowconfigure $alf 0 -weight 1
    grid columnconfigure $alf 1 -weight 1

# Control frame------------------------------------
#    tkinspect:Deal_with_NavWin $top.n $iw $h $alert_text
#    tkinspect:PackCF $cf $iw $h $alert_text


# text frame-----------------------------------------------------
    set t [text $tf.t -setgrid true -relief sunken -height $tkinspectvalues(twinhdef)\
	    -yscrollcommand "$tf.y set" -background white]
#    set tkinspectvalues($t,twinheight) $tkinspectvalues(twinhdef)
    scrollbar $tf.y -orient vert -command "$t yview"
    pack $tf.y -side right -fill y
    pack $t -side right -fill both -expand true
     set tkinspectvalues($h,twin) $t
    bind $t <Any-Key> {break}   ;# read only
    switch $tcl_platform(platform) {
	windows  { ;# strange fonts in windows!
	  $t tag configure highlight -foreground blue -underline true 
          $t tag configure phighlight -foreground darkgreen  
        }
	default {
	  $t tag configure highlight -foreground blue -font tkecllabel
          $t tag configure phighlight -foreground darkgreen \
		  -font tkecllabel
	}
    }
    $t tag configure truncated -background pink
    bind $t <ButtonRelease-2> {break}   ;# disable paste


# Menu bar-----------------------------------

    set tkinspectvalues(prdepthmlabel) "Print Depth:"
    set tkinspectvalues(ldepthmlabel)  "Structure Threshold:"

    menu $mbar.opt -postcommand "tkinspect:Update_optmenu $mbar.opt \
	    0 {$tkinspectvalues(prdepthmlabel)} tkecl(pref,inspect_prdepth) \
	    1 {$tkinspectvalues(ldepthmlabel)} tkecl(pref,inspect_ldepth)" \
         -tearoff 0

    menu $mbar.select 

    menu $mbar.win 


    $mbar.opt add command -label "$tkinspectvalues(prdepthmlabel) $tkecl(pref,inspect_prdepth) ..." \
	    -command "tkinspect:EditDepth $iw prdepth 0 100 {Inspector uses the global output mode for printing terms. Change it from the global settings.} {Print Depth}"
    $mbar.opt add command -label "$tkinspectvalues(ldepthmlabel) $tkecl(pref,inspect_ldepth) ..." \
	    -command "tkinspect:EditDepth $iw ldepth 1 100 {Parameter for displaying list specially/printing argument number for structures} {Structure Threshold}" 
    $mbar.opt add command -label "Display current path" \
	    -command "tkinspect:DisplayPath $h $t"
    $mbar.opt add check -label "Display Symbols" -variable tkecl(pref,inspect_nosymbols) -onvalue 0 -offvalue 1
    $mbar.opt add check -label "Balloon Help" -variable tkecl(pref,balloonhelp) -onvalue 1 -offvalue 0
    $mbar.select add command -label "Current goal" \
	    -command "tkinspect:SelectCurrent $iw $h $alert_text"
    $mbar.select add command -label "Invoked goal ..." \
	    -command "tkinspect:SelectInvoc $iw $h $alert_text"

    $mbar.win add command -label "Navigation panel" \
	    -command "tkinspect:Deal_with_NavWin $top.n $iw $h $alert_text"
    $mbar.win add command -label "Browser y-scroll control" \
	    -command "tkinspect:Deal_with_yscroll $top.ys $iw $h"
    $mbar.win add command -label "Symbol key" \
	    -command "tkinspect:DisplayKey $iw"
    $mbar.win add command -label "Inspector Help" \
	    -command "tkecl:Get_helpfileinfo insp $iw"
    $mbar.win add separator
    $mbar.win add command -label "Close this Inspector" \
	    -command "destroy $iw"

    pane $h $tf -orient vertical -initfrac [list 0.75 0.25]

proc tkinspect:Update_optmenu {m prdpos prdlabel prdvar ldpos ldlabel ldvar} {

    tkinspect:Update_menu $m $prdpos $prdlabel $prdvar
    tkinspect:Update_menu $m $ldpos $ldlabel $ldvar
}

#------------------------------------------------------------------
# Balloon Help
#------------------------------------------------------------------

#    balloonhelp $bot.exit "Exit from Inspector"
    balloonhelp $h "Term browser area - browsing and inspection of term\n \
      Double leftmouse click toggles node expansion\n Arrow keys move \
      selected term relative to current position.\n Right (or control-left) click on a term \
      brings up a menu with summary information and options. \n Note: cannot interact \
      with other ECLiPSe windows while inspector is active.\n\
      Y scrollbars defaults to `follow-node' mode - change with yscroll control panel."
    balloonhelp $tf "Text display area - printed form of term and path information"
    balloonhelp $alf "Message area - alert/error messages from Inspector"
    balloonhelp $top.__h1 "Press and drag mouse button 1 to adjust text/Term window sizes"

#-------------------------------------------------------------------
# Window bindings
#-------------------------------------------------------------------
    bind $iw <Alt-h> "tkecl:Get_helpfileinfo insp $iw"
    bind $iw <Key-Up> "tkinspect:Move $iw $h left $alert_text"
    bind $iw <Key-Left> "tkinspect:Move $iw $h up $alert_text"
    bind $iw <Key-Right> "tkinspect:MoveDown $iw $h $alert_text"
    bind $iw <Key-Down> "tkinspect:Move $iw $h right $alert_text"
    bind $h <Button-3> "tkinspect:Popup_menu $iw $h %X %Y %x %y $alert_text"
    bind $h <Control-Button-1> "tkinspect:Popup_menu $iw $h %X %Y %x %y $alert_text"

#-------------------------------------------------------------------
# Initialisations
#-------------------------------------------------------------------


    set tkinspectvalues($iw,movesize) 1
    set tkinspectvalues($iw,argpos) 2  ;# default for tail of list
    set hroot [$h indexnp 1]
    foreach {yfollow le re te be} [$h yfollowstate] {
	set tkinspectvalues(yfollow,$iw) $yfollow
	set tkinspectvalues(ysle,$iw) [expr round($le*100)]
	set tkinspectvalues(ysre,$iw) [expr round($re*100)]
	set tkinspectvalues(yste,$iw) [expr round($te*100)]
	set tkinspectvalues(ysbe,$iw) [expr round($be*100)]
    }
    $h selection set $hroot ;# select top-level
    tkinspect:Display $h $hroot {} 
    if {![winfo viewable $iw]} {
	tkwait visibility $iw   ;# the grab may sometimes happen before visibility 
    }
    grab $iw

}



proc tkinspect:PackYS {iw h ys} {
    global tkinspectvalues

    set bf [frame $ys.b]
    pack [button $bf.update -text "Update" -command "tkinspect:Update_ys $iw $h"] \
	    -side left -expand true -fill x
    pack [button $bf.close -text "Close" -command "destroy $ys"] \
	    -side right -expand true -fill x
    pack $bf -side bottom -fill x

    set yff [frame $ys.yff -borderwidth 3 -relief ridge]
    pack [checkbutton $yff.yfollow -text "Viewable x area follows node on y scroll" -variable tkinspectvalues(yfollow,$iw)] 
    pack $yff -side top -expand true -fill x

    set xf [frame $ys.xef -borderwidth 3 -relief ridge]
    grid [label $xf.l -text "Centre when leading node is:"] -row 0 \
	    -column 0 -columnspan 2 -sticky news
    grid [scale $xf.le -label "% from left edge" -from 0 -to 50 -length 200 \
            -orient horizontal -tickinterval 25 -variable \
            tkinspectvalues(ysle,$iw)] -row 1 -column 0 -sticky news
    grid [scale $xf.re -label "% from right edge" -from 0 -to 50 -length 200 \
	    -orient horizontal -tickinterval 25 -variable \
            tkinspectvalues(ysre,$iw)] -row 1 -column 1 -sticky news
    pack $xf -side bottom -expand true

    set yf [frame $ys.xf -borderwidth 3 -relief ridge]
    grid [label $yf.l -text "Leading node is:"] -row 0 -column 0 \
	    -columnspan 2 -sticky news
    grid [scale $yf.te -label "% from top edge" -from 0 -to 50 -length 200 \
	    -orient horizontal -tickinterval 25 -variable \
	    tkinspectvalues(yste,$iw)] -row 1 -column 0 -sticky news
    grid [scale $yf.be -label "% from bottom edge" -from 0 -to 50 -length 200 \
	    -orient horizontal -tickinterval 25 -variable \
	    tkinspectvalues(ysbe,$iw)] -row 1 -column 1 -sticky news
    pack $yf -side bottom -expand true

    balloonhelp $yff "Enable or disable follow-node mode for y scrollbar in term browser area."
    balloonhelp $xf "These scales control the placement of the boundaries in the x viewable area of the term browser area.\nThe leading node will be displayed within these boundaries, by adjustment of the positioning of y viewable area,\n when the y scrollbar is manipulated in follow-node mod. Expressed as % of viewable width"
    balloonhelp $yf "These scales control the position of where the leading node will be selected in the term browser area.\nExpressed as % of viewable height."
    balloonhelp $bf.update "Update y scroll parameters - note that these parameters are not updated until this button is clicked."
    balloonhelp $bf.close "Close this window"
}

proc tkinspect:Update_ys {iw h} {
    global tkinspectvalues

    ;# .0 forces real arithmatic...
    set le [expr $tkinspectvalues(ysle,$iw).0/100]
    set re [expr $tkinspectvalues(ysre,$iw).0/100]
    set te [expr $tkinspectvalues(yste,$iw).0/100]
    set be [expr $tkinspectvalues(ysbe,$iw).0/100]
    $h yfollowitem  $le $re $te $be
	     
    if {!$tkinspectvalues(yfollow,$iw)} {
	$h ynofollowitem
    }
}

# Packing of control frame moved out as procedure because it is needed to
# reconstruct frame if it was destroyed.
proc tkinspect:PackCF {cf iw h alert_text} {
    global tkinspectvalues

    set upimage $tkinspectvalues(upimage) 
    set downimage $tkinspectvalues(downimage) 
    set leftimage $tkinspectvalues(leftimage) 
    set rightimage $tkinspectvalues(rightimage) 

    grid [button $cf.up -image $upimage -relief raised \
	    -command "tkinspect:Move $iw $h left $alert_text"] -row 1 -column 2 
    grid [button $cf.left -image $leftimage -relief raised \
	    -command "tkinspect:Move $iw $h up $alert_text"] -row 2 -column 1
    grid [button $cf.down -image $downimage -relief raised \
	    -command "tkinspect:Move $iw $h right $alert_text"]  -row 3 -column 2 
    grid [label $cf.downlabel -font tkeclsmall -text "right sibling" -justify center] -row 4 -column 1 -columnspan 3 -sticky news
    grid [label $cf.uplabel -font tkeclsmall -text "left sibling" -justify center] -row 0 -column 1 -columnspan 3 -sticky news
    grid [label $cf.leftlabel -font tkeclsmall -text "parent"] -row 2 -column 0

    grid [frame $cf.nentry] -row 2 -column 2
       grid [label $cf.nentry.l -font tkeclsmall -text "repeat"] -row 0 -column 0
       tkinspect:Numentry $cf.nentry.e {} tkinspectvalues($iw,movesize) 1 1 2 1 0 3
    grid [frame $cf.f -borderwidth 3 -relief ridge] -row 2 -column 3 -columnspan 2 -sticky news 

       grid [button $cf.f.right -image $rightimage -relief raised \
	    -command "tkinspect:MoveDown $iw $h $alert_text"]  \
	    -row 0 -column 0 
       grid [frame $cf.f.aentry] -row 0 -column 1
          grid [label $cf.f.aentry.l -font tkeclsmall -text "child#"] -row 0 -column 0
          tkinspect:Numentry $cf.f.aentry.e {} tkinspectvalues($iw,argpos) 2 1 2 1 0 3

    bind $cf.f.aentry <Return> "tkinspect:MoveDown $iw $h $alert_text"

    balloonhelp $cf "Navigation control panel - move selection relative to currently selected term"
    balloonhelp $cf.up "Move to left sibling term"
    balloonhelp $cf.left "Move to ancestral term"
    balloonhelp $cf.f.right "Move to a descendant term"
    balloonhelp $cf.down "Move to right sibling term"
    balloonhelp $cf.nentry "Repeat count -- number of times movement is repeated"
    balloonhelp $cf.f.aentry "Argument position of descendant term move"
}


proc tkinspect:CreateImage {name format} {
    global tkecl
    return [image create photo -format $format -file [file join $tkecl(ECLIPSEDIR) lib_tcl Images $name.$format]]
}

proc tkinspect:MarkLevels {n np t} {

	$t delete 1.0 end
	$t insert end "remaining levels: $n"
        update idletasks
}
    
proc tkinspect:MoveDown {iw h t} {
    global tkinspectvalues

    set cur_idx [tkinspect:CurrentSelection $h]
    set current [lindex [$h get $cur_idx] 0]
    set downlevels [tkinspect:Get_numentry tkinspectvalues($iw,movesize) 1]
    set arg [tkinspect:Get_numentry tkinspectvalues($iw,argpos) 2]    
    foreach {status left np} [$h expandbranch $current $arg $downlevels \
	    [list tkinspect:MarkLevels $t]] {
	break
    }
    $t delete 1.0 end
    if {!$status} {
	set n [expr $downlevels - $left]
	$t insert end "Out of range after traversing down $n levels at \
		argument position $arg" bold
	bell
    } 
    set cur_idx [$h indexnp $np]
    tkinspect:Newselection $h $cur_idx
    $h centreitem $cur_idx 1.0 0.0 1.0 0.0 ;# always centre
}


# move to a new selection, clear old selection and display new one
proc tkinspect:Newselection {h cur_idx} {
    set selected [$h curselection]
    $h selection clear
    $h selection set $cur_idx 
    ;# $h see $cur_idx
    tkinspect:Display $h $cur_idx $selected
}


proc tkinspect:Numentry {w ltext tvar default matchtype width row col pad} {
    upvar #0 $tvar entryvar

    set entryvar $default
    switch -exact -- $matchtype {
	-1 { ;# can take negative integers
	    set vstring {regexp {^-?[0-9]*$} %P}
	}

	0  {;# can take zero + positive integers
	    set vstring {regexp {^[0-9]*$} %P}
	}

	1  {;# can take positive integers
	    set vstring {regexp {^([1-9][0-9]*|[1-9]?)$} %P}
	}
    }
    set entry [ventry $w -labeltext $ltext -vcmd $vstring \
       -validate key -invalidcmd bell -relief sunken -width $width \
       -textvariable $tvar -selectbackground red]
    grid  $entry -row $row -column $col -padx $pad -pady $pad -sticky news
}


proc tkinspect:Move {iw h dir t} {
    global tkinspectvalues

    set current [tkinspect:CurrentSelection $h]
    foreach {status current}  [tkinspect:move_command $iw $dir $current] {break}
    ;# current is now new current subterm's path
    set index [$h indexnp $current]

    tkinspect:Newselection $h $index
    $h centreitem $index 1.0 0.0 1.0 0.0 ;# always centre
    if {[string match $status "true"]} {
	$t delete 1.0 end
    } else {
	$t delete 1.0 end 
	$t insert end "Out of range. Stop before move completed" bold
	bell
    }
}


proc tkinspect:Exit {iw} {
    global tkinspectvalues

    tkinspect:inspect_command $tkinspectvalues($iw,source) [list end] {}
    destroy $iw
}


proc tkinspect:Get_subterms {hw path} {
    global tkinspectvalues

    set iw $tkinspectvalues($hw,iw)
    foreach {termtype arity} [tkinspect:Get_subterm_info $iw $path \
	    termname summary] {break}
    return [tkinspect:Expand_termtype $iw $path $termtype $termname $arity $summary]
}


proc tkinspect:Expand_termtype {iw path termtype termname arity summary} {
    global tkinspectvalues

    ;# code changed so that ECLiPSe now does the expansion, always.
    return [tkinspect:inspect_command $tkinspectvalues($iw,source) [list childnodes $termtype $arity \
       [tkinspect:Get_numentry tkecl(pref,inspect_ldepth) 20] $path] {()II[S*]}]
}

proc tkinspect:Look_term {hw np isopen} {
    global tkinspectvalues tkecl

    set iw $tkinspectvalues($hw,iw)
    foreach {termtype arity} [tkinspect:Get_subterm_info $iw $np termname0 \
	    summary0] {break}
    set lastpos [lindex $np end]
    set modifier [tkinspect:Modify_name $iw $lastpos]
    if {[string match modifier {}]} {
	set termname $termname0
	set summary $summary0
    } else {
	append termname $modifier $termname0
	append summary  $modifier $summary0
    }
    ;# modify name text string if required (e.g. at top-level of attribute)
    ;# now truncate string if too long...
    if {[string length $termname] >= $tkecl(pref,text_truncate)} {
	set termname [string range $termname 0 $tkecl(pref,text_truncate)]
	append termname "..."
    }
    switch -exact -- $termtype {
	db_reference -
	handle      {
	    set image [tkinspect:DisplayImage $hw blankimage]
	    return [list $termname {} $image {black}]
	}

	exphandle      {
	    set image [tkinspect:DisplayImage $hw blankimage]
	    if {$isopen == 1} {
		set font {-family fixed}
		set colour black
	    } else {
		set font {-family fixed -weight bold}
		set colour red
	    }
	    return [list $termname $font $image $colour]
	}

	atom {
	    set image [tkinspect:DisplayImage $hw atomimage]
	    return [list $termname {}  $image {black}]
	}

	list -
	ncompound -
	compound {
	    set image [tkinspect:DisplayImage $hw structureimage]
	    if {$isopen == 1} {
		set retname $summary
		set colour black
		set font tkeclmono
	    } else {
		set retname $termname
		set colour red
		set font tkecllabel
	    }
	    return [list $retname $font $image $colour]
	}

	var {
	    set image [tkinspect:DisplayImage $hw varimage]
	    return [list $termname {} $image {black}]
	}

	attributed {
	    set image [tkinspect:DisplayImage $hw attrimage]
	    if {$isopen == 1} {
		set font {-family fixed}
		set colour black
	    } else {
		set font {-family fixed -weight bold}
		set colour red
	    }
	    return [list $termname $font $image $colour]
	}

	float    -
	rational -
	breal    -
	integer {
	    set image [tkinspect:DisplayImage $hw numimage]
	    return [list $termname {} $image {black}]
	}

	string {
	    set image [tkinspect:DisplayImage $hw stringimage]
	    return [list $termname {} $image {black}]
	}

	suspended {
	    set image [tkinspect:DisplayImage $hw suspimage]
	    if {$isopen == 1} {
		set font {-family fixed}
		set colour black
	    } else {
		set font {-family fixed -weight bold}
		set colour red
	    }
	    return [list $termname $font $image $colour]
	}

	scheduled {
	    set image [tkinspect:DisplayImage $hw schedimage]
	    if {$isopen == 1} {
		set font {-family fixed}
		set colour black
	    } else {
		set font {-family fixed -weight bold}
		set colour red
	    }
	    return [list $termname $font $image $colour]
	}


	dead {
	    set image [tkinspect:DisplayImage $hw deadimage]
	    return [list $termname {} $image {black}]
	}

	default     { 
	    tk_messageBox -icon warning -type ok -message "Unknown subterm type `$termtype'. Please report this problem (follow the links in About this ECLiPSe)"
	    set image [tkinspect:DisplayImage $hw blankimage]
	    return [list $termname {} $image {black}]
	}

    }
}


proc tkinspect:DisplayImage {hw itype} {
    global tkinspectvalues tkecl

    if {$tkecl(pref,inspect_nosymbols) == 0} {
	set image $tkinspectvalues($itype)
    } else {
	set image $tkinspectvalues(blankimage)
	;# get around bug with hierarchy widget
    }
}

proc tkinspect:info_command {iw np} {
    global tkecl tkinspectvalues

    return [tkinspect:inspect_command $tkinspectvalues($iw,source) \
       [list info [tkinspect:Get_numentry tkecl(pref,inspect_prdepth) 6] $np] \
       {I[S*]}]
}

proc tkinspect:move_command {iw dir np} {
    global tkecl tkinspectvalues

    return [tkinspect:inspect_command $tkinspectvalues($iw,source) \
       [list movepath $dir [tkinspect:Get_numentry \
        tkinspectvalues($iw,movesize) 1] $np]  {()I[S*]}]
}

proc tkinspect:select_command {iw source} {
    global tkecl tkinspectvalues

    return [tkinspect:inspect_command $tkinspectvalues($iw,source) [list select $source] S]
}


proc tkinspect:inspect_command {source command type} {

    return [lindex [ \
      lindex [ec_rpc [list : tracer_tcl \
        [list inspect_command $source $command _]] (()(S($type)_))\
      ] 2 \
    ] 3]
}

proc tkinspect:Get_subterm_info {iw np subname sumname} {
    global tkinspectvalues tkecl
    upvar $subname subterm
    upvar $sumname summary

    foreach {subterm summary type arity} [tkinspect:info_command $iw $np] {break}
    return "$type $arity"
}



proc tkinspect:MayCentre {h np isopen} {
    global tkinspectvalues

    set iw $tkinspectvalues($h,iw)
    set current [tkinspect:CurrentSelection $h]
    if {![string match "" $current]} {
	;# for some reason, sometimes CurrentSelection does not return
	;# a correct selection...
	$h centreitem $current 0.05 0.85 0.0 0.75
    }
}

proc tkinspect:Display {h selected prevsel} {
    global tkinspectvalues tkecl

    set iw $tkinspectvalues($h,iw) 
    ;# need to get iw (inspect window) this way because proc. may be called
    ;# by hierarchy widget where iw is not one of the arguments.

    if {[lsearch $prevsel $selected] != -1} {return 0} 
    ;# return immediately if previously selected
    set tw $tkinspectvalues($h,twin)
    set current [tkinspect:CurrentSelection $h]

    set wake 0
    after 500 {set wake 1}
    vwait wake
    
    if {![string match "" $current]} {
	$h centreitem $current 0.05 0.9 0.1 0.8
	foreach {subterm summary type arity } \
		[tkinspect:info_command $iw $current] {break}
	if {[string length $subterm] >= $tkecl(pref,text_truncate)} {
	    set subterm [string range $subterm 0 $tkecl(pref,text_truncate)]
	    set truncated 1
	} else {
	    set truncated 0
	}

	if {[string match *compound $type]} {
	    if {[$h isopen $current]} {
		set printedterm $subterm
	    } else {
		set printedterm $summary
		set truncated 0
	    }
	} else {
	    set printedterm $subterm
	}
	$tw tag remove highlight 1.0 end
	$tw tag remove phighlight 1.0 end
	$tw insert end "\n\n"
	$tw insert end  $printedterm highlight
	if $truncated {
	    $tw insert end "..." truncated
	}
	$tw yview moveto 1.0

	return 1
    } else {
	return 0
    }

}



proc tkinspect:Get_numentry {tvar default} {
    upvar #0 $tvar numvar

    if {[string length $numvar] == 0} {set numvar $default}
    return $numvar
}

#------------------------------------------------------------



package require AllWidgets

proc tkinspect:ec_resume_inspect {} {
    ec_rpc_check tracer_tcl:inspect_term
}


proc tkinspect:EditDepth {iw type min max message typetext} {
    global tkecl 

    set w $iw.predit$type
    if {[winfo exists $w]} {
	tkinspect:RaiseWindow $w
    } else {
	set oldprdepth $tkecl(pref,inspect_$type)

	toplevel $w
	wm title $w "Change Inspector's $typetext"
	wm resizable $w 1 0
	message $w.m -justify center -aspect 1000 -text $message
	pack [frame $w.f] -expand 1 -fill both -side bottom
	pack [frame $w.f.b] -side bottom -expand 1 -fill both
	pack [label $w.f.label -text $typetext] -side left
	pack [ventry $w.f.e -vcmd {regexp {^([1-9][0-9]*|[1-9]?)$} %P} -validate key -invalidcmd bell -relief sunken -width 4 -textvariable tkecl(pref,inspect_$type)] -side left
	pack [scale $w.f.scale -from $min -to $max -orient horizontal \
		-tickinterval 10 -length 60m -sliderlength 4m \
		-variable tkecl(pref,inspect_$type)] -expand 1 -fill x
	pack [button $w.f.b.exit -command "destroy $w" -text "Done"] -side left -expand 1 -fill both
	pack [button $w.f.b.cancel -text "Cancel" -command "set tkecl(pref,inspect_$type) $oldprdepth ; destroy $w"] -side right -expand 1 -fill both
	pack $w.m -side top -expand 1 -fill both 
    }
}


proc tkinspect:Popnumentry {iw tvar default valname exclusive} {
    upvar #0 $tvar numvar

    if {[string match [grab current $iw] $iw]} {
	set hasgrab 1
    } else {
	set hasgrab 0
    }
    set origval $numvar
    set popup $iw.numpop$tvar ;# $tvar should be in a appendable format! 
    if {[winfo exists $popup]} {
	tkinspect:RaiseWindow $popup
	return
    }
    toplevel $popup
    wm title $popup "Change $valname"
    if {$exclusive == 1} {
	tkwait visibility $popup
	grab $popup
    }
    tkinspect:Numentry $popup.entry {New Value: } $tvar $default 0 4 0 0 0
    ;# zero or positive numbers acceptable
    set getvalue "tkinspect:Get_numentry $tvar $default; destroy $popup"
    $popup config -borderwidth 5
    frame $popup.enter -borderwidth 2 -relief sunken

    pack [button $popup.enter.b -text "Finish" -command $getvalue] \
	    -padx 1 -pady 1
    grid $popup.enter -row 1 -column 0

    grid [button $popup.cancel -text "Cancel" -command "destroy $popup; \
	    set $tvar $origval"] -row 1 -column 2
    bind $popup.entry <Return> $getvalue

    if {$exclusive == 1} {
	tkwait window $popup
	if {$hasgrab} {grab $iw}
    }
}


proc tkinspect:Update_menu {m mindex mtext mvar} {
    
    upvar #0 $mvar numvar

    set mlabel "$mtext: $numvar ..."
    $m entryconfigure $mindex -label $mlabel
}



proc tkinspect:DisplayKey {iw} {
    global tkinspectvalues

    if {[winfo exists $iw.inspectkey]} {
	tkinspect:RaiseWindow $iw.inspectkey
	return
    }
    set keywin [toplevel $iw.inspectkey] 
    wm title $keywin "Key to Symbols"
    set i 0
    foreach {imagename type} "$tkinspectvalues(atomimage) atom \
	    $tkinspectvalues(varimage) variable $tkinspectvalues(attrimage) \
	    {attributed variable} $tkinspectvalues(numimage) number \
	    $tkinspectvalues(structureimage) structure \
	    $tkinspectvalues(stringimage) string $tkinspectvalues(suspimage) \
	    {sleeping suspension} $tkinspectvalues(schedimage) {scheduled \
	    suspension} $tkinspectvalues(deadimage) {dead suspension}" {
	grid [label $keywin.i$type -image $imagename] -row $i -column 1 
	grid [label $keywin.$type -text $type -justify left \
		-font {times 14 bold}] -row $i -column 0 -sticky w 
	incr i 1
    }
    grid [button $keywin.exit -text "Dismiss" \
	    -command "destroy $keywin"] -row $i -column 0 -columnspan 2
}


proc tkinspect:SelectCurrent {iw h t} {
    global tkinspectvalues

    tkinspect:PostSelect $iw $h $t current [tkinspect:select_command $tkinspectvalues($iw,source) current]

}


proc tkinspect:SelectInvoc {iw h t} {
    global tkinspectvalues

    tkinspect:Popnumentry $iw tkinspectvalues(invocnum) 1 "Goal invocation number" 1
    set source invoc($tkinspectvalues(invocnum))
    tkinspect:PostSelect $iw $h $t $source [tkinspect:select_command $iw $source]

}


proc tkinspect:PostSelect {iw h t source status} {
    global tkinspectvalues

    if {[string match $status "ok"]} {
	$t delete 1.0 end
	set hroot [$h index root]
	$h close $hroot
	set tkinspectvalues($iw,source) $source ;# set only after close!
	$h open $hroot  ;# update root term. Also consistent with initial display
	$h selection set $hroot
	tkinspect:Display $h $hroot {}
    } else {
	$t delete 1.0 end
	$t insert end "Inspected term not changed due to invalid \
		specification." bold
	bell
    }
}



# Pass the last argument position back to Prolog to see if term name should be
# modified, e.g. to indicate attribute (another possibility is structure
# field names. This might be done in Tcl, but want to leave all interpretation
# of path position to Prolog
proc tkinspect:Modify_name {iw lastpos} {
    global tkinspectvalues

    if {[regexp {^[0-9]+$} $lastpos] == 0} {
	;# goto Prolog if not a pure number
	return [tkinspect:inspect_command $tkinspectvalues($iw,source) [list modify $lastpos] S]
    } else {
	return {}
    }
}


proc tkinspect:CurrentSelection {h} {
    ;# we only allow one selected item at a time. so get first element only
    return [lindex [$h get [$h curselection]] 0] 
}


proc tkinspect:DisplayPath   {h t} {
    global tkinspectvalues

    set iw $tkinspectvalues($h,iw) 
    set current [tkinspect:CurrentSelection $h]

    ;# do not remove highlight from previously displayed goal because that is
    ;# the selected goal for which this is the valid path
    $t insert end "\n"
    $t insert end "path:  "
    set path [$h get index $current]
    tkinspect:TkOutputPath [list $current] $t $iw
    $t yview moveto 1.0
}


# Raise window w
proc tkinspect:RaiseWindow {w} {
    if {![winfo exists $w]} {
	return 0
    } else {
	wm deiconify $w
	raise $w
	return 1
    }
}

proc tkinspect:TkOutputPath {rawpath tw iw} {
    global tkinspectvalues

    set path [lrange [lindex $rawpath 0] 1 end] ;# miss out top-level 1

    $tw insert end "{  " phighlight
    foreach pos $path {
	if [regexp {^[0-9]+$} $pos match] {
	    ;# A simple number, no need for special handling
	    $tw insert end $pos phighlight
	    $tw insert end "  "
	} else {
	    ;# let Prolog handle the appearance of special path positions
	    
	    $tw insert end [tkinspect:inspect_command \
                    $tkinspectvalues($iw,source) [list translate $pos] S] phighlight
	    $tw insert end "  "

	}
    }
    $tw insert end "}\n" phighlight

}


proc tkinspect:Deal_with_NavWin {nav iw h alert_text} {
    if {![tkinspect:RaiseWindow $nav]} {
	toplevel $nav
        wm resizable $nav 0 0
        wm title $nav "Navigation control"
          set cf [frame $nav.control]
          pack $cf -side top
	tkinspect:PackCF $cf $iw $h $alert_text
    }
}


proc tkinspect:Deal_with_yscroll {ys iw h} {
    if {![tkinspect:RaiseWindow $ys]} {
	toplevel $ys
        wm resizable $ys 0 0
        wm title $ys "Inspector YScroll Control Panel "
	tkinspect:PackYS $iw $h $ys
    }
}

proc tkinspect:helpinfo {iw topic filename key} {
    global tkecl

    set w $iw.helpwindow$key
    if [tkinspect:RaiseWindow $w] {
	return
    }
    toplevel $w
    wm title $w "$topic Help"
    set t [text $w.t -relief groove -background lightgray -width 80 \
	    -yscrollcommand "$w.y set"]
    scrollbar $w.y -orient vert -command "$t yview"
    pack $w.y -side right -fill y
    pack [button $w.exit -text "Close" -command "destroy $w"] \
	    -side bottom -fill x
    pack $t -expand true -fill both
    bind $t <Any-Key> {break} ;# read only
    bind $t <Any-Button> {break} ;# really read only!
    bind $t <Any-Motion> {break}
    bind $t <Any-ButtonRelease> {break} 
    bind $t <Leave> {break}
    set file [file join $tkecl(ECLIPSEDIR) lib_tcl $filename]
    if {![file exists $file]} {
	$t insert end "Help file for $topic is missing"
    } else {
	set input [open $file r]
	$t insert end [read $input]
	close $input
    }

}

#--------------------------------------------------
# Menu display
#--------------------------------------------------

proc tkinspect:Popup_menu {iw h x y cx cy t} {
# x and y are root-window relative co-ordinates (for placing menu)
# cx and cy are widget-relative co-ordinates (for knowing where mouse is)
# t is the alert text message box
    global tkinspectvalues

    if [winfo exists $h.hpopup] {
	destroy $h.hpopup
    }

    set idx [$h index @$cx,$cy]
    set np [lindex [$h get $idx] 0]
    set lastargpos [lindex $np end]
    foreach {subterm summary type arity} [tkinspect:info_command $iw $np] {
	break
    }
    set posinfo [tkinspect:inspect_command $tkinspectvalues($iw,source) [list translate $lastargpos] S]

    set m [menu $h.hpopup -tearoff 0]
    $m add command -label "$summary (type: $type, arg pos: $posinfo)" -state disabled
    $m add command -label "Observe this term" -command "tkinspect:Make_observed $iw {$np} $t"
    tk_popup $m $x $y
}

proc tkinspect:Make_observed {iw np t} {
    global tkinspectvalues

    toplevel $iw.o
    set tkinspectvalues($iw,obslabel) ""
    pack [label $iw.o.l -text "Please give a label for this observed term"] -side top
    pack [entry $iw.o.e -textvariable tkinspectvalues($iw,obslabel)] \
	    -side top -expand 1 -fill both
    bind $iw.o.e <Return> "destroy $iw.o"
    pack [button $iw.o.ok -text "OK" -command "destroy $iw.o"] \
	    -side left -expand 1 -fill x
    pack [button $iw.o.cancel -text "Cancel" -command "set tkinspectvalues($iw,obslabel) {Cancel Observed}; destroy $iw.o"]
    focus $iw.o.e
    balloonhelp $iw.o.l \
"If you are at a tracer debug port, a display matrix will be created with \n\
 this term which you have selected for observing."
    balloonhelp $iw.o.e "Enter the label you want to see next to the term in the display matrix"
    balloonhelp $iw.o.cancel "Click this button if you don't want to add the term for observation."
    tkwait window $iw.o

    $t delete 1.0 end
    if {$tkinspectvalues($iw,obslabel) != "Cancel Observed"} {
	tkinspect:inspect_command $tkinspectvalues($iw,source) \
          [list record_observed $tkinspectvalues($iw,source) $np \
           $tkinspectvalues($iw,obslabel)] {S[S*]S}

        $t insert end "Added term for observation with label $tkinspectvalues($iw,obslabel)" bold
    } else {
	$t insert end "Cancelled adding term for observation" bold
    }
}

