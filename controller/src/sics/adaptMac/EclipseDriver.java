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

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.LinkedList;
import java.util.Properties;

import org.apache.log4j.Logger;

import com.parctechnologies.eclipse.EXDRInputStream;
import com.parctechnologies.eclipse.EXDROutputStream;
import com.parctechnologies.eclipse.EclipseEngine;
import com.parctechnologies.eclipse.EclipseEngineOptions;
import com.parctechnologies.eclipse.EclipseException;
import com.parctechnologies.eclipse.EmbeddedEclipse;
import com.parctechnologies.eclipse.FromEclipseQueue;
import com.parctechnologies.eclipse.ToEclipseQueue;

/**
 * ECLiPSe driver enabling the Java controller to estimate the
 * current network performance or to trigger the optimization
 * using the Inverval Constraint (IC) solver provided by the 
 * ECLiPSe constraint programming system.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.ethz.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class EclipseDriver {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(EclipseDriver.class.getName());
	
	// ECLiPSe queues
	private static final String TO_ECLIPSE = "java_to_eclipse";
	private static final String FROM_ECLIPSE = "eclipse_to_java";
	
	// X-MAC parameters optimized for reliability
	public static final int XMAC_REL_TL = 16;
	public static final int XMAC_REL_TS = 20;
	public static final int XMAC_REL_N = 10;
	
	// LPP parameters optimized for reliability
	public static final int LPP_REL_TL = 71;
	public static final int LPP_REL_TS = 20;
	public static final int LPP_REL_N = 10;
	
	// EXDR translation of data going out from Java
	private EXDROutputStream java_to_eclipse_formatted;

	// EXDR translation of data coming in from eclipse
	private EXDRInputStream eclipse_to_java_formatted;

	// Object representing the Eclipse process
	EclipseEngine eclipse;
	
	public EclipseDriver(String pathToECL, String pathToEclipse) {	
		Properties eclipseOpts = new Properties();
		eclipseOpts.setProperty("eclipse.directory", pathToEclipse);

		// Create some default Eclipse options
		EclipseEngineOptions eclipseEngineOptions = new EclipseEngineOptions(eclipseOpts);

		// Path of the Eclipse program
		File eclipseProgram;

		// Data going out from java
		ToEclipseQueue java_to_eclipse;

		// Data coming in from eclipse
		FromEclipseQueue eclipse_to_java;

		// Connect the Eclipse's standard streams to the JVM's
		eclipseEngineOptions.setUseQueues(false);

		try {
			eclipse = EmbeddedEclipse.getInstance(eclipseEngineOptions);
			eclipseProgram = new File(pathToECL);
			eclipse.compile(eclipseProgram);
			java_to_eclipse = eclipse.getToEclipseQueue(TO_ECLIPSE);
			eclipse_to_java = eclipse.getFromEclipseQueue(FROM_ECLIPSE);

			// Set up the two formatting streams
			java_to_eclipse_formatted = new EXDROutputStream(java_to_eclipse);
			eclipse_to_java_formatted = new EXDRInputStream(eclipse_to_java);

		} catch (EclipseException e) {
			logger.error("Setting up ECLiPSe failed", e);
		} catch (IOException e) {
			logger.error("Setting up ECLiPSe failed", e);
		}
		
		logger.info("Started ECLiPSe driver");
	}

	@SuppressWarnings("unchecked")
	public synchronized NetworkPerformance performance(Collection<Object> topologies, MacConfiguration macConf) {
		// Prepare current parameterization
		Collection<Integer> variables = new LinkedList<Integer>();
		variables.add(macConf.getTl());
		variables.add(macConf.getTs());
		variables.add(macConf.getN());

		LinkedList<Object> performance = null;
		try {
			// Send data to ECLiPSe
			logger.info("Estimating network performance ...");
			logger.info("Sending current topology and configuration to ECLiPSe");
			java_to_eclipse_formatted.write(topologies);
			java_to_eclipse_formatted.write(variables);
			java_to_eclipse_formatted.flush();
			
			logger.info("Estimating");
			// Call performance predicate
			eclipse.rpc("performance");
			
			// Retrieve performance estimates from ECLiPSe
			logger.info("Retrieving estimates from ECLiPSe");
			performance = (LinkedList<Object>) eclipse_to_java_formatted.readTerm();
			NetworkPerformance netPerf = new NetworkPerformance(((Double) performance.get(0)).doubleValue(),
					((Double) performance.get(1)).doubleValue(),
					((Double) performance.get(2)).doubleValue(),
					((Double) performance.get(3)).doubleValue());
			logger.info("Estimated network performance: " + netPerf);
			
			return netPerf; 
		} catch (IOException e) {
			logger.error("Estimating network performance failed", e);
		} catch (EclipseException e) {
			logger.error("Estimating network performance failed", e);
		}

		return null;
	}

	@SuppressWarnings("unchecked")
	public synchronized MacConfiguration optimize(Collection<Object> topologies, double reliabilityConstraint, double latencyConstraint) {
		try {
			// Prepare end-to-end constraints
			Collection<Double> constraints = new LinkedList<Double>();
			constraints.add(reliabilityConstraint);
			constraints.add(latencyConstraint);
			
			// Send topologies and bounds to ECLiPSe
			logger.info("Computing optimal MAC configuration ...");
			logger.info("Sending topologies and end-to-end constraints to ECLiPSe");
			java_to_eclipse_formatted.write(topologies);
			java_to_eclipse_formatted.write(constraints);
			java_to_eclipse_formatted.flush();
			
			// Execute optimization
			System.out.println("Optimizing");
			eclipse.rpc("optimize");

			// Output results
			logger.info("Retrieving optimal MAC configuration from ECLiPSe");
			LinkedList<Object> params = (LinkedList<Object>) eclipse_to_java_formatted.readTerm();
			MacConfiguration optMacConf = new MacConfiguration(((Integer) params.get(0)).intValue(),
					((Integer) params.get(1)).intValue(),
					((Integer) params.get(2)).intValue());
			logger.info("Optimal MAC configuration: " + optMacConf);

			return optMacConf; 

		} catch (IOException e) {
			logger.error("Computing optimal MAC configuration failed", e);
		} catch (EclipseException e) {
			logger.error("Computing optimal MAC configuration failed", e);
		}

		return null;
	}

	private void destroyEclipse() {
		// Destroy the Eclipse driver
		logger.warn("Destroying ECLiPSe driver");
		try {
			((EmbeddedEclipse) eclipse).destroy();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	protected void finalize() throws Throwable {
		destroyEclipse();
		super.finalize();
	}

}