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

import java.io.BufferedWriter;
import java.io.IOException;
import java.util.HashSet;
import java.util.LinkedList;

import org.apache.log4j.Logger;

import sics.adaptMac.EclipseDriver;
import sics.adaptMac.MacConfiguration;
import sics.adaptMac.NodeTopologyInfo;
import sics.adaptMac.Topology;

/**
 * A trigger that optimizes periodically, but optimizes immediately when all nodes
 * switched to a new data rate.
 * 
 * Used for lifetime gain experiments with optimizer continuously enabled.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public class UnifiedDataRateTrigger extends AbstractTrigger {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(UnifiedDataRateTrigger.class.getName());

	// Statistics logger
	private static Logger statsLogger = Logger.getLogger("stats");
	
	// Maximum number of optimization retries
	private static final int MAX_RETRIES = 4;
	
	// ECLiPSe driver
	private final EclipseDriver driver;
	
	// History of topologies
	private final LinkedList<Topology> topologyHistory;
	
	// Stream to output determined MAC parameters via serialdump to the sink node
	private final BufferedWriter output;
	
	// Helper object to notify and wake up the unified data rate trigger thread
	private final WaitNotify waitNotify;
	
	// Initial delay in minutes
	private final int initialDelay;

	// Optimization period (in number of Glossy phases)
	private final int period;
	
	// Number of Glossy phases between failed optimization and retry
	private final int retryPeriod;
	
	// End-to-end constraints
	private final double reliabilityConstraint;
	private final double latencyConstraint;
	
	// Current data rate (is updated only if all nodes have switched to another data rate)  
	private double dataRate;
	
	/**
	 * Constructor to create an instance of this trigger.
	 * 
	 * @param driver The ECLiPSe driver.
	 * @param topologyHistory History of topologies.
	 * @param initialDelay Initial delay in minutes.
	 * @param period Optimization period in number of Glossy phases.
	 * @param retryPeriod Number of Glossy phases between failed optimization and retry.
	 * @param output Stream to output MAC parameters to serialdump.
	 * @param reliabilityConstraint End-to-end constraint on reliability.
	 * @param latencyConstraint End-to-end constraint on latency.
	 */
	public UnifiedDataRateTrigger(final EclipseDriver driver,
			final LinkedList<Topology> topologyHistory,
			final int initialDelay,
			final int period,
			final int retryPeriod,
			final BufferedWriter output,
			final double reliabilityConstraint,
			final double latencyConstraint) {
		this.driver = driver;
		this.topologyHistory = topologyHistory;
		this.period = period;
		this.waitNotify = new WaitNotify(period);
		this.initialDelay = initialDelay;
		this.retryPeriod = retryPeriod;
		this.output = output;
		this.reliabilityConstraint = reliabilityConstraint;
		this.latencyConstraint = latencyConstraint;
		this.dataRate = Double.NaN;
		
		// Start the timed trigger thread
		startDataRateTrigger();
		logger.info("Started UnifiedDataRateTrigger with OptimizationTriggerInitialDelay " + this.initialDelay
				+ " minutes, OptimizationPeriod " + this.period
				+ ", OptimizationRetryPeriod = " + this.retryPeriod
				+ ", ReliabilityConstraint " + this.reliabilityConstraint
				+ ", LatencyContraint " + this.latencyConstraint + " seconds");
	}

	/**
	 * Instantiates and starts the unified data rate trigger thread.
	 */	
	private void startDataRateTrigger() {
		new Thread(new Runnable() {
			public synchronized void run() {
				logger.info("Starting initial delay of " + initialDelay + " minutes");
				try {
					wait(initialDelay * 60 * 1000);
				} catch (InterruptedException e) {
					logger.error("Error during initial delay of timed trigger thread", e);
				}
				logger.info("Initial delay finished, starting to work");
				
				int retries = 0;
				while (true) {
					if (retries > 0) {
						// This is a retry, so we wait for the retryPeriod collection phase 
						logger.info("Retry optimization after next collection phase, " + retries + " retries");
						retries--;
						waitNotify.setPeriod(retryPeriod);
						waitNotify.doWait();
					} else {
						// Wait until period number of collection (Glossy) phases elapsed
						logger.info("Waiting for next optimization");
						retries = MAX_RETRIES;
						waitNotify.setPeriod(period);
						waitNotify.doWait();
					}
					
					statsLogger.info("OPTIMIZE");
					MacConfiguration newConf = driver.optimize(prepareTopologies(topologyHistory, true),
							reliabilityConstraint,
							latencyConstraint);
					if (newConf != null) {
						logger.debug("Optimization successful, injecting new MAC parameters " + newConf);
						waitNotify.setPeriod(period);
						retries = 0;
						try {
							statsLogger.info("INJECT "+newConf.getTs()+" "+newConf.getTl()+" "+newConf.getN());
							newConf.inject(output);
						} catch (IOException e) {
							logger.error("Injecting new MAC parameters failed", e);
						}
					} else {
						logger.debug("Optimization failed, retrying in " + retryPeriod + " seconds");
					}

					
				}
			}
		}, "unified data rate trigger").start();
	}

	/**
	 * Callback function. Signals that Glossy has finished the
	 * collection of network state information. 
	 */
	public void collectionFinished() {
		if (topologyHistory.isEmpty()) {
			return;
		}
		
		// Check if all nodes switched to another data rate
		double newDataRate = 0.0;
		boolean allNodesSwitchedDataRate = true;
		for (NodeTopologyInfo n : topologyHistory.getFirst().getNodes()) {
			// We are not interested in the sink at it doesn't sent any packets (i.e., its pktRate is always 0.0)
			if (!n.isSink()) {
				if (Double.isNaN(dataRate)) {
					// This is the first time we check the data rate of any node, so we just initialize dataRate
					dataRate = n.getPktRate();
					break;
				}
				
				if (Double.compare(n.getPktRate(), dataRate) == 0) {
					// We found a node with the dataRate, i.e., not all nodes switched to another data rate
					allNodesSwitchedDataRate = false;
					break;
				} else {
					// Remember the new data rate
					newDataRate = n.getPktRate();
				}
			}
		}
		
		if (allNodesSwitchedDataRate) {
			// All nodes switched to another data rate, so we optimize immediately
			logger.debug("All nodes switched data rate from " + dataRate + " to " + newDataRate + ", will optimize immediatly");
			waitNotify.setPeriod(1);
			dataRate = newDataRate;
		}
		
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
