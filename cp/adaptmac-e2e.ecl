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

% :- module(adaptmace2e).

:- use_module(xmac).
:- use_module(constants).

:- lib(ic).
:- lib(lists).
:- lib(branch_and_bound).
:- lib(util).

:- local struct(topology(weight,nodes,paths)).

:- pragma(nodebug).
:- pragma(expand).

%
% Top-level predicate called by Java controller to determine
% new optimized parameters.
%
optimize :-
	set_threshold(1e-5),
	retrieveTopologiesAndBounds(TopologyInfo, Bounds),
	% Bounds = [0.95,1.0],
 	createTopologies(TopologyInfo, Topologies),
	createVariables(Vars),
	attachVariablesToNodes(Topologies, Vars),
	( setupEndToEndConstraints(Topologies, Bounds) ->
		% End-to-end constraints are satisfiable, so we go on normally
		setupQueuingConstraint(Topologies),
		createCost(Topologies, Cost),
		solve(Cost, Vars),
		outputOptimalParameters(Vars)
	;
		% End-to-end constraints are not satisfiable, so we return
		% parameters that are optimized for reliability
		optimizedForReliabilityParameters(RelVars),
		outputOptimalParameters(RelVars) 
	).
	
%
% Top-level predicate called by Java controller to estimate
% the current network performance.
%
performance :-
	retrieveTopologyAndVariables(TopologyInfo, Vars),
	% Vars = [202,92,9],
	% Vars = [6,137,2],
	createTopologies(TopologyInfo, Topologies),
	determinePerformance(Topologies, Vars, Performance),
	% printf("Performance = %p%n", [Performance]),
	% Topologies = [topology{nodes:NodesRecent}|_],
	% ( foreach(node{id:Id, fqueuing:Fqueuing}, NodesRecent) do
	% 	printf("Id = %p, Fqueuing = %p%n", [Id,Fqueuing])
	% ).
	outputPerformance(Performance).
	
% 
% Retrieves topology information as well as bounds on end-to-end
% reliability and latency via queues from Java. 
% 
retrieveTopologiesAndBounds(TopologyInfo, Bounds) :-
	read_exdr(java_to_eclipse, TopologyInfo),
	read_exdr(java_to_eclipse, Bounds).

%
% Creates the tree topologies by creating the nodes with their respective
% Ids, PRRs,and packet generation rates and establishing child-parent
% relationships. Also constructs paths from leaves to sink.
%
createTopologies(TopologyInfo, Topologies) :-
	% Some toy and real topologies for testing purposes
