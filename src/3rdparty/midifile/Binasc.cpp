//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Mon Feb 16 12:26:32 PST 2015 Adapted from binasc program.
// Last Modified: Sat Apr 21 10:52:19 PDT 2018 Removed using namespace std;
// Filename:      midifile/src/Binasc.cpp
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// description:   Interface to convert bytes between binary and ASCII forms.
//

#include "Binasc.h"

#include <cstdlib>
#include <sstream>


namespace smf {

const char* Binasc::GMinstrument[128] = {
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
// Binasc::Binasc -- Constructor: set the default option values.
//

Binasc::Binasc(void) {
	m_bytesQ    = 1; // for printing HEX bytes when converting to ASCII
	m_commentsQ = 0; // for printing text comments when converting to ASCII
	m_midiQ     = 0; // for printing ASCII as parsed MIDI file.
	m_maxLineLength = 75;
	m_maxLineBytes  = 25;
}



//////////////////////////////
//
// Binasc::~Binasc -- Destructor.
//

Binasc::~Binasc() {
	// do nothing
}



//////////////////////////////
//
// Binasc::setLineLength -- Set the maximum length of a line when converting
//    binary content into ASCII bytes.  If the input size is less than one,
//    set to the default value of 75 characters per line.
//

int Binasc::setLineLength(int length) {
	if (length < 1) {
		m_maxLineLength = 75;
	} else {
		m_maxLineLength = length;
	}
	return m_maxLineLength;
}



//////////////////////////////
//
// Binasc::getLineLength -- Set the maximum length of a line when converting
//    binary content into ASCII bytes.
//

int Binasc::getLineLength(void) {
	return m_maxLineLength;
}



//////////////////////////////
//
// Binasc::setLineBytes -- Set the maximum number of hex bytes in ASCII output.
//    If the input size is less than one, set to the default value of 25
//    hex bytes per line.
//

int Binasc::setLineBytes(int length) {
	if (length < 1) {
		m_maxLineBytes = 25;
	} else {
		m_maxLineBytes = length;
	}
	return m_maxLineBytes;
}



//////////////////////////////
//
// Binasc::getLineBytes -- Get the maximum number of hex bytes in ASCII output.
//

int Binasc::getLineBytes(void) {
	return m_maxLineLength;
}



//////////////////////////////
//
// Binasc::setComments -- Display or not display printable characters
//    as comments when converting binary files to ASCII byte codes.
//

void Binasc::setComments(int state) {
	m_commentsQ = state ? 1 : 0;
}


void Binasc::setCommentsOn(void) {
	setComments(true);
}


void Binasc::setCommentsOff(void) {
	setComments(false);
}



//////////////////////////////
//
// Binasc::getComments -- Get the comment display style for
//    showing comments in ASCII output;
//

int Binasc::getComments(void) {
	return m_commentsQ;
}



//////////////////////////////
//
// Binasc::setBytes -- Display or not display hex codes (only
//    print ASCII printable characters).
//

void Binasc::setBytes(int state) {
	m_bytesQ = state ? 1 : 0;
}


void Binasc::setBytesOn(void) {
	setBytes(true);
}


void Binasc::setBytesOff(void) {
	setBytes(false);
}


//////////////////////////////
//
// Binasc::getBytes -- Get hex byte display status.
//

int Binasc::getBytes(void) {
	return m_bytesQ;
}


//////////////////////////////
//
// Binasc::setMidi -- Display or not display parsed MIDI data.
//

void Binasc::setMidi(int state) {
	m_midiQ = state ? 1 : 0;
}


void Binasc::setMidiOn(void) {
	setMidi(true);
}


void Binasc::setMidiOff(void) {
	setMidi(false);
}



//////////////////////////////
//
// Binasc::getMidi -- Get the MIDI file printing style option state.
//

int Binasc::getMidi(void) {
	return m_midiQ;
}



//////////////////////////////
//
// Binasc::writeToBinary -- Convert an ASCII representation of bytes into
//     the binary file that it describes.  Returns 0 if there was a problem
//     otherwise returns 1.
//

int Binasc::writeToBinary(const std::string& outfile,
		const std::string& infile) {
	std::ifstream input;
	input.open(infile.c_str());
	if (!input.is_open()) {
		std::cerr << "Cannot open " << infile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	std::ofstream output;
	output.open(outfile.c_str());
	if (!output.is_open()) {
		std::cerr << "Cannot open " << outfile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = writeToBinary(output, input);
	input.close();
	output.close();
	return status;
}


int Binasc::writeToBinary(const std::string& outfile, std::istream& input) {
	std::ofstream output;
	output.open(outfile.c_str());
	if (!output.is_open()) {
		std::cerr << "Cannot open " << outfile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = writeToBinary(output, input);
	output.close();
	return status;
}


int Binasc::writeToBinary(std::ostream& out, const std::string& infile) {
	std::ifstream input;
	input.open(infile.c_str());
	if (!input.is_open()) {
		std::cerr << "Cannot open " << infile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = writeToBinary(out, input);
	input.close();
	return status;
}


int Binasc::writeToBinary(std::ostream& out, std::istream& input) {
	std::string inputLine;
	inputLine.reserve(8196);
	int  lineNum = 0;              // current line number
	getline(input, inputLine, '\n');
	lineNum++;
	while (!input.eof()) {
		int status = processLine(out, inputLine, lineNum);
		if (!status) {
			return 0;
		}
		getline(input, inputLine, '\n');
		lineNum++;
	}
	return 1;
}



//////////////////////////////
//
// Binasc::readFromBinary -- convert an ASCII representation of bytes into
//     the binary file that it describes.
//

int Binasc::readFromBinary(const std::string& outfile, const std::string& infile) {
	std::ifstream input;
	input.open(infile.c_str());
	if (!input.is_open()) {
		std::cerr << "Cannot open " << infile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	std::ofstream output;
	output.open(outfile.c_str());
	if (!output.is_open()) {
		std::cerr << "Cannot open " << outfile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = readFromBinary(output, input);
	input.close();
	output.close();
	return status;
}


int Binasc::readFromBinary(const std::string& outfile, std::istream& input) {
	std::ofstream output;
	output.open(outfile.c_str());
	if (!output.is_open()) {
		std::cerr << "Cannot open " << outfile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = readFromBinary(output, input);
	output.close();
	return status;
}


int Binasc::readFromBinary(std::ostream& out, const std::string& infile) {
	std::ifstream input;
	input.open(infile.c_str());
	if (!input.is_open()) {
		std::cerr << "Cannot open " << infile
		          << " for reading in binasc." << std::endl;
		return 0;
	}

	int status = readFromBinary(out, input);
	input.close();
	return status;
}


int Binasc::readFromBinary(std::ostream& out, std::istream& input) {
	int status;
	if (m_midiQ) {
		status = outputStyleMidi(out, input);
	} else if (!m_bytesQ) {
		status = outputStyleAscii(out, input);
	} else if (m_bytesQ && m_commentsQ) {
		status = outputStyleBoth(out, input);
	} else {
		status = outputStyleBinary(out, input);
	}
	return status;
}



///////////////////////////////////////////////////////////////////////////
//
// protected functions --
//

//////////////////////////////
//
// Binasc::outputStyleAscii -- read an input file and output bytes in ascii
//    form, not displaying any blank lines.  Output words are not
//    broken unless they are longer than 75 characters.
//

int Binasc::outputStyleAscii(std::ostream& out, std::istream& input) {
	uchar outputWord[256] = {0};   // storage for current word
	int index     = 0;             // current length of word
	int lineCount = 0;             // current length of line
	int type      = 0;             // 0=space, 1=printable
	uchar ch;                      // current input byte

	ch = static_cast<uchar>(input.get());
	while (!input.eof()) {
		int lastType = type;
		type = (isprint(ch) && !isspace(ch)) ? 1 : 0;

		if ((type == 1) && (lastType == 0)) {
			// start of a new word.  check where to put old word
			if (index + lineCount >= m_maxLineLength) {  // put on next line
				outputWord[index] = '\0';
				out << '\n' << outputWord;
				lineCount = index;
				index = 0;
			} else {                                   // put on current line
				outputWord[index] = '\0';
				if (lineCount != 0) {
					out << ' ';
					lineCount++;
				}
				out << outputWord;
				lineCount += index;
				index = 0;
			}
		}
		if (type == 1) {
			outputWord[index++] = ch;
		}
		ch = static_cast<uchar>(input.get());
	}

	if (index != 0) {
		out << std::endl;
	}

	return 1;
}



//////////////////////////////
//
// Binasc::outputStyleBinary -- read an input binary file and output bytes
//     in ascii form, hexadecimal numbers only.
//

int Binasc::outputStyleBinary(std::ostream& out, std::istream& input) {
	int currentByte = 0;    // current byte output in line
	uchar ch;               // current input byte

	ch = static_cast<uchar>(input.get());
	if (input.eof()) {
		std::cerr << "End of the file right away!" << std::endl;
		return 0;
	}

	while (!input.eof()) {
		if (ch < 0x10) {
			out << '0';
		}
		out << std::hex << (int)ch << ' ';
		currentByte++;
		if (currentByte >= m_maxLineBytes) {
			out << '\n';
			currentByte = 0;
		}
		ch = static_cast<uchar>(input.get());
	}

	if (currentByte != 0) {
		out << std::endl;
	}

	return 1;
}



//////////////////////////////
//
// Binasc::outputStyleBoth -- read an input file and output bytes in ASCII
//     form with both hexadecimal numbers and ascii representation
//

int Binasc::outputStyleBoth(std::ostream& out, std::istream& input) {
	uchar asciiLine[256] = {0};    // storage for output line
	int currentByte = 0;           // current byte output in line
	int index = 0;                 // current character in asciiLine
	uchar ch;                      // current input byte

	ch = static_cast<uchar>(input.get());
	while (!input.eof()) {
		if (index == 0) {
			asciiLine[index++] = ';';
			out << ' ';
		}
		if (ch < 0x10) {
			out << '0';
		}
		out << std::hex << (int)ch << ' ';
		currentByte++;

		asciiLine[index++] = ' ';
		if (isprint(ch)) {
			asciiLine[index++] = ch;
		} else {
			asciiLine[index++] = ' ';
		}
		asciiLine[index++] = ' ';

		if (currentByte >= m_maxLineBytes) {
			out << '\n';
			asciiLine[index] = '\0';
			out << asciiLine << "\n\n";
			currentByte = 0;
			index = 0;
		}
		ch = static_cast<uchar>(input.get());
	}

	if (currentByte != 0) {
		out << '\n';
		asciiLine[index] = '\0';
		out << asciiLine << '\n' << std::endl;
	}

	return 1;
}



///////////////////////////////
//
// Binasc::processLine -- Read a line of input and output any specified bytes.
//

int Binasc::processLine(std::ostream& out, const std::string& input,
		int lineCount) {
	int status = 1;
	int i = 0;
	int length = (int)input.size();
	std::string word;
	while (i<length) {
		if ((input[i] == ';') || (input[i] == '#') || (input[i] == '/')) {
			// comment to end of line, so ignore
			return status;
		} else if ((input[i] == ' ') || (input[i] == '\n')
				|| (input[i] == '\t')) {
			// ignore whitespace
			i++;
			continue;
		} else if (input[i] == '+') {
			i = getWord(word, input, " \n\t", i);
			status = processAsciiWord(out, word, lineCount);
		} else if (input[i] == '"') {
			i = getWord(word, input, "\"", i);
			status = processStringWord(out, word, lineCount);
		} else if (input[i] == 'v') {
			i = getWord(word, input, " \n\t", i);
			status = processVlvWord(out, word, lineCount);
		} else if (input[i] == 'p') {
			i = getWord(word, input, " \n\t", i);
			status = processMidiPitchBendWord(out, word, lineCount);
		} else if (input[i] == 't') {
			i = getWord(word, input, " \n\t", i);
			status = processMidiTempoWord(out, word, lineCount);
		} else {
			i = getWord(word, input, " \n\t", i);
			if (word.find('\'') != std::string::npos) {
				status = processDecimalWord(out, word, lineCount);
			} else if ((word.find(',') != std::string::npos)
					|| (word.size() > 2)) {
				status = processBinaryWord(out, word, lineCount);
			} else {
				status = processHexWord(out, word, lineCount);
			}
		}

		if (status == 0) {
			return 0;
		}

	}

	return 1;
}



//////////////////////////////
//
// Binasc::getWord -- extract a sub string, stopping at any of the given
//   terminator characters.
//

int Binasc::getWord(std::string& word, const std::string& input,
		const std::string& terminators, int index) {
	word.resize(0);
	int i = index;
	int escape = 0;
	int ecount = 0;
	if (terminators.find('"') != std::string::npos) {
		escape = 1;
	}
	while (i < (int)input.size()) {
		if (escape && input[i] == '\"') {
			ecount++;
			i++;
			if (ecount >= 2) {
				break;
			}
		}
		if (escape && (i<(int)input.size()-1) && (input[i] == '\\')
				&& (input[i+1] == '"')) {
			word.push_back(input[i+1]);
			i += 2;
		} else if (terminators.find(input[i]) == std::string::npos) {
			word.push_back(input[i]);
			i++;
		} else {
			i++;
			return i;
		}
	}
	return i;
}



///////////////////////////////
//
// Binasc::getVLV -- read a Variable-Length Value from the file
//

int Binasc::getVLV(std::istream& infile, int& trackbytes) {
	int output = 0;
	uchar ch = 0;
	infile.read((char*)&ch, 1);
	trackbytes++;
	output = (output << 7) | (0x7f & ch);
	while (ch >= 0x80) {
		infile.read((char*)&ch, 1);
		trackbytes++;
		output = (output << 7) | (0x7f & ch);
	}
	return output;
}



//////////////////////////////
//
// Binasc::readMidiEvent -- Read a delta time and then a MIDI message
//     (or meta message).  Returns 1 if not end-of-track meta message;
//     0 otherwise.
//

int Binasc::readMidiEvent(std::ostream& out, std::istream& infile,
		int& trackbytes, int& command) {

	// Read and print Variable Length Value for delta ticks
	int vlv = getVLV(infile, trackbytes);

	std::stringstream output;

	output << "v" << std::dec << vlv << "\t";

	std::string comment;

	int status = 1;
	uchar ch = 0;
	char byte1, byte2;
	infile.read((char*)&ch, 1);
	trackbytes++;
	if (ch < 0x80) {
		// running status: command byte is previous one in data stream
		output << "   ";
	} else {
		// midi command byte
		output << std::hex << (int)ch;
		command = ch;
		infile.read((char*)&ch, 1);
		trackbytes++;
	}
	byte1 = ch;
	switch (command & 0xf0) {
		case 0x80:    // note-off: 2 bytes
			output << " '" << std::dec << (int)byte1;
			infile.read((char*)&ch, 1);
			trackbytes++;
			byte2 = ch;
			output << " '" << std::dec << (int)byte2;
			if (m_commentsQ) {
				comment += "note-off " + keyToPitchName(byte1);
			}
			break;
		case 0x90:    // note-on: 2 bytes
			output << " '" << std::dec << (int)byte1;
			infile.read((char*)&ch, 1);
			trackbytes++;
			byte2 = ch;
			output << " '" << std::dec << (int)byte2;
			if (m_commentsQ) {
				if (byte2 == 0) {
					comment += "note-off " + keyToPitchName(byte1);
				} else {
					comment += "note-on " + keyToPitchName(byte1);
				}
			}
			break;
		case 0xA0:    // aftertouch: 2 bytes
			output << " '" << std::dec << (int)byte1;
			infile.read((char*)&ch, 1);
			trackbytes++;
			byte2 = ch;
			output << " '" << std::dec << (int)byte2;
			if (m_commentsQ) {
				comment += "after-touch";
			}
			break;
		case 0xB0:    // continuous controller: 2 bytes
			output << " '" << std::dec << (int)byte1;
			infile.read((char*)&ch, 1);
			trackbytes++;
			byte2 = ch;
			output << " '" << std::dec << (int)byte2;
			if (m_commentsQ) {
				comment += "controller";
			}
			break;
		case 0xE0:    // pitch-bend: 2 bytes
			output << " '" << std::dec << (int)byte1;
			infile.read((char*)&ch, 1);
			trackbytes++;
			byte2 = ch;
			output << " '" << std::dec << (int)byte2;
			if (m_commentsQ) {
				comment += "pitch-bend";
			}
			break;
		case 0xC0:    // patch change: 1 bytes
			output << " '" << std::dec << (int)byte1;
			if (m_commentsQ) {
				output << "\t";
				comment += "patch-change (";
				comment += GMinstrument[byte1 & 0x7f];
				comment += ")";
			}
			break;
		case 0xD0:    // channel pressure: 1 bytes
			output << " '" << std::dec << (int)byte1;
			if (m_commentsQ) {
				comment += "channel pressure";
			}
			break;
		case 0xF0:    // various system bytes: variable bytes
			switch (command) {
				case 0xf0:
					{
					// A system exclusive message.   The first byte
					// is 0xf0, then a VLV of the length of the message
					// and then the message itself (which must end with 0xf7).
					int length = getVLV(infile, trackbytes);
					output << " v" << std::dec << length;
					for (int b=0; b<length; b++) {
						infile.read((char*)&ch, 1);
						trackbytes++;
						if (ch < 0x10) {
						   output << " 0" << std::hex << (int)ch;
						} else {
						   output << " " << std::hex << (int)ch;
						}
					}
					}
					break;
				case 0xf7:
					// Read the first byte which is either 0xf0 or 0xf7.
					// Then a VLV byte count for the number of bytes
					// that remain in the message will follow.
					// Then read that number of bytes.
					{
					infile.putback(byte1);
					trackbytes--;
					int length = getVLV(infile, trackbytes);
					output << " v" << std::dec << length;
					for (int i=0; i<length; i++) {
						infile.read((char*)&ch, 1);
						trackbytes++;
						if (ch < 0x10) {
						   output << " 0" << std::hex << (int)ch;
						} else {
						   output << " " << std::hex << (int)ch;
						}
					}
					}
					break;
				case 0xf1:
					break;
				case 0xf2:
					break;
				case 0xf3:
					break;
				case 0xf4:
					break;
				case 0xf5:
					break;
				case 0xf6:
					break;
				case 0xf8:
					break;
				case 0xf9:
					break;
				case 0xfa:
					break;
				case 0xfb:
					break;
				case 0xfc:
					break;
				case 0xfd:
					break;
				case 0xfe:
					std::cerr << "Error command not yet handled" << std::endl;
					return 0;
					break;
				case 0xff:  // meta message
					{
					int metatype = ch;
					output << " " << std::hex << metatype;
					int length = getVLV(infile, trackbytes);
					output << " v" << std::dec << length;
					switch (metatype) {

						case 0x00:  // sequence number
						   // display two-byte big-endian decimal value.
						   {
						   infile.read((char*)&ch, 1);
						   trackbytes++;
						   int number = ch;
						   infile.read((char*)&ch, 1);
						   trackbytes++;
						   number = (number << 8) | ch;
						   output << " 2'" << number;
						   }
						   break;

						case 0x20: // MIDI channel prefix
						case 0x21: // MIDI port
						   // display single-byte decimal number
						   infile.read((char*)&ch, 1);
						   trackbytes++;
						   output << " '" << (int)ch;
						   break;

						case 0x51: // Tempo
						    // display tempo as "t" word.
						    {
						    int number = 0;
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    number = (number << 8) | ch;
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    number = (number << 8) | ch;
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    number = (number << 8) | ch;
						    double tempo = 1000000.0 / number * 60.0;
						    output << " t" << tempo;
						    }
						    break;

						case 0x54: // SMPTE offset
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // hour
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // minutes
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // seconds
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // frames
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // subframes
						    break;

						case 0x58: // time signature
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // numerator
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // denominator power
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // clocks per beat
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // 32nd notes per beat
						    break;

						case 0x59: // key signature
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // accidentals
						    infile.read((char*)&ch, 1);
						    trackbytes++;
						    output << " '" << (int)ch;  // mode
						    break;

						case 0x01: // text
						case 0x02: // copyright
						case 0x03: // track name
						case 0x04: // instrument name
						case 0x05: // lyric
						case 0x06: // marker
						case 0x07: // cue point
						case 0x08: // program name
						case 0x09: // device name
						   output << " \"";
						   for (int i=0; i<length; i++) {
						      infile.read((char*)&ch, 1);
						      trackbytes++;
								if (ch == '"') {
									output << '\\';
								}
						      output << (char)ch;
						   }
						   output << "\"";
						   break;
						default:
						   for (int i=0; i<length; i++) {
						      infile.read((char*)&ch, 1);
						      trackbytes++;
						      output << " ";
						      if (ch < 0x10) {
						         output << "0";
						      }
						      output << std::hex << (int)ch;
						   }
					}
					switch (metatype) {
						case 0x00: comment += "sequence number";     break;
						case 0x01: comment += "text";                break;
						case 0x02: comment += "copyright notice";    break;
						case 0x03: comment += "track name";          break;
						case 0x04: comment += "instrument name";     break;
						case 0x05: comment += "lyric";               break;
						case 0x06: comment += "marker";              break;
						case 0x07: comment += "cue point";           break;
						case 0x08: comment += "program name";        break;
						case 0x09: comment += "device name";         break;
						case 0x20: comment += "MIDI channel prefix"; break;
						case 0x21: comment += "MIDI port";           break;
						case 0x51: comment += "tempo";               break;
						case 0x54: comment += "SMPTE offset";        break;
						case 0x58: comment += "time signature";      break;
						case 0x59: comment += "key signature";       break;
						case 0x7f: comment += "system exclusive";    break;
						case 0x2f:
						   status = 0;
						   comment += "end-of-track";
						   break;
						default:
						   comment += "meta-message";
					}
					}
					break;

			}
			break;
	}

	out << output.str();
	if (m_commentsQ) {
		out << "\t; " << comment;
	}

	return status;
}



/////////////////////////////
//
// Binasc::keyToPitchName -- Convert a MIDI key number to scientific
//     pitch notation.
//

std::string Binasc::keyToPitchName(int key) {
	int pc = key % 12;
	int octave = key / 12 - 1;
	std::stringstream output;
	switch (pc) {
		case  0: output << "C";  break;
		case  1: output << "C#"; break;
		case  2: output << "D";  break;
		case  3: output << "D#"; break;
		case  4: output << "E";  break;
		case  5: output << "F";  break;
		case  6: output << "F#"; break;
		case  7: output << "G";  break;
		case  8: output << "G#"; break;
		case  9: output << "A";  break;
		case 10: output << "A#"; break;
		case 11: output << "B";  break;
	}
	output << octave;
	return output.str().c_str();
}



//////////////////////////////
//
// Binasc::outputStyleMidi -- Read an input file and output bytes parsed
//     as a MIDI file (return false if not a MIDI file).
//

int Binasc::outputStyleMidi(std::ostream& out, std::istream& input) {
	uchar ch = 0;                      // current input byte
	std::stringstream tempout;
	input.read((char*)&ch, 1);

	if (input.eof()) {
		std::cerr << "End of the file right away!" << std::endl;
		return 0;
	}

	// Read the MIDI file header:

	// The first four bytes must be the characters "MThd"
	if (ch != 'M') { std::cerr << "Not a MIDI file M" << std::endl; return 0; }
	input.read((char*)&ch, 1);
	if (ch != 'T') { std::cerr << "Not a MIDI file T" << std::endl; return 0; }
	input.read((char*)&ch, 1);
	if (ch != 'h') { std::cerr << "Not a MIDI file h" << std::endl; return 0; }
	input.read((char*)&ch, 1);
	if (ch != 'd') { std::cerr << "Not a MIDI file d" << std::endl; return 0; }
	tempout << "\"MThd\"";
	if (m_commentsQ) {
		tempout << "\t\t\t; MIDI header chunk marker";
	}
	tempout << std::endl;

	// The next four bytes are a big-endian byte count for the header
	// which should nearly always be "6".
	int headersize = 0;
	input.read((char*)&ch, 1); headersize = (headersize << 8) | ch;
	input.read((char*)&ch, 1); headersize = (headersize << 8) | ch;
	input.read((char*)&ch, 1); headersize = (headersize << 8) | ch;
	input.read((char*)&ch, 1); headersize = (headersize << 8) | ch;
	tempout << "4'" << headersize;
	if (m_commentsQ) {
		tempout << "\t\t\t; bytes to follow in header chunk";
	}
	tempout << std::endl;

	// First number in header is two-byte file type.
	int filetype = 0;
	input.read((char*)&ch, 1);
	filetype = (filetype << 8) | ch;
	input.read((char*)&ch, 1);
	filetype = (filetype << 8) | ch;
	tempout << "2'" << filetype;
	if (m_commentsQ) {
		tempout << "\t\t\t; file format: Type-" << filetype << " (";
		switch (filetype) {
			case 0:  tempout << "single track"; break;
			case 1:  tempout << "multitrack";   break;
			case 2:  tempout << "multisegment"; break;
			default: tempout << "unknown";      break;
		}
		tempout << ")";
	}
	tempout << std::endl;

	// Second number in header is two-byte trackcount.
	int trackcount = 0;
	input.read((char*)&ch, 1);
	trackcount = (trackcount << 8) | ch;
	input.read((char*)&ch, 1);
	trackcount = (trackcount << 8) | ch;
	tempout << "2'" << trackcount;
	if (m_commentsQ) {
		tempout << "\t\t\t; number of tracks";
	}
	tempout << std::endl;

	// Third number is divisions.  This can be one of two types:
	// regular: top bit is 0: number of ticks per quarter note
	// SMPTE:   top bit is 1: first byte is negative frames, second is
	//          ticks per frame.
	uchar byte1 = 0;
	uchar byte2 = 0;
	input.read((char*)&byte1, 1);
	input.read((char*)&byte2, 1);
	if (byte1 & 0x80) {
		// SMPTE divisions
		tempout << "'-" << 0xff - (ulong)byte1 + 1;
		if (m_commentsQ) {
			tempout << "\t\t\t; SMPTE frames/second";
		}
		tempout << std::endl;
		tempout << "'" << std::dec << (long)byte2;
		if (m_commentsQ) {
			tempout << "\t\t\t; subframes per frame";
		}
		tempout << std::endl;
	} else {
		// regular divisions
		int divisions = 0;
		divisions = (divisions << 8) | byte1;
		divisions = (divisions << 8) | byte2;
		tempout << "2'" << divisions;
		if (m_commentsQ) {
			tempout << "\t\t\t; ticks per quarter note";
		}
		tempout << std::endl;
	}

	// Print any strange bytes in header:
	int i;
	for (i=0; i<headersize - 6; i++) {
		input.read((char*)&ch, 1);
		if (ch < 0x10) {
			tempout << '0';
		}
		tempout << std::hex << (int)ch;
	}
	if (headersize - 6 > 0) {
		tempout << "\t\t\t; unknown header bytes";
		tempout << std::endl;
	}

	for (i=0; i<trackcount; i++) {
		tempout << "\n;;; TRACK "
				  << i << " ----------------------------------" << std::endl;

		input.read((char*)&ch, 1);
		// The first four bytes of a track must be the characters "MTrk"
		if (ch != 'M') { std::cerr << "Not a MIDI file M2" << std::endl; return 0; }
		input.read((char*)&ch, 1);
		if (ch != 'T') { std::cerr << "Not a MIDI file T2" << std::endl; return 0; }
		input.read((char*)&ch, 1);
		if (ch != 'r') { std::cerr << "Not a MIDI file r" << std::endl; return 0; }
		input.read((char*)&ch, 1);
		if (ch != 'k') { std::cerr << "Not a MIDI file k" << std::endl; return 0; }
		tempout << "\"MTrk\"";
		if (m_commentsQ) {
			tempout << "\t\t\t; MIDI track chunk marker";
		}
		tempout << std::endl;

		// The next four bytes are a big-endian byte count for the track
		int tracksize = 0;
		input.read((char*)&ch, 1); tracksize = (tracksize << 8) | ch;
		input.read((char*)&ch, 1); tracksize = (tracksize << 8) | ch;
		input.read((char*)&ch, 1); tracksize = (tracksize << 8) | ch;
		input.read((char*)&ch, 1); tracksize = (tracksize << 8) | ch;
		tempout << "4'" << tracksize;
		if (m_commentsQ) {
			tempout << "\t\t\t; bytes to follow in track chunk";
		}
		tempout << std::endl;

		int trackbytes = 0;
		int command = 0;

		// process MIDI events until the end of the track
		while (readMidiEvent(tempout, input, trackbytes, command)) {
			tempout << "\n";
		};
		tempout << "\n";

		if (trackbytes != tracksize) {
			tempout << "; TRACK SIZE ERROR, ACTUAL SIZE: " << trackbytes << std::endl;
		}
	}

	// print #define definitions if requested.


	// print main content of MIDI file parsing:
	out << tempout.str();
	return 1;
}



//////////////////////////////
//
// Binasc::processDecimalWord -- interprets a decimal word into
//     constituent bytes
//

int Binasc::processDecimalWord(std::ostream& out, const std::string& word,
		int lineNum) {
	int length = (int)word.size();        // length of ascii binary number
	int byteCount = -1;              // number of bytes to output
	int quoteIndex = -1;             // index of decimal specifier
	int signIndex = -1;              // index of any sign for number
	int periodIndex = -1;            // index of period for floating point
	int endianIndex = -1;            // index of little endian specifier
	int i = 0;

	// make sure that all characters are valid
	for (i=0; i<length; i++) {
		switch (word[i]) {
			case '\'':
				if (quoteIndex != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "extra quote in decimal number" << std::endl;
					return 0;
				} else {
					quoteIndex = i;
				}
				break;
			case '-':
				if (signIndex != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "cannot have more than two minus signs in number"
						  << std::endl;
					return 0;
				} else {
					signIndex = i;
				}
				if (i == 0 || word[i-1] != '\'') {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "minus sign must immediately follow quote mark" << std::endl;
					return 0;
				}
				break;
			case '.':
				if (quoteIndex == -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "cannot have decimal marker before quote" << std::endl;
					return 0;
				}
				if (periodIndex != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "extra period in decimal number" << std::endl;
					return 0;
				} else {
					periodIndex = i;
				}
				break;
			case 'u':
			case 'U':
				if (quoteIndex != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "cannot have endian specified after quote" << std::endl;
					return 0;
				}
				if (endianIndex != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "extra \"u\" in decimal number" << std::endl;
					return 0;
				} else {
					endianIndex = i;
				}
				break;
			case '8':
			case '1': case '2': case '3': case '4':
				if (quoteIndex == -1 && byteCount != -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "invalid byte specificaton before quote in "
						  << "decimal number" << std::endl;
					return 0;
				} else if (quoteIndex == -1) {
					byteCount = word[i] - '0';
				}
				break;
			case '0': case '5': case '6': case '7': case '9':
				if (quoteIndex == -1) {
					std::cerr << "Error on line " << lineNum << " at token: " << word
						  << std::endl;
					std::cerr << "cannot have numbers before quote in decimal number"
						  << std::endl;
					return 0;
				}
				break;
			default:
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "Invalid character in decimal number"
						  " (character number " << i <<")" << std::endl;
				return 0;
		}
	}

	// there must be a quote character to indicate a decimal number
	// and there must be a decimal number after the quote
	if (quoteIndex == -1) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "there must be a quote to signify a decimal number" << std::endl;
		return 0;
	} else if (quoteIndex == length - 1) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "there must be a decimal number after the quote" << std::endl;
		return 0;
	}

	// 8 byte decimal output can only occur if reading a double number
	if (periodIndex == -1 && byteCount == 8) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "only floating-point numbers can use 8 bytes" << std::endl;
		return 0;
	}

	// default size for floating point numbers is 4 bytes
	if (periodIndex != -1) {
		if (byteCount == -1) {
			byteCount = 4;
		}
	}

	// process any floating point numbers possibilities
	if (periodIndex != -1) {
		double doubleOutput = atof(&word[quoteIndex+1]);
		float  floatOutput  = (float)doubleOutput;
		switch (byteCount) {
			case 4:
			  if (endianIndex == -1) {
				  writeBigEndianFloat(out, floatOutput);
			  } else {
				  writeLittleEndianFloat(out, floatOutput);
			  }
			  return 1;
			  break;
			case 8:
			  if (endianIndex == -1) {
				  writeBigEndianDouble(out, doubleOutput);
			  } else {
				  writeLittleEndianDouble(out, doubleOutput);
			  }
			  return 1;
			  break;
			default:
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "floating-point numbers can be only 4 or 8 bytes" << std::endl;
				return 0;
		}
	}

	// process any integer decimal number possibilities

	// default integer size is one byte, if size is not specified, then
	// the number must be in the one byte range and cannot overflow
	// the byte if the size of the decimal number is not specified
	if (byteCount == -1) {
		if (signIndex != -1) {
			long tempLong = atoi(&word[quoteIndex + 1]);
			if (tempLong > 127 || tempLong < -128) {
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "Decimal number out of range from -128 to 127" << std::endl;
				return 0;
			}
			char charOutput = (char)tempLong;
			out << charOutput;
			return 1;
		} else {
			ulong tempLong = (ulong)atoi(&word[quoteIndex + 1]);
			uchar ucharOutput = (uchar)tempLong;
			if (tempLong > 255) { // || (tempLong < 0)) {
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "Decimal number out of range from 0 to 255" << std::endl;
				return 0;
			}
			out << ucharOutput;
			return 1;
		}
	}

	// left with an integer number with a specified number of bytes
	switch (byteCount) {
		case 1:
			if (signIndex != -1) {
				long tempLong = atoi(&word[quoteIndex + 1]);
				char charOutput = (char)tempLong;
				out << charOutput;
				return 1;
			} else {
				ulong tempLong = (ulong)atoi(&word[quoteIndex + 1]);
				uchar ucharOutput = (uchar)tempLong;
				out << ucharOutput;
				return 1;
			}
			break;
		case 2:
			if (signIndex != -1) {
				long tempLong = atoi(&word[quoteIndex + 1]);
				short shortOutput = (short)tempLong;
				if (endianIndex == -1) {
					writeBigEndianShort(out, shortOutput);
				} else {
					writeLittleEndianShort(out, shortOutput);
				}
				return 1;
			} else {
				ulong tempLong = (ulong)atoi(&word[quoteIndex + 1]);
				ushort ushortOutput = (ushort)tempLong;
				if (endianIndex == -1) {
					writeBigEndianUShort(out, ushortOutput);
				} else {
					writeLittleEndianUShort(out, ushortOutput);
				}
				return 1;
			}
			break;
		case 3:
			{
			if (signIndex != -1) {
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "negative decimal numbers cannot be stored in 3 bytes"
					  << std::endl;
				return 0;
			}
			ulong tempLong = (ulong)atoi(&word[quoteIndex + 1]);
			uchar byte1 = (uchar)((tempLong & 0x00ff0000) >> 16);
			uchar byte2 = (uchar)((tempLong & 0x0000ff00) >> 8);
			uchar byte3 = (uchar)((tempLong & 0x000000ff));
			if (endianIndex == -1) {
				out << byte1;
				out << byte2;
				out << byte3;
			} else {
				out << byte3;
				out << byte2;
				out << byte1;
			}
			return 1;
			}
			break;
		case 4:
			if (signIndex != -1) {
				long tempLong = atoi(&word[quoteIndex + 1]);
				if (endianIndex == -1) {
					writeBigEndianLong(out, tempLong);
				} else {
					writeLittleEndianLong(out, tempLong);
				}
				return 1;
			} else {
				ulong tempuLong = (ulong)atoi(&word[quoteIndex + 1]);
				if (endianIndex == -1) {
					writeBigEndianULong(out, tempuLong);
				} else {
					writeLittleEndianULong(out, tempuLong);
				}
				return 1;
			}
			break;
		default:
			std::cerr << "Error on line " << lineNum << " at token: " << word
				  << std::endl;
			std::cerr << "invalid byte count specification for decimal number" << std::endl;
			return 0;
	}
}



//////////////////////////////
//
// Binasc::processHexWord -- interprets a hexadecimal word and converts into
//     its binary byte form.
//

int Binasc::processHexWord(std::ostream& out, const std::string& word,
		int lineNum) {
	int length = (int)word.size();
	uchar outputByte;

	if (length > 2) {
		std::cerr << "Error on line " << lineNum << " at token: " << word << std::endl;
		std::cerr << "Size of hexadecimal number is too large.  Max is ff." << std::endl;
		return 0;
	}

	if (!isxdigit(word[0]) || (length == 2 && !isxdigit(word[1]))) {
		std::cerr << "Error on line " << lineNum << " at token: " << word << std::endl;
		std::cerr << "Invalid character in hexadecimal number." << std::endl;
		return 0;
	}

	outputByte = (uchar)strtol(word.c_str(), (char**)NULL, 16);
	out << outputByte;
	return 1;
}



//////////////////////////////
//
// Binasc::processStringWord -- interprets a binary word into
//     its constituent byte
//

int Binasc::processStringWord(std::ostream& out, const std::string& word,
		int /* lineNum */) {
	out << word;
	return 1;
}



//////////////////////////////
//
// Binasc::processAsciiWord -- interprets a binary word into
//     its constituent byte
//

int Binasc::processAsciiWord(std::ostream& out, const std::string& word,
		int lineNum) {
	int length = (int)word.size();
	uchar outputByte;

	if (word[0] != '+') {
		std::cerr << "Error on line " << lineNum << " at token: " << word << std::endl;
		std::cerr << "character byte must start with \'+\' sign: " << std::endl;
		return 0;
	}

	if (length > 2) {
		std::cerr << "Error on line " << lineNum << " at token: " << word << std::endl;
		std::cerr << "character byte word is too long -- specify only one character"
			  << std::endl;
		return 0;
	}

	if (length == 2) {
		outputByte = (uchar)word[1];
	} else {
		outputByte = ' ';
	}
	out << outputByte;
	return 1;
}



//////////////////////////////
//
// Binasc::processBinaryWord -- interprets a binary word into
//     its constituent byte
//

int Binasc::processBinaryWord(std::ostream& out, const std::string& word,
		int lineNum) {
	int length = (int)word.size();        // length of ascii binary number
	int commaIndex = -1;             // index location of comma in number
	int leftDigits = -1;             // number of digits to left of comma
	int rightDigits = -1;            // number of digits to right of comma
	int i = 0;

	// make sure that all characters are valid
	for (i=0; i<length; i++) {
		if (word [i] == ',') {
			if (commaIndex != -1) {
				std::cerr << "Error on line " << lineNum << " at token: " << word
					  << std::endl;
				std::cerr << "extra comma in binary number" << std::endl;
				return 0;
			} else {
				commaIndex = i;
			}
		} else if (!(word[i] == '1' || word[i] == '0')) {
			std::cerr << "Error on line " << lineNum << " at token: " << word
				  << std::endl;
			std::cerr << "Invalid character in binary number"
					  " (character is " << word[i] <<")" << std::endl;
			return 0;
		}
	}

	// comma cannot start or end number
	if (commaIndex == 0) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "cannot start binary number with a comma" << std::endl;
		return 0;
	} else if (commaIndex == length - 1 ) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "cannot end binary number with a comma" << std::endl;
		return 0;
	}

	// figure out how many digits there are in binary number
	// number must be able to fit into one byte.
	if (commaIndex != -1) {
		leftDigits = commaIndex;
		rightDigits = length - commaIndex - 1;
	} else if (length > 8) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "too many digits in binary number" << std::endl;
		return 0;
	}
	// if there is a comma, then there cannot be more than 4 digits on a side
	if (leftDigits > 4) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "too many digits to left of comma" << std::endl;
		return 0;
	}
	if (rightDigits > 4) {
		std::cerr << "Error on line " << lineNum << " at token: " << word
			  << std::endl;
		std::cerr << "too many digits to right of comma" << std::endl;
		return 0;
	}

	// OK, we have a valid binary number, so calculate the byte

	uchar output = 0;

	// if no comma in binary number
	if (commaIndex == -1) {
		for (i=0; i<length; i++) {
			output = output << 1;
			output |= word[i] - '0';
		}
	}
	// if comma in binary number
	else {
		for (i=0; i<leftDigits; i++) {
			output = output << 1;
			output |= word[i] - '0';
		}
		output = output << (4-rightDigits);
		for (i=0+commaIndex+1; i<rightDigits+commaIndex+1; i++) {
			output = output << 1;
			output |= word[i] - '0';
		}
	}

	// send the byte to the output
	out << output;
	return 1;
}



//////////////////////////////
//
// Binasc::processVlvWord -- print a number in Variable Length Value form.
//   The int is split into 7-bit groupings, the MSB's that are zero
//   are dropped.  A continuation bit is added as the MSbit to each
//   7-bit grouping.  The continuation bit is "1" if there is another
//   byte in the VLV; "0" for the last byte.  VLVs are always
//   big-endian.  The input word starts with the character "v" followed
//   without space by an integer.
//

int Binasc::processVlvWord(std::ostream& out, const std::string& word,
		int lineNum) {
	if (word.size() < 2) {
		std::cerr << "Error on line: " << lineNum
			  << ": 'v' needs to be followed immediately by a decimal digit"
			  << std::endl;
		return 0;
	}
	if (!isdigit(word[1])) {
		std::cerr << "Error on line: " << lineNum
			  << ": 'v' needs to be followed immediately by a decimal digit"
			  << std::endl;
		return 0;
	}
	ulong value = atoi(&word[1]);

	uchar byte[5];
	byte[0] = (value >> 28) & 0x7f;
	byte[1] = (value >> 21) & 0x7f;
	byte[2] = (value >> 14) & 0x7f;
	byte[3] = (value >>  7) & 0x7f;
	byte[4] = (value >>  0) & 0x7f;

	int i;
	int flag = 0;
	for (i=0; i<4; i++) {
		if (byte[i] != 0) {
			flag = 1;
		}
		if (flag) {
			byte[i] |= 0x80;
		}
	}

	for (i=0; i<5; i++) {
		if (byte[i] >= 0x80 || i == 4) {
			out << byte[i];
		}
	}

	return 1;
}



////////////////////////////
//
// Binasc::processMidiTempoWord -- convert a floating point tempo into
//   a three-byte number of microseconds per beat per minute value.
//

int Binasc::processMidiTempoWord(std::ostream& out, const std::string& word,
		int lineNum) {
	if (word.size() < 2) {
		std::cerr << "Error on line: " << lineNum
			  << ": 't' needs to be followed immediately by "
			  << "a floating-point number" << std::endl;
		return 0;
	}
	if (!(isdigit(word[1]) || word[1] == '.' || word[1] == '-'
			|| word[1] == '+')) {
		std::cerr << "Error on line: " << lineNum
			  << ": 't' needs to be followed immediately by "
			  << "a floating-point number" << std::endl;
		return 0;
	}
	double value = strtod(&word[1], NULL);

	if (value < 0.0) {
		value = -value;
	}

	int intval = int(60.0 * 1000000.0 / value + 0.5);

	uchar byte0 = intval & 0xff;
	uchar byte1 = (intval >>  8) & 0xff;
	uchar byte2 = (intval >> 16) & 0xff;
	out << byte2 << byte1 << byte0;
	return 1;
}



////////////////////////////
//
// Binasc::processMidiPitchBendWord -- convert a floating point number in
//   the range from +1.0 to -1.0 into a 14-point integer with -1.0 mapping
//   to 0 and +1.0 mapping to 2^15-1.  This integer will be packed into
//   two bytes, with the LSB coming first and containing the bottom
//   7-bits of the 14-bit value, then the MSB coming second and containing
//   the top 7-bits of the 14-bit value.

int Binasc::processMidiPitchBendWord(std::ostream& out, const std::string& word,
		int lineNum) {
	if (word.size() < 2) {
		std::cerr << "Error on line: " << lineNum
			  << ": 'p' needs to be followed immediately by "
			  << "a floating-point number" << std::endl;
		return 0;
	}
	if (!(isdigit(word[1]) || word[1] == '.' || word[1] == '-'
			|| word[1] == '+')) {
		std::cerr << "Error on line: " << lineNum
			  << ": 'p' needs to be followed immediately by "
			  << "a floating-point number" << std::endl;
		return 0;
	}
	double value = strtod(&word[1], NULL);

	if (value > 1.0) {
		value = 1.0;
	}
	if (value < -1.0) {
		value = -1.0;
	}

	int intval = (int)(((1 << 13)-0.5)  * (value + 1.0) + 0.5);
	uchar LSB = intval & 0x7f;
	uchar MSB = (intval >>  7) & 0x7f;
	out << LSB << MSB;
	return 1;
}



///////////////////////////////////////////////////////////////////////////
//
// Ordered byte writing functions --
//

//////////////////////////////
//
// Binasc::writeLittleEndianUShort --
//

std::ostream& Binasc::writeLittleEndianUShort(std::ostream& out, ushort value) {
	union { char bytes[2]; ushort us; } data;
	data.us = value;
	out << data.bytes[0];
	out << data.bytes[1];
	return out;
}



//////////////////////////////
//
// Binasc::writeBigEndianUShort --
//

std::ostream& Binasc::writeBigEndianUShort(std::ostream& out, ushort value) {
	union { char bytes[2]; ushort us; } data;
	data.us = value;
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// Binasc::writeLittleEndianShort --
//

std::ostream& Binasc::writeLittleEndianShort(std::ostream& out, short value) {
	union { char bytes[2]; short s; } data;
	data.s = value;
	out << data.bytes[0];
	out << data.bytes[1];
	return out;
}



//////////////////////////////
//
// writeBigEndianShort --
//

std::ostream& Binasc::writeBigEndianShort(std::ostream& out, short value) {
	union { char bytes[2]; short s; } data;
	data.s = value;
	out << data.bytes[1];
	out << data.bytes[0];
	return out;
}



//////////////////////////////
//
// Binasc::writeLittleEndianULong --
//

std::ostream& Binasc::writeLittleEndianULong(std::ostream& out, ulong value) {
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
// Binasc::writeBigEndianULong --
//

std::ostream& Binasc::writeBigEndianULong(std::ostream& out, ulong value) {
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
// Binasc::writeLittleEndianLong --
//

std::ostream& Binasc::writeLittleEndianLong(std::ostream& out, long value) {
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
// Binasc::writeBigEndianLong --
//

std::ostream& Binasc::writeBigEndianLong(std::ostream& out, long value) {
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
// Binasc::writeBigEndianFloat --
//

std::ostream& Binasc::writeBigEndianFloat(std::ostream& out, float value) {
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
// Binasc::writeLittleEndianFloat --
//

std::ostream& Binasc::writeLittleEndianFloat(std::ostream& out, float value) {
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
// Binasc::writeBigEndianDouble --
//

std::ostream& Binasc::writeBigEndianDouble(std::ostream& out, double value) {
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
// Binasc::writeLittleEndianDouble --
//

std::ostream& Binasc::writeLittleEndianDouble(std::ostream& out, double value) {
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


} // end namespace smf



