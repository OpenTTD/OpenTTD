//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Fri Nov 26 14:12:01 PST 1999
// Last Modified: Mon Jan 18 20:54:04 PST 2021 Added readSmf().
// Filename:      midifile/include/MidiFile.h
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   A class that can read/write Standard MIDI files.
//                MIDI data is stored by track in an array.
//

#ifndef _MIDIFILE_H_INCLUDED
#define _MIDIFILE_H_INCLUDED

#include "MidiEventList.h"

#include <fstream>
#include <istream>
#include <string>
#include <vector>


namespace smf {

enum {
    TRACK_STATE_SPLIT  = 0, // Tracks are separated into separate vector postions.
    TRACK_STATE_JOINED = 1  // Tracks are merged into a single vector position,
};                          // like a Type-0 MIDI file, but reversible.

enum {
    TIME_STATE_DELTA    = 0, // MidiMessage::ticks are in delta time format (like MIDI file).
    TIME_STATE_ABSOLUTE = 1  // MidiMessage::ticks are in absolute time format (0=start time).
};

class _TickTime {
	public:
		int    tick;
		double seconds;
};


class MidiFile {
	public:
		               MidiFile                    (void);
		               MidiFile                    (const std::string& filename);
		               MidiFile                    (std::istream& input);
		               MidiFile                    (const MidiFile& other);
		               MidiFile                    (MidiFile&& other);

		              ~MidiFile                    ();

		MidiFile&      operator=                   (const MidiFile& other);
		MidiFile&      operator=                   (MidiFile&& other);

		// Reading/writing functions:

		// Auto-detected SMF or ASCII-encoded SMF (decoded with Binasc class):
		bool           read                        (const std::string& filename);
		bool           read                        (std::istream& instream);
		bool           readBase64                  (const std::string& base64data);
		bool           readBase64                  (std::istream& instream);

		// Only allow Standard MIDI File input:
		bool           readSmf                     (const std::string& filename);
		bool           readSmf                     (std::istream& instream);

		bool           write                       (const std::string& filename);
		bool           write                       (std::ostream& out);
		bool           writeBase64                 (const std::string& out, int width = 0);
		bool           writeBase64                 (std::ostream& out, int width = 0);
		std::string    getBase64                   (int width = 0);
		bool           writeHex                    (const std::string& filename, int width = 25);
		bool           writeHex                    (std::ostream& out, int width = 25);
		bool           writeBinasc                 (const std::string& filename);
		bool           writeBinasc                 (std::ostream& out);
		bool           writeBinascWithComments     (const std::string& filename);
		bool           writeBinascWithComments     (std::ostream& out);
		bool           status                      (void) const;

		// track-related functions:
		const MidiEventList& operator[]            (int aTrack) const;
		MidiEventList&   operator[]                (int aTrack);
		int              getTrackCount             (void) const;
		int              getNumTracks              (void) const;
		int              size                      (void) const;
		void             removeEmpties             (void);

		// tick-related functions:
		void             makeDeltaTicks            (void);
		void             deltaTicks                (void);
		void             makeAbsoluteTicks         (void);
		void             absoluteTicks             (void);
		int              getTickState              (void) const;
		bool             isDeltaTicks              (void) const;
		bool             isAbsoluteTicks           (void) const;

		// join/split track functionality:
		void             joinTracks                (void);
		void             splitTracks               (void);
		void             splitTracksByChannel      (void);
		int              getTrackState             (void) const;
		int              hasJoinedTracks           (void) const;
		int              hasSplitTracks            (void) const;
		int              getSplitTrack             (int track, int index) const;
		int              getSplitTrack             (int index) const;

		// track sorting funcionality:
		void             sortTrack                 (int track);
		void             sortTracks                (void);
		void             markSequence              (void);
		void             markSequence              (int track, int sequence = 1);
		void             clearSequence             (void);
		void             clearSequence             (int track);

		// track manipulation functionality:
		int              addTrack                  (void);
		int              addTrack                  (int count);
		int              addTracks                 (int count);
		void             deleteTrack               (int aTrack);
		void             mergeTracks               (int aTrack1, int aTrack2);
		int              getTrackCountAsType1      (void);

		// ticks-per-quarter related functions:
		void             setMillisecondTicks       (void);
		int              getTicksPerQuarterNote    (void) const;
		int              getTPQ                    (void) const;
		void             setTicksPerQuarterNote    (int ticks);
		void             setTPQ                    (int ticks);

		// physical-time analysis functions:
		void             doTimeAnalysis            (void);
		double           getTimeInSeconds          (int aTrack, int anIndex);
		double           getTimeInSeconds          (int tickvalue);
		double           getAbsoluteTickTime       (double starttime);
		int              getFileDurationInTicks    (void);
		double           getFileDurationInQuarters (void);
		double           getFileDurationInSeconds  (void);

		// note-analysis functions:
		int              linkNotePairs             (void);
		int              linkEventPairs            (void);
		void             clearLinks                (void);

		// filename functions:
		void             setFilename               (const std::string& aname);
		const char*      getFilename               (void) const;

		// event functionality:
		MidiEvent*       addEvent                  (int aTrack, int aTick,
		                                            std::vector<uchar>& midiData);
		MidiEvent*       addEvent                  (MidiEvent& mfevent);
		MidiEvent*       addEvent                  (int aTrack, MidiEvent& mfevent);
		MidiEvent&       getEvent                  (int aTrack, int anIndex);
		const MidiEvent& getEvent                  (int aTrack, int anIndex) const;
		int              getEventCount             (int aTrack) const;
		int              getNumEvents              (int aTrack) const;
		void             allocateEvents            (int track, int aSize);
		void             erase                     (void);
		void             clear                     (void);
		void             clear_no_deallocate       (void);

		// MIDI message adding convenience functions:
		MidiEvent*        addNoteOn               (int aTrack, int aTick,
		                                           int aChannel, int key,
		                                           int vel);
		MidiEvent*        addNoteOff              (int aTrack, int aTick,
		                                           int aChannel, int key,
		                                           int vel);
		MidiEvent*        addNoteOff              (int aTrack, int aTick,
		                                           int aChannel, int key);
		MidiEvent*        addController           (int aTrack, int aTick,
		                                           int aChannel, int num,
		                                           int value);
		MidiEvent*        addPatchChange          (int aTrack, int aTick,
		                                           int aChannel, int patchnum);
		MidiEvent*        addTimbre               (int aTrack, int aTick,
		                                           int aChannel, int patchnum);
		MidiEvent*        addPitchBend            (int aTrack, int aTick,
		                                           int aChannel, double amount);

		// RPN settings:
		void              setPitchBendRange       (int aTrack, int aTick,
		                                           int aChannel, double range);

		// Controller message adding convenience functions:
		MidiEvent*        addSustain              (int aTrack, int aTick,
		                                           int aChannel, int value);
		MidiEvent*        addSustainPedal         (int aTrack, int aTick,
		                                           int aChannel, int value);
		MidiEvent*        addSustainOn            (int aTrack, int aTick,
		                                           int aChannel);
		MidiEvent*        addSustainPedalOn       (int aTrack, int aTick,
		                                           int aChannel);
		MidiEvent*        addSustainOff           (int aTrack, int aTick,
		                                           int aChannel);
		MidiEvent*        addSustainPedalOff      (int aTrack, int aTick,
		                                           int aChannel);

		// Meta-event adding convenience functions:
		MidiEvent*         addMetaEvent           (int aTrack, int aTick,
		                                           int aType,
		                                           std::vector<uchar>& metaData);
		MidiEvent*         addMetaEvent           (int aTrack, int aTick,
		                                           int aType,
		                                           const std::string& metaData);
		MidiEvent*         addText                (int aTrack, int aTick,
		                                           const std::string& text);
		MidiEvent*         addCopyright           (int aTrack, int aTick,
		                                           const std::string& text);
		MidiEvent*         addTrackName           (int aTrack, int aTick,
		                                           const std::string& name);
		MidiEvent*         addInstrumentName      (int aTrack, int aTick,
		                                           const std::string& name);
		MidiEvent*         addLyric               (int aTrack, int aTick,
		                                           const std::string& text);
		MidiEvent*         addMarker              (int aTrack, int aTick,
		                                           const std::string& text);
		MidiEvent*         addCue                 (int aTrack, int aTick,
		                                           const std::string& text);
		MidiEvent*         addTempo               (int aTrack, int aTick,
		                                           double aTempo);
		MidiEvent*         addKeySignature        (int aTrack, int aTick,
		                                           int fifths, bool mode = 0);
		MidiEvent*         addTimeSignature       (int aTrack, int aTick,
		                                           int top, int bottom,
		                                           int clocksPerClick = 24,
		                                           int num32dsPerQuarter = 8);
		MidiEvent*         addCompoundTimeSignature(int aTrack, int aTick,
		                                           int top, int bottom,
		                                           int clocksPerClick = 36,
		                                           int num32dsPerQuarter = 8);