/*
	Duration1 = 100,
	NodeIds1 = [1,2,3,4,5,6,7],
	PRRs1 = [0.9,0.9,0.9,0.9,0.9,0.9,0.9],
	Fs1 = [0.1,0.1,0.1,0.1,0.1,0.1,0.1],
	ParentList1 = [[],[1],[2],[1],[4],[4],[6]],
	ChildrenList1 = [[2,4],[3],[],[5,6],[],[7],[]],
	PathList1 = [[3,2,1],[5,4,1],[7,6,4,1]],
	
	Duration2 = 999,
	NodeIds2 = [1,2,3,4,5,6,7],
	PRRs2 = [0.8,0.8,0.8,0.8,0.8,0.8,0.8],
	Fs2 = [0.1,0.1,0.1,0.1,0.1,0.1,0.1],
	ParentList2 = [[],[1],[2],[1],[2],[4],[6]],
	ChildrenList2 = [[2,4],[3,5],[],[6],[],[7],[]],
	% PathList2 = [[3,2,1],[5,2,1],[7,6,4,1]],
	PathList2 = [[3,2,1],[5,2,1],[7,6,4,1],[2,1],[6,4,1],[4,1]],
	
	TopologyInfo = [[Duration2,NodeIds2,PRRs2,Fs2,ParentList2,ChildrenList2,PathList2]],

	Duration1 = 1,
	NodeIds1 = [137, 1, 139, 2, 3, 4, 5, 200, 6, 142, 7, 8, 9, 10, 11, 12, 133, 132, 14, 135, 15, 17, 16, 19, 18, 21, 20, 23, 22, 144, 24, 26, 28, 34],
	PRRs1 = [0.9126883367283708, 0.9011104260855048, 0.8596510920134982, 0.807465169527454, 0.6228964600958975, 0.8723531395025755, 0.79435508432942, 1.0, 0.8660254037844386, 0.7842193570679061, 0.8860022573334675, 0.8994442728707543, 0.6410928169929843, 0.8860022573334675, 0.8944271909999159, 0.9391485505499116, 0.8561541917201597, 0.8497058314499201, 0.6826419266350405, 0.8526429498916882, 0.8526429498916882, 0.8994442728707543, 0.9126883367283708, 0.8288546314040841, 0.7745966692414834, 0.7556454194925024, 0.7867655305108378, 0.9126883367283708, 0.8561541917201597, 0.9423375191511797, 0.8160882305241266, 0.8561541917201597, 0.9176055797563569, 0.9486832980505138],
	Fs1 = [0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.0, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666],
	ParentList1 = [[133], [26], [135], [135], [200], [133], [135], [], [135], [26], [135], [133], [135], [200], [26], [200], [200], [200], [133], [200], [135], [22], [26], [135], [135], [135], [135], [10], [26], [200], [135], [21], [200], [12]],
	ChildrenList1 = [[], [], [], [], [], [], [], [3, 10, 12, 133, 132, 135, 144, 28], [], [], [], [], [], [23], [], [34], [137, 4, 8, 14], [], [], [139, 2, 5, 6, 7, 9, 15, 19, 18, 21, 20, 24], [], [], [], [], [], [26], [], [], [17], [], [], [1, 142, 11, 16, 22], [], []],
	PathList1 = [[137, 133, 200], [1, 26, 21, 135, 200], [139, 135, 200], [2, 135, 200], [3, 200], [4, 133, 200], [5, 135, 200], [6, 135, 200], [142, 26, 21, 135, 200], [7, 135, 200], [8, 133, 200], [9, 135, 200], [10, 200], [11, 26, 21, 135, 200], [12, 200], [133, 200], [132, 200], [14, 133, 200], [135, 200], [15, 135, 200], [17, 22, 26, 21, 135, 200], [16, 26, 21, 135, 200], [19, 135, 200], [18, 135, 200], [21, 135, 200], [20, 135, 200], [23, 10, 200], [22, 26, 21, 135, 200], [144, 200], [24, 135, 200], [26, 21, 135, 200], [28, 200], [34, 12, 200]],
	
	TopologyInfo = [[Duration1,NodeIds1,PRRs1,Fs1,ParentList1,ChildrenList1,PathList1]],

	Duration1 = 1,
	NodeIds1 = [1, 2, 3, 4, 5, 200, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30, 34, 102, 103, 32, 101, 33, 108, 109, 106, 107, 104, 105],
	PRRs1 = [0.9757048734120374, 0.9570788891204319, 0.9011104260855048, 0.8318653737234168, 0.960728889958036, 1.0, 0.9126883367283708, 0.950789145920377, 0.9126883367283708, 0.9534149149242422, 0.9257429448826494, 0.8160882305241266, 0.8860022573334675, 0.960728889958036, 0.7071067811865476, 0.9570788891204319, 0.9534149149242422, 0.9197825830053535, 0.9746794344808963, 0.9746794344808963, 0.8561541917201597, 0.9126883367283708, 1.0, 0.9257429448826494, 0.9197825830053535, 0.8944271909999159, 0.9570788891204319, 0.9534149149242422, 1.0, 0.8561541917201597, 0.9757048734120374, 0.9715966241192895, 0.8740709353364863, 0.9746794344808963, 0.9757048734120374, 0.8579044235810886, 0.8449852069711044, 0.9176055797563569, 0.9534149149242422, 1.0, 0.967987603226405, 0.8814760348415606, 0.9746794344808963, 0.8449852069711044],
	Fs1 = [0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.0, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666, 0.016666666666666666],
	ParentList1 = [[17], [33], [200], [12], [12], [], [33], [14], [12], [33], [29], [6], [29], [29], [29], [12], [3], [104], [33], [6], [31], [102], [12], [26], [18], [6], [28], [104], [200], [200], [29], [5], [105], [33], [29], [29], [102], [200], [102], [29], [29], [12], [29], [200]],
	ChildrenList1 = [[], [], [17], [], [30], [3, 29, 28, 33, 105], [11, 18, 24], [], [], [], [], [], [4, 5, 8, 15, 23, 107], [], [7], [], [1], [], [], [25], [], [], [], [], [], [], [], [22], [10, 12, 13, 14, 31, 103, 32, 109, 106, 104], [27], [21], [], [], [20, 101, 108], [], [], [], [2, 6, 9, 19, 102], [], [], [], [], [16, 26], [34]],
	PathList1 = [[1, 17, 3, 200], [2, 33, 200], [3, 200], [4, 12, 29, 200], [5, 12, 29, 200], [6, 33, 200], [7, 14, 29, 200], [8, 12, 29, 200], [9, 33, 200], [10, 29, 200], [11, 6, 33, 200], [12, 29, 200], [13, 29, 200], [14, 29, 200], [15, 12, 29, 200], [17, 3, 200], [16, 104, 29, 200], [19, 33, 200], [18, 6, 33, 200], [21, 31, 29, 200], [20, 102, 33, 200], [23, 12, 29, 200], [22, 26, 104, 29, 200], [25, 18, 6, 33, 200], [24, 6, 33, 200], [27, 28, 200], [26, 104, 29, 200], [29, 200], [28, 200], [31, 29, 200], [30, 5, 12, 29, 200], [34, 105, 200], [102, 33, 200], [103, 29, 200], [32, 29, 200], [101, 102, 33, 200], [33, 200], [108, 102, 33, 200], [109, 29, 200], [106, 29, 200], [107, 12, 29, 200], [104, 29, 200], [105, 200]],
	
	TopologyInfo = [[Duration1,NodeIds1,PRRs1,Fs1,ParentList1,ChildrenList1,PathList1]],
*/
	% Create topologies from supplied topology information.
	( foreach([Duration,NodeIds,PRRs,Fs,ParentList,ChildrenList,PathList], TopologyInfo),
	  foreach(topology{weight:Weight,nodes:Nodes,paths:Paths}, Topologies) do
	    % Assign a weight to the topology.
	    Weight $= Duration, 
		% Create the nodes with ID, PRR, and packet generation rate.
		( foreach(NodeId, NodeIds),
	  	  foreach(PRR, PRRs),
	  	  foreach(F, Fs),
	  	  foreach(N, Nodes) do
	  	  	( PRR == 1.0 ->
	  	  		% To get node lifetime metric to work. 
	  	  		N = node{id : NodeId, prr : 0.9999999, f : F}
	  	  	;
	  	  		N = node{id : NodeId, prr : PRR, f : F}
	  	  	)
		),
		% Set for each node its parent and children (if any).
		( foreach(node{parent:P,children:C}, Nodes),
	  	  foreach(ParentListEntry, ParentList),
	  	  foreach(ChildrenListEntry, ChildrenList), param(Nodes) do
	  		( foreach(ParentId, ParentListEntry),
	  	  	  foreach(Parent,P), param(Nodes) do
	  			getNodeById(Nodes,ParentId,Parent)
	  		),
	  		( foreach(ChildId, ChildrenListEntry),
	  	  	  foreach(Child,C), param(Nodes) do
	  	  		getNodeById(Nodes,ChildId,Child)
	  		)
		),
		% Construct paths from leaf nodes to the sink.
		( foreach(PathListEntry, PathList),
	  	  foreach(Path, Paths), param(Nodes) do
	  		( foreach(NodeId, PathListEntry),
	  	  	  foreach(Node, Path), param(Nodes) do
	  	  		getNodeById(Nodes,NodeId,Node)
	  		)
		)
	).

