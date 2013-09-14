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

/**
 * This class encapsulates important pieces of information
 * about a single node in a tree-based routing topology.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class NodeTopologyInfo {

	private long timestamp; // in seconds
	private int nodeId;
	private int parentId;
	private int prr;
	private double pktRate; // pkt per minute

	private MacConfiguration macConf;

	public NodeTopologyInfo(int nodeId, int parentId, double pktRate, int prr, MacConfiguration macConf) {
		this.nodeId = nodeId;
		this.parentId = parentId;
		this.prr = prr;
		this.pktRate = pktRate;
		this.macConf = macConf;
		
		setTimestamp();
	}

	public boolean isSink() {
		return this.nodeId == this.parentId;
	}

	protected Object clone() {
		NodeTopologyInfo n = new NodeTopologyInfo(this.nodeId, this.parentId, this.pktRate, this.prr, this.macConf);
		n.setTimestamp(this.timestamp);
		
		return n;
	}

	public MacConfiguration getMacConf() {
		return macConf;
	}

	public void setMacConf(MacConfiguration macConf) {
		this.macConf = macConf;
	}

	public void setTimestamp() {
		this.timestamp = System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE;
	}

	public double getPktRate() {
		return pktRate;
	}

	public void setPktRate(double pktRate) {
		this.pktRate = pktRate;
	}

	public int getNodeId() {
		return nodeId;
	}

	public int getParentId() {
		return parentId;
	}

	public void setParentId(int parentId) {
		this.parentId = parentId;
	}

	public int getPrr() {
		return prr;
	}

	public void setPrr(int prr) {
		this.prr = prr;
	}

	public long getTimestamp() {
		return timestamp;
	}
	
	public void setTimestamp(long timestamp) {
		this.timestamp = timestamp;
	}
	
	public boolean equals(Object o) {
		if (!(o instanceof NodeTopologyInfo)) {
			return false;
		}

		NodeTopologyInfo n = (NodeTopologyInfo) o;
		
		return (this.nodeId == n.nodeId);
	}

	public int hashCode() {
		return this.nodeId;
	}

	public String toString() {
		if (nodeId != parentId) {
			return "Node=" + nodeId + " parent=" + parentId + " pktRate=" + pktRate + " prr=" + prr + " " + macConf	+ " @" + timestamp;
		} else {
			// This is the sink
			return "Node=" + nodeId + " is SINK";
		}
	}
}
