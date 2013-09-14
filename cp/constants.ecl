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

:- module(constants).

:- export(declareConstants/1).

:- pragma(nodebug).
:- pragma(expand).

%
% Provides several constants.
%
declareConstants(Consts) :-
	Consts = [Ptx,Prx,Ps,U,Q,Tturn,Tstr,Tack,Tdata,Tsl,Twait,Ttimeout,Titer,Scale,Ton,Tpr,Trandmax,Tdacktimeout,Tdack],
	% Hardware-dependent constants (see Tmote Sky datasheet).
	Itx is 17.4e-3,			% current at Tx [A]
	Irx is 19.7e-3,			% current at Rx [A]
	Is is 426e-6,			% current in idle mode [A]
	U is 3,				% supply voltage [V]
	Ptx is U*Itx,			% power consumption at Tx [W]
	Prx is U*Irx,			% power consumption at Rx [W]
	Ps is U*Is,			% power consumption in idle mode [W]
	Tturn is 192e-6,		% Rx/Tx turnaround time [s]
	Q is 7200,			% battery capacity [As]
	% Implementation-dependent constants.
	Tsl is 4.15e-3,			% duration sender acknowledgment listen [s]
	Twait is 2.4414e-3,		% duration of sender waiting for data acknowledgment [s]
	Ttimeout is 5.1269e-3,		% duration of receiver waiting for data packet [s]
	% Scale is 2.4414e-4,		% granularity of timer [tics/s]
	Scale is 1e-3,
	Tstr is 416e-6,			% duration of strobe transmission [s]
	Tack is 416e-6,			% duration of ack transmission [s]
	Tdata is 2.34e-3,		% duration of data transmission [s]
	Titer is 2*Tturn + Tstr + Tsl,	% duration of strobe transmission and listen for strobe iteration [s]
	% LPP-specific constants.
	Ton is 7.8125e-3,		% duration of radio on-time (LPP) [s]
	Tpr is 416e-6,			% duration of probe transmission (LPP) [s]
	Trandmax is 15.625e-3,		% 
	Tdacktimeout is 4.16564941e-3,  % duration of sender waiting for data acknowledgment (LPP) [s]
	Tdack is 416e-6.		% duration of data acknowledgment transmission (LPP) [s]
    