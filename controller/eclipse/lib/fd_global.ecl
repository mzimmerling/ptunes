% BEGIN LICENSE BLOCK
% Version: CMPL 1.1
%
% The contents of this file are subject to the Cisco-style Mozilla Public
% License Version 1.1 (the "License"); you may not use this file except
% in compliance with the License.  You may obtain a copy of the License
% at www.eclipse-clp.org/license.
% 
% Software distributed under the License is distributed on an "AS IS"
% basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
% the License for the specific language governing rights and limitations
% under the License. 
% 
% The Original Code is  The ECLiPSe Constraint Logic Programming System. 
% The Initial Developer of the Original Code is  Cisco Systems, Inc. 
% Portions created by the Initial Developer are
% Copyright (C) 1995 - 2006 Cisco Systems, Inc.  All Rights Reserved.
% 
% Contributor(s): Joachim Schimpf, Stefano Novello, Vassilis Liatsos,
%                 Mark Wallace, Andrew Sadler, IC-Parc
% 
% END LICENSE BLOCK
% ----------------------------------------------------------------------
% System:	ECLiPSe Constraint Logic Programming System
% Version:	$Id: fd_global.ecl,v 1.1.1.1.4.1 2009/02/19 05:45:20 jschimpf Exp $
%
%
% IDENTIFICATION:	fd_global.ecl
%
% AUTHORS:		Joachim Schimpf, IC-Parc, Imperial College, London
%			Stefano Novello, IC-Parc, Imperial College, London
%			Vassilis Liatsos, IC-Parc, Imperial College, London
%			Mark Wallace, IC-Parc, Imperial College, London
%                       Andrew Sadler, IC-Parc, Imperial College, London
%
% Specialise the generic code of generic_global_constraints.ecl to
% create the FD global constraints library.
% ----------------------------------------------------------------------

:- module(fd_global).

:- comment(categories, ["Constraints"]).
:- comment(summary, "Various global constraints over lists of FD variables").
:- comment(author, "J.Schimpf, V.Liatsos, S.Novello, M.Wallace, A.Sadler, IC-Parc").
:- comment(copyright, "Cisco Systems, Inc.").
:- comment(date, "$Date: 2009/02/19 05:45:20 $").

:- lib(fd).
:- use_module(fd_generic_interface).

:- include(generic_global_constraints).

