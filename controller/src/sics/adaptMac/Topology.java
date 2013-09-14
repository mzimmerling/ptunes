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

package sics.adaptMac;

import java.util.Collection;
import java.util.HashSet;

import org.apache.log4j.Logger;

/**
 * Representation of a complete tree-based routing topology
 * as captured by pTunes at a specific moment in time.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class Topology {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(Topology.class.getName());
	
	private long currentTimestamp;
	private long initialTimestamp;
	private HashSet<NodeTopologyInfo> nodes;

	public Topology(NodeTopologyInfo sink, NodeTopologyInfo n) {
		this.nodes = new HashSet<NodeTopologyInfo>();
		nodes.add(sink);
		nodes.add(n);
		setTimestamp();
		initialTimestamp = this.getTimestamp();
	}

	public Topology(Topology t, NodeTopologyInfo n) {
		this.nodes = new HashSet<NodeTopologyInfo>();

		for (NodeTopologyInfo node : t.nodes) {
			nodes.add((NodeTopologyInfo) node.clone());
		}
		// Removes the info on node n if any: id-based comparison
		nodes.remove(n);
		nodes.add(n); // Adds the new info
		setTimestamp();
		initialTimestamp = this.getTimestamp();
	}
	
	public Topology(Topology t, HashSet<NodeTopologyInfo> toPurge) {
		this.nodes = new HashSet<NodeTopologyInfo>();

		for (NodeTopologyInfo n : t.nodes) {
			if (!toPurge.contains(n)) {
				logger.debug("adding node id = " + n.getNodeId());
				nodes.add((NodeTopologyInfo) n.clone());
			}
		}
		setTimestamp();
		initialTimestamp = this.getTimestamp();
	}

	public long getInitialTimestamp() {
		return initialTimestamp;
	}

	public Collection<NodeTopologyInfo> getNodes() {
		return nodes;
	}

	public long getTimestamp() {
		return currentTimestamp;
	}

	public void setTimestamp() {
		this.currentTimestamp = System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE;
	}

	public NodeTopologyInfo getNodeInfo(int nodeId) {
		for (NodeTopologyInfo n : nodes) {
			if (nodeId == n.getNodeId()) {
				return n;
			}
		}
		return null;
	}

	public NodeTopologyInfo getSink() {
		for (NodeTopologyInfo n : nodes) {
			if (n.isSink()) {
				return n;
			}
		}
		return null;
	}
	
	public boolean isConsistent() {
		for (NodeTopologyInfo n : nodes) {
			if (!n.isSink() && getNodeInfo(n.getParentId()) == null) {
				// Node doesn't have a parent
				logger.debug("Topology contains a node without a parent (node = " + n.getNodeId() + "), discarding");
				return false;
			}
		}

/*		for (NodeTopologyInfo n : nodes) {
			for (NodeTopologyInfo m : nodes) {
				// Checking MAC params: if some are different; Trickle is not yet done
				if (!n.isSink() && !m.isSink() && !(n.getMacConf().equals(m.getMacConf()))) {
					logger.debug("Node " + n.getNodeId() + " and "
							+ m.getNodeId() + " have diff MAC" + n.getMacConf()
							+ " " + m.getMacConf());
					return false;
				}
			}
		}
*/		
		for (NodeTopologyInfo n : nodes) {
			if (!n.isSink()) {
				NodeTopologyInfo currentNode = n;
				HashSet<Integer> visitedNodes = new HashSet<Integer>();
				visitedNodes.add(currentNode.getNodeId());
				// Walk up the path in the tree as long as the current node's parent is not the sink and not the node from which we started
				while (!getNodeInfo(currentNode.getParentId()).isSink()) {
					// If the parent of the current node is among the nodes we already visited, we detected a cycle.
					if (visitedNodes.contains(currentNode.getParentId())) {
						logger.debug("Topology contains a cycle starting at node " + currentNode.getParentId());
						return false;
					}
					
					// Move on to the parent and add it to the list of visited nodes
					currentNode = getNodeInfo(currentNode.getParentId());
					visitedNodes.add(currentNode.getNodeId());
				}
			}
		}
		
		return true;
	}
	
	public String toString() {
		String s = new String();
		s = s.concat("Topology @" + currentTimestamp + " since " + initialTimestamp + "\n");
		for (NodeTopologyInfo n : nodes) {
			s = s.concat(n.toString() + "\n");
		}
		s = s.concat("\n");
		return s;
	}
}
