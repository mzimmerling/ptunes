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

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Properties;
import java.util.StringTokenizer;

import org.apache.log4j.Logger;
import org.apache.log4j.PropertyConfigurator;

import sics.adaptMac.triggers.AbstractTrigger;
import sics.adaptMac.triggers.AdaptiveTimedTrigger;
import sics.adaptMac.triggers.EstimationTrigger;
import sics.adaptMac.triggers.InitialOptimizationTrigger;
import sics.adaptMac.triggers.TimedPerformanceTrigger;
import sics.adaptMac.triggers.TimedTrigger;
import sics.adaptMac.triggers.UnifiedDataRateTrigger;

/**
 * Main pTunes controller class.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class AdaptMac { 
	
	// Controller logger
	private static Logger logger = Logger.getLogger(AdaptMac.class.getName());
	
	// Statistics logger
	private static Logger statsLogger = Logger.getLogger("stats");
	
	// Sequence of recently seen topologies
	private static LinkedList<Topology> topologyHistory;

	// Time scale to compute packet rate (1000 converts ms to s)
	protected static final int TIME_SCALE_PACKET_RATE = 1000;
	
	// Controller start-up time: used to compute initial packet rates
	private static final long startTime = System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE;

	// Flushes some lines from the serial buffers before the actual parsing
	private static final int SERIAL_FLUSH_LINES = 5;

	// Representation of sink node
	private static NodeTopologyInfo sink;
	
	// Parameters read from configuration file
	private static int purgeCheckInterval;
	private static int topologyTimeWindow;
	private static int sinkId;
	private static int estimationInterval;
	private static int optimizationPeriod;
	private static int optimizationInitialDelay;
	private static int optimizationRetryPeriod;
	protected static int maximumPeriodOfSilence;
	private static long serialDumpBaudrate;
	private static double reliabilityConstraint;
	private static double latencyConstraint;
	private static boolean withEstimation;
	private static boolean withOptimizationTrigger;
	private static String eclPath;
	private static String eclipsePath;
	private static String serialDumpPath;
	private static String optimizationTriggerName;
	private static String serialPort;
	
	private static EstimationTrigger estimationTrigger;
	private static AbstractTrigger optimizationTrigger;
	
	static class PRRStatistics {
		double min, max, avg;
	}
	
	public static void main(String[] args) {
		// Parse command line arguments
		if (args.length < 1 || args.length > 1) {
			showUsage();
			System.exit(-1);
		}
		
		// Read and parse configuration file
		parseConfiguration(args[0]);
		
		// Creating topology history
		topologyHistory = new LinkedList<Topology>();

		// Representation of sink node
		sink = new NodeTopologyInfo(sinkId, sinkId, 0.0, 1000, new MacConfiguration());
		sink.setPktRate(0);

		// Periodic thread to purge old topologies
		new Thread(new Runnable() {
			public synchronized void run() {
				while (true) {
					try {
						wait(purgeCheckInterval * 60 * 1000);
					} catch (InterruptedException e) {
						logger.error("Error while running purge old topologies thread", e);
					}
					
					HashSet<Topology> toPurge = new HashSet<Topology>();
					synchronized (topologyHistory) {
						if (!topologyHistory.isEmpty()) {
							
							// Determine topologies to be purged
							for (Topology t : topologyHistory) {
								if ((System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE)
										- topologyTimeWindow * 60 > t.getTimestamp()) {
									logger.debug("Purging topology " + t);
									toPurge.add(t);
								}
							}
							topologyHistory.removeAll(toPurge);
							
							// Collect all silent nodes in the current topology from which
							// we haven't heard a packet for maximumPeriodOfSilence.
							Topology lastTopology = topologyHistory.getFirst();
							HashSet<NodeTopologyInfo> silentNodes = new HashSet<NodeTopologyInfo>();
							for (NodeTopologyInfo n : lastTopology.getNodes()) {
								if (  n.getNodeId() != sinkId
								   && (System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE) - maximumPeriodOfSilence * 60 >= n.getTimestamp()) {
									logger.debug("Node " + n.getNodeId() + " is silent (maximumPeriodOfSilence = " + maximumPeriodOfSilence + "), will be purged");
									statsLogger.info("PURGENODE "+n.getNodeId());
									silentNodes.add(n);
								}
							}
							// If there are silent nodes, create a new topology without
							// the silent nodes and notify estimation and optimization triggers
							if (!silentNodes.isEmpty()) {
								StringBuffer buf = new StringBuffer();
								for (NodeTopologyInfo n : silentNodes) {
									buf.append(" ");
									buf.append(n.getNodeId());
								}
								logger.debug("Creating new topology without silent nodes:" + buf.toString());
								topologyHistory.addFirst(new Topology(lastTopology, silentNodes));
								
								if (withEstimation) {
									estimationTrigger.nodesHaveBeenPurged(silentNodes);
								}
								if (withOptimizationTrigger) {
									optimizationTrigger.nodesHaveBeenPurged(silentNodes);
								}
							}
						}
						topologyHistory.notifyAll();
					}
				}
			}
		}, "purge old topologies").start();
		logger.info("Started purge old topologies thread with PurgeCheckInterval " + purgeCheckInterval + " minutes");
		
		// Connect to serial port using serialdump
		String connectToCom = serialDumpPath + " " + "-b" + serialDumpBaudrate + " " + serialPort;
		BufferedWriter serialOutput = connectToCOMPort(connectToCom);
		logger.info("Connected to serial port");
		
		// Create ECLiPSe driver
		EclipseDriver driver = new EclipseDriver(eclPath, eclipsePath);
		
		// Start periodic network performance estimation if selected 
		if (withEstimation) {
			estimationTrigger = new EstimationTrigger(driver, topologyHistory);
		}
		
		// Create and start optimization trigger if selected
		if (withOptimizationTrigger) {
			if (optimizationTriggerName.equals("TimedTrigger")) {
				optimizationTrigger = new TimedTrigger(driver,
						topologyHistory,
						optimizationInitialDelay,
						optimizationPeriod,
						optimizationRetryPeriod,
						serialOutput,
						reliabilityConstraint,
						latencyConstraint);
			} else if (optimizationTriggerName.equals("AdaptiveTimedTrigger")) {
				optimizationTrigger = new AdaptiveTimedTrigger(driver,
						topologyHistory,
						optimizationInitialDelay,
						optimizationRetryPeriod,
						serialOutput,
						reliabilityConstraint,
						latencyConstraint);			
			} else if (optimizationTriggerName.equals("TimedPerformanceTrigger")) {
				optimizationTrigger = new TimedPerformanceTrigger(driver,
						topologyHistory,
						optimizationInitialDelay,
						optimizationPeriod,
						optimizationRetryPeriod,
						serialOutput,
						reliabilityConstraint,
						latencyConstraint);
			} else if (optimizationTriggerName.equals("UnifiedDataRateTrigger")) {
				optimizationTrigger = new UnifiedDataRateTrigger(driver,
						topologyHistory,
						optimizationInitialDelay,
						optimizationPeriod,
						optimizationRetryPeriod,
						serialOutput,
						reliabilityConstraint,
						latencyConstraint);
			} else if (optimizationTriggerName.equals("InitialOptimizationTrigger")) {
				optimizationTrigger = new InitialOptimizationTrigger(driver,
						topologyHistory,
						optimizationInitialDelay,
						serialOutput,
						reliabilityConstraint,
						latencyConstraint);
			} else {
				logger.error("Unknown trigger " + optimizationTriggerName + ", exiting");
				System.exit(-1);
			}
		}
		
		logger.info("Controller successfully started at " + startTime);
	}
	
	private static PRRStatistics computePRRStats(Topology t) {
		AdaptMac.PRRStatistics s = new AdaptMac.PRRStatistics();
		
		// Collect source nodes
		LinkedList<NodeTopologyInfo> sources = new LinkedList<NodeTopologyInfo>();
		for (NodeTopologyInfo n : t.getNodes()) {
			if (!n.isSink()) {
				sources.add(n);
			}
		}
		
		// Compute average, minimum, and maximum
		double sum = 0.0;
		int count = 0;
		s.min = Double.MAX_VALUE;
		s.max = Double.MIN_VALUE;
		for (NodeTopologyInfo l : sources) {
			// Compute path PRR
			double pathPRR = 1.0;
			NodeTopologyInfo iterator = t.getNodeInfo(l.getNodeId());
			while (iterator.getNodeId() != t.getSink().getNodeId()) {
				pathPRR *= Math.sqrt((double) iterator.getPrr() / 1000.0);
				iterator = t.getNodeInfo(iterator.getParentId());
			}
			pathPRR *= Math.sqrt((double) t.getSink().getPrr() / 1000.0);
			
			// Update sum, minimum, and maximum
			count++;
			sum += pathPRR;
			if (pathPRR < s.min) {
				s.min = pathPRR;
			}
			if (pathPRR > s.max) {
				s.max = pathPRR;
			}
		}
		s.avg = sum/count;
		
		return s;
	}
	
	/**
	 * Parses GLOSSY message received from the sink. 
	 * 
	 * @param msg The Glossy message. 
	 * @return NodeTopologyInfo object representing information about a single node.
	 */
	private static NodeTopologyInfo processMsg(String msg) {

		StringTokenizer tokenizer = new StringTokenizer(msg, " ");

		int nodeId = 0, parentId = 0, prr = 0, tl = 0, ts = 0, n = 0;
		double pktRate = 0.0;
		
		// Examines the string
		while (tokenizer.hasMoreTokens()) {

			String token = tokenizer.nextToken();

			if (token.startsWith("o=")) {
				nodeId = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("p=")) {
				parentId = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("pr=")) {
				pktRate = (double) (1.0 / Double.valueOf(token.substring(token.indexOf('=') + 1)));
			} else if (token.startsWith("prr=")) {
				prr = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("tl=")) {
				tl = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("ts=")) {
				ts = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("n=")) {
				n = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			}
		}
		
		return new NodeTopologyInfo(nodeId, parentId, pktRate, prr, new MacConfiguration(tl, ts, n));
	}

	private static void updateTopologyHistory(NodeTopologyInfo nodeInfo) {

		synchronized (topologyHistory) {
			if (!topologyHistory.isEmpty()) {
				// Check the last known topology
				Topology lastTopology = topologyHistory.getFirst();
				if (!lastTopology.getNodes().contains(nodeInfo)) {
					// This is a new node
					// Create a new topology by adding the new node
					topologyHistory.addFirst(new Topology(lastTopology,	nodeInfo));
				} else {
					// Check node info
					LinkedList<NodeTopologyInfo> nodes = new LinkedList<NodeTopologyInfo>(lastTopology.getNodes());
					NodeTopologyInfo knownNodeInfo = nodes.get(nodes.indexOf(nodeInfo));

					// Topology was created less than 5 seconds ago, so we only update. Otherwise, we create a new topology.
					if (lastTopology.getTimestamp() + 5 > System.currentTimeMillis() / AdaptMac.TIME_SCALE_PACKET_RATE) {
/*					// Check whether the parent has changed
					if (knownNodeInfo.getParentId() == nodeInfo.getParentId()) {
*/						// The parent hasn't changed: update node info
						// and re-stamp topology and node
						// PRR = 0 points to a corrupted message, so we don't update
						if (nodeInfo.getPrr() != 0) {
							knownNodeInfo.setPrr(nodeInfo.getPrr());
						}
						knownNodeInfo.setParentId(nodeInfo.getParentId());
						knownNodeInfo.setMacConf(nodeInfo.getMacConf());
						knownNodeInfo.setPktRate(nodeInfo.getPktRate());
						knownNodeInfo.setTimestamp();
						
						lastTopology.setTimestamp();
					} else {
						// The parent has changed: create new topology
						// Carry over pktRate of node whose parent has changed
						nodeInfo.setPktRate(knownNodeInfo.getPktRate());
						topologyHistory.addFirst(new Topology(lastTopology,	nodeInfo));
						
						statsLogger.info("CHANGE "+nodeInfo.getNodeId()+" "+knownNodeInfo.getParentId()+" "+nodeInfo.getParentId());
					}
				}
			} else {
				// This is the very first node reporting
				topologyHistory.addFirst(new Topology(sink, nodeInfo));
			}
			
			topologyHistory.notifyAll();
		}
	}

	private static void printCurrentTopology() {
		if (!topologyHistory.isEmpty()) {
			logger.info(topologyHistory.getFirst());
		}
	}
		
	private static BufferedWriter connectToCOMPort(String connectoToCom) {
		logger.debug("Connecting to serial port using " + connectoToCom);
		
		// Connect to COM using external serialdump application
		try {
			String[] cmd = connectoToCom.split(" ");

			Process serialDumpProcess = Runtime.getRuntime().exec(cmd);
			final BufferedReader input = new BufferedReader(new InputStreamReader(serialDumpProcess.getInputStream()));
			final BufferedReader err = new BufferedReader(new InputStreamReader(serialDumpProcess.getErrorStream()));

			// Start thread listening on stdout
			new Thread(new Runnable() {
				public void run() {
					String line;
					try {
						// Flushes out the first few lines in the serial buffer,
						// this is to avoid using stale info
						int flushLines = SERIAL_FLUSH_LINES;
						while (flushLines > 0 && input.readLine() != null) {
							flushLines--;
						}

						while ((line = input.readLine()) != null) {
							// Remove all non-printable characters
							line = line.replaceAll("[^\\p{Print}]", "");
							logger.info("SERIAL OUT: " + line);
							try {
								if (line.startsWith("A")) {
									statsLogger.info(generatePacketLogMessage(line));
									printCurrentTopology();
								} else if (line.startsWith("G")) {
									updateTopologyHistory(processMsg(line));
								} else if (line.startsWith("F")) {
									if (!topologyHistory.isEmpty()) {
										if (topologyHistory.getFirst().isConsistent()) {
											statsLogger.info(generatePRRLogMessage(topologyHistory.getFirst()));
										}
									}
									printCurrentTopology();
									if (withEstimation) {
										estimationTrigger.collectionFinished();
									}
									if (withOptimizationTrigger) {
										optimizationTrigger.collectionFinished();
									}
								} else {
									logger.warn("Unknown serial message");
								}
							} catch (ArithmeticException e) {
								logger.warn("Not enough stats have been collected for the figures to be meaningful", e); 
							} catch (NumberFormatException e) {
								logger.warn("Received corrupted data from serialdump", e);
							}
						}
						input.close();
						logger.warn("Serialdump process shut down, exiting");
						System.exit(1);
					} catch (IOException e) {
						logger.error("Reading from serialdump stdout failed", e);
						System.exit(1);
					}
				}
			}, "read input stream thread").start();
			logger.info("Started thread listening on serialdump stdout");
			
			// Start thread listening on stderr
			new Thread(new Runnable() {
				public void run() {
					String line;
					try {
						while ((line = err.readLine()) != null) {
							logger.info("SERIAL ERR: serialdump error stream: " + line);
						}
						err.close();
					} catch (IOException e) {
						logger.error("Reading from serialdump stderr failed", e);
						System.exit(1);
					}
				}
			}, "read error stream thread").start();
			logger.info("Started thread listening on serialdump stderr");
			
			return new BufferedWriter(new OutputStreamWriter(serialDumpProcess.getOutputStream()));
			
		} catch (Exception e) {
			logger.error("Connecting to serial port using " + connectoToCom + " failed, exiting", e);
			System.exit(1);
		}
		
		return null;
	}
	
	private static void parseConfiguration(String filename) {
		// Read configuration file
		Properties p = new Properties();
		try {
			FileInputStream in = new FileInputStream(filename);
			p.load(in);
			in.close();

		} catch (IOException e) {
			System.err.println("Reading configuration file " + filename + " failed, exiting");
			e.printStackTrace();
			System.exit(1);
		}
		
		// Setup logging
		String log4jPropertiesPath = p.getProperty("Log4jPropertiesPath");
		if (log4jPropertiesPath == null) {
			System.err.println("Log4jPropertiesPath not defined, exiting");
			System.exit(1);
		}
		PropertyConfigurator.configure(log4jPropertiesPath);
		
		// Parse configuration properties
		logger.info("Parsing Controller configuration");
		logger.debug(filename + " contains the following properties: "+p.toString());
		try {
			reliabilityConstraint = Double.parseDouble(p.getProperty("ReliabilityConstraint"));
			latencyConstraint = Double.parseDouble(p.getProperty("LatencyConstraint"));
			purgeCheckInterval = Integer.parseInt(p.getProperty("PurgeCheckInterval"));
			topologyTimeWindow = Integer.parseInt(p.getProperty("TopologyTimeWindow"));
			maximumPeriodOfSilence = Integer.parseInt(p.getProperty("MaximumPeriodOfSilence"));
			sinkId = Integer.parseInt(p.getProperty("SinkId"));
			serialDumpBaudrate = Long.parseLong(p.getProperty("SerialDumpBaudrate"));
			estimationInterval = Integer.parseInt(p.getProperty("EstimationInterval"));
			optimizationPeriod = Integer.parseInt(p.getProperty("OptimizationPeriod"));
			optimizationInitialDelay = Integer.parseInt(p.getProperty("OptimizationInitialDelay"));
			optimizationRetryPeriod = Integer.parseInt(p.getProperty("OptimizationRetryPeriod"));
			withEstimation = Boolean.parseBoolean(p.getProperty("WithEstimation"));
			withOptimizationTrigger = Boolean.parseBoolean(p.getProperty("WithOptimizationTrigger"));
			eclPath = p.getProperty("EclPath");
			if (eclPath == null) {
				throw new Exception("EclPath not defined");
			}
			eclipsePath = p.getProperty("EclipsePath");
			if (eclipsePath == null) {
				throw new Exception("EclipsePath not defined");
			}
			serialDumpPath = p.getProperty("SerialDumpPath");
			if (serialDumpPath == null) {
				throw new Exception("SerialDumpPath not defined");
			}
			optimizationTriggerName = p.getProperty("OptimizationTrigger");
			if (optimizationTriggerName == null) {
				throw new Exception("OptimizationTrigger not defined");
			}
			serialPort = p.getProperty("SerialPort");
			if (serialPort == null) {
				throw new Exception("SerialPort not defined");
			}
		} catch (Exception e) {
			logger.error("Parsing configuration failed, exiting", e);
			System.exit(1);
		}
		
		StringBuffer c = new StringBuffer();
		c.append("\nSerialDumpPath = ");
		c.append(serialDumpPath);
		c.append("\nSerialDumpBaudrate = ");
		c.append(serialDumpBaudrate);		
		c.append("\nSerialPort = ");
		c.append(serialPort);
		c.append("\nSinkId = ");
		c.append(sinkId);
		c.append("\nEclipsePath = ");
		c.append(eclipsePath);
		c.append("\nEclPath = ");
		c.append(eclPath);		
		c.append("\nPurgeCheckInterval = ");
		c.append(purgeCheckInterval);
		c.append("\nTopologyTimeWindow = ");
		c.append(topologyTimeWindow);
		c.append("\nMaximumPeriodOfSilence = ");
		c.append(maximumPeriodOfSilence);
		c.append("\nWithEstimation = ");
		c.append(withEstimation);
		c.append("\nEstimationInterval = ");
		c.append(estimationInterval);
		c.append("\nWithOptimizationTrigger = ");
		c.append(withOptimizationTrigger);		
		c.append("\nOptimizationTrigger = ");
		c.append(optimizationTriggerName);
		c.append("\nOptimizationInitialDelay = ");
		c.append(optimizationInitialDelay);
		c.append("\nOptimizationPeriod = ");
		c.append(optimizationPeriod);
		c.append("\nOptimizationRetryPeriod = ");
		c.append(optimizationRetryPeriod);
		c.append("\nReliabilityConstraint = ");
		c.append(reliabilityConstraint);
		c.append("\nLatencyConstraint = ");
		c.append(latencyConstraint);
		logger.info("Starting Controller with the following configuration:" + c);
	}
	
	private static void showUsage() {
		System.err.println("Usage: java -jar AdaptMac.jar <configFile>");
	}
	
	private static String generatePacketLogMessage(String msg) {
		int nodeId = 0, seqNo = 0;
		int queuing_delay = 0, dropped_packets_count = 0, queue_size = 0;
		long t_tx = 0, t_rx = 0, t_cpu = 0, t_lpm = 0;
		long t_glossy_tx = 0, t_glossy_rx = 0, t_glossy_cpu = 0;
		long latency = 0;
		
		StringTokenizer tokenizer = new StringTokenizer(msg, " ");
		while (tokenizer.hasMoreTokens()) {
			String token = tokenizer.nextToken();

			if (token.startsWith("o=")) {
				nodeId = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("seq=")) {
				seqNo = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("qd=")) {
				queuing_delay = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("dp=")) {
				dropped_packets_count = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("qs=")) {
				queue_size = Integer.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("tx=")) {
				t_tx = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("rx=")) {
				t_rx = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("cpu=")) {
				t_cpu = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("lpm=")) {
				t_lpm = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("lat=")) {
				latency = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("gtx=")) {
				t_glossy_tx = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("grx=")) {
				t_glossy_rx = Long.valueOf(token.substring(token.indexOf('=') + 1));
			} else if (token.startsWith("gcpu=")) {
				t_glossy_cpu = Long.valueOf(token.substring(token.indexOf('=') + 1));
			}
		}
		
		StringBuffer buf = new StringBuffer();
		buf.append("PACKET");
		buf.append(" ");
		buf.append(nodeId);
		buf.append(" ");
		buf.append(seqNo);
		buf.append(" ");
		buf.append(queuing_delay);
		buf.append(" ");
		buf.append(dropped_packets_count);
		buf.append(" ");
		buf.append(queue_size);
		buf.append(" ");
		buf.append(t_rx);
		buf.append(" ");
		buf.append(t_tx);
		buf.append(" ");
		buf.append(t_cpu);
		buf.append(" ");
		buf.append(t_lpm);
		buf.append(" ");
		buf.append(t_glossy_rx);
		buf.append(" ");
		buf.append(t_glossy_tx);
		buf.append(" ");
		buf.append(t_glossy_cpu);
		buf.append(" ");
		buf.append(latency);
		
		return buf.toString();
	}
	
	private static String generatePRRLogMessage(Topology t) {
		PRRStatistics s = computePRRStats(t);
		
		StringBuffer buf = new StringBuffer();
		buf.append("PRR");
		buf.append(" ");
		buf.append(s.min);
		buf.append(" ");
		buf.append(s.max);
		buf.append(" ");
		buf.append(s.avg);
		
		return buf.toString();
	}
	
}
