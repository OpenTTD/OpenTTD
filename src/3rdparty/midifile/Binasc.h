//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Mon Feb 16 12:26:32 PST 2015 Adapted from binasc program.
// Last Modified: Sat Apr 21 10:52:19 PDT 2018 Removed using namespace std;
// Filename:      midifile/include/Binasc.h
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// description:   Interface to convert bytes between binary and ASCII forms.
//

#ifndef _BINASC_H_INCLUDED
#define _BINASC_H_INCLUDED

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>


namespace smf {

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;

class Binasc {

	public:
		                     Binasc                  (void);

		                    ~Binasc                  ();

		// functions for setting options:
		int                  setLineLength           (int length);
		int                  getLineLength           (void);
		int                  setLineBytes            (int length);
		int                  getLineBytes            (void);
		void                 setComments             (int state);
		void                 setCommentsOn           (void);
		void                 setCommentsOff          (void);
		int                  getComments             (void);
		void                 setBytes                (int state);
		void                 setBytesOn              (void);
		void                 setBytesOff             (void);
		int                  getBytes                (void);
		void                 setMidi                 (int state);
		void                 setMidiOn               (void);
		void                 setMidiOff              (void);
		int                  getMidi                 (void);

		// functions for converting into a binary file:
		int                  writeToBinary           (const std::string& outfile,
		                                              const std::string& infile);
		int                  writeToBinary           (const std::string& outfile,
		                                              std::istream& input);
		int                  writeToBinary           (std::ostream& out,
		                                              const std::string& infile);
		int                  writeToBinary           (std::ostream& out,
		                                              std::istream& input);

		// functions for converting into an ASCII file with hex bytes:
		int                  readFromBinary          (const std::string&
		                                              outfile,
		                                              const std::string& infile);
		int                  readFromBinary          (const std::string& outfile,
		                                              std::istream& input);
		int                  readFromBinary          (std::ostream& out,
		                                              const std::string& infile);
		int                  readFromBinary          (std::ostream& out,
		                                              std::istream& input);

		// static functions for writing ordered bytes:
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

		static std::string   keyToPitchName          (int key);

	protected:
		int m_bytesQ;       // option for printing hex bytes in ASCII output.
		int m_commentsQ;    // option for printing comments in ASCII output.
		int m_midiQ;        // output ASCII data as parsed MIDI file.
		int m_maxLineLength;// number of character in ASCII output on a line.
		int m_maxLineBytes; // number of hex bytes in ASCII output on a line.

	private:
		// helper functions for reading ASCII content to conver to binary:
		int                  processLine             (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processAsciiWord        (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processStringWord       (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processBinaryWord       (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processDecimalWord      (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processHexWord          (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processVlvWord          (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processMidiPitchBendWord(std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);
		int                  processMidiTempoWord    (std::ostream& out,
		                                              const std::string& input,
		                                              int lineNum);

		// helper functions for reading binary content to convert to ASCII:
		int  outputStyleAscii   (std::ostream& out, std::istream& input);
		int  outputStyleBinary  (std::ostream& out, std::istream& input);
		int  outputStyleBoth    (std::ostream& out, std::istream& input);
		int  outputStyleMidi    (std::ostream& out, std::istream& input);

		// MIDI parsing helper functions:
		int  readMidiEvent  (std::ostream& out, std::istream& infile,
		                     int& trackbytes, int& command);
		int  getVLV         (std::istream& infile, int& trackbytes);
		int  getWord        (std::string& word, const std::string& input,
		                     const std::string& terminators, int index);

		static const char *GMinstrument[128];

};

} // end of namespace smf

#endif /* _BINASC_H_INCLUDED */



