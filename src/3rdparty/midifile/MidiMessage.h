//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat Feb 14 20:36:32 PST 2015
// Last Modified: Sat Apr 21 10:52:19 PDT 2018 Removed using namespace std;
// Filename:      midifile/include/MidiMessage.h
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   Storage for bytes of a MIDI message for use in MidiFile
//                class.
//

#ifndef _MIDIMESSAGE_H_INCLUDED
#define _MIDIMESSAGE_H_INCLUDED

#include <iostream>
#include <string>
#include <utility>
#include <vector>


namespace smf {

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned long  ulong;

class MidiMessage : public std::vector<uchar> {

	public:
		               MidiMessage          (void);
		               MidiMessage          (int command);
		               MidiMessage          (int command, int p1);
		               MidiMessage          (int command, int p1, int p2);
		               MidiMessage          (const MidiMessage& message);
		               MidiMessage          (const std::vector<uchar>& message);
		               MidiMessage          (const std::vector<char>& message);
		               MidiMessage          (const std::vector<int>& message);

		              ~MidiMessage          ();

		MidiMessage&   operator=            (const MidiMessage& message);
		MidiMessage&   operator=            (const std::vector<uchar>& bytes);
		MidiMessage&   operator=            (const std::vector<char>& bytes);
		MidiMessage&   operator=            (const std::vector<int>& bytes);

		void           sortTrack            (void);
		void           sortTrackWithSequence(void);

		static std::vector<uchar> intToVlv  (int value);
		static double  frequencyToSemitones (double frequency, double a4frequency = 440.0);

		// data access convenience functions (returns -1 if not present):
		int            getP0                (void) const;
		int            getP1                (void) const;
		int            getP2                (void) const;
		int            getP3                (void) const;
		void           setP0                (int value);
		void           setP1                (int value);
		void           setP2                (int value);
		void           setP3                (int value);

		int            getSize              (void) const;
		void           setSize              (int asize);
		int            setSizeToCommand     (void);
		int            resizeToCommand      (void);

		// note-message convenience functions:
		int            getKeyNumber         (void) const;
		int            getVelocity          (void) const;
		void           setKeyNumber         (int value);
		void           setVelocity          (int value);
		void           setSpelling          (int base7, int accidental);
		void           getSpelling          (int& base7, int& accidental);

		// controller-message convenience functions:
		int            getControllerNumber  (void) const;
		int            getControllerValue   (void) const;

		int            getCommandNibble     (void) const;
		int            getCommandByte       (void) const;
		int            getChannelNibble     (void) const;
		int            getChannel           (void) const;

		void           setCommandByte       (int value);
		void           setCommand           (int value);
		void           setCommand           (int value, int p1);
		void           setCommand           (int value, int p1, int p2);
		void           setCommandNibble     (int value);
		void           setChannelNibble     (int value);
		void           setChannel           (int value);
		void           setParameters        (int p1, int p2);
		void           setParameters        (int p1);
		void           setMessage           (const std::vector<uchar>& message);
		void           setMessage           (const std::vector<char>& message);
		void           setMessage           (const std::vector<int>& message);

		// message-type convenience functions:
		bool           isMetaMessage        (void) const;
		bool             isMeta             (void) const;
		bool           isNote               (void) const;
		bool             isNoteOff          (void) const;
		bool             isNoteOn           (void) const;
		bool           isAftertouch         (void) const;
		bool           isController         (void) const;
		bool             isSustain          (void) const;  // controller 64
		bool             isSustainOn        (void) const;
		bool             isSustainOff       (void) const;
		bool             isSoft             (void) const;  // controller 67
		bool             isSoftOn           (void) const;
		bool             isSoftOff          (void) const;
		bool           isPatchChange        (void) const;
		bool             isTimbre           (void) const;
		bool           isPressure           (void) const;
		bool           isPitchbend          (void) const;
		bool           isEmpty              (void) const;  // see MidiFile::removeEmpties()

		// helper functions to create various MidiMessages:
		void           makeNoteOn           (int channel, int key, int velocity);
		void           makeNoteOff          (int channel, int key, int velocity);
		void           makeNoteOff          (int channel, int key);
		void           makeNoteOff          (void);
		void           makePatchChange      (int channel, int patchnum);
		void           makeTimbre           (int channel, int patchnum);
		void           makeController       (int channel, int num, int value);
		void           makePitchBend        (int channel, int lsb, int msb);
		void           makePitchBend        (int channel, int value);
		void           makePitchBendDouble  (int channel, double value);
		void           makePitchbend        (int channel, int lsb, int msb) { makePitchBend(channel, lsb, msb); }
		void           makePitchbend        (int channel, int value) { makePitchBend(channel, value); }
		void           makePitchbendDouble  (int channel, double value) { makePitchBendDouble(channel, value); }

