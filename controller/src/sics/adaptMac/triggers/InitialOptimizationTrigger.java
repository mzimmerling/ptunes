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
 * A trigger that uses ECLiPSe exactly once to determine an optimized MAC configuration.
 * However, the trigger retries the optimization up to ten times if it failed (e.g., due
 * to inconsistant topology such as a missing parent) or if the end-to-end constraints
 * were not satisfiable and the optimizer returned parameters geared toward reliability.
 * In that sense, the trigger tries to determine a proper optimized MAC configuration.  
 *  
 * Used for experiments where we want an initial, optimization MAC configuration. 
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public class InitialOptimizationTrigger extends AbstractTrigger {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(InitialOptimizationTrigger.class.getName());

	// Statistics logger
	private static Logger statsLogger = Logger.getLogger("stats");
	
	// Maximum number of optimization retries
	private static final int MAX_RETRIES = 10;
	
	// ECLiPSe driver
	private final EclipseDriver driver;
	
	// History of topologies
	private final LinkedList<Topology> topologyHistory;
	
	// Stream to output determined MAC parameters via serialdump to the sink node
	private final BufferedWriter output;
	
	// Helper object to notify and wake up the initial optimization trigger thread
	private final WaitNotify waitNotify;
	
	// Initial delay in minutes.
	private final int initialDelay;
	
	// End-to-end constraints
	private final double reliabilityConstraint;
	private final double latencyConstraint;
	
	/**
	 * Constructor to create an instance of this trigger.
	 * 
	 * @param driver The ECLiPSe driver.
	 * @param topologyHistory History of topologies.
	 * @param initialDelay Initial delay in minutes.
	 * @param output Stream to output parameters to serialdump.
	 * @param reliabilityConstraint End-to-end constraint on reliability.
	 * @param latencyConstraint End-to-end constraint on latency.
	 */
	public InitialOptimizationTrigger(final EclipseDriver driver,
			final LinkedList<Topology> topologyHistory,
			final int initialDelay,
			final BufferedWriter output,
			final double reliabilityConstraint,
			final double latencyConstraint) {
		this.driver = driver;
		this.topologyHistory = topologyHistory;
		this.initialDelay = initialDelay;
		this.output = output;
		this.reliabilityConstraint = reliabilityConstraint;
		this.latencyConstraint = latencyConstraint;
		this.waitNotify = new WaitNotify();
		
		// Start the initial optimization trigger thread
		startInitialOptimizationTrigger();
		logger.info("Started InitialOptimizationTrigger with OptimizationTriggerInitialDelay " + this.initialDelay
				+ " minutes, ReliabilityConstraint " + this.reliabilityConstraint
				+ ", LatencyContraint " + this.latencyConstraint + " seconds");
	}

	/**
	 * Instantiates and starts the initial optimization trigger thread.
	 */
	private void startInitialOptimizationTrigger() {
		new Thread(new Runnable() {
			public synchronized void run() {
				logger.info("Starting initial delay of " + initialDelay + " minutes");
				try {
					wait(initialDelay * 60 * 1000);
				} catch (final InterruptedException e) {
					logger.error("Error during initial delay of initial optimization trigger thread", e);
				}
				logger.info("Initial delay finished, starting to work");
				
				int retries = 0;
				boolean initialOptimizationDone = false;
				while (!initialOptimizationDone && retries <= MAX_RETRIES) {
					// Wait for next Glossy phase
					waitNotify.doWait();
					
					// Glossy reported, so we trigger an optimization
					statsLogger.info("OPTIMIZE");
					final MacConfiguration newConf = driver.optimize(prepareTopologies(topologyHistory, true), reliabilityConstraint, latencyConstraint);
					if (newConf != null) {
						if (  (newConf.getTl() == EclipseDriver.XMAC_REL_TL && newConf.getTs() == EclipseDriver.XMAC_REL_TS && newConf.getN() == EclipseDriver.XMAC_REL_N) 
						   || (newConf.getTl() == EclipseDriver.LPP_REL_TL && newConf.getTs() == EclipseDriver.LPP_REL_TS && newConf.getN() == EclipseDriver.LPP_REL_N) ) {
							// The end-to-end constraints were not satisfiable and the optimizer thus
							// returned parameters that are optimized for reliability. So we optimize
							// again at the next Glossy phase, hoping the constraints become satisfiable.
							logger.debug("Optimization successful but constraints were not satisfiable, injecting parameters optimized for reliability " + newConf);
						} else {
							// The end-to-end constraints were satisfiable, so we are done
							logger.debug("Optimization successful and constraints were satisfiable, injecting parameters " + newConf);							
							initialOptimizationDone = true;
						}
						
						// Inject the new parameters.
						try {
							statsLogger.info("INJECT " + newConf.getTs() + " " + newConf.getTl() + " " + newConf.getN());
							newConf.inject(output);
						} catch (final IOException e) {
							logger.error("Injecting new MAC parameters failed", e);
						}
					} else {
						logger.debug("Optimization failed, retrying at next Glossy phase");
						retries++;
					}
				}
				logger.debug("Initial optimization trigger finished after " + retries + " retries");
			}
		}, "initial optimization trigger").start();
	}

	/**
	 * Callback function. Signals that Glossy has finished the
	 * collection of network state information. 
	 */
	public void collectionFinished() {
		if (topologyHistory.isEmpty()) {
			return;
		}
		
		// Notify and wake up the initial optimization trigger thread.
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
