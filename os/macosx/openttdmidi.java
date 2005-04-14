//
//  OpenTTDMidi.java
//  OpenTTDMidi
//
//  Created by Joshua King on Sun Apr 25 2004.
//  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
//
import java.io.*;
import java.util.*;
import javax.sound.midi.*;

public class OpenTTDMidi {

    public static void main (String args[]) {
        // Currently command line is the MIDI file
		if (args.length == 1) {
			Sequencer s2 = null;
			
			try {
				s2 = MidiSystem.getSequencer();
				s2.open();
			} catch (MidiUnavailableException mue) {
				System.exit(1);
			}
			
			Sequence s = null;
			
			try {
				s = MidiSystem.getSequence(new File(args[0]));
			} catch (InvalidMidiDataException imde) {
				System.exit(2);
			} catch (IOException ioe) {
				System.exit(3);
			}
			
			try {
				s2.setSequence(s);
				s2.setMicrosecondPosition(0);
				s2.start();
				for (long l = 0; l < (s.getMicrosecondLength() / 1000000); l++) {
					try {
						//System.out.print(".");
						Thread.currentThread().sleep(1000);
					} catch (InterruptedException ie) {}
				}
				System.out.println();
			} catch (InvalidMidiDataException imde) {
			}
			
			s2.stop();
			s2.close();
			System.exit(0);
		}	
    }
}