%
% Attach decision variables to all nodes in all topologies.
%    
attachVariablesToNodes(Topologies, Vars) :-
	( foreach(topology{nodes:Nodes}, Topologies), param(Vars) do 
        ( foreach(node{vars:Vars}, Nodes), param(Vars) do
    		true
    	)
    ).

%
% Creates end-to-end constraints on latency and reliability, and
% attaches node lifetime metric with each node.
%
setupEndToEndConstraints(Topologies, [BR,BL]) :-
	% Create end-to-end constraints on latency and reliability on
	% all topologies.
	( foreach(topology{nodes:Nodes,paths:Paths}, Topologies), param(BR, BL) do
		( foreach(N, Nodes) do
			perHopReliability(N),
			perHopLatency(N)
		),
		( foreach(P, Paths),
		  fromto(Rs, [R|Rs], Rs, []),
		  fromto(Ls, [L|Ls], Ls, []) do
			endToEndReliability(P,R),
			endToEndLatency(P,L)
		),
		length(Paths, Len),
		( foreach(R, Rs),
		  foreach(L, Ls),
    	  fromto(RSum, S1, S2, 0),
    	  fromto(LSum, S3, S4, 0) do
    		S1 = R + S2,
    		S3 = L + S4
    	),
    	AvgR $= eval(RSum)/Len,
    	AvgL $= eval(LSum)/Len,
	   	AvgR $> BR,
    	AvgR $=< 1.0,
    	AvgL $< BL,
    	AvgL $>= 0.0
	).

%
% Creates queuing constraint on each node.
%
setupQueuingConstraint(Topologies) :-
	Topologies = [topology{nodes:NodesRecent}|_],
	( foreach(N, NodesRecent) do
	  	queuingRate(N),
	  	arg(parent of node, N, P),
	  	( P \== [] ->
	  		arg(fqueuing of node, N, Q),
	  		Q $=< 0
	  	;
	  		true
	  	)
	).

