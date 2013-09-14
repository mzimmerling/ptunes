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
 * Simple class encapsulating the performance figures
 * considered by pTunes.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class NetworkPerformance {

	private double lifetime;
	private double reliability;
	private double latency;
	private double maxQueuingRate;

	public NetworkPerformance(double lifetime, double reliability, double latency, double queuingRate) {
		this.lifetime = lifetime;
		this.reliability = reliability;
		this.latency = latency;
		this.maxQueuingRate = queuingRate;
	}

	public double getLifetime() {
		return lifetime;
	}

	public double getReliability() {
		return reliability;
	}

	public double getLatency() {
		return latency;
	}
	
	public double getMaxQueuingRate() {
		return maxQueuingRate;
	}

	public String toString() {
		return "Lifetime=" + lifetime
		        + " Min E-E Reliability=" + reliability
				+ " Max E-E Latency=" + latency
				+ " Max Queuing Rate=" + maxQueuingRate;
	}
}
