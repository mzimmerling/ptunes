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

:- module(xmac).

:- export struct(node(id,parent,children,f,fout,fqueuing,prr,vars,nodeLifetime,perHopLatency,perHopReliability,k,ponestrobe,niter,tmax,psack,tbackoff)).

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
% Creates per-hop reliability metric of X-MAC along the outgoing link
% of a given node and attaches it to that node.
%
perHopReliability(node{prr:Prout, vars:[Tl,_,N], parent:P, perHopReliability:R, k:K, ponestrobe:Ponestrobe}) :-
	( P == [] ->
		R $= 1.0
	;	
		declareConstants([_,_,_,_,_,_,Tstr,_,_,_,_,_,Titer,Scale,_,_,_,_,_]),
    		K $= (Scale*Tl - Tstr)/Titer,
    		Ponestrobe $= 1.0 - (1.0 - Prout)^K,
    		R $= 1.0 - (1.0 - Ponestrobe*Prout^2)^(N+1)
	).
	
%
% Creates per-hop latency metric of X-MAC along the outgoing link
% of a given node and attaches it to that node.
%
perHopLatency(node{prr:Prout, vars:[Tl,Ts,N], parent:P, perHopLatency:L, ponestrobe:Ponestrobe, niter:Niter, tmax:Tmax, psack:Psack, tbackoff:Tbackoff}) :-
	( P == [] ->
		L $= 0.0
	;
		declareConstants([_,_,_,_,_,Tturn,_,_,Tdata,_,Twait,_,Titer,Scale,_,_,_,_,_]),
    		% Number of failed transmission Nftx before final successful transmission
    		Pf $= 1.0 - Ponestrobe*Prout^2,
    		Niter $= (Scale*(Tl + Ts))/(2.0*Titer),
    		Nftx $= Pf/(1.0 - Pf) - (N+1)*Pf^(N+1)/(1.0 - Pf^(N+1)),
    		% Time needed for failed transmission Tftx
    		Ttxdata $= 2*Tturn + Tdata,
    		Psack $= Ponestrobe*Prout,
    		Tmax $= Scale*(2.0*Tl + Ts),
    		Tbackoff $= Scale*(Tl + Ts)*1.5,
    		Tftx $= (Niter*Titer + Ttxdata + Twait)*Psack + Tmax*(1.0 - Psack) + Tbackoff,
    		% Time needed for successful transmission Tstx
    		Tstx $= Niter*Titer + Ttxdata,
    		% Per-hop latency L.
    		L $= 0.5*(Tl + Ts)*Scale + Nftx*Tftx + Tstx
	).

%
% Creates node lifetime metric of X-MAC of a given node and
% attaches it to that node.
%
nodeLifetime(node{parent:P, children:C, f:F, fout:Fout, prr:Prout, vars:[Tl,Ts,N], nodeLifetime:T, k:K, ponestrobe:Ponestrobe, niter:Niter, tmax:Tmax, psack:Psack}) :-
	( P == [] ->
		T $= 1.0Inf
	;
		declareConstants([Ptx,Prx,Ps,U,Q,Tturn,Tstr,Tack,Tdata,Tsl,Twait,Ttimeout,Titer,Scale,_,_,_,_,_]),
		( C == [] ->
    			Drxc1 $= 0.0,
    			Dtx1 $= 0.0
    		; 
    			( foreach(node{fout:Fin,prr:Prin}, C), 
      	  	  	  fromto(Expr1, S1, S2, 0),
      	  	  	  fromto(Expr2, S3, S4, 0), param(N, K, Tturn, Tack, Tdata, Ttimeout) do
    				Ponestrobei $= 1.0 - (1.0 - Prin)^K,
    				Prtxi $= 1.0 - Ponestrobei*Prin^3,
    				Nrtxi $= Prtxi*(1.0 - Prtxi^N)/(1.0 - Prtxi),
    				Frxi $= (Nrtxi + 1)*Fin*Ponestrobei,
    				Trxri $= 2*Tturn + (Tturn + Tdata)*Prin^2 + Ttimeout*(1.0 - Prin^2),
    				Drxc1i $= Frxi*Trxri,
    				S1 = Drxc1i + S2,
    				Trxti $= Tack + Tack*Prin^2,
    				Dtx1i $= Frxi*Trxti,
	    			S3 = Dtx1i + S4
    			),
    			Drxc1 $= eval(Expr1),
    			Dtx1 $= eval(Expr2)
    		),
    		% Fraction of time in receive mode
    		Prtxout $= 1.0 - Ponestrobe*Prout^3,
    		Nrtxout $= Prtxout*(1.0 - Prtxout^N)/(1.0 - Prtxout),
    		Ftx $= (Nrtxout + 1)*Fout,
    		Ttxr $= (Niter*(2*Tturn + Tsl) + 2*Tturn + Tack*Prout^2 + Twait*(1.0 - Prout^2))*Psack + (Tmax/Titer)*(2*Tturn + Tsl)*(1.0 - Psack), 
    		Drxc $= Drxc1 + Ftx*Ttxr,
    		Drx $= Drxc + (1.0 - Drxc)*Tl/(Tl + Ts),
		% Fraction of time in transmit mode
    		Ttxt $= (Niter*Tstr + Tdata)*Psack + (Tmax/Titer)*Tstr*(1.0 - Psack),
		Dtx $= Dtx1 + Ftx*Ttxt,
    		% Fraction of time in idle mode
		Didle $= 1.0 - Dtx - Drx,
    		% Total power draw
		Ptotal $= Dtx*Ptx + Drx*Prx + Didle*Ps,
    		% Node lifetime T
    		T $= Q*U/Ptotal
	).

%
% Creates queuing rate and attaches it to that node. 
%
queuingRate(node{parent:P, vars:[_,_,N], fqueuing:Fqueuing, fout:Fout, prr:Prout, ponestrobe:Ponestrobe, psack:Psack, niter:Niter, tmax:Tmax, tbackoff:Tbackoff}) :-
	( P == [] ->
		Fqueuing $= -1.0Inf
	;
		declareConstants([_,_,_,_,_,Tturn,_,Tack,Tdata,_,Twait,_,Titer,_,_,_,_,_,_]),
		Prtxout $= 1.0 - Ponestrobe*Prout^3,
    		Nrtxout $= Prtxout*(1.0 - Prtxout^N)/(1.0 - Prtxout),
    		Ttx $= (Niter*Titer + 2.0*Tturn + Tdata + Tack*Prout^2 + (Twait + Tbackoff)*(1.0 - Prout^2))*Psack + (Tmax + Tbackoff)*(1.0 - Psack),
		Fforwarding $= 1.0/((Nrtxout + 1)*Ttx),
		Fqueuing $= Fout - Fforwarding
	).

%
% Creates the decision variables (Tl, Ts, and N) with their associated
% (integer) domains. Attaches variables to each node.
%
createVariables([Tl,Ts,N]) :-
	declareConstants([_,_,_,_,_,Tturn,Tstr,_,_,Tsl,_,_,Titer,Scale,_,_,_,_,_]),
	% for lifetime gain experiments: UBTs is integer(round(1.0/Scale)),
	UBTs is integer(round(0.5/Scale)),
    	LBTs is integer(round(0.02/Scale)),
    	LBTl is integer(ceiling((2*Tstr + 2*Tturn + Tsl)/Scale)),
    	UBTl is integer(ceiling((3*Titer + Tstr)/Scale)),
  	Ts #>= LBTs,
    	Ts #=< UBTs,
    	Tl #>= LBTl,
    	Tl #=< UBTl, 
    	N #>= 0,
    	N #=< 10.
    
% Protocol parameters geared toward reliability.
%    
optimizedForReliabilityParameters([Tl,Ts,N]) :-
	declareConstants([_,_,_,_,_,_,Tstr,_,_,_,_,_,Titer,Scale,_,_,_,_,_]),
    	Tl is integer(ceiling((3*Titer + Tstr)/Scale)),
    	Ts is 20,
    	N is 10.

%
% Determines decisions variables such that cost function is minimized.
%
solve(Cost, [Tl,Ts,N]) :-
	bb_min(
   	search([N,Tl,Ts],0,input_order,indomain_split,complete,[]),
  		(Cost),
    	bb_options{strategy:continue,delta:3600,factor:1.0,timeout:110,report_success:doNothing,report_failure:doNothing}
    ).

%
% Helper predicate to suppress output of bb_min.
%
doNothing(_,_,_).