%
% Constructs the cost function to be minimized. 
%
createCost(Topologies, Cost) :-
	% Attach node lifetime metric only to the most recent topology.
	% Also, attach it only to non-leaf nodes.
	Topologies = [topology{nodes:NodesRecent}|_],
	( foreach(N, NodesRecent),
	  fromto([], In, Out, Ts), param(Ts) do
		packetsToSend(N),
		arg(parent of node, N, Parent),
		arg(children of node, N, Children),
		arg(nodeLifetime of node, N, T),
		( Children \== [] ->
			( Parent \== [] ->
				nodeLifetime(N),
				append(In,[T],Out)
			;
				append(In,[],Out)
			)	
		;
			append(In,[],Out)
		)
	),
	ic:min(Ts,MinT),
	Cost $= -MinT.

%
% Send optimal values back to Java controller via queues.
%
outputOptimalParameters(Vars) :-
	write_exdr(eclipse_to_java, Vars),
    	flush(eclipse_to_java).

% 
% Retrieves information about the current topology and
% parametrization via queues from Java. 
% 
retrieveTopologyAndVariables(TopologyInfo, Vars) :-
	read_exdr(java_to_eclipse, TopologyInfo),
	read_exdr(java_to_eclipse, Vars).

%
% Determines network performance for a given topology and
% parametrization in terms of
%  - network lifetime = min. node lifetime
%  - average end-to-end reliability
%  - average end-to-end latency.
%
determinePerformance(Topologies, Vars, Performance) :-
	% Attach variables to all nodes in the given topology.
	Topologies = [topology{nodes:Nodes,paths:Paths}|_],
    	( foreach(N, Nodes), param(Vars) do
    	  arg(vars of node,N,Vars),
    	  	packetsToSend(N),
    		queuingRate(N),
    		perHopReliability(N),
		perHopLatency(N),
		nodeLifetime(N)
	),
    	% Collect all node lifetimes and queuing rates in a list.
    	( foreach(node{nodeLifetime:T,fqueuing:Q}, Nodes),
	  fromto(Ts, [T|Ts], Ts, []),
	  fromto(Qs, [Q|Qs], Qs, []) do
		true
	),
	% Collect all end-to-end reliabilites and latencies in lists.
	( foreach(P, Paths),
	  fromto(Rs, [R|Rs], Rs, []),
	  fromto(Ls, [L|Ls], Ls, []) do
		endToEndReliability(P,R),
		endToEndLatency(P,L)
	),
	% Grab minimum node lifetime
	ic:min(Ts,_MinT),
	get_min(_MinT,MinT),
	% Grab maximum queuing rate
	ic:max(Qs,_MaxQ),
	get_max(_MaxQ,MaxQ),
	% Compute average end-to-end reliability and latency
	( foreach(R, Rs),
	  foreach(L, Ls),
          fromto(RSum, S1, S2, 0),
          fromto(LSum, S3, S4, 0) do
		S1 = R + S2,
		S3 = L + S4
    	),
    	length(Paths, Len),
    	_AvgR $= eval(RSum)/Len,
    	_AvgL $= eval(LSum)/Len,
    	% Prepare and output results
    	get_min(_AvgR,AvgR),
    	get_max(_AvgL,AvgL),
	Performance = [MinT,AvgR,AvgL,MaxQ].

%
% Send network performance back to Java controller via queues.
%
outputPerformance(Performance) :-
	write_exdr(eclipse_to_java, Performance),
	flush(eclipse_to_java).

%
% Creates end-to-end reliability metric by multiplying the per-hop
% reliabilities along a given path.
%
endToEndReliability(Path, R) :-
	( foreach(node{perHopReliability:Ri}, Path),
	  fromto(Expr, P1, P2, 1) do
		P1 = Ri * P2  
	),
	R $= eval(Expr).

%
% Creates end-to-end latency metric by summing up the per-hop
% latencies along a given path.
%   
endToEndLatency(Path, L) :-
	( foreach(node{perHopLatency:Li}, Path),
	  fromto(Expr, S1, S2, 0) do
		S1 = Li + S2  
	),
	L $= eval(Expr).

%
% Creates outgoing packet rate metric of a node and
% attaches it to that node.
%
packetsToSend(node{parent:P, children:C, f:F, fout:Fout}) :-
	( P = [] ->
		Fout = 0.0
	;
		( C == [] ->
			% We have no children and thus only sent packets we have generated.
			Fout $= F
		;
			% We forward packets received from our children and our own packets.
			( foreach(node{perHopReliability:Ri,fout:Fouti}, C),
	  	  	  fromto(Expr, S1, S2, 0) do
				S1 = Ri * Fouti + S2  
			),
    			Fout $= eval(Expr) + F
    		)
    	).

%
% Helper predicate that unifies Node with node specified by NodeId.
%
getNodeById([H|T], NodeId, Node) :-
	arg(id of node, H, Id),
	( NodeId == Id ->
		Node = H,
		!
	;
		getNodeById(T,NodeId,Node)
	).
