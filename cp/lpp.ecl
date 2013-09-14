%
% Copyright 2013 ETH Zurich and SICS Swedish ICT 
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions
% are met:
% 1. Redistributions of source code must retain the above copyright
%    notice, this list of conditions and the following disclaimer.
% 2. Redistributions in binary form must reproduce the above copyright
%    notice, this list of conditions and the following disclaimer in the
%    documentation and/or other materials provided with the distribution.
% 3. Neither the name of the Institute nor the names of its contributors
%    may be used to endorse or promote products derived from this software
%    without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
% ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
% FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
% DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
% OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
% HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
% LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
% OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
% SUCH DAMAGE.
%

%
% author: Marco Zimmerling (zimmerling@tik.ee.ethz.ch)
%

:- module(lpp).

:- export struct(node(id,parent,children,f,fout,fqueuing,prr,vars,nodeLifetime,perHopLatency,perHopReliability,k,poneprobe,toneprobe,tbackoff)).

:- export(perHopReliability/1).
:- export(perHopLatency/1).
:- export(nodeLifetime/1).
:- export(queuingRate/1).
:- export(createVariables/1).
:- export(optimizedForReliabilityParameters/1).
:- export(solve/2).

:- use_module(constants).

:- lib(ic).
:- lib(lists).
:- lib(branch_and_bound).
:- lib(util).

:- pragma(nodebug).
:- pragma(expand).

%
% Creates per-hop reliability metric of LPP along the outgoing link
% of a given node and attaches it to that node.
%
perHopReliability(node{prr:Prout, vars:[_,Ts,N], parent:P, perHopReliability:R, k:K, poneprobe:Poneprobe}) :-
	( P == [] ->
		R $= 1.0
	;
		declareConstants([_,_,_,_,_,Tturn,_,_,_,_,_,_,_,Scale,Ton,Tpr,Trandmax,_,_]),
		K $= 1,
    		Poneprobe $= 1.0 - (1.0 - Prout)^K,
    		R $= 1.0 - (1.0 - Poneprobe*Prout)^(N+1)
	).

%
% Creates per-hop latency metric of LPP along the outgoing link
% of a given node and attaches it to that node.
%
perHopLatency(node{prr:Prout, vars:[Tl,Ts,N], parent:P, perHopLatency:L, k:K, poneprobe:Poneprobe, toneprobe:Toneprobe, tbackoff:Tbackoff}) :-
	( P == [] ->
		L $= 0.0
	;
		declareConstants([_,_,_,_,_,Tturn,_,_,Tdata,_,_,_,_,Scale,Ton,Tpr,Trandmax,Tdacktimeout,_]),
		% Number of failed transmission Nftx before successful transmission
    		Pf $= 1.0 - Poneprobe*Prout,
    		Nftx $= Pf/(1.0 - Pf) - (N+1)*Pf^(N+1)/(1.0 - Pf^(N+1)),
    		% Time needed for failed transmission Tftx
    		Ttxdata $= 2*Tturn + Tdata,
    		Tbackoff $= (Ton + Scale*Ts)*1.5,
    		Toneprobe $= Tpr + 0.5*(Ton + Scale*Ts + 0.5*Trandmax)*(Prout + K^2*(1.0 - Prout))/(Prout + K*(1.0 - Prout)),
    		Tftx $= (Toneprobe + Ttxdata + Tdacktimeout)*Poneprobe + Scale*Tl*(1.0 - Poneprobe) + Tbackoff,
    		% Time needed for successful transmission Tstx
    		Tstx $= Toneprobe + Ttxdata,
    		% Per-hop latency L.
    		L $= 0.5*(Ton + Scale*Ts) + Nftx*Tftx + Tstx
	).