		// helper functions to create various continuous controller messages:
		void           makeSustain          (int channel, int value);
		void           makeSustainPedal     (int channel, int value);
		void           makeSustainOn        (int channel);
		void           makeSustainPedalOn   (int channel);
		void           makeSustainOff       (int channel);
		void           makeSustainPedalOff  (int channel);

		// meta-message creation and helper functions:
		void           makeMetaMessage      (int mnum, const std::string& data);
		void           makeText             (const std::string& name);
		void           makeCopyright        (const std::string& text);
		void           makeTrackName        (const std::string& name);
		void           makeInstrumentName   (const std::string& name);
		void           makeLyric            (const std::string& text);
		void           makeMarker           (const std::string& text);
		void           makeCue              (const std::string& text);
		void           makeKeySignature     (int fifths, bool mode = 0);
		void           makeTimeSignature    (int top, int bottom,
		                                     int clocksPerClick = 24,
		                                     int num32dsPerQuarter = 8);

		void           makeTempo            (double tempo) { setTempo(tempo); }
		int            getTempoMicro        (void) const;
		int            getTempoMicroseconds (void) const;
		double         getTempoSeconds      (void) const;
		double         getTempoBPM          (void) const;
		double         getTempoTPS          (int tpq) const;
		double         getTempoSPT          (int tpq) const;

		int            getMetaType          (void) const;
		bool           isText               (void) const;
		bool           isCopyright          (void) const;
		bool           isTrackName          (void) const;
		bool           isInstrumentName     (void) const;
		bool           isLyricText          (void) const;
		bool           isMarkerText         (void) const;
		bool           isTempo              (void) const;
		bool           isTimeSignature      (void) const;
		bool           isKeySignature       (void) const;
		bool           isEndOfTrack         (void) const;

		std::string    getMetaContent       (void) const;
		void           setMetaContent       (const std::string& content);
		void           setTempo             (double tempo);
		void           setTempoMicroseconds (int microseconds);
		void           setMetaTempo         (double tempo);


		void           makeSysExMessage     (const std::vector<uchar>& data);

		// helper functions to create MTS tunings by key (real-time sysex)

		// MTS type 2: Real-time frequency assignment to a arbitrary list of MIDI key numbers.
		// See page 2 of: https://docs.google.com/viewer?url=https://www.midi.org/component/edocman/midi-tuning-updated/fdocument?Itemid=9999
		void           makeMts2_KeyTuningByFrequency  (int key, double frequency, int program = 0);
		void           makeMts2_KeyTuningsByFrequency (int key, double frequency, int program = 0);
		void           makeMts2_KeyTuningsByFrequency (std::vector<std::pair<int, double>>& mapping, int program = 0);
		void           makeMts2_KeyTuningBySemitone   (int key, double semitone, int program = 0);
		void           makeMts2_KeyTuningsBySemitone  (int key, double semitone, int program = 0);
		void           makeMts2_KeyTuningsBySemitone  (std::vector<std::pair<int, double>>& mapping, int program = 0);

		// MTS type 9: Real-time octave temperaments by +/- 100 cents deviation from ET
		// See page 7 of: https://docs.google.com/viewer?url=https://www.midi.org/component/edocman/midi-tuning-updated/fdocument?Itemid=9999
		void           makeMts9_TemperamentByCentsDeviationFromET (std::vector<double>& mapping, int referencePitchClass = 0, int channelMask = 0b1111111111111111);
		void           makeTemperamentEqual(int referencePitchClass = 0, int channelMask = 0b1111111111111111);
		void           makeTemperamentBad(double maxDeviationCents = 100.0, int referencePitchClass = 0, int channelMask = 0b1111111111111111);
		void           makeTemperamentPythagorean(int referencePitchClass = 2, int channelMask = 0b1111111111111111);
		void           makeTemperamentMeantone(double fraction = 0.25, int referencePitchClass = 2, int channelMask = 0b1111111111111111);
		void           makeTemperamentMeantoneCommaQuarter(int referencePitchClass = 2, int channelMask = 0b1111111111111111);
		void           makeTemperamentMeantoneCommaThird(int referencePitchClass = 2, int channelMask = 0b1111111111111111);
		void           makeTemperamentMeantoneCommaHalf(int referencePitchClass = 2, int channelMask = 0b1111111111111111);

};


std::ostream& operator<<(std::ostream& out, MidiMessage& event);


} // end of namespace smf


#endif /* _MIDIMESSAGE_H_INCLUDED */



