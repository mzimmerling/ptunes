/*
 * Copyright 2013 ETH Zurich and SICS Swedish ICT 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

package sics.adaptMac.triggers;

import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedList;

import org.apache.log4j.Logger;

import sics.adaptMac.MacConfiguration;
import sics.adaptMac.NodeTopologyInfo;
import sics.adaptMac.Topology;

/**
 * Abstract super class of all optimization triggers in pTunes.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public abstract class AbstractTrigger {
	
	// Controller logger 
	private static Logger logger = Logger.getLogger(AbstractTrigger.class.getName());
	
	// Determines whether paths start from leaf nodes are from any node (besides the sink) in the tree 
	private static final boolean FROM_LEAVES = false;
	
	/**
	 * Callback function. Signals triggers that Glossy has finished
	 * the collection of network state information.
	 */
	public abstract void collectionFinished();
	
	/**
	 * Callback function. Signals triggers that node have been purged
	 * from the topology.
	 * 
	 * @param purgedNodes The nodes that have been purged.
	 */
	public abstract void nodesHaveBeenPurged(HashSet<NodeTopologyInfo> purgedNodes);
	
	/**
	 * Prepares a collection of objects to be send over to ECLiPSe for
	 * estimation or optimization.
	 * 
	 * @param topologyHistory History of last seen topologies.
	 * @param onlyCurrentTopology If true, considers only the current topology; otherwise all.
	 * @return
	 */
	protected Collection<Object> prepareTopologies(LinkedList<Topology> topologyHistory, boolean onlyCurrentTopology) {
		Collection<Object> topologies = new LinkedList<Object>();
		synchronized (topologyHistory) {
			if (!topologyHistory.isEmpty()) {
				if (onlyCurrentTopology) {
					Topology currentTopology = topologyHistory.getFirst(); 
					if (currentTopology.isConsistent()) {
						Collection<Object> topology = new LinkedList<Object>();
						topology.add(currentTopology.getTimestamp() - currentTopology.getInitialTimestamp());
						topology.add(buildNodeIds(currentTopology));
						topology.add(buildPRRs(currentTopology));
						topology.add(buildFs(currentTopology));
						topology.add(buildParents(currentTopology));
						topology.add(buildChildrenList(currentTopology));
						topology.add(buildPathList(currentTopology));

						topologies.add(topology);
					}
				} else {
					// We only trigger the optimizer if the current topology is consistent.
					if (topologyHistory.getFirst().isConsistent()) {
						for (Topology t : topologyHistory) {
							if (t.isConsistent()) {
								// Avoids examining topologies with incomplete info
								Collection<Object> topology = new LinkedList<Object>();
								topology.add(t.getTimestamp() - t.getInitialTimestamp());
								topology.add(buildNodeIds(t));
								topology.add(buildPRRs(t));
								topology.add(buildFs(t));
								topology.add(buildParents(t));
								topology.add(buildChildrenList(t));
								topology.add(buildPathList(t));

								topologies.add(topology);
							}
						}
					}
				}
			}
			topologyHistory.notifyAll();
		}

		return topologies;
	}
	
	/**
	 * Constructs collection of integers representing the node ids
	 * in the supplied topology.
	 * 
	 * @param t The topology.
	 * @return Collection of node ids.
	 */
	private Collection<Integer> buildNodeIds(Topology t) {
		Collection<Integer> nodeIds = new LinkedList<Integer>();
		for (NodeTopologyInfo n : t.getNodes()) {
			nodeIds.add(n.getNodeId());
		}
		logger.debug("TRIGGER: nodeIds:" + nodeIds);

		return nodeIds;
	}
	
	/**
	 * Constructs a collection of doubles representing the PRRs
	 * in the supplied topology.
	 * 
	 * @param t The topology.
	 * @return Collection of PRRs.
	 */
	private Collection<Double> buildPRRs(Topology t) {
		Collection<Double> prrs = new LinkedList<Double>();
		for (NodeTopologyInfo n : t.getNodes()) {
			prrs.add(Math.sqrt((double) n.getPrr() / 1000.0));

		}
		logger.debug("TRIGGER: prrs:" + prrs);

		return prrs;
	}
	
	/**
	 * Constructs a collection of doubles representing the 
	 * packet generation rates in the supplied topology.
	 * 
	 * @param t The topology.
	 * @return Collection of packet generation rates.
	 */
	private Collection<Double> buildFs(Topology t) {
		Collection<Double> fs = new LinkedList<Double>();
		for (NodeTopologyInfo n : t.getNodes()) {
			fs.add(n.getPktRate());
		}
		logger.debug("TRIGGER: fs:" + fs);

		return fs;
	}

	/**
	 * Constructs for each node in the supplied topology a list of node ids
	 * that contains the node's parent or is empty (sink). 
	 * 
	 * @param t The topology.
	 * @return List of lists containing each node's parent.
	 */
	private LinkedList<LinkedList<Integer>> buildParents(Topology t) {
		LinkedList<LinkedList<Integer>> parentList = new LinkedList<LinkedList<Integer>>();
		for (NodeTopologyInfo n : t.getNodes()) {
			LinkedList<Integer> parentOfN = new LinkedList<Integer>();
			// Adds nothing if the node is the sink
			if (n.getParentId() != n.getNodeId()) {
				parentOfN.add(n.getParentId());
			}
			parentList.add(parentOfN);
		}
		logger.debug("TRIGGER: parentList:" + parentList);
		
		return parentList;
	}
	
	/**
	 * Constructs for each node in the supplied topology a list of node ids
	 * that contains the node's children or is empty (leaf nodes).
	 * 
	 * @param t The topology.
	 * @return List of lists containing each node's children.
	 */
	private LinkedList<LinkedList<Integer>> buildChildrenList(Topology t) {
		LinkedList<LinkedList<Integer>> childrenList = new LinkedList<LinkedList<Integer>>();
		for (NodeTopologyInfo n : t.getNodes()) {
			LinkedList<Integer> childrenOfN = new LinkedList<Integer>();
			for (NodeTopologyInfo m : t.getNodes()) {
				if (!m.equals(n) && m.getParentId() == n.getNodeId()) {
					childrenOfN.add(m.getNodeId());
				}
			}
			childrenList.add(childrenOfN);
		}
		logger.debug("TRIGGER: childrenList:" + childrenList);

		return childrenList;
	}
	
	/**
	 * Constructs for each node in the supplied topology a list of nodes ids
	 * that represents the node's path to the sink or is empty (sink).
	 * 
	 * @param t The topology.
	 * @return List of lists containing each node's path to the sink
	 */
	private LinkedList<LinkedList<Integer>> buildPathList(Topology t) {
		LinkedList<LinkedList<Integer>> pathList = new LinkedList<LinkedList<Integer>>();
		// Find the source nodes
		LinkedList<NodeTopologyInfo> sources = new LinkedList<NodeTopologyInfo>();
		if (FROM_LEAVES) {
			for (NodeTopologyInfo n : t.getNodes()) {
				boolean isLeaf = true;
				for (NodeTopologyInfo m : t.getNodes()) {
					if (m.getParentId() == n.getNodeId()) {
						isLeaf = false;
						break;
					}
				}
				if (isLeaf) {
					sources.add(n);
				}
			}
		} else {
			for (NodeTopologyInfo n : t.getNodes()) {
				if (!n.isSink()) {
					sources.add(n);
				}
			}
		}

		// Building paths from sources to sink
		for (NodeTopologyInfo l : sources) {
			LinkedList<Integer> path = new LinkedList<Integer>();
			NodeTopologyInfo iterator = t.getNodeInfo(l.getNodeId());
			while (iterator.getNodeId() != t.getSink().getNodeId()) {
				path.add(iterator.getNodeId());
				iterator = t.getNodeInfo(iterator.getParentId());
			}
			// The path ends with the sink
			path.add(t.getSink().getNodeId());
			pathList.add(path);
		}
		logger.debug("TRIGGER: pathList:" + pathList);

		return pathList;
	}
	
	/**
	 * Determines the MAC configuration in the current topology.
	 * 
	 * @param topologyHistory History of topologies. 
	 * @return The current MAC configuration.
	 */
	protected MacConfiguration extractCurrentMacConfiguration(LinkedList<Topology> topologyHistory) {
		if (!topologyHistory.isEmpty()) {
			// Check the last known topology
			Topology lastTopology = topologyHistory.getFirst();
			for (NodeTopologyInfo n : lastTopology.getNodes()) {
				// Find the first non-sink node
				if (!n.isSink()) {
					return n.getMacConf();
				}
			}
		}

		return new MacConfiguration();
	}
	
	/**
	 * Determines the average path PRR (i.e., path PRR is the product of all
	 * PRRs along the path) in the current topology.
	 * 
	 * @param topologyHistory History of topologies.
	 * @return The current average path PRR.
	 */
	protected double extractAveragePRR(LinkedList<Topology> topologyHistory) {
		if (topologyHistory.isEmpty()) {
			return 0.0;
		}
		
		// Check last known topology
		Topology lastTopology = topologyHistory.getFirst();
		// Collect source nodes
		LinkedList<NodeTopologyInfo> sources = new LinkedList<NodeTopologyInfo>();
		for (NodeTopologyInfo n : lastTopology.getNodes()) {
			if (!n.isSink()) {
				sources.add(n);
			}
		}
			
		// Compute average path PRR
		double sum = 0.0;
		int count = 0;
		for (NodeTopologyInfo l : sources) {
			// Compute path PRR
			double pathPRR = 1.0;
			NodeTopologyInfo iterator = lastTopology.getNodeInfo(l.getNodeId());
			while (iterator.getNodeId() != lastTopology.getSink().getNodeId()) {
				pathPRR *= Math.sqrt((double) iterator.getPrr() / 1000.0);
				iterator = lastTopology.getNodeInfo(iterator.getParentId());
			}
			pathPRR *= Math.sqrt((double) lastTopology.getSink().getPrr() / 1000.0);
			
			// Update sum and count
			count++;
			sum += pathPRR;
		} 
		
		return sum/count;
	}
	
}
