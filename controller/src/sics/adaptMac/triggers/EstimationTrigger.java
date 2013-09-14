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

import java.util.HashSet;
import java.util.LinkedList;

import org.apache.log4j.Logger;

import sics.adaptMac.EclipseDriver;
import sics.adaptMac.NetworkPerformance;
import sics.adaptMac.NodeTopologyInfo;
import sics.adaptMac.Topology;

/**
 * A trigger that uses ECLiPSe to compute an estimate of the current network performance
 * each time Glossy finished the collection of network state information. 
 * 
 * Used in almost all experiments to see how good/bad are the estimations.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public class EstimationTrigger extends AbstractTrigger {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(EstimationTrigger.class.getName());
	
	// Statistics logger
	private static Logger statsLogger = Logger.getLogger("stats");
	
	// ECLiPSe driver
	private final EclipseDriver driver;
	
	// History of topologies
	private final LinkedList<Topology> topologyHistory;
	
	// Helper object to notify and wake up the estimation trigger thread
	private final WaitNotify waitNotify;
	
	/**
	 * Constructor to create an instance of this trigger.
	 * 
	 * @param driver The ECLiPSe driver.
	 * @param topologyHistory History of topologies.
	 */
	public EstimationTrigger(final EclipseDriver driver,
			final LinkedList<Topology> topologyHistory) {
		this.driver = driver;
		this.topologyHistory = topologyHistory;
		this.waitNotify = new WaitNotify();

		// Start the estimation trigger thread
		startEstimationTrigger();
		logger.info("Started EstimationTrigger");
	}
	
	/**
	 * Instantiates and starts the estimation trigger thread.
	 */
	private void startEstimationTrigger() {
		new Thread(new Runnable() {
			public synchronized void run() {
				while (true) {
					// Wait until we get notified that Glossy finished
					waitNotify.doWait();
					
					// Estimate current network performance using ECLiPSe
					NetworkPerformance netPerf = driver.performance(prepareTopologies(topologyHistory, true), extractCurrentMacConfiguration(topologyHistory));
					if (netPerf != null) {
						statsLogger.info("ESTIMATE " + netPerf.getLifetime() + " " + netPerf.getReliability() + " " + netPerf.getLatency() + " " + netPerf.getMaxQueuingRate());
					}
				}
			}
		}, "estimation trigger").start();
	}
	
	/**
	 * Callback function. Signals that Glossy has finished the
	 * collection of network state information. 
	 */
	public void collectionFinished() {
		if (topologyHistory.isEmpty()) {
			return;
		}		
		
		// Notify and wake up the estimation trigger thread.
		waitNotify.doNotify();
	}
	
	/**
	 * Callback function. Signals triggers that node have been
	 * purged from the topology.
	 * 
	 * @param purgedNodes The nodes that have been purged.
	 */
	public void nodesHaveBeenPurged(HashSet<NodeTopologyInfo> purgedNodes) {
		
	}

}
