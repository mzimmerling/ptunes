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

import org.apache.log4j.Logger;

/**
 * Helper class to let triggers notify, wake up, and sleep.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 */
public class WaitNotify {
	
	// Controller logger
	private static Logger logger = Logger.getLogger(WaitNotify.class.getName());
	
	// Helper class
	private class MonitorObject { }
	
	// Helper object, our monitor
	private final MonitorObject monitor = new MonitorObject();
	
	// Counts the number of notifications on the monitor object
	// A notification corresponds to a Glossy phase in our case
	private int wasSignalled = 0;
	
	// Number of notifications for the trigger thread should sleep
	private int period;
	
	/**
	 * Default constructor with period 1.
	 */
	public WaitNotify() {
		this.period = 1;
	}
	
	/**
	 * Constructor with variable period.
	 * 
	 * @param period Ther period.
	 */
	public WaitNotify(final int period) {
		this.period = period;
	}
	
	/**
	 * Let's the thread sleep until wasSignalled exceeds period.
	 * This method is called by the trigger threads to enter
	 * sleep mode.
	 * 
	 */
	public void doWait() {
		synchronized (monitor) {
			while (wasSignalled < period) {
				try {
					monitor.wait();
				} catch (InterruptedException e) {
					logger.error("Error while waiting", e);
				}
			}
			wasSignalled = 0;
		}
	}
	
	/**
	 * Increments wasSignalled and notifies the monitor object.
	 * This method is called by the triggers collectionFinished().
	 */
	public void doNotify() {
		synchronized (monitor) {
			wasSignalled++;
			monitor.notify();
		}
	}
	
	/**
	 * Sets a new period.
	 * 
	 * @param period The new period.
	 */
	public void setPeriod(final int period) {
		synchronized (monitor) {
			this.period = period; 
			wasSignalled = 0;
		}
	}
	
}
