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
 * A trigger that optimizes periodically, but optimizes (almost) immediately if it detects
 * the beginning or the end of a traffic peak. In addition, the trigger optimizes more
 * more often during traffic peakes (SHORT_PERIOD) and less often otherwise (LONG_PERIOD).
 * 
 * The trigger uses a data rate threshold (CHECK_DATA_RATE) to detect traffic peaks.
 * If at least one node starts to send with a data rate > CHECK_DATA_RATE, the trigger
 * perceives this as the start of a peak and will optimize after the next Glossy phase;
 * if all nodes start to send with a data rate < CHECK_DATA_RATE, the trigger perceives
 * this as the end of a peak and will immediately optimize. 
 * 
 * Used for traffic peak experiments.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public class AdaptiveTimedTrigger extends AbstractTrigger {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(AdaptiveTimedTrigger.class.getName());

	// Statistics logger
	private static Logger statsLogger = Logger.getLogger("stats");
	
	// Maximum number of optimization retries
	private static final int MAX_RETRIES = 4;
	
	// Optimization period during traffic peaks (in number of Glossy phases)
	private static final int SHORT_PERIOD = 5;
	
	// Optimization period if there is no traffic peak (in number of Glossy phases)
	private static final int LONG_PERIOD = 10;
	
	// Data rate threshold to detect traffic peaks
	private static final double CHECK_DATA_RATE = 1.0/20.0;

	// ECLiPSe driver
	private final EclipseDriver driver;
	
	// History of topologies
	private final LinkedList<Topology> topologyHistory;
	
	// Stream to output determined MAC parameters via serialdump to the sink node
	private final BufferedWriter output;
	
	// Helper object to notify and wake up the timed trigger thread
	private final WaitNotify waitNotify;
	
	// Initial delay in minutes
	private final int initialDelay;
	
	// Current optimization period (in number of Glossy phases)
	private int period;

	// Number of Glossy phases between failed optimization and retry
	private final int retryPeriod;
	
	// End-to-end constraints
	private final double reliabilityConstraint;
	private final double latencyConstraint;
	
	public AdaptiveTimedTrigger(final EclipseDriver driver,
			final LinkedList<Topology> topologyHistory,
			final int initialDelay,
			final int retryPeriod,
			final BufferedWriter output,
			final double reliabilityConstraint,
			final double latencyConstraint) {
		this.driver = driver;
		this.topologyHistory = topologyHistory;
		this.period = LONG_PERIOD;
		this.waitNotify = new WaitNotify(LONG_PERIOD);
		this.initialDelay = initialDelay;
		this.retryPeriod = retryPeriod;
		this.output = output;
		this.reliabilityConstraint = reliabilityConstraint;
		this.latencyConstraint = latencyConstraint;
		
		// Start the adaptive timed trigger thread
		startDataRateTrigger();
		logger.info("Started AdaptiveTimedTrigger with OptimizationTriggerInitialDelay " + this.initialDelay
				+ " minutes, ShortOptimizationPeriod " + SHORT_PERIOD
				+ ", LongOptimizationPeriod " + LONG_PERIOD
				+ ", OptimizationRetryPeriod = " + this.retryPeriod
				+ ", ReliabilityConstraint " + this.reliabilityConstraint
				+ ", LatencyContraint " + this.latencyConstraint + " seconds");
	}

	/**
	 * Instantiates and starts the adaptive timed trigger thread.
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
		}, "adaptive timed trigger").start();
	}

	/**
	 * Callback function. Signals that Glossy has finished the
	 * collection of network state information. 
	 */
	public void collectionFinished() {
		if (topologyHistory.isEmpty()) {
			return;
		}
		
		// Check if there is a node sending at the high data rate
		boolean containsNodeWithHighDataRate = false;
		for (NodeTopologyInfo n : topologyHistory.getFirst().getNodes()) {
			if (Double.compare(n.getPktRate(), CHECK_DATA_RATE) > 0) {
				logger.debug("Found a node (id = " + n.getNodeId() + ") with data rate " + n.getPktRate() + " which is higher than check data rate " + CHECK_DATA_RATE);
				containsNodeWithHighDataRate = true;
				break;
			}
		}
		
		if (containsNodeWithHighDataRate && period == LONG_PERIOD) {
			// Peak just started: optimize at next Glossy phase and shorten the optimization period
			logger.debug("Detected start of peak, will optimize in 1 period and set period to " + SHORT_PERIOD);
			waitNotify.setPeriod(2);
			period = SHORT_PERIOD;
		} else if (!containsNodeWithHighDataRate && period == SHORT_PERIOD) {
			// Peak just ended: optimize immediately and prolong the optimization period
			logger.debug("Detected end of peak, will optimize immediately and set period to " + LONG_PERIOD);
			waitNotify.setPeriod(1);
			period = LONG_PERIOD;
		} else {
			// Do nothing
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