%
% Creates node lifetime metric of LPP of a given node and
% attaches it to that node.
%
nodeLifetime(node{parent:P, children:C, f:F, fout:Fout, prr:Prout, vars:[Tl,Ts,N], nodeLifetime:T, k:K, poneprobe:Poneprobe, toneprobe:Toneprobe}) :-
	( P == [] ->
		T $= 1.0Inf
	;
		declareConstants([Ptx,Prx,Ps,U,Q,Tturn,_,_,Tdata,_,_,_,_,Scale,Ton,Tpr,Trandmax,Tdacktimeout,Tdack]),
		% Duty-cycle and packet reception
		( C == [] ->
			% Reception frequency is 0 for leaf nodes
    			Frx $= 0.0
    		;
   			( foreach(node{fout:Fin,prr:Prin}, C),
   		  	  fromto(Expr1, S1, S2, 0), param(N, K) do
    				Poneprobei $= 1.0 - (1.0 - Prin)^K,
    				Prtxi $= 1.0 - Poneprobei*Prin^2,
	    			Nrtxi $= Prtxi*(1.0 - Prtxi^N)/(1.0 - Prtxi),
    				Frxi $= (Nrtxi + 1)*Fin*Poneprobei*Prin,
   				S1 = Frxi + S2
    			),
   			Frx $= eval(Expr1)
    		),
    		Fdc $= 1.0/(Ton + Scale*Ts + 0.5*Trandmax),
		Trxdctx $= Tpr + Tdack*Frx/Fdc,
		Trxdcrx $= Ton - Trxdctx,
		% Packet transmission
   		Prtxout $= 1.0 - Poneprobe*Prout^2,
		Nrtxout $= Prtxout*(1.0 - Prtxout^N)/(1.0 - Prtxout),
   		Ftx $= (Nrtxout + 1)*Fout,
    		Don $= Ton*Fdc,
   		Ttxptx $= Poneprobe*Tdata,
    		Ttxprx $= Poneprobe*(Toneprobe*(1.0 - Don) + 4*Tturn + Prout^2*Tdack + (1.0 - Prout^2)*Tdacktimeout) + (1.0 - Poneprobe)*((Scale*Tl - 2*Tturn)*(1.0 - Don) + 2*Tturn),
		% Fractions of time in Tx, Rx, and idle mode
		Dtx $= Fdc*Trxdctx + Ftx*Ttxptx,
		Drx $= Fdc*Trxdcrx + Frx*Ttxprx,
		Didle $= 1.0 - Dtx - Drx,
		% Total power draw
		Ptotal $= Dtx*Ptx + Drx*Prx + Didle*Ps,
		% Node lifetime T
   		T $= Q*U/Ptotal
   	).

%
% Creates queuing rate and attaches it to that node. 
%
queuingRate(node{parent:P, vars:[Tl,_,N], fqueuing:Fqueuing, fout:Fout, prr:Prout, poneprobe:Poneprobe, toneprobe:Toneprobe, tbackoff:Tbackoff}) :-
	( P == [] ->
		Fqueuing $= -1.0Inf
	;
		declareConstants([_,_,_,_,_,Tturn,_,_,Tdata,_,_,_,_,Scale,Ton,_,_,Tdacktimeout,Tdack]),
		Prtxout $= 1.0 - Poneprobe*Prout^2,
		Nrtxout $= Prtxout*(1.0 - Prtxout^N)/(1.0 - Prtxout),
    		Ttx $= (Toneprobe + 4.0*Tturn + Tdata + Tdack*Prout^2 + (Tdacktimeout + Tbackoff)*(1.0 - Prout^2))*Poneprobe + (Scale*Tl + Tbackoff)*(1.0 - Poneprobe),
		Fforwarding $= 1.0/((Nrtxout + 1)*Ttx),
		Fqueuing $= Fout - Fforwarding
	).
   	
%
% Creates the decision variables (Tl, Ts, and N) with their associated
% (integer) domains. Attaches variables to each node.
%
createVariables([Tl,Ts,N]) :-
	declareConstants([_,_,_,_,_,Tturn,_,_,_,_,_,_,_,Scale,Ton,Tpr,Trandmax,_,_]),
	% for lifetime gain experiments: UBTs is integer(round(1.0/Scale)),
    	UBTs is integer(round(0.5/Scale)),
    	Fixed is integer(round((Ton + 0.5*Trandmax + Tpr + Tturn)/Scale)),
    	Ts #>= 0,
    	Ts #=< UBTs,
    	Tl #= Fixed + Ts,
    	N #>= 0,
    	N #=< 10.

%
% Protocol parameters geared toward reliability.
%    
optimizedForReliabilityParameters([Tl,Ts,N]) :-
	declareConstants([_,_,_,_,_,Tturn,_,_,_,_,_,_,_,Scale,Ton,Tpr,Trandmax,_,_]),
	Ts is 60,
	Tl is integer(round((Ton + Scale*Ts + 0.5*Trandmax + Tpr + Tturn)/Scale)),
	N is 10.    	

%
% Determines decisions variables such that cost function is minimized.
%	
solve(Cost, [_,Ts,N]) :-
	bb_min(
   		search([N,Ts],0,input_order,indomain_split,complete,[]),
   		(Cost),
    	bb_options{strategy:continue,delta:3600,factor:1.0,timeout:110,report_success:doNothing,report_failure:doNothing}
    ).

%
% Helper predicate to suppress output of bb_min.
%
doNothing(_,_,_).
