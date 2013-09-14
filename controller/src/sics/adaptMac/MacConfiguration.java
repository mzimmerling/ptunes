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

import java.io.BufferedWriter;
import java.io.IOException;

import org.apache.log4j.Logger;

/**
 * Simple class encapsulating the MAC parameters considered
 * for adaptation using pTunes.
 * 
 * @author Marco Zimmerling (zimmerling@tik.ee.eth.ch)
 * @author Luca Mottola (luca.mottola@polimi.it)
 *
 */
public class MacConfiguration {

	// Controller logger
	private static Logger logger = Logger.getLogger(MacConfiguration.class.getName());

	// MAC params
	private int tl;
	private int ts;
	private int n;

	public MacConfiguration() {
		this.tl = 0;
		this.ts = 0;
		this.n = 0;
	}

	public MacConfiguration(int tl, int ts, int n) {
		this.tl = tl;
		this.ts = ts;
		this.n = n;
	}

	public int getTl() {
		return tl;
	}

	public void setTl(int tl) {
		this.tl = tl;
	}

	public int getTs() {
		return ts;
	}

	public void setTs(int ts) {
		this.ts = ts;
	}

	public int getN() {
		return n;
	}

	public void setN(int n) {
		this.n = n;
	}

	public String toString() {
		return "MacConf: t_l=" + tl + " t_s=" + ts + " n=" + n;
	}

	public void inject(BufferedWriter output) throws IOException {
		String inject = tl + "," + ts + "," + n + ",\n";
		logger.info("MAC_CONFIGURATION: Injecting string " + inject);

		output.write(inject);
		output.flush();
	}

	public boolean equals(Object o) {
		if (!(o instanceof MacConfiguration)) {
			return false;
		}
		MacConfiguration c = (MacConfiguration) o;
		
		return (c.tl == this.tl && c.ts == this.ts && c.n == this.n);
	}
}
