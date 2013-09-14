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
# Copyright (C) 2006 Cisco Systems, Inc.  All Rights Reserved.
# 
# Contributor(s): 
# 
# END LICENSE BLOCK
#
# $Id: eclipse_arch.tcl,v 1.2 2007/07/03 00:10:27 jschimpf Exp $
#
# compute the ECLiPSe architecture name using Tcl primitives
#

proc ec_arch {} {
    global tcl_platform
    switch -glob $tcl_platform(os) {
	Windows* {
	    return i386_nt
	}
	SunOS {
	    switch -glob $tcl_platform(osVersion) {
		4.*	{ return sun4 }
		5.* {
		    switch -glob $tcl_platform(machine) {
			sun4*	{ return sparc_sunos5 }
			i86pc	{
			    # This requires tcl8.4 or later:
			    switch -glob $tcl_platform(wordSize) {
				4 { return i386_sunos5 }
				8 { return x86_64_sunos5 }
			    }
			}
		    }
		}
	    }
	}
	Linux {
	    switch -glob $tcl_platform(machine) {
		alpha	{ return alpha_linux }
		x86_64	{ return x86_64_linux }
		i?86	{ return i386_linux }
	    }
	}
	Darwin {
	    switch -glob $tcl_platform(machine) {
		Power*	{ return ppc_macosx }
		i?86	{ return i386_macosx }
	    }
	}
    }
    error "Platform $tcl_platform(os) $tcl_platform(osVersion) ($tcl_platform(machine)) not supported"
}

# returns the platform that Tk is running under. The tricky part is that
# MacOS X can be using either the native Aqua or X11, and unfortunately
# there is apparently no way to tell before Tk 8.4 (winfo server . crashes 
# under Aqua!)
proc ec_tk_platform {} {
    global tcl_platform

    switch $tcl_platform(platform) {
	unix {
	    if {[info tclversion] >= 8.4} {
		return unix_[tk windowingsystem]
	    } else {
		# Tk < 8.4 does not have tk windowingsystem
		if {$tcl_platform(os) == "Darwin"} {
		    return unix_acqua  ;# just assume it is acqua
		} else {
		    return unix_x11
		}
	    }
	}
	windows {
	    return windows
	}
    }
}