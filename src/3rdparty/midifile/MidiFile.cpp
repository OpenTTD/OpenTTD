//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Fri Nov 26 14:12:01 PST 1999
// Last Modified: Thu Jun 24 18:35:30 PDT 2021 Added base64 encoding read/write
// Filename:      midifile/src/MidiFile.cpp
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   A class which can read/write Standard MIDI files.
//                MIDI data is stored by track in an array.  This
//                class is used for example in the MidiPerform class.
//

#include "MidiFile.h"
#include "Binasc.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>


namespace smf {


const std::string MidiFile::encodeLookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

const std::vector<int> MidiFile::decodeLookup {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};


const char* MidiFile::GMinstrument[128] = {
   	"acoustic grand piano",   "bright acoustic piano",  "electric grand piano",  "honky-tonk piano", "rhodes piano",   "chorused piano",
   	"harpsichord",  "clavinet",  "celeste",   "glockenspiel",   "music box",  "vibraphone",
   	"marimba",   "xylophone",  "tubular bells",  "dulcimer",    "hammond organ",   "percussive organ",
   	"rock organ",   "church organ", "reed organ",   "accordion",   "harmonica", "tango accordion",
   	"nylon guitar",  "steel guitar",  "jazz guitar",   "clean guitar",  "muted guitar",   "overdriven guitar",
   	"distortion guitar",   "guitar harmonics",   "acoustic bass",    "fingered electric bass",  "picked electric bass",  "fretless bass",
   	"slap bass 1",  "slap bass 2",  "synth bass 1",  "synth bass 2",  "violin",    "viola",
   	"cello",     "contrabass",  "tremolo strings",   "pizzcato strings",  "orchestral harp",      "timpani",
   	"string ensemble 1",   "string ensemble 2",   "synth strings 1",   "synth strings 1",   "choir aahs",     "voice oohs",
   	"synth voices",    "orchestra hit",   "trumpet",   "trombone",  "tuba",      "muted trumpet",
   	"frenc horn", "brass section",  "syn brass 1",  "synth brass 2",  "soprano sax",  "alto sax",
   	"tenor sax",  "baritone sax",   "oboe",      "english horn",  "bassoon",   "clarinet",
   	"piccolo",   "flute",     "recorder",  "pan flute",  "bottle blow",    "shakuhachi",
   	"whistle",   "ocarina",   "square wave",   "saw wave",   "calliope lead",  "chiffer lead",
   	"charang lead",   "voice lead",   "fifths lead",   "brass lead",  "newage pad",  "warm pad",
   	"polysyn pad",   "choir pad",   "bowed pad",  "metallic pad",  "halo pad",   "sweep pad",
   	"rain",    "soundtrack",  "crystal",   "atmosphere",  "brightness",  "goblins",
   	"echoes",   "sci-fi",  "sitar",     "banjo",     "shamisen",  "koto",
   	"kalimba",   "bagpipes",  "fiddle",    "shanai",   "tinkle bell",  "agogo",
   	"steel drums", "woodblock", "taiko drum",     "melodoc tom",      "synth drum",    "reverse cymbal",
   	"guitar fret noise",   "breath noise",   "seashore",  "bird tweet",    "telephone ring", "helicopter",
   	"applause",  "gunshot"
};



//////////////////////////////
//
// MidiFile::MidiFile -- Constructor.
//

MidiFile::MidiFile(void) {
	m_events.resize(1);
	for (auto &event : m_events) {
		event = new MidiEventList;
	}
}


MidiFile::MidiFile(const std::string& filename) {
	m_events.resize(1);
	for (auto &event : m_events) {
		event = new MidiEventList;
	}
	read(filename);
}


MidiFile::MidiFile(std::istream& input) {
	m_events.resize(1);
	for (auto &event : m_events) {
		event = new MidiEventList;
	}
	read(input);
}



MidiFile::MidiFile(const MidiFile& other) {
	*this = other;
}



MidiFile::MidiFile(MidiFile&& other) {
	*this = std::move(other);
}



//////////////////////////////
//
// MidiFile::~MidiFile -- Deconstructor.
//

MidiFile::~MidiFile() {
	m_readFileName.clear();
	clear();
	if (m_events[0] != NULL) {
		delete m_events[0];
		m_events[0] = NULL;
	}
	m_events.resize(0);
	m_rwstatus = false;
	m_timemap.clear();
	m_timemapvalid = 0;
}



//////////////////////////////
//
// MidiFile::operator= -- Copying another
//

MidiFile& MidiFile::operator=(const MidiFile& other) {
	if (this == &other) {
		return *this;
	}
	m_events.reserve(other.m_events.size());
	auto it = other.m_events.begin();
	std::generate_n(std::back_inserter(m_events), other.m_events.size(),
		[&]()->MidiEventList* {
			return new MidiEventList(**it++);
		}
	);
	m_ticksPerQuarterNote = other.m_ticksPerQuarterNote;
	m_theTrackState       = other.m_theTrackState;
	m_theTimeState        = other.m_theTimeState;
	m_readFileName        = other.m_readFileName;
	m_timemapvalid        = other.m_timemapvalid;
	m_timemap             = other.m_timemap;
	m_rwstatus            = other.m_rwstatus;
	if (other.m_linkedEventsQ) {
		linkEventPairs();
	}
	return *this;
}


MidiFile& MidiFile::operator=(MidiFile&& other) {
	m_events = std::move(other.m_events);
	m_linkedEventsQ = other.m_linkedEventsQ;
	other.m_linkedEventsQ = false;
	other.m_events.clear();
	other.m_events.emplace_back(new MidiEventList);
	m_ticksPerQuarterNote = other.m_ticksPerQuarterNote;
	m_theTrackState       = other.m_theTrackState;
	m_theTimeState        = other.m_theTimeState;
	m_readFileName        = other.m_readFileName;
	m_timemapvalid        = other.m_timemapvalid;
	m_timemap             = other.m_timemap;
	m_rwstatus            = other.m_rwstatus;
	return *this;
}


///////////////////////////////////////////////////////////////////////////
//
// reading/writing functions --
//


//////////////////////////////
//
// MidiFile::read -- Parse a Standard MIDI File or ASCII-encoded Standard MIDI
//      File and store its contents in the object.
//

bool MidiFile::read(const std::string& filename) {
	m_timemapvalid = 0;
	setFilename(filename);
	m_rwstatus = true;

	std::fstream input;
	input.open(filename.c_str(), std::ios::binary | std::ios::in);

	if (!input.is_open()) {
		m_rwstatus = false;
		return m_rwstatus;
	}

	m_rwstatus = read(input);
	return m_rwstatus;
}

//
// istream version of read().
//

bool MidiFile::read(std::istream& input) {
	m_rwstatus = true;
	if (input.peek() != 'M') {
		// If the first byte in the input stream is not 'M', then presume that
		// the MIDI file is in the binasc format which is an ASCII representation
		// of the MIDI file.  Convert the binasc content into binary content and
		// then continue reading with this function.
		std::stringstream binarydata;
		Binasc binasc;
		binasc.writeToBinary(binarydata, input);
		binarydata.seekg(0, std::ios_base::beg);
		if (binarydata.peek() != 'M') {
			std::cerr << "Bad MIDI data input" << std::endl;
			m_rwstatus = false;
			return m_rwstatus;
		} else {
			m_rwstatus = readSmf(binarydata);
			return m_rwstatus;
		}
	} else {
		m_rwstatus = readSmf(input);
		return m_rwstatus;
	}
}



//////////////////////////////
//
// MidiFile::readBase64 -- First decode base64 string and then parse as either a
//      Standard MIDI File or binasc-encoded Standard MIDI File.
//

bool MidiFile::readBase64(const std::string& base64data) {
	std::stringstream stream;
	stream << MidiFile::base64Decode(base64data);
	return MidiFile::read(stream);
}

bool MidiFile::readBase64(std::istream& instream) {
	std::string base64data((std::istreambuf_iterator<char>(instream)),
			std::istreambuf_iterator<char>());
	std::stringstream stream;
	stream << MidiFile::base64Decode(base64data);
	return MidiFile::read(stream);
}



//////////////////////////////
//
// MidiFile::readSmf -- Parse a Standard MIDI File and store its contents
//      in the object.
//

bool MidiFile::readSmf(const std::string& filename) {
	m_timemapvalid = 0;
	setFilename(filename);
	m_rwstatus = true;

	std::fstream input;
	input.open(filename.c_str(), std::ios::binary | std::ios::in);

	if (!input.is_open()) {
		m_rwstatus = false;
		return m_rwstatus;
	}

	m_rwstatus = readSmf(input);
	return m_rwstatus;
}



//////////////////////////////
//
// MidiFile::readSmf -- Parse a Standard MIDI File and store its contents in the object.
//

bool MidiFile::readSmf(std::istream& input) {
	m_rwstatus = true;

	std::string filename = getFilename();

	int    character;
	// uchar  buffer[123456] = {0};
	ulong  longdata;
	ushort shortdata;

	// Read the MIDI header (4 bytes of ID, 4 byte data size,
	// anticipated 6 bytes of data.

	character = input.get();
	if (character == EOF) {
		std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
		std::cerr << "Expecting 'M' at first byte, but found nothing." << std::endl;
		m_rwstatus = false; return m_rwstatus;
	} else if (character != 'M') {
		std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
		std::cerr << "Expecting 'M' at first byte but got '"
		     << (char)character << "'" << std::endl;
		m_rwstatus = false; return m_rwstatus;
	}

	character = input.get();
	if (character == EOF) {
		std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
		std::cerr << "Expecting 'T' at second byte, but found nothing." << std::endl;
		m_rwstatus = false; return m_rwstatus;
	} else if (character != 'T') {
		std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
		std::cerr << "Expecting 'T' at second byte but got '"
		     << (char)character << "'" << std::endl;
		m_rwstatus = false; return m_rwstatus;
	}

	character = input.get();
	if (character == EOF) {
		std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
		std::cerr << "Expecting 'h' at third byte, but found nothing." << std::endl;
		m_rwstatus = false; return m_rwstatus;
	} else if (character != 'h') {
		std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
		std::cerr << "Expecting 'h' at third byte but got '"
		     << (char)character << "'" << std::endl;
		m_rwstatus = false; return m_rwstatus;
	}

	character = input.get();
	if (character == EOF) {
		std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
		std::cerr << "Expecting 'd' at fourth byte, but found nothing." << std::endl;
		m_rwstatus = false; return m_rwstatus;
	} else if (character != 'd') {
		std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
		std::cerr << "Expecting 'd' at fourth byte but got '"
		     << (char)character << "'" << std::endl;
		m_rwstatus = false; return m_rwstatus;
	}

	// read header size (allow larger header size?)
	longdata = readLittleEndian4Bytes(input);
	if (longdata != 6) {
		std::cerr << "File " << filename
		     << " is not a MIDI 1.0 Standard MIDI file." << std::endl;
		std::cerr << "The header size is " << longdata << " bytes." << std::endl;
		m_rwstatus = false; return m_rwstatus;
	}

	// Header parameter #1: format type
	int type;
	shortdata = readLittleEndian2Bytes(input);
	switch (shortdata) {
		case 0:
			type = 0;
			break;
		case 1:
			type = 1;
			break;
		case 2:
			// Type-2 MIDI files should probably be allowed as well,
			// but I have never seen one in the wild to test with.
		default:
			std::cerr << "Error: cannot handle a type-" << shortdata
			     << " MIDI file" << std::endl;
			m_rwstatus = false; return m_rwstatus;
	}

	// Header parameter #2: track count
	int tracks;
	shortdata = readLittleEndian2Bytes(input);
	if (type == 0 && shortdata != 1) {
		std::cerr << "Error: Type 0 MIDI file can only contain one track" << std::endl;
		std::cerr << "Instead track count is: " << shortdata << std::endl;
		m_rwstatus = false; return m_rwstatus;
	} else {
		tracks = shortdata;
	}
	clear();
	if (m_events[0] != NULL) {
		delete m_events[0];
	}
	m_events.resize(tracks);
	for (int z=0; z<tracks; z++) {
		m_events[z] = new MidiEventList;
		m_events[z]->reserve(10000);   // Initialize with 10,000 event storage.
		m_events[z]->clear();
	}

	// Header parameter #3: Ticks per quarter note
	shortdata = readLittleEndian2Bytes(input);
	if (shortdata >= 0x8000) {
		int framespersecond = 255 - ((shortdata >> 8) & 0x00ff) + 1;
		int subframes       = shortdata & 0x00ff;
		switch (framespersecond) {
			case 25:  framespersecond = 25; break;
			case 24:  framespersecond = 24; break;
			case 29:  framespersecond = 29; break;  // really 29.97 for color television
			case 30:  framespersecond = 30; break;
			default:
					std::cerr << "Warning: unknown FPS: " << framespersecond << std::endl;
					std::cerr << "Using non-standard FPS: " << framespersecond << std::endl;
		}
		m_ticksPerQuarterNote = framespersecond * subframes;

		// std::cerr << "SMPTE ticks: " << m_ticksPerQuarterNote << " ticks/sec" << std::endl;
		// std::cerr << "SMPTE frames per second: " << framespersecond << std::endl;
		// std::cerr << "SMPTE subframes per frame: " << subframes << std::endl;
	}  else {
		m_ticksPerQuarterNote = shortdata;
	}


	//////////////////////////////////////////////////
	//
	// now read individual tracks:
	//

	uchar runningCommand;
	MidiEvent event;
	std::vector<uchar> bytes;
	int xstatus;

	for (int i=0; i<tracks; i++) {
		runningCommand = 0;

		// std::cout << "\nReading Track: " << i + 1 << flush;

		// read track header...

		character = input.get();
		if (character == EOF) {
			std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
			std::cerr << "Expecting 'M' at first byte in track, but found nothing."
			     << std::endl;
			m_rwstatus = false; return m_rwstatus;
		} else if (character != 'M') {
			std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
			std::cerr << "Expecting 'M' at first byte in track but got '"
			     << (char)character << "'" << std::endl;
			m_rwstatus = false; return m_rwstatus;
		}

		character = input.get();
		if (character == EOF) {
			std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
			std::cerr << "Expecting 'T' at second byte in track, but found nothing."
			     << std::endl;
			m_rwstatus = false; return m_rwstatus;
		} else if (character != 'T') {
			std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
			std::cerr << "Expecting 'T' at second byte in track but got '"
			     << (char)character << "'" << std::endl;
			m_rwstatus = false; return m_rwstatus;
		}

		character = input.get();
		if (character == EOF) {
			std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
			std::cerr << "Expecting 'r' at third byte in track, but found nothing."
			     << std::endl;
			m_rwstatus = false; return m_rwstatus;
		} else if (character != 'r') {
			std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
			std::cerr << "Expecting 'r' at third byte in track but got '"
			     << (char)character << "'" << std::endl;
			m_rwstatus = false; return m_rwstatus;
		}

		character = input.get();
		if (character == EOF) {
			std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
			std::cerr << "Expecting 'k' at fourth byte in track, but found nothing."
			     << std::endl;
			m_rwstatus = false; return m_rwstatus;
		} else if (character != 'k') {
			std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
			std::cerr << "Expecting 'k' at fourth byte in track but got '"
			     << (char)character << "'" << std::endl;
			m_rwstatus = false; return m_rwstatus;
		}

		// Now read track chunk size and throw it away because it is
		// not really necessary since the track MUST end with an
		// end of track meta event, and many MIDI files found in the wild
		// do not correctly give the track size.
		longdata = readLittleEndian4Bytes(input);

		// Set the size of the track allocation so that it might
		// approximately fit the data.
		m_events[i]->reserve((int)longdata/2);
		m_events[i]->clear();

		// Read MIDI events in the track, which are pairs of VLV values
		// and then the bytes for the MIDI message.  Running status messages
		// will be filled in with their implicit command byte.
		// The timestamps are converted from delta ticks to absolute ticks,
		// with the absticks variable accumulating the VLV tick values.
		int absticks = 0;
		while (!input.eof()) {
			longdata = readVLValue(input);
			absticks += longdata;
			xstatus = extractMidiData(input, bytes, runningCommand);
			if (xstatus == 0) {
				m_rwstatus = false; return m_rwstatus;
			}
			event.setMessage(bytes);
			event.tick = absticks;
			event.track = i;

			if (bytes[0] == 0xff && bytes[1] == 0x2f) {
				// end-of-track message
				// comment out the following line if you don't want to see the
				// end of track message (which is always required, and will added
				// automatically when a MIDI is written, so it is not necessary.
				m_events[i]->push_back(event);
				break;
			}
			m_events[i]->push_back(event);
		}
	}

	m_theTimeState = TIME_STATE_ABSOLUTE;

	// The original order of the MIDI events is marked with an enumeration which
	// allows for reconstruction of the order when merging/splitting tracks to/from
	// a type-0 configuration.
	markSequence();

	return m_rwstatus;
}



//////////////////////////////
//
// MidiFile::write -- write a standard MIDI file to a file or an output
//    stream.
//

bool MidiFile::write(const std::string& filename) {
	std::fstream output(filename.c_str(), std::ios::binary | std::ios::out);

	if (!output.is_open()) {
		std::cerr << "Error: could not write: " << filename << std::endl;
		return false;
	}
	m_rwstatus = write(output);
	output.close();
	return m_rwstatus;
}

//
// ostream version of MidiFile::write().
//

bool MidiFile::write(std::ostream& out) {
	int oldTimeState = getTickState();
	if (oldTimeState == TIME_STATE_ABSOLUTE) {
		makeDeltaTicks();
	}

	// write the header of the Standard MIDI File
	char ch;
	// 1. The characters "MThd"
	ch = 'M'; out << ch;
	ch = 'T'; out << ch;
	ch = 'h'; out << ch;
	ch = 'd'; out << ch;

	// 2. write the size of the header (always a "6" stored in unsigned long
	//    (4 bytes).
	ulong longdata = 6;
	writeBigEndianULong(out, longdata);

	// 3. MIDI file format, type 0, 1, or 2
	ushort shortdata;
	shortdata = static_cast<ushort>(getNumTracks() == 1 ? 0 : 1);
	writeBigEndianUShort(out,shortdata);

	// 4. write out the number of tracks.
	shortdata = static_cast<ushort>(getNumTracks());
	writeBigEndianUShort(out, shortdata);

	// 5. write out the number of ticks per quarternote. (avoiding SMPTE for now)
	shortdata = static_cast<ushort>(getTicksPerQuarterNote());
	writeBigEndianUShort(out, shortdata);

	// now write each track.
	std::vector<uchar> trackdata;
	uchar endoftrack[4] = {0, 0xff, 0x2f, 0x00};
	int i, j, k;
	int size;
	for (i=0; i<getNumTracks(); i++) {
		trackdata.reserve(123456);   // make the track data larger than
		                             // expected data input
		trackdata.clear();
		for (j=0; j<(int)m_events[i]->size(); j++) {
			if ((*m_events[i])[j].empty()) {
				// Don't write empty m_events (probably a delete message).
				continue;
			}
			if ((*m_events[i])[j].isEndOfTrack()) {
				// Suppress end-of-track meta messages (one will be added
				// automatically after all track data has been written).
				continue;
			}
			writeVLValue((*m_events[i])[j].tick, trackdata);
			if (((*m_events[i])[j].getCommandByte() == 0xf0) ||
					((*m_events[i])[j].getCommandByte() == 0xf7)) {
				// 0xf0 == Complete sysex message (0xf0 is part of the raw MIDI).
				// 0xf7 == Raw byte message (0xf7 not part of the raw MIDI).
				// Print the first byte of the message (0xf0 or 0xf7), then
				// print a VLV length for the rest of the bytes in the message.
				// In other words, when creating a 0xf0 or 0xf7 MIDI message,
				// do not insert the VLV byte length yourself, as this code will
				// do it for you automatically.
				trackdata.push_back((*m_events[i])[j][0]); // 0xf0 or 0xf7;
				writeVLValue(((int)(*m_events[i])[j].size())-1, trackdata);
				for (k=1; k<(int)(*m_events[i])[j].size(); k++) {
					trackdata.push_back((*m_events[i])[j][k]);
				}
			} else {
				// non-sysex type of message, so just output the
				// bytes of the message:
				for (k=0; k<(int)(*m_events[i])[j].size(); k++) {
					trackdata.push_back((*m_events[i])[j][k]);
				}
			}
		}
		size = (int)trackdata.size();
		if ((size < 3) || !((trackdata[size-3] == 0xff)
				&& (trackdata[size-2] == 0x2f))) {
			trackdata.push_back(endoftrack[0]);
			trackdata.push_back(endoftrack[1]);
			trackdata.push_back(endoftrack[2]);
			trackdata.push_back(endoftrack[3]);
		}

		// now ready to write to MIDI file.

		// first write the track ID marker "MTrk":
		ch = 'M'; out << ch;
		ch = 'T'; out << ch;
		ch = 'r'; out << ch;
		ch = 'k'; out << ch;

		// A. write the size of the MIDI data to follow:
		longdata = (int)trackdata.size();
		writeBigEndianULong(out, longdata);

		// B. write the actual data
		out.write((char*)trackdata.data(), trackdata.size());
	}

	if (oldTimeState == TIME_STATE_ABSOLUTE) {
		makeAbsoluteTicks();
	}

	return true;
}



//////////////////////////////
//
// MidiFile::writeBase64 -- Write Standard MIDI file with base64 encoding.
//    The width parameter can be used to add line breaks.  Zero or negative
//    width will prevent linebreaks from being added to the data.
//    Default value: width = 0
//

bool MidiFile::writeBase64(const std::string& filename, int width) {
	std::fstream output(filename.c_str(), std::ios::binary | std::ios::out);

	if (!output.is_open()) {
		std::cerr << "Error: could not write: " << filename << std::endl;
		return false;
	}
	m_rwstatus = writeBase64(output, width);
	output.close();
	return m_rwstatus;
}


bool MidiFile::writeBase64(std::ostream& out, int width) {
	std::stringstream raw;
	bool status = MidiFile::write(raw);
	if (!status) {
		return status;
	}
	std::string encoded = MidiFile::base64Encode(raw.str());
	if (width <= 0) {
		out << encoded;
		return status;
	}
	int length = (int)encoded.size();
	for (int i=0; i<length; i++) {
		out << encoded[i];
		if ((i + 1) % width == 0) {
			out << "\n";
		}
	}
	if ((length + 1) % width != 0) {
		out << "\n";
	}
	return status;
}



//////////////////////////////
//
// MidiFile::getBase64 -- Convert the MIDI contents to a base-64 string.
//     Default value: width = 0
//

std::string MidiFile::getBase64(int width) {
	std::stringstream output;
	bool status = MidiFile::writeBase64(output, width);
	if (!status) {
		return "";
	} else {
		return output.str();
	}
}



//////////////////////////////
//
// MidiFile::writeHex -- print the Standard MIDI file as a list of
//    ASCII Hex bytes, formatted 25 to a line by default, and
//    two digits for each hex byte code.  If the input width is 0,
//    then don't wrap lines.
//
//  default value: width=25
//

bool MidiFile::writeHex(const std::string& filename, int width) {
	std::fstream output(filename.c_str(), std::ios::out);
	if (!output.is_open()) {
		std::cerr << "Error: could not write: " << filename << std::endl;
		return false;
	}
	m_rwstatus = writeHex(output, width);
	output.close();
	return m_rwstatus;
}

//
// ostream version of MidiFile::writeHex().
//

bool MidiFile::writeHex(std::ostream& out, int width) {
	std::stringstream tempstream;
	MidiFile::write(tempstream);
	int len = (int)tempstream.str().length();
	int wordcount = 1;
	int linewidth = width >= 0 ? width : 25;
	for (int i=0; i<len; i++) {
		int value = (uchar)tempstream.str()[i];
		out << std::hex << std::setw(2) << std::setfill('0') << value;
		if (linewidth) {
			if (i < len - 1) {
				out << ((wordcount % linewidth) ? ' ' : '\n');
			}
			wordcount++;
		} else {
			// print with no line breaks
			if (i < len - 1) {
				out << ' ';
			}
		}
	}
	if (linewidth) {
		out << '\n';
	}
	return true;
}



//////////////////////////////
//
// MidiFile::writeBinasc -- write a standard MIDI file from data into
//    the binasc format (ASCII version of the MIDI file).
//

bool MidiFile::writeBinasc(const std::string& filename) {
	std::fstream output(filename.c_str(), std::ios::out);

	if (!output.is_open()) {
		std::cerr << "Error: could not write: " << filename << std::endl;
		return false;
	}
	m_rwstatus = writeBinasc(output);
	output.close();
	return m_rwstatus;
}

//
// ostream version of MidiFile::writeBinasc().
//

bool MidiFile::writeBinasc(std::ostream& output) {
	std::stringstream binarydata;
	m_rwstatus = write(binarydata);
	if (m_rwstatus == false) {
		return false;
	}

	Binasc binasc;
	binasc.setMidiOn();
	binarydata.seekg(0, std::ios_base::beg);
	binasc.readFromBinary(output, binarydata);
	return true;
}



//////////////////////////////
//
// MidiFile::writeBinascWithComments -- write a standard MIDI
//    file from data into the binasc format (ASCII version
//    of the MIDI file), including commentary about the MIDI messages.
//

bool MidiFile::writeBinascWithComments(const std::string& filename) {
	std::fstream output(filename.c_str(), std::ios::out);

	if (!output.is_open()) {
		std::cerr << "Error: could not write: " << filename << std::endl;
		return 0;
	}
	m_rwstatus = writeBinascWithComments(output);
	output.close();
	return m_rwstatus;
}

//
// ostream version of MidiFile::writeBinascWithComments().
//

bool MidiFile::writeBinascWithComments(std::ostream& output) {
	std::stringstream binarydata;
	m_rwstatus = write(binarydata);
	if (m_rwstatus == false) {
		return false;
	}

	Binasc binasc;
	binasc.setMidiOn();
	binasc.setCommentsOn();
	binarydata.seekg(0, std::ios_base::beg);
	binasc.readFromBinary(output, binarydata);
	return true;
}



//////////////////////////////
//
// MidiFile::status -- return the success flag from the last read or
//    write (writeHex, writeBinasc).
//

bool MidiFile::status(void) const {
	return m_rwstatus;
}


///////////////////////////////////////////////////////////////////////////
//
// track-related functions --
//

//////////////////////////////
//
// MidiFile::operator[] -- return the event list for the specified track.
//

MidiEventList& MidiFile::operator[](int aTrack) {
	return *m_events[aTrack];
}

const MidiEventList& MidiFile::operator[](int aTrack) const {
	return *m_events[aTrack];
}


//////////////////////////////
//
// MidiFile::getTrackCount -- return the number of tracks in
//   the Midi File.
//

int MidiFile::getTrackCount(void) const {
	return (int)m_events.size();
}

//
// Alias for getTrackCount()
//

int MidiFile::getNumTracks(void) const {
	return getTrackCount();
}

//
// Alias for getTrackCount()
//

int MidiFile::size(void) const {
	return getTrackCount();
}



//////////////////////////////
//
// MidiFile::removeEmpties -- Remove any MIDI message that
//     contains no bytes.
//

void MidiFile::removeEmpties(void) {
	for (auto &event : m_events) {
		event->removeEmpties();
	}
}



//////////////////////////////
//
// MidiFile::markSequence -- Assign a sequence serial number to
//   every MidiEvent in every track in the MIDI file.  This is
//   useful if you want to preseve the order of MIDI messages in
//   a track when they occur at the same tick time.  Particularly
//   for use with joinTracks() or sortTracks().  markSequence will
//   be done automatically when a MIDI file is read, in case the
//   ordering of m_events occurring at the same time is important.
//   Use clearSequence() to use the default sorting behavior of
//   sortTracks().
//

void MidiFile::markSequence(void) {
	int sequence = 1;
	for (int i=0; i<getTrackCount(); i++) {
		sequence = operator[](i).markSequence(sequence);
	}
}

//
// MidiFile::markSequence -- default value: sequence = 1.
//

void MidiFile::markSequence(int track, int sequence) {
	if ((track >= 0) && (track < getTrackCount())) {
		operator[](track).markSequence(sequence);
	} else {
		std::cerr << "Warning: track " << track << " does not exist." << std::endl;
	}
}



//////////////////////////////
//
// MidiFile::clearSequence -- Remove any sequence serial numbers from
//   MidiEvents in the MidiFile.  This will cause the default ordering by
//   sortTracks() to be used, in which case the ordering of MidiEvents
//   occurring at the same tick may switch their ordering.
//

void MidiFile::clearSequence(void) {
	for (int i=0; i<getTrackCount(); i++) {
		operator[](i).clearSequence();
	}
}


void MidiFile::clearSequence(int track) {
	if ((track >= 0) && (track < getTrackCount())) {
		operator[](track).clearSequence();
	} else {
		std::cerr << "Warning: track " << track << " does not exist." << std::endl;
	}
}



//////////////////////////////
//
// MidiFile::joinTracks -- Interleave the data from all tracks,
//   but keeping the identity of the tracks unique so that
//   the function splitTracks can be called to split the
//   tracks into separate units again.  The style of the
//   MidiFile when read from a file is with tracks split.
//   The original track index is stored in the MidiEvent::track
//   variable.
//

void MidiFile::joinTracks(void) {
	if (getTrackState() == TRACK_STATE_JOINED) {
		return;
	}
	if (getNumTracks() == 1) {
		m_theTrackState = TRACK_STATE_JOINED;
		return;
	}

	MidiEventList* joinedTrack;
	joinedTrack = new MidiEventList;

	int messagesum = 0;
	int length = getNumTracks();
	int i, j;
	for (i=0; i<length; i++) {
		messagesum += (*m_events[i]).size();
	}
	joinedTrack->reserve((int)(messagesum + 32 + messagesum * 0.1));

	int oldTimeState = getTickState();
	if (oldTimeState == TIME_STATE_DELTA) {
		makeAbsoluteTicks();
	}
	for (i=0; i<length; i++) {
		for (j=0; j<(int)m_events[i]->size(); j++) {
			joinedTrack->push_back_no_copy(&(*m_events[i])[j]);
		}
	}

	clear_no_deallocate();

	delete m_events[0];
	m_events.resize(0);
	m_events.push_back(joinedTrack);
	sortTracks();
	if (oldTimeState == TIME_STATE_DELTA) {
		makeDeltaTicks();
	}

	m_theTrackState = TRACK_STATE_JOINED;
}



//////////////////////////////
//
// MidiFile::splitTracks -- Take the joined tracks and split them
//   back into their separate track identities.
//

void MidiFile::splitTracks(void) {
	if (getTrackState() == TRACK_STATE_SPLIT) {
		return;
	}
	int oldTimeState = getTickState();
	if (oldTimeState == TIME_STATE_DELTA) {
		makeAbsoluteTicks();
	}

	int maxTrack = 0;
	int i;
	int length = m_events[0]->size();
	for (i=0; i<length; i++) {
		if ((*m_events[0])[i].track > maxTrack) {
			maxTrack = (*m_events[0])[i].track;
		}
	}
	int trackCount = maxTrack + 1;

	if (trackCount <= 1) {
		return;
	}

	MidiEventList* olddata = m_events[0];
	m_events[0] = NULL;
	m_events.resize(trackCount);
	for (i=0; i<trackCount; i++) {
		m_events[i] = new MidiEventList;
	}

	for (i=0; i<length; i++) {
		int trackValue = (*olddata)[i].track;
		m_events[trackValue]->push_back_no_copy(&(*olddata)[i]);
	}

	olddata->detach();
	delete olddata;

	if (oldTimeState == TIME_STATE_DELTA) {
		makeDeltaTicks();
	}

	m_theTrackState = TRACK_STATE_SPLIT;
}



//////////////////////////////
//
// MidiFile::splitTracksByChannel -- Take the joined tracks and split them
//   back into their separate track identities.
//

void MidiFile::splitTracksByChannel(void) {
	joinTracks();
	if (getTrackState() == TRACK_STATE_SPLIT) {
		return;
	}

	int oldTimeState = getTickState();
	if (oldTimeState == TIME_STATE_DELTA) {
		makeAbsoluteTicks();
	}

	int maxTrack = 0;
	int i;
	MidiEventList& eventlist = *m_events[0];
	MidiEventList* olddata = &eventlist;
	int length = eventlist.size();
	for (i=0; i<length; i++) {
		if (eventlist[i].size() == 0) {
			continue;
		}
		if ((eventlist[i][0] & 0xf0) == 0xf0) {
			// ignore system and meta messages.
			continue;
		}
		if (maxTrack < (eventlist[i][0] & 0x0f)) {
			maxTrack = eventlist[i][0] & 0x0f;
		}
	}
	int trackCount = maxTrack + 2; // + 1 for expression track

	if (trackCount <= 1) {
		// only one channel, so don't do anything (leave as Type-0 file).
		return;
	}

	m_events[0] = NULL;
	m_events.resize(trackCount);
	for (i=0; i<trackCount; i++) {
		m_events[i] = new MidiEventList;
	}

	for (i=0; i<length; i++) {
		int trackValue = 0;
		if ((eventlist[i][0] & 0xf0) == 0xf0) {
			trackValue = 0;
		} else if (eventlist[i].size() > 0) {
			trackValue = (eventlist[i][0] & 0x0f) + 1;
		}
		m_events[trackValue]->push_back_no_copy(&eventlist[i]);
	}

	olddata->detach();
	delete olddata;

	if (oldTimeState == TIME_STATE_DELTA) {
		makeDeltaTicks();
	}

	m_theTrackState = TRACK_STATE_SPLIT;
}



//////////////////////////////
//
// MidiFile::getTrackState -- returns what type of track method
//     is being used: either TRACK_STATE_JOINED or TRACK_STATE_SPLIT.
//

int MidiFile::getTrackState(void) const {
	return m_theTrackState;
}



//////////////////////////////
//
// MidiFile::hasJoinedTracks -- Returns true if the MidiFile tracks
//    are in a joined state.
//

int MidiFile::hasJoinedTracks(void) const {
	return m_theTrackState == TRACK_STATE_JOINED;
}



//////////////////////////////
//
// MidiFile::hasSplitTracks -- Returns true if the MidiFile tracks
//     are in a split state.
//

int MidiFile::hasSplitTracks(void) const {
	return m_theTrackState == TRACK_STATE_SPLIT;
}



//////////////////////////////
//
// MidiFile::getSplitTrack --  Return the track index when the MidiFile
//   is in the split state.  This function returns the original track
//   when the MidiFile is in the joined state.  The MidiEvent::track
//   variable is used to store the original track index when the
//   MidiFile is converted to the joined-track state.
//

int MidiFile::getSplitTrack(int track, int index) const {
	if (hasSplitTracks()) {
		return track;
	} else {
		return getEvent(track, index).track;
	}
}

//
// When the parameter is void, assume track 0:
//

int MidiFile::getSplitTrack(int index) const {
	if (hasSplitTracks()) {
		return 0;
	} else {
		return getEvent(0, index).track;
	}
}



///////////////////////////////////////////////////////////////////////////
//
// tick-related functions --
//

//////////////////////////////
//
// MidiFile::makeDeltaTicks -- convert the time data to
//     delta time, which means that the time field
//     in the MidiEvent struct represents the time
//     since the last event was played. When a MIDI file
//     is read from a file, this is the default setting.
//

void MidiFile::makeDeltaTicks(void) {
	if (getTickState() == TIME_STATE_DELTA) {
		return;
	}
	int i, j;
	int temp;
	int length = getNumTracks();
	int *timedata = new int[length];
	for (i=0; i<length; i++) {
		timedata[i] = 0;
		if (m_events[i]->size() > 0) {
			timedata[i] = (*m_events[i])[0].tick;
		} else {
			continue;
		}
		for (j=1; j<(int)m_events[i]->size(); j++) {
			temp = (*m_events[i])[j].tick;
			int deltatick = temp - timedata[i];
			if (deltatick < 0) {
				std::cerr << "Error: negative delta tick value: " << deltatick << std::endl
				     << "Timestamps must be sorted first"
				     << " (use MidiFile::sortTracks() before writing)." << std::endl;
			}
			(*m_events[i])[j].tick = deltatick;
			timedata[i] = temp;
		}
	}
	m_theTimeState = TIME_STATE_DELTA;
	delete [] timedata;
}

//
// MidiFile::deltaTicks -- Alias for MidiFile::makeDeltaTicks().
//

void MidiFile::deltaTicks(void) {
	makeDeltaTicks();
}



//////////////////////////////
//
// MidiFile::makeAbsoluteTicks -- convert the time data to
//    absolute time, which means that the time field
//    in the MidiEvent struct represents the exact tick
//    time to play the event rather than the time since
//    the last event to wait until playing the current
//    event.
//

void MidiFile::makeAbsoluteTicks(void) {
	if (getTickState() == TIME_STATE_ABSOLUTE) {
		return;
	}
	int i, j;
	int length = getNumTracks();
	int* timedata = new int[length];
	for (i=0; i<length; i++) {
		timedata[i] = 0;
		if (m_events[i]->size() > 0) {
			timedata[i] = (*m_events[i])[0].tick;
		} else {
			continue;
		}
		for (j=1; j<(int)m_events[i]->size(); j++) {
			timedata[i] += (*m_events[i])[j].tick;
			(*m_events[i])[j].tick = timedata[i];
		}
	}
	m_theTimeState = TIME_STATE_ABSOLUTE;
	delete [] timedata;
}

//
// MidiFile::absoluteTicks -- Alias for MidiFile::makeAbsoluteTicks().
//

void MidiFile::absoluteTicks(void) {
	makeAbsoluteTicks();
}



//////////////////////////////
//
// MidiFile::getTickState -- returns what type of time method is
//   being used: either TIME_STATE_ABSOLUTE or TIME_STATE_DELTA.
//

int MidiFile::getTickState(void) const {
	return m_theTimeState;
}



//////////////////////////////
//
// MidiFile::isDeltaTicks -- Returns true if MidiEvent .tick
//    variables are in delta time mode.
//

bool MidiFile::isDeltaTicks(void) const {
	return m_theTimeState == TIME_STATE_DELTA ? true : false;
}



//////////////////////////////
//
// MidiFile::isAbsoluteTicks -- Returns true if MidiEvent .tick
//    variables are in absolute time mode.
//

bool MidiFile::isAbsoluteTicks(void) const {
	return m_theTimeState == TIME_STATE_ABSOLUTE ? true : false;
}



//////////////////////////////
//
// MidiFile::getFileDurationInTicks -- Returns the largest
//    tick value in any track.  The tracks must be sorted
//    before calling this function, since this function
//    assumes that the last MidiEvent in the track has the
//    highest tick timestamp.  The file state can be in delta
//    ticks since this function will temporarily go to absolute
//    tick mode for the calculation of the max tick.
//

int MidiFile::getFileDurationInTicks(void) {
	bool revertToDelta = false;
	if (isDeltaTicks()) {
		makeAbsoluteTicks();
		revertToDelta = true;
	}
	const MidiFile& mf = *this;
	int output = 0;
	for (int i=0; i<mf.getTrackCount(); i++) {
		if (mf[i].back().tick > output) {
			output = mf[i].back().tick;
		}
	}
	if (revertToDelta) {
		deltaTicks();
	}
	return output;
}



///////////////////////////////
//
// MidiFile::getFileDurationInQuarters -- Returns the Duration of the MidiFile
//    in units of quarter notes.  If the MidiFile is in delta tick mode,
//    then temporarily got into absolute tick mode to do the calculations.
//    Note that this is expensive, so you should normally call this function
//    while in absolute tick (default) mode.
//

double MidiFile::getFileDurationInQuarters(void) {
	return (double)getFileDurationInTicks() / (double)getTicksPerQuarterNote();
}



//////////////////////////////
//
// MidiFile::getFileDurationInSeconds -- returns the duration of the
//    longest track in the file.  The tracks must be sorted before
//    calling this function, since this function assumes that the
//    last MidiEvent in the track has the highest timestamp.
//    The file state can be in delta ticks since this function
//    will temporarily go to absolute tick mode for the calculation
//    of the max time.

double MidiFile::getFileDurationInSeconds(void) {
	if (m_timemapvalid == 0) {
		buildTimeMap();
		if (m_timemapvalid == 0) {
			return -1.0;    // something went wrong
		}
	}
	bool revertToDelta = false;
	if (isDeltaTicks()) {
		makeAbsoluteTicks();
		revertToDelta = true;
	}
	const MidiFile& mf = *this;
	double output = 0.0;
	for (int i=0; i<mf.getTrackCount(); i++) {
		if (mf[i].back().seconds > output) {
			output = mf[i].back().seconds;
		}
	}
	if (revertToDelta) {
		deltaTicks();
	}
	return output;
}


///////////////////////////////////////////////////////////////////////////
//
// physical-time analysis functions --
//

//////////////////////////////
//
// MidiFile::doTimeAnalysis -- Identify the real-time position of
//    all events by monitoring the tempo in relations to the tick
//    times in the file.
//

void MidiFile::doTimeAnalysis(void) {
	buildTimeMap();
}



//////////////////////////////
//
// MidiFile::getTimeInSeconds -- return the time in seconds for
//     the current message.
//

double MidiFile::getTimeInSeconds(int aTrack, int anIndex) {
	return getTimeInSeconds(getEvent(aTrack, anIndex).tick);
}


double MidiFile::getTimeInSeconds(int tickvalue) {
	if (m_timemapvalid == 0) {
		buildTimeMap();
		if (m_timemapvalid == 0) {
			return -1.0;    // something went wrong
		}
	}

	_TickTime key;
	key.tick    = tickvalue;
	key.seconds = -1;

	void* ptr = bsearch(&key, m_timemap.data(), m_timemap.size(),
			sizeof(_TickTime), ticksearch);

	if (ptr == NULL) {
		// The specific tick value was not found, so do a linear
		// search for the two tick values which occur before and
		// after the tick value, and do a linear interpolation of
		// the time in seconds values to figure out the final
		// time in seconds.
		// Since the code is not yet written, kill the program at this point:
		return linearSecondInterpolationAtTick(tickvalue);
	} else {
		return ((_TickTime*)ptr)->seconds;
	}
}



//////////////////////////////
//
// MidiFile::getAbsoluteTickTime -- return the tick value represented
//    by the input time in seconds.  If there is not tick entry at
//    the given time in seconds, then interpolate between two values.
//

double MidiFile::getAbsoluteTickTime(double starttime) {
	if (m_timemapvalid == 0) {
		buildTimeMap();
		if (m_timemapvalid == 0) {
			return -1.0;    // something went wrong
		}
	}

	_TickTime key;
	key.tick    = -1;
	key.seconds = starttime;

	void* ptr = bsearch(&key, m_timemap.data(), m_timemap.size(),
			sizeof(_TickTime), secondsearch);

	if (ptr == NULL) {
		// The specific seconds value was not found, so do a linear
		// search for the two time values which occur before and
		// after the given time value, and do a linear interpolation of
		// the time in tick values to figure out the final time in ticks.
		return linearTickInterpolationAtSecond(starttime);
	} else {
		return ((_TickTime*)ptr)->tick;
	}

}



///////////////////////////////////////////////////////////////////////////
//
// note-analysis functions --
//

//////////////////////////////
//
// MidiFile::linkNotePairs --  Link note-ons to note-offs separately
//     for each track.  Returns the total number of note message pairs
//     that were linked.
//

int MidiFile::linkNotePairs(void) {
	int i;
	int sum = 0;
	for (i=0; i<getTrackCount(); i++) {
		if (m_events[i] == NULL) {
			continue;
		}
		sum += m_events[i]->linkNotePairs();
	}
	m_linkedEventsQ = true;
	return sum;
}

//
// MidiFile::linkEventPairs -- Alias for MidiFile::linkNotePairs().
//

int MidiFile::linkEventPairs(void) {
	return linkNotePairs();
}


///////////////////////////////////////////////////////////////////////////
//
// filename functions --
//

//////////////////////////////
//
// MidiFile::setFilename -- sets the filename of the MIDI file.
//      Currently removed any directory path.
//

void MidiFile::setFilename(const std::string& aname) {
	auto loc = aname.rfind('/');
	if (loc != std::string::npos) {
		m_readFileName = aname.substr(loc+1);
	} else {
		m_readFileName = aname;
	}
}



//////////////////////////////
//
// MidiFile::getFilename -- returns the name of the file read into the
//    structure (if the data was read from a file).
//

const char* MidiFile::getFilename(void) const {
	return m_readFileName.c_str();
}



//////////////////////////////
//
// MidiFile::addEvent --
//

MidiEvent* MidiFile::addEvent(int aTrack, int aTick,
		std::vector<uchar>& midiData) {
	m_timemapvalid = 0;
	MidiEvent* me = new MidiEvent;
	me->tick = aTick;
	me->track = aTrack;
	me->setMessage(midiData);
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addEvent -- Some bug here when joinedTracks(), but track==1...
//

MidiEvent* MidiFile::addEvent(MidiEvent& mfevent) {
	if (getTrackState() == TRACK_STATE_JOINED) {
		m_events[0]->push_back(mfevent);
		return &m_events[0]->back();
	} else {
		m_events.at(mfevent.track)->push_back(mfevent);
		return &m_events.at(mfevent.track)->back();
	}
}

//
// Variant where the track is an input parameter:
//

MidiEvent* MidiFile::addEvent(int aTrack, MidiEvent& mfevent) {
	if (getTrackState() == TRACK_STATE_JOINED) {
		m_events[0]->push_back(mfevent);
      m_events[0]->back().track = aTrack;
		return &m_events[0]->back();
	} else {
		m_events.at(aTrack)->push_back(mfevent);
		m_events.at(aTrack)->back().track = aTrack;
		return &m_events.at(aTrack)->back();
	}
}



///////////////////////////////
//
// MidiFile::addMetaEvent --
//

MidiEvent* MidiFile::addMetaEvent(int aTrack, int aTick, int aType,
		std::vector<uchar>& metaData) {
	m_timemapvalid = 0;
	int i;
	int length = (int)metaData.size();
	std::vector<uchar> fulldata;
	uchar size[23] = {0};
	int lengthsize = makeVLV(size, length);

	fulldata.resize(2+lengthsize+length);
	fulldata[0] = 0xff;
	fulldata[1] = aType & 0x7F;
	for (i=0; i<lengthsize; i++) {
		fulldata[2+i] = size[i];
	}
	for (i=0; i<length; i++) {
		fulldata[2+lengthsize+i] = metaData[i];
	}

	return addEvent(aTrack, aTick, fulldata);
}


MidiEvent* MidiFile::addMetaEvent(int aTrack, int aTick, int aType,
		const std::string& metaData) {
	int length = (int)metaData.size();
	std::vector<uchar> buffer;
	buffer.resize(length);
	int i;
	for (i=0; i<length; i++) {
		buffer[i] = (uchar)metaData[i];
	}
	return addMetaEvent(aTrack, aTick, aType, buffer);
}



//////////////////////////////
//
// MidiFile::addText --  Add a text meta-message (#1).
//

MidiEvent* MidiFile::addText(int aTrack, int aTick, const std::string& text) {
	MidiEvent* me = new MidiEvent;
	me->makeText(text);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addCopyright --  Add a copyright notice meta-message (#2).
//

MidiEvent* MidiFile::addCopyright(int aTrack, int aTick, const std::string& text) {
	MidiEvent* me = new MidiEvent;
	me->makeCopyright(text);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addTrackName --  Add an track name meta-message (#3).
//

MidiEvent* MidiFile::addTrackName(int aTrack, int aTick, const std::string& name) {
	MidiEvent* me = new MidiEvent;
	me->makeTrackName(name);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addInstrumentName --  Add an instrument name meta-message (#4).
//

MidiEvent* MidiFile::addInstrumentName(int aTrack, int aTick,
		const std::string& name) {
	MidiEvent* me = new MidiEvent;
	me->makeInstrumentName(name);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addLyric -- Add a lyric meta-message (meta #5).
//

MidiEvent* MidiFile::addLyric(int aTrack, int aTick, const std::string& text) {
	MidiEvent* me = new MidiEvent;
	me->makeLyric(text);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addMarker -- Add a marker meta-message (meta #6).
//

MidiEvent* MidiFile::addMarker(int aTrack, int aTick, const std::string& text) {
	MidiEvent* me = new MidiEvent;
	me->makeMarker(text);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addCue -- Add a cue-point meta-message (meta #7).
//

MidiEvent* MidiFile::addCue(int aTrack, int aTick, const std::string& text) {
	MidiEvent* me = new MidiEvent;
	me->makeCue(text);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addTempo -- Add a tempo meta message (meta #0x51).
//

MidiEvent* MidiFile::addTempo(int aTrack, int aTick, double aTempo) {
	MidiEvent* me = new MidiEvent;
	me->makeTempo(aTempo);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addKeySignature -- Add a key signature meta message
//      (meta #0x59).
//
// Default values:
//      fifths ==  0 (C)
//      mode   ==  0 (major)
//
// Key signature of b minor would be:
//      fifths = 2
//      mode   = 1
//

MidiEvent* MidiFile::addKeySignature (int aTrack, int aTick, int fifths, bool mode) {
    MidiEvent* me = new MidiEvent;
    me->makeKeySignature(fifths, mode);
    me->tick = aTick;
    m_events[aTrack]->push_back_no_copy(me);
    return me;
}



//////////////////////////////
//
// MidiFile::addTimeSignature -- Add a time signature meta message
//      (meta #0x58).  The "bottom" parameter must be a power of two;
//      otherwise, it will be set to the next highest power of two.
//
// Default values:
//     clocksPerClick     == 24 (quarter note)
//     num32ndsPerQuarter ==  8 (8 32nds per quarter note)
//
// Time signature of 4/4 would be:
//    top    = 4
//    bottom = 4 (converted to 2 in the MIDI file for 2nd power of 2).
//    clocksPerClick = 24 (2 eighth notes based on num32ndsPerQuarter)
//    num32ndsPerQuarter = 8
//
// Time signature of 6/8 would be:
//    top    = 6
//    bottom = 8 (converted to 3 in the MIDI file for 3rd power of 2).
//    clocksPerClick = 36 (3 eighth notes based on num32ndsPerQuarter)
//    num32ndsPerQuarter = 8
//

MidiEvent* MidiFile::addTimeSignature(int aTrack, int aTick, int top, int bottom,
		int clocksPerClick, int num32ndsPerQuarter) {
	MidiEvent* me = new MidiEvent;
	me->makeTimeSignature(top, bottom, clocksPerClick, num32ndsPerQuarter);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addCompoundTimeSignature -- Add a time signature meta message
//      (meta #0x58), where the clocksPerClick parameter is set to three
//      eighth notes for compound meters such as 6/8 which represents
//      two beats per measure.
//
// Default values:
//     clocksPerClick     == 36 (quarter note)
//     num32ndsPerQuarter ==  8 (8 32nds per quarter note)
//

MidiEvent* MidiFile::addCompoundTimeSignature(int aTrack, int aTick, int top,
		int bottom, int clocksPerClick, int num32ndsPerQuarter) {
	return addTimeSignature(aTrack, aTick, top, bottom, clocksPerClick,
		num32ndsPerQuarter);
}



//////////////////////////////
//
// MidiFile::makeVLV --  This function is used to create
//   size byte(s) for meta-messages.  If the size of the data
//   in the meta-message is greater than 127, then the size
//   should (?) be specified as a VLV.
//

int MidiFile::makeVLV(uchar *buffer, int number) {

	unsigned long value = (unsigned long)number;

	if (value >= (1 << 28)) {
		std::cerr << "Error: Meta-message size too large to handle" << std::endl;
		buffer[0] = 0;
		buffer[1] = 0;
		buffer[2] = 0;
		buffer[3] = 0;
		return 1;
	}

	buffer[0] = (value >> 21) & 0x7f;
	buffer[1] = (value >> 14) & 0x7f;
	buffer[2] = (value >>  7) & 0x7f;
	buffer[3] = (value >>  0) & 0x7f;

	int i;
	int flag = 0;
	int length = -1;
	for (i=0; i<3; i++) {
		if (buffer[i] != 0) {
			flag = 1;
		}
		if (flag) {
			buffer[i] |= 0x80;
		}
		if (length == -1 && buffer[i] >= 0x80) {
			length = 4-i;
		}
	}

	if (length == -1) {
		length = 1;
	}

	if (length < 4) {
		for (i=0; i<length; i++) {
			buffer[i] = buffer[4-length+i];
		}
	}

	return length;
}



//////////////////////////////
//
// MidiFile::addNoteOn -- Add a note-on message to the given track at the
//    given time in the given channel.
//

MidiEvent* MidiFile::addNoteOn(int aTrack, int aTick, int aChannel, int key, int vel) {
	MidiEvent* me = new MidiEvent;
	me->makeNoteOn(aChannel, key, vel);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addNoteOff -- Add a note-off message (using 0x80 messages).
//

MidiEvent* MidiFile::addNoteOff(int aTrack, int aTick, int aChannel, int key,
		int vel) {
	MidiEvent* me = new MidiEvent;
	me->makeNoteOff(aChannel, key, vel);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addNoteOff -- Add a note-off message (using 0x90 messages with
//   zero attack velocity).
//

MidiEvent* MidiFile::addNoteOff(int aTrack, int aTick, int aChannel, int key) {
	MidiEvent* me = new MidiEvent;
	me->makeNoteOff(aChannel, key);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addController -- Add a controller message in the given
//    track at the given tick time in the given channel.
//

MidiEvent* MidiFile::addController(int aTrack, int aTick, int aChannel,
		int num, int value) {
	MidiEvent* me = new MidiEvent;
	me->makeController(aChannel, num, value);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addPatchChange -- Add a patch-change message in the given
//    track at the given tick time in the given channel.
//

MidiEvent* MidiFile::addPatchChange(int aTrack, int aTick, int aChannel,
		int patchnum) {
	MidiEvent* me = new MidiEvent;
	me->makePatchChange(aChannel, patchnum);
	me->tick = aTick;
	m_events[aTrack]->push_back_no_copy(me);
	return me;
}



//////////////////////////////
//
// MidiFile::addTimbre -- Add a patch-change message in the given
//    track at the given tick time in the given channel.  Alias for
//    MidiFile::addPatchChange().
//

MidiEvent* MidiFile::addTimbre(int aTrack, int aTick, int aChannel, int patchnum) {
	return addPatchChange(aTrack, aTick, aChannel, patchnum);
}



//////////////////////////////
//
// MidiFile::addPitchBend -- convert  number in the range from -1 to +1
//     into two 7-bit numbers (smallest piece first)
//
//   -1.0 maps to 0 (0x0000)
//    0.0 maps to 8192 (0x2000 --> 0x40 0x00)
//   +1.0 maps to 16383 (0x3FFF --> 0x7F 0x7F)
//

MidiEvent* MidiFile::addPitchBend(int aTrack, int aTick, int aChannel, double amount) {
	m_timemapvalid = 0;
	amount += 1.0;
	int value = int(amount * 8192 + 0.5);

	// prevent any wrap-around in case of round-off errors
	if (value > 0x3fff) {
		value = 0x3fff;
	}
	if (value < 0) {
		value = 0;
	}

	int lsbint = 0x7f & value;
	int msbint = 0x7f & (value  >> 7);

	std::vector<uchar> mididata;
	mididata.resize(3);
	if (aChannel < 0) {
		aChannel = 0;
	} else if (aChannel > 15) {
		aChannel = 15;
	}
	mididata[0] = uchar(0xe0 | aChannel);
	mididata[1] = uchar(lsbint);
	mididata[2] = uchar(msbint);

	return addEvent(aTrack, aTick, mididata);
}



///////////////////////////////////////////////////////////////////////////
//
// RPN convenience functions:
//

//////////////////////////////
//
// MidiFile::setPitchBendRange -- Set the range for the min/max pitch bend
//   alteration of a note.  Default is 2.0 (meaning +/- 2 semitones from given pitch).
//   Fractional values are cents, so 2.5 means a range of two semitones plus 50 cents,
//   which is two semitones plus a quarter tone.
//

void MidiFile::setPitchBendRange(int aTrack, int aTick, int aChannel, double range) {
	if (range < 0.0) {
		range = -range;
	}
	if (range > 24.0) {
		std::cerr << "Warning: pitch bend range is too large: " << range << std::endl;
		std::cerr << "Setting to 24." << std::endl;
		range = 24.0;
	}
	int irange = int(range);
	int cents = int((range - irange) * 100.0 + 0.5);

	// Select pitch bend RPN:
	addController(aTrack, aTick, aChannel, 101, 0);  // RPN selector (byte 1)
	addController(aTrack, aTick, aChannel, 100, 0);  // RPN selector (byte 2)

	// Set the semitone range (will be +/-range above/below a note):
	addController(aTrack, aTick, aChannel,  6,  irange);  // coarse: number of semitones
	addController(aTrack, aTick, aChannel, 38,  cents);   // fine: cents (1/100ths of semitone)
}



///////////////////////////////////////////////////////////////////////////
//
// Controller message adding convenience functions:
//

//////////////////////////////
//
// MidiFile::addSustain -- Add a continuous controller message for the sustain pedal.
//

MidiEvent* MidiFile::addSustain(int aTrack, int aTick, int aChannel, int value) {
	return addController(aTrack, aTick, aChannel, 64, value);
}

//
// MidiFile::addSustainPedal -- Alias for MidiFile::addSustain().
//

MidiEvent* MidiFile::addSustainPedal(int aTrack, int aTick, int aChannel, int value) {
	return addSustain(aTrack, aTick, aChannel, value);
}



//////////////////////////////
//
// MidiFile::addSustainOn -- Add a continuous controller message for the sustain pedal on.
//

MidiEvent* MidiFile::addSustainOn(int aTrack, int aTick, int aChannel) {
	return addSustain(aTrack, aTick, aChannel, 127);
}

//
// MidiFile::addSustainPedalOn -- Alias for MidiFile::addSustainOn().
//

MidiEvent* MidiFile::addSustainPedalOn(int aTrack, int aTick, int aChannel) {
	return addSustainOn(aTrack, aTick, aChannel);
}



//////////////////////////////
//
// MidiFile::addSustainOff -- Add a continuous controller message for the sustain pedal off.
//

MidiEvent* MidiFile::addSustainOff(int aTrack, int aTick, int aChannel) {
	return addSustain(aTrack, aTick, aChannel, 0);
}

//
// MidiFile::addSustainPedalOff -- Alias for MidiFile::addSustainOff().
//

MidiEvent* MidiFile::addSustainPedalOff(int aTrack, int aTick, int aChannel) {
	return addSustainOff(aTrack, aTick, aChannel);
}



//////////////////////////////
//
// MidiFile::addTrack -- adds a blank track at end of the
//    track list.  Returns the track number of the added
//    track.
//

int MidiFile::addTrack(void) {
	int length = getNumTracks();
	m_events.resize(length+1);
	m_events[length] = new MidiEventList;
	m_events[length]->reserve(10000);
	m_events[length]->clear();
	return length;
}

int MidiFile::addTrack(int count) {
	int length = getNumTracks();
	m_events.resize(length+count);
	int i;
	for (i=0; i<count; i++) {
		m_events[length + i] = new MidiEventList;
		m_events[length + i]->reserve(10000);
		m_events[length + i]->clear();
	}
	return length + count - 1;
}

//
// MidiFile::addTracks -- Alias for MidiFile::addTrack().
//

int MidiFile::addTracks(int count) {
	return addTrack(count);
}




//////////////////////////////
//
// MidiFile::allocateEvents --
//

void MidiFile::allocateEvents(int track, int aSize) {
	int oldsize = m_events[track]->size();
	if (oldsize < aSize) {
		m_events[track]->reserve(aSize);
	}
}



//////////////////////////////
//
// MidiFile::deleteTrack -- remove a track from the MidiFile.
//   Tracks are numbered starting at track 0.
//

void MidiFile::deleteTrack(int aTrack) {
	int length = getNumTracks();
	if (aTrack < 0 || aTrack >= length) {
		return;
	}
	if (length == 1) {
		return;
	}
	delete m_events[aTrack];
	for (int i=aTrack; i<length-1; i++) {
		m_events[i] = m_events[i+1];
	}

	m_events[length-1] = NULL;
	m_events.resize(length-1);
}



//////////////////////////////
//
// MidiFile::clear -- make the MIDI file empty with one
//     track with no data in it.
//

void MidiFile::clear(void) {
	int length = getNumTracks();
	for (int i=0; i<length; i++) {
		delete m_events[i];
		m_events[i] = NULL;
	}
	m_events.resize(1);
	m_events[0] = new MidiEventList;
	m_timemapvalid=0;
	m_timemap.clear();
	m_theTrackState = TRACK_STATE_SPLIT;
	m_theTimeState = TIME_STATE_ABSOLUTE;
}


void MidiFile::erase(void) {
	clear();
}



//////////////////////////////
//
// MidiFile::getEvent -- return the event at the given index in the
//    specified track.
//

MidiEvent& MidiFile::getEvent(int aTrack, int anIndex) {
	return (*m_events[aTrack])[anIndex];
}


const MidiEvent& MidiFile::getEvent(int aTrack, int anIndex) const {
	return (*m_events[aTrack])[anIndex];
}



//////////////////////////////
//
// MidiFile::getTicksPerQuarterNote -- returns the number of
//   time units that are supposed to occur during a quarternote.
//

int MidiFile::getTicksPerQuarterNote(void) const {
	if (m_ticksPerQuarterNote == 0xE728) {
		// this is a special case which is the SMPTE time code
		// setting for 25 frames a second with 40 subframes
		// which means one tick per millisecond.  When SMPTE is
		// being used, there is no real concept of the quarter note,
		// so presume 60 bpm as a simplification here.
		// return 1000;
	}
	return m_ticksPerQuarterNote;
}

//
// Alias for getTicksPerQuarterNote:
//

int MidiFile::getTPQ(void) const {
	return getTicksPerQuarterNote();
}



//////////////////////////////
//
// MidiFile::getEventCount -- returns the number of events
//   in a given track.
//

int MidiFile::getEventCount(int aTrack) const {
	return m_events[aTrack]->size();
}


int MidiFile::getNumEvents(int aTrack) const {
	return m_events[aTrack]->size();
}



//////////////////////////////
//
// MidiFile::mergeTracks -- combine the data from two
//   tracks into one.  Placing the data in the first
//   track location listed, and Moving the other tracks
//   in the file around to fill in the spot where Track2
//   used to be.  The results of this function call cannot
//   be reversed.
//

void MidiFile::mergeTracks(int aTrack1, int aTrack2) {
	MidiEventList* mergedTrack;
	mergedTrack = new MidiEventList;
	int oldTimeState = getTickState();
	if (oldTimeState == TIME_STATE_DELTA) {
		makeAbsoluteTicks();
	}
	int length = getNumTracks();
	for (int i=0; i<(int)m_events[aTrack1]->size(); i++) {
		mergedTrack->push_back((*m_events[aTrack1])[i]);
	}
	for (int j=0; j<(int)m_events[aTrack2]->size(); j++) {
		(*m_events[aTrack2])[j].track = aTrack1;
		mergedTrack->push_back((*m_events[aTrack2])[j]);
	}

	mergedTrack->sort();

	delete m_events[aTrack1];

	m_events[aTrack1] = mergedTrack;

	for (int i=aTrack2; i<length-1; i++) {
		m_events[i] = m_events[i+1];
		for (int j=0; j<(int)m_events[i]->size(); j++) {
			(*m_events[i])[j].track = i;
		}
	}

	m_events[length-1] = NULL;
	m_events.resize(length-1);

	if (oldTimeState == TIME_STATE_DELTA) {
		deltaTicks();
	}
}



//////////////////////////////
//
// MidiFile::setTicksPerQuarterNote --
//

void MidiFile::setTicksPerQuarterNote(int ticks) {
	m_ticksPerQuarterNote = ticks;
}

//
// Alias for setTicksPerQuarterNote:
//

void MidiFile::setTPQ(int ticks) {
	setTicksPerQuarterNote(ticks);
}


//////////////////////////////
//
// MidiFile::setMillisecondTicks -- set the ticks per quarter note
//   value to milliseconds.  The format for this specification is
//   highest 8-bits: SMPTE Frame rate (as a negative 2's compliment value).
//   lowest 8-bits: divisions per frame (as a positive number).
//   for millisecond resolution, the SMPTE value is -25, and the
//   frame rate is 40 frame per division.  In hexadecimal, these
//   values are: -25 = 1110,0111 = 0xE7 and 40 = 0010,1000 = 0x28
//   So setting the ticks per quarter note value to 0xE728 will cause
//   delta times in the MIDI file to represent milliseconds.  Calling
//   this function will not change any exiting timestamps, it will
//   only change the meaning of the timestamps.
//

void MidiFile::setMillisecondTicks(void) {
	m_ticksPerQuarterNote = 0xE728;
}



//////////////////////////////
//
// MidiFile::sortTrack -- Sort the specified track in tick order.
//    If the MidiEvent::seq variables have been filled in with
//    a sequence value, this will preserve the order of the
//    events that occur at the same tick time before the sort
//    was done.
//

void MidiFile::sortTrack(int track) {
	if ((track >= 0) && (track < getTrackCount())) {
		m_events.at(track)->sort();
	} else {
		std::cerr << "Warning: track " << track << " does not exist." << std::endl;
	}
}



//////////////////////////////
//
// MidiFile::sortTracks -- sort all tracks in the MidiFile.
//

void MidiFile::sortTracks(void) {
	if (m_theTimeState == TIME_STATE_ABSOLUTE) {
		for (int i=0; i<getTrackCount(); i++) {
			m_events.at(i)->sort();
		}
	} else {
		std::cerr << "Warning: Sorting only allowed in absolute tick mode.";
	}
}



//////////////////////////////
//
// MidiFile::getTrackCountAsType1 --  Return the number of tracks in the
//    MIDI file.  Returns the size of the events if not in joined state.
//    If in joined state, reads track 0 to find the maximum track
//    value from the original unjoined tracks.
//

int MidiFile::getTrackCountAsType1(void) {
	if (getTrackState() == TRACK_STATE_JOINED) {
		int output = 0;
		int i;
		for (i=0; i<(int)m_events[0]->size(); i++) {
			if (getEvent(0,i).track > output) {
				output = getEvent(0,i).track;
			}
		}
		return output+1;  // I think the track values are 0 offset...
	} else {
		return (int)m_events.size();
	}
}



//////////////////////////////
//
// MidiFile::clearLinks --
//

void MidiFile::clearLinks(void) {
	for (int i=0; i<getTrackCount(); i++) {
		if (m_events[i] == NULL) {
			continue;
		}
		m_events[i]->clearLinks();
	}
	m_linkedEventsQ = false;
}



///////////////////////////////////////////////////////////////////////////
//
// private functions
//

//////////////////////////////
//
// MidiFile::linearTickInterpolationAtSecond -- return the tick value at the
//    given input time.
//

double MidiFile::linearTickInterpolationAtSecond(double seconds) {
	if (m_timemapvalid == 0) {
		buildTimeMap();
		if (m_timemapvalid == 0) {
			return -1.0;    // something went wrong
		}
	}

	int i;
	double lasttime = m_timemap[m_timemap.size()-1].seconds;
	// give an error value of -1 if time is out of range of data.
	if (seconds < 0.0) {
		return -1.0;
	}
	if (seconds > m_timemap[m_timemap.size()-1].seconds) {
		return -1.0;
	}

	// Guess which side of the list is closest to target:
	// Could do a more efficient algorithm since time values are sorted,
	// but good enough for now...
	int startindex = -1;
	if (seconds < lasttime / 2) {
		for (i=0; i<(int)m_timemap.size(); i++) {
			if (m_timemap[i].seconds > seconds) {
				startindex = i-1;
				break;
			} else if (m_timemap[i].seconds == seconds) {
				startindex = i;
				break;
			}
		}
	} else {
		for (i=(int)m_timemap.size()-1; i>0; i--) {
			if (m_timemap[i].seconds < seconds) {
				startindex = i+1;
				break;
			} else if (m_timemap[i].seconds == seconds) {
				startindex = i;
				break;
			}
		}
	}

	if (startindex < 0) {
		return -1.0;
	}
	if (startindex >= (int)m_timemap.size()-1) {
		return -1.0;
	}

	double x1 = m_timemap[startindex].seconds;
	double x2 = m_timemap[startindex+1].seconds;
	double y1 = m_timemap[startindex].tick;
	double y2 = m_timemap[startindex+1].tick;
	double xi = seconds;

	return (xi-x1) * ((y2-y1)/(x2-x1)) + y1;
}



//////////////////////////////
//
// MidiFile::linearSecondInterpolationAtTick -- return the time in seconds
//    value at the given input tick time. (Ticks input could be made double).
//

double MidiFile::linearSecondInterpolationAtTick(int ticktime) {
	if (m_timemapvalid == 0) {
		buildTimeMap();
		if (m_timemapvalid == 0) {
			return -1.0;    // something went wrong
		}
	}

	int i;
	double lasttick = m_timemap[m_timemap.size()-1].tick;
	// give an error value of -1 if time is out of range of data.
	if (ticktime < 0.0) {
		return -1;
	}
	if (ticktime > m_timemap.back().tick) {
		return -1;  // don't try to extrapolate
	}

	// Guess which side of the list is closest to target:
	// Could do a more efficient algorithm since time values are sorted,
	// but good enough for now...
	int startindex = -1;
	if (ticktime < lasttick / 2) {
		for (i=0; i<(int)m_timemap.size(); i++) {
			if (m_timemap[i].tick > ticktime) {
				startindex = i-1;
				break;
			} else if (m_timemap[i].tick == ticktime) {
				startindex = i;
				break;
			}
		}
	} else {
		for (i=(int)m_timemap.size()-1; i>0; i--) {
			if (m_timemap[i].tick < ticktime) {
				startindex = i;
				break;
			} else if (m_timemap[i].tick == ticktime) {
				startindex = i;
				break;
			}
		}
	}

	if (startindex < 0) {
		return -1;
	}
	if (startindex >= (int)m_timemap.size()-1) {
		return -1;
	}

	if (m_timemap[startindex].tick == ticktime) {
		return m_timemap[startindex].seconds;
	}

	double x1 = m_timemap[startindex].tick;
	double x2 = m_timemap[startindex+1].tick;
	double y1 = m_timemap[startindex].seconds;
	double y2 = m_timemap[startindex+1].seconds;
	double xi = ticktime;

	return (xi-x1) * ((y2-y1)/(x2-x1)) + y1;
}



//////////////////////////////
//
// MidiFile::buildTimeMap -- build an index of the absolute tick values
//      found in a MIDI file, and their corresponding time values in
//      seconds, taking into consideration tempo change messages.  If no
//      tempo messages are given (or until they are given, then the
//      tempo is set to 120 beats per minute).  If SMPTE time code is
//      used, then ticks are actually time values.  So don't build
//      a time map for SMPTE ticks, and just calculate the time in
//      seconds from the tick value (1000 ticks per second SMPTE
//      is the only mode tested (25 frames per second and 40 subframes
//      per frame).
//

void MidiFile::buildTimeMap(void) {

	// convert the MIDI file to absolute time representation
	// in single track mode (and undo if the MIDI file was not
	// in that state when this function was called.
	//
	int trackstate = getTrackState();
	int timestate  = getTickState();

	makeAbsoluteTicks();
	joinTracks();

	int allocsize = getNumEvents(0);
	m_timemap.reserve(allocsize+10);
	m_timemap.clear();

	_TickTime value;

	int lasttick = 0;
	int tickinit = 0;

	int i;
	int tpq = getTicksPerQuarterNote();
	double defaultTempo = 120.0;
	double secondsPerTick = 60.0 / (defaultTempo * tpq);

	double lastsec = 0.0;
	double cursec = 0.0;

	for (i=0; i<getNumEvents(0); i++) {
		int curtick = getEvent(0, i).tick;
		getEvent(0, i).seconds = cursec;
		if ((curtick > lasttick) || !tickinit) {
			tickinit = 1;

			// calculate the current time in seconds:
			cursec = lastsec + (curtick - lasttick) * secondsPerTick;
			getEvent(0, i).seconds = cursec;

			// store the new tick to second mapping
			value.tick = curtick;
			value.seconds = cursec;
			m_timemap.push_back(value);
			lasttick   = curtick;
			lastsec    = cursec;
		}

		// update the tempo if needed:
		if (getEvent(0,i).isTempo()) {
			secondsPerTick = getEvent(0,i).getTempoSPT(getTicksPerQuarterNote());
		}
	}

	// reset the states of the tracks or time values if necessary here:
	if (timestate == TIME_STATE_DELTA) {
		deltaTicks();
	}
	if (trackstate == TRACK_STATE_SPLIT) {
		splitTracks();
	}

	m_timemapvalid = 1;

}



//////////////////////////////
//
// MidiFile::extractMidiData -- Extract MIDI data from input
//    stream.  Return value is 0 if failure; otherwise, returns 1.
//

int MidiFile::extractMidiData(std::istream& input, std::vector<uchar>& array,
	uchar& runningCommand) {

	int character;
	uchar byte;
	array.clear();
	int runningQ;

	character = input.get();
	if (character == EOF) {
		std::cerr << "Error: unexpected end of file." << std::endl;
		return 0;
	} else {
		byte = (uchar)character;
	}

	if (byte < 0x80) {
		runningQ = 1;
		if (runningCommand == 0) {
			std::cerr << "Error: running command with no previous command" << std::endl;
			return 0;
		}
		if (runningCommand >= 0xf0) {
			std::cerr << "Error: running status not permitted with meta and sysex"
			     << " event." << std::endl;
			std::cerr << "Byte is 0x" << std::hex << (int)byte << std::dec << std::endl;
			return 0;
		}
	} else {
		runningCommand = byte;
		runningQ = 0;
	}

	array.push_back(runningCommand);
	if (runningQ) {
		array.push_back(byte);
	}

	switch (runningCommand & 0xf0) {
		case 0x80:        // note off (2 more bytes)
		case 0x90:        // note on (2 more bytes)
		case 0xA0:        // aftertouch (2 more bytes)
		case 0xB0:        // cont. controller (2 more bytes)
		case 0xE0:        // pitch wheel (2 more bytes)
			byte = readByte(input);
			if (!status()) { return m_rwstatus; }
			if (byte > 0x7f) {
				std::cerr << "MIDI data byte too large: " << (int)byte << std::endl;
				m_rwstatus = false; return m_rwstatus;
			}
			array.push_back(byte);
			if (!runningQ) {
				byte = readByte(input);
				if (!status()) { return m_rwstatus; }
				if (byte > 0x7f) {
					std::cerr << "MIDI data byte too large: " << (int)byte << std::endl;
					m_rwstatus = false; return m_rwstatus;
				}
				array.push_back(byte);
			}
			break;
		case 0xC0:        // patch change (1 more byte)
		case 0xD0:        // channel pressure (1 more byte)
			if (!runningQ) {
				byte = readByte(input);
				if (!status()) { return m_rwstatus; }
				if (byte > 0x7f) {
					std::cerr << "MIDI data byte too large: " << (int)byte << std::endl;
					m_rwstatus = false; return m_rwstatus;
				}
				array.push_back(byte);
			}
			break;
		case 0xF0:
			switch (runningCommand) {
				case 0xff:                 // meta event
					{
					if (!runningQ) {
						byte = readByte(input); // meta type
						if (!status()) { return m_rwstatus; }
						array.push_back(byte);
					}
					ulong length = 0;
					uchar byte1 = 0;
					uchar byte2 = 0;
					uchar byte3 = 0;
					uchar byte4 = 0;
					byte1 = readByte(input);
					if (!status()) { return m_rwstatus; }
					array.push_back(byte1);
					if (byte1 >= 0x80) {
						byte2 = readByte(input);
						if (!status()) { return m_rwstatus; }
						array.push_back(byte2);
						if (byte2 > 0x80) {
							byte3 = readByte(input);
							if (!status()) { return m_rwstatus; }
							array.push_back(byte3);
							if (byte3 >= 0x80) {
								byte4 = readByte(input);
								if (!status()) { return m_rwstatus; }
								array.push_back(byte4);
								if (byte4 >= 0x80) {
									std::cerr << "Error: cannot handle large VLVs" << std::endl;
									m_rwstatus = false; return m_rwstatus;
								} else {
									length = unpackVLV(byte1, byte2, byte3, byte4);
									if (!m_rwstatus) { return m_rwstatus; }
								}
							} else {
								length = unpackVLV(byte1, byte2, byte3);
								if (!m_rwstatus) { return m_rwstatus; }
							}
						} else {
							length = unpackVLV(byte1, byte2);
							if (!m_rwstatus) { return m_rwstatus; }
						}
					} else {
						length = byte1;
					}
					for (int j=0; j<(int)length; j++) {
						byte = readByte(input); // meta type
						if (!status()) { return m_rwstatus; }
						array.push_back(byte);
					}
					}
					break;

				// The 0xf0 and 0xf7 meta commands deal with system-exclusive
				// messages. 0xf0 is used to either start a message or to store
				// a complete message.  The 0xf0 is part of the outgoing MIDI
				// bytes.  The 0xf7 message is used to send arbitrary bytes,
				// typically the middle or ends of system exclusive messages.  The
				// 0xf7 byte at the start of the message is not part of the
				// outgoing raw MIDI bytes, but is kept in the MidiFile message
				// to indicate a raw MIDI byte message (typically a partial
				// system exclusive message).
				case 0xf7:   // Raw bytes. 0xf7 is not part of the raw
				             // bytes, but are included to indicate
				             // that this is a raw byte message.
				case 0xf0:   // System Exclusive message
					{         // (complete, or start of message).
					int length = (int)readVLValue(input);
					for (int i=0; i<length; i++) {
						byte = readByte(input);
						if (!status()) { return m_rwstatus; }
						array.push_back(byte);
					}
					}
					break;

				// other "F" MIDI commands are not expected, but can be
				// handled here if they exist.
			}
			break;
		default:
			std::cout << "Error reading midifile" << std::endl;
			std::cout << "Command byte was " << (int)runningCommand << std::endl;
			return 0;
	}
	return 1;
}



//////////////////////////////
//
// MidiFile::readVLValue -- The VLV value is expected to be unpacked into
//   a 4-byte integer no greater than 0x0fffFFFF, so a VLV value up to
//   4-bytes in size (FF FF FF 7F) will only be considered.  Longer
//   VLV values are not allowed in standard MIDI files, so the extract
//   delta time would be truncated and the extra byte(s) will be parsed
//   incorrectly as a MIDI command.
//

ulong MidiFile::readVLValue(std::istream& input) {
	uchar b[5] = {0};

    for (uchar &item : b) {
            item = readByte(input);
            if (!status()) {
                    return m_rwstatus;
            }
            if (item < 0x80) {
                    break;
            }
    }

    return unpackVLV(b[0], b[1], b[2], b[3], b[4]);
}



//////////////////////////////
//
// MidiFile::unpackVLV -- converts a VLV value to an unsigned long value.
//     The bytes a, b, c, d, e are in big-endian order (the order they would
//     be read out of the MIDI file).
// default values: a = b = c = d = 0;
//

ulong MidiFile::unpackVLV(uchar a, uchar b, uchar c, uchar d, uchar e) {
	uchar bytes[5] = {a, b, c, d, e};
	int count = 0;
	while ((count < 5) && (bytes[count] > 0x7f)) {
		count++;
	}
	count++;
	if (count >= 6) {
		std::cerr << "VLV number is too large" << std::endl;
		m_rwstatus = false;
		return 0;
	}

	ulong output = 0;
	for (int i=0; i<count; i++) {
		output = output << 7;
		output = output | (bytes[i] & 0x7f);
	}

	return output;
}



//////////////////////////////
//
// MidiFile::writeVLValue -- write a number to the midifile
//    as a variable length value which segments a file into 7-bit
//    values and adds a continuation bit to each.  Maximum size of input
//    aValue is 0x0FFFffff.
//

void MidiFile::writeVLValue(long aValue, std::vector<uchar>& outdata) {
	uchar bytes[4] = {0};

	if ((unsigned long)aValue >= (1 << 28)) {
		std::cerr << "Error: number too large to convert to VLV" << std::endl;
		aValue = 0x0FFFffff;
	}

	bytes[0] = (uchar)(((ulong)aValue >> 21) & 0x7f);  // most significant 7 bits
	bytes[1] = (uchar)(((ulong)aValue >> 14) & 0x7f);
	bytes[2] = (uchar)(((ulong)aValue >> 7)  & 0x7f);
	bytes[3] = (uchar)(((ulong)aValue)       & 0x7f);  // least significant 7 bits

	int start = 0;
	while ((start<4) && (bytes[start] == 0))  start++;

	for (int i=start; i<3; i++) {
		bytes[i] = bytes[i] | 0x80;
		outdata.push_back(bytes[i]);
	}
	outdata.push_back(bytes[3]);
}



//////////////////////////////
//
// MidiFile::clear_no_deallocate -- Similar to clear() but does not
//   delete the Events in the lists.  This is primarily used internally
//   to the MidiFile class, so don't use unless you really know what you
//   are doing (otherwise you will end up with memory leaks or
//   segmentation faults).
//

void MidiFile::clear_no_deallocate(void) {
	for (int i=0; i<getTrackCount(); i++) {
		m_events[i]->detach();
		delete m_events[i];
		m_events[i] = NULL;
	}
	m_events.resize(1);
	m_events[0] = new MidiEventList;
	m_timemapvalid=0;
	m_timemap.clear();
	// m_events.resize(0);   // causes a memory leak [20150205 Jorden Thatcher]
}



//////////////////////////////
//
// MidiFile::ticksearch -- for finding a tick entry in the time map.
//

int MidiFile::ticksearch(const void* A, const void* B) {
	_TickTime& a = *((_TickTime*)A);
	_TickTime& b = *((_TickTime*)B);

	if (a.tick < b.tick) {
		return -1;
	} else if (a.tick > b.tick) {
		return 1;
	}
	return 0;
}



//////////////////////////////
//
// MidiFile::secondsearch -- for finding a second entry in the time map.
//

int MidiFile::secondsearch(const void* A, const void* B) {
	_TickTime& a = *((_TickTime*)A);
	_TickTime& b = *((_TickTime*)B);

	if (a.seconds < b.seconds) {
		return -1;
	} else if (a.seconds > b.seconds) {
		return 1;
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////
//
// Static functions:
//


//////////////////////////////
//
// MidiFile::readLittleEndian4Bytes -- Read four bytes which are in
//      little-endian order (smallest byte is first).  Then flip
//      the order of the bytes to create the return value.
//

ulong MidiFile::readLittleEndian4Bytes(std::istream& input) {
	uchar buffer[4] = {0};
	input.read((char*)buffer, 4);
	if (input.eof()) {
		std::cerr << "Error: unexpected end of file." << std::endl;
		return 0;
	}
	return buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
}



//////////////////////////////
//
// MidiFile::readLittleEndian2Bytes -- Read two bytes which are in
//       little-endian order (smallest byte is first).  Then flip
//       the order of the bytes to create the return value.
//

ushort MidiFile::readLittleEndian2Bytes(std::istream& input) {
	uchar buffer[2] = {0};
	input.read((char*)buffer, 2);
	if (input.eof()) {
		std::cerr << "Error: unexpected end of file." << std::endl;
		return 0;
	}
	return buffer[1] | (buffer[0] << 8);
}



//////////////////////////////
//
// MidiFile::readByte -- Read one byte from input stream.  Set
//     fail status error if there was a problem (calling function
//     has to check this status for an error after reading).
//

uchar MidiFile::readByte(std::istream& input) {
	uchar buffer[1] = {0};
	input.read((char*)buffer, 1);
	if (input.eof()) {
		std::cerr << "Error: unexpected end of file." << std::endl;
		m_rwstatus = false;
		return 0;
	}
	return buffer[0];
}



//////////////////////////////
//
// MidiFile::writeLittleEndianUShort --
//

std::ostream& MidiFile::writeLittleEndianUShort(std::ostream& out, ushort value) {
	union { char bytes[2]; ushort us; } data;
	data.us = value;
	out << data.bytes[0];
	out << data.bytes[1];
	return out;
}



//////////////////////////////
//
// MidiFile::writeBigEndianUShort --
//

std::ostream& MidiFile::writeBigEndianUShort(std::ostream& out, ushort value) {
	union { char bytes[2]; ushort us; } data;
	data.us = value;
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// MidiFile::writeLittleEndianShort --
//

std::ostream& MidiFile::writeLittleEndianShort(std::ostream& out, short value) {
	union { char bytes[2]; short s; } data;
	data.s = value;
	out << data.bytes[0];
	out << data.bytes[1];
	return out;
}



//////////////////////////////
//
// MidiFile::writeBigEndianShort --
//

std::ostream& MidiFile::writeBigEndianShort(std::ostream& out, short value) {
	union { char bytes[2]; short s; } data;
	data.s = value;
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// MidiFile::writeLittleEndianULong --
//

std::ostream& MidiFile::writeLittleEndianULong(std::ostream& out, ulong value) {
	union { char bytes[4]; ulong ul; } data;
	data.ul = value;
	out << data.bytes[0];
	out << data.bytes[1];
	out << data.bytes[2];
	out << data.bytes[3];
	return out;
}



//////////////////////////////
//
// MidiFile::writeBigEndianULong --
//

std::ostream& MidiFile::writeBigEndianULong(std::ostream& out, ulong value) {
	union { char bytes[4]; long ul; } data;
	data.ul = value;
	out << data.bytes[3];
	out << data.bytes[2];
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// MidiFile::writeLittleEndianLong --
//

std::ostream& MidiFile::writeLittleEndianLong(std::ostream& out, long value) {
	union { char bytes[4]; long l; } data;
	data.l = value;
	out << data.bytes[0];
	out << data.bytes[1];
	out << data.bytes[2];
	out << data.bytes[3];
	return out;
}



//////////////////////////////
//
// MidiFile::writeBigEndianLong --
//

std::ostream& MidiFile::writeBigEndianLong(std::ostream& out, long value) {
	union { char bytes[4]; long l; } data;
	data.l = value;
	out << data.bytes[3];
	out << data.bytes[2];
	out << data.bytes[1];
	out << data.bytes[0];
	return out;

}



//////////////////////////////
//
// MidiFile::writeBigEndianFloat --
//

std::ostream& MidiFile::writeBigEndianFloat(std::ostream& out, float value) {
	union { char bytes[4]; float f; } data;
	data.f = value;
	out << data.bytes[3];
	out << data.bytes[2];
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// MidiFile::writeLittleEndianFloat --
//

std::ostream& MidiFile::writeLittleEndianFloat(std::ostream& out, float value) {
	union { char bytes[4]; float f; } data;
	data.f = value;
	out << data.bytes[0];
	out << data.bytes[1];
	out << data.bytes[2];
	out << data.bytes[3];
	return out;
}



//////////////////////////////
//
// MidiFile::writeBigEndianDouble --
//

std::ostream& MidiFile::writeBigEndianDouble(std::ostream& out, double value) {
	union { char bytes[8]; double d; } data;
	data.d = value;
	out << data.bytes[7];
	out << data.bytes[6];
	out << data.bytes[5];
	out << data.bytes[4];
	out << data.bytes[3];
	out << data.bytes[2];
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// MidiFile::writeLittleEndianDouble --
//

std::ostream& MidiFile::writeLittleEndianDouble(std::ostream& out, double value) {
	union { char bytes[8]; double d; } data;
	data.d = value;
	out << data.bytes[0];
	out << data.bytes[1];
	out << data.bytes[2];
	out << data.bytes[3];
	out << data.bytes[4];
	out << data.bytes[5];
	out << data.bytes[6];
	out << data.bytes[7];
	return out;
}



////////////////////
//
// MidiFile::getGMInstrumentName -- return the General MIDI instrument name
//    for the given patch change index (in the range from 0 to 127).
//

std::string MidiFile::getGMInstrumentName(int patchIndex) {
	if (patchIndex < 0) {
		return "";
	}
	if (patchIndex > 127) {
		return "";
	}
	return GMinstrument[patchIndex];
}



//////////////////////////////
//
// MidiFile::base64Encode -- Encode a string as base64.
//

std::string MidiFile::base64Encode(const std::string& input) {
	std::string output;
	output.reserve(((input.size()/3) + (input.size() % 3 > 0)) * 4);
	int vala = 0;
	int valb = -6;
	for (uchar c : input) {
		vala = (vala << 8) + c;
		valb += 8;
		while (valb >=0) {
			output.push_back(MidiFile::encodeLookup[(vala >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6) {
		output.push_back(MidiFile::encodeLookup[((vala << 8) >> (valb + 8)) & 0x3F]);
	}
	while (output.size() % 4) {
		output.push_back(MidiFile::encodeLookup.back());
	}
	return output;
}



//////////////////////////////
//
// MidiFile::base64Decode -- Decode a base64 string.
//

std::string MidiFile::base64Decode(const std::string& input) {
	// vector<int> decodeLookup(256,-1);
	// for (int i=0; i<64; i++) decodeLookup[encodeLookup[i]] = i;

	std::string output;
	int vala = 0;
	int valb = -8;
	for (uchar c : input) {
		if (c == '=') {
			break;
		} else if (MidiFile::decodeLookup[c] == -1) {
         // Ignore whitespace, for example.
			continue;
		}
		vala = (vala << 6) + MidiFile::decodeLookup[c];
		valb += 6;
		if (valb >= 0) {
			output.push_back(char((vala >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return output;
}



} // end namespace smf

///////////////////////////////////////////////////////////////////////////
//
// external functions
//

//////////////////////////////
//
// operator<< -- for printing an ASCII version of the MIDI file
//

std::ostream& operator<<(std::ostream& out, smf::MidiFile& aMidiFile) {
	aMidiFile.writeBinascWithComments(out);
	return out;
}