		uchar              readByte               (std::istream& input);

		// static functions:
		static ushort        readLittleEndian2Bytes  (std::istream& input);
		static ulong         readLittleEndian4Bytes  (std::istream& input);
		static std::ostream& writeLittleEndianUShort (std::ostream& out,
		                                              ushort value);
		static std::ostream& writeBigEndianUShort    (std::ostream& out,
		                                              ushort value);
		static std::ostream& writeLittleEndianShort  (std::ostream& out,
		                                              short value);
		static std::ostream& writeBigEndianShort     (std::ostream& out,
		                                              short value);
		static std::ostream& writeLittleEndianULong  (std::ostream& out,
		                                              ulong value);
		static std::ostream& writeBigEndianULong     (std::ostream& out,
		                                              ulong value);
		static std::ostream& writeLittleEndianLong   (std::ostream& out,
		                                              long value);
		static std::ostream& writeBigEndianLong      (std::ostream& out,
		                                              long value);
		static std::ostream& writeLittleEndianFloat  (std::ostream& out,
		                                              float value);
		static std::ostream& writeBigEndianFloat     (std::ostream& out,
		                                              float value);
		static std::ostream& writeLittleEndianDouble (std::ostream& out,
		                                              double value);
		static std::ostream& writeBigEndianDouble    (std::ostream& out,
		                                              double value);
		static std::string   getGMInstrumentName     (int patchIndex);

	protected:
		// m_events == Lists of MidiEvents for each MIDI file track.
		std::vector<MidiEventList*> m_events;

		// m_ticksPerQuarterNote == A value for the MIDI file header
		// which represents the number of ticks in a quarter note
		// that are used as units for the delta times for MIDI events
		// in MIDI file track data.
		int m_ticksPerQuarterNote = 120;

		// m_theTrackState == state variable for whether the tracks
		// are joined or split.
		int m_theTrackState = TRACK_STATE_SPLIT;

		// m_theTimeState == state variable for whether the MidiEvent::tick
		// variable contain absolute ticks since the start of the file's
		// time, or delta ticks since the last MIDI event in the track.
		int m_theTimeState = TIME_STATE_ABSOLUTE;

		// m_readFileName == the filename of the last file read into
		// the object.
		std::string m_readFileName;

		// m_timemapvalid ==
		bool m_timemapvalid = false;

		// m_timemap ==
		std::vector<_TickTime> m_timemap;

		// m_rwstatus == True if last read was successful, false if a problem.
		bool m_rwstatus = true;

		// m_linkedEventQ == True if link analysis has been done.
		bool m_linkedEventsQ = false;

	private:
		int         extractMidiData                 (std::istream& inputfile,
		                                             std::vector<uchar>& array,
		                                             uchar& runningCommand);
		ulong       readVLValue                     (std::istream& inputfile);
		ulong       unpackVLV                       (uchar a = 0, uchar b = 0,
		                                             uchar c = 0, uchar d = 0,
		                                             uchar e = 0);
		void        writeVLValue                    (long aValue,
		                                             std::vector<uchar>& data);
		int         makeVLV                         (uchar *buffer, int number);
		static int  ticksearch                      (const void* A, const void* B);
		static int  secondsearch                    (const void* A, const void* B);
		void        buildTimeMap                    (void);
		double      linearTickInterpolationAtSecond (double seconds);
		double      linearSecondInterpolationAtTick (int ticktime);
		std::string base64Encode                    (const std::string &input);
		std::string base64Decode                    (const std::string &input);

		static const std::string encodeLookup;
		static const std::vector<int> decodeLookup;
		static const char *GMinstrument[128];
};

} // end of namespace smf

std::ostream& operator<<(std::ostream& out, smf::MidiFile& aMidiFile);

#endif /* _MIDIFILE_H_INCLUDED */



