//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat Feb 14 20:49:21 PST 2015
// Last Modified: Sun Apr 15 11:11:05 PDT 2018 Added event removal system.
// Filename:      midifile/src/MidiMessage.cpp
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   Storage for bytes of a MIDI message for Standard
//                MIDI Files.
//

#include "MidiMessage.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>


namespace smf {

//////////////////////////////
//
// MidiMessage::MidiMessage -- Constructor.
//

MidiMessage::MidiMessage(void) : vector<uchar>() {
	// do nothing
}


MidiMessage::MidiMessage(int command) : vector<uchar>(1, (uchar)command) {
	// do nothing
}


MidiMessage::MidiMessage(int command, int p1) : vector<uchar>(2) {
	(*this)[0] = (uchar)command;
	(*this)[1] = (uchar)p1;
}


MidiMessage::MidiMessage(int command, int p1, int p2) : vector<uchar>(3) {
	(*this)[0] = (uchar)command;
	(*this)[1] = (uchar)p1;
	(*this)[2] = (uchar)p2;
}


MidiMessage::MidiMessage(const MidiMessage& message) : vector<uchar>() {
	(*this) = message;
}


MidiMessage::MidiMessage(const std::vector<uchar>& message) : vector<uchar>() {
	setMessage(message);
}


MidiMessage::MidiMessage(const std::vector<char>& message) : vector<uchar>() {
	setMessage(message);
}


MidiMessage::MidiMessage(const std::vector<int>& message) : vector<uchar>() {
	setMessage(message);
}



//////////////////////////////
//
// MidiMessage::~MidiMessage -- Deconstructor.
//

MidiMessage::~MidiMessage() {
	resize(0);
}



//////////////////////////////
//
// MidiMessage::operator= --
//

MidiMessage& MidiMessage::operator=(const MidiMessage& message) {
	if (this == &message) {
		return *this;
	}
	std::vector<uchar>::operator=(static_cast<const std::vector<uchar> &>(message));
	return *this;
}


MidiMessage& MidiMessage::operator=(const std::vector<uchar>& bytes) {
	if (this == &bytes) {
		return *this;
	}
	setMessage(bytes);
	return *this;
}


MidiMessage& MidiMessage::operator=(const std::vector<char>& bytes) {
	setMessage(bytes);
	return *this;
}


MidiMessage& MidiMessage::operator=(const std::vector<int>& bytes) {
	setMessage(bytes);
	return *this;
}



//////////////////////////////
//
// MidiMessage::setSize -- Change the size of the message byte list.
//     If the size is increased, then the new bytes are not initialized
//     to any specific values.
//

void MidiMessage::setSize(int asize) {
	this->resize(asize);
}



//////////////////////////////
//
// MidiMessage::getSize -- Return the size of the MIDI message bytes.
//

int MidiMessage::getSize(void) const {
	return (int)this->size();
}



//////////////////////////////
//
// MidiMessage::setSizeToCommand -- Set the number of parameters if the
//   command byte is set in the range from 0x80 to 0xef.  Any newly
//   added parameter bytes will be set to 0. Commands in the range
//   of 0xF) should not use this function, and they will ignore
//   modification by this command.
//

int MidiMessage::setSizeToCommand(void) {
	int osize = (int)this->size();
	if (osize < 1) {
		return 0;
	}
	int command = getCommandNibble();
	if (command < 0) {
		return 0;
	}
	int bytecount = 1;
	switch (command) {
		case 0x80: bytecount = 2; break;  // Note Off
		case 0x90: bytecount = 2; break;  // Note On
		case 0xA0: bytecount = 2; break;  // Aftertouch
		case 0xB0: bytecount = 2; break;  // Continuous Controller
		case 0xC0: bytecount = 1; break;  // Patch Change
		case 0xD0: bytecount = 1; break;  // Channel Pressure
		case 0xE0: bytecount = 2; break;  // Pitch Bend
		case 0xF0:
		default:
			return (int)size();
	}
	if (bytecount + 1 < osize) {
		resize(bytecount+1);
		for (int i=osize; i<bytecount+1; i++) {
			(*this)[i] = 0;
		}
	}

	return (int)size();
}


int MidiMessage::resizeToCommand(void) {
	return setSizeToCommand();
}



//////////////////////////////
//
// MidiMessage::getTempoMicro -- Returns the number of microseconds per
//      quarter note if the MidiMessage is a tempo meta message.
//      Returns -1 if the MIDI message is not a tempo meta message.
//

int MidiMessage::getTempoMicro(void) const {
	if (!isTempo()) {
		return -1;
	} else {
		return ((*this)[3] << 16) + ((*this)[4] << 8) + (*this)[5];
	}
}


int MidiMessage::getTempoMicroseconds(void) const {
	return getTempoMicro();
}



//////////////////////////////
//
// MidiMessage::getTempoSeconds -- Returns the number of seconds per
//      quarter note.  Returns -1.0 if the MIDI message is not a
//      tempo meta message.
//

double MidiMessage::getTempoSeconds(void) const {
	int microseconds = getTempoMicroseconds();
	if (microseconds < 0) {
		return -1.0;
	} else {
		return (double)microseconds / 1000000.0;
	}
}



//////////////////////////////
//
// MidiMessage::getTempoBPM -- Returns the tempo in terms of beats per minute.
//   Returns -1 if the MidiMessage is note a tempo meta message.
//

double MidiMessage::getTempoBPM(void) const {
	int microseconds = getTempoMicroseconds();
	if (microseconds < 0) {
		return -1.0;
	}
	return 60000000.0 / (double)microseconds;
}



//////////////////////////////
//
// MidiMessage::getTempoTPS -- Returns the tempo in terms of ticks per seconds.
//

double MidiMessage::getTempoTPS(int tpq) const {
	int microseconds = getTempoMicroseconds();
	if (microseconds < 0) {
		return -1.0;
	} else {
		return tpq * 1000000.0 / (double)microseconds;
	}
}



//////////////////////////////
//
// MidiMessage::getTempoSPT -- Returns the tempo in terms of seconds per tick.
//

double MidiMessage::getTempoSPT(int tpq) const {
	int microseconds = getTempoMicroseconds();
	if (microseconds < 0) {
		return -1.0;
	} else {
		return (double)microseconds / 1000000.0 / tpq;
	}
}



//////////////////////////////
//
// MidiMessage::isMeta -- Returns true if message is a Meta message
//      (when the command byte is 0xff).
//

bool MidiMessage::isMeta(void) const {
	if (size() == 0) {
		return false;
	} else if ((*this)[0] != 0xff) {
		return false;
	} else if (size() < 3) {
		// meta message is ill-formed.
		// meta messages must have at least three bytes:
		//    0: 0xff == meta message marker
		//    1: meta message type
		//    2: meta message data bytes to follow
		return false;
	} else {
		return true;
	}
}


bool MidiMessage::isMetaMessage(void) const {
	return isMeta();
}



//////////////////////////////
//
// MidiMessage::isNoteOff -- Returns true if the command nibble is 0x80
//     or if the command nibble is 0x90 with p2=0 velocity.
//

bool MidiMessage::isNoteOff(void) const {
	const MidiMessage& message = *this;
	const vector<uchar>& chars = message;
	if (message.size() != 3) {
		return false;
	} else if ((chars[0] & 0xf0) == 0x80) {
		return true;
	} else if (((chars[0] & 0xf0) == 0x90) && (chars[2] == 0x00)) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isNoteOn -- Returns true if the command byte is in the 0x90
//    range and the velocity is non-zero
//

bool MidiMessage::isNoteOn(void) const {
	if (size() != 3) {
		return false;
	} else if (((*this)[0] & 0xf0) != 0x90) {
		return false;
	} else if ((*this)[2] == 0) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isNote -- Returns true if either a note-on or a note-off
//     message.
//

bool MidiMessage::isNote(void) const {
	return isNoteOn() || isNoteOff();
}



//////////////////////////////
//
// MidiMessage::isAftertouch -- Returns true if the command byte is in the 0xA0
//    range.
//

bool MidiMessage::isAftertouch(void) const {
	if (size() != 3) {
		return false;
	} else if (((*this)[0] & 0xf0) != 0xA0) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isController -- Returns true if the command byte is in the 0xB0
//    range and there are two additional data bytes.
//

bool MidiMessage::isController(void) const {
	if (size() != 3) {
		return false;
	} else if (((*this)[0] & 0xf0) != 0xB0) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isSustain -- Returns true if the MidiMessages is a sustain pedal
//    control event.  Controller 64 is the sustain pedal for general MIDI.
//

bool MidiMessage::isSustain(void) const {
	if (!isController()) {
		return false;
	}
	if (getP1() == 64) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isSustainOn -- Returns true if a sustain-pedal-on control message.
//     Sustain-on is a value in the range from 64-127 for controller 64.
//

bool MidiMessage::isSustainOn(void) const {
	if (!isSustain()) {
		return false;
	}
	if (getP2() >= 64) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isSustainOff -- Returns true if a sustain-pedal-off control message.
//     Sustain-off is a value in the range from 0-63 for controller 64.
//

bool MidiMessage::isSustainOff(void) const {
	if (!isSustain()) {
		return false;
	}
	if (getP2() < 64) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isSoft -- Returns true if the MidiMessages is a soft pedal
//    control event.  Controller 67 is the sustain pedal for general MIDI.
//

bool MidiMessage::isSoft(void) const {
	if (!isController()) {
		return false;
	}
	if (getP1() == 67) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isSoftOn -- Returns true if a sustain-pedal-on control message.
//     Soft-on is a value in the range from 64-127 for controller 67.
//

bool MidiMessage::isSoftOn(void) const {
	if (!isSoft()) {
		return false;
	}
	if (getP2() >= 64) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isSoftOff -- Returns true if a sustain-pedal-off control message.
//     Soft-off is a value in the range from 0-63 for controller 67.
//

bool MidiMessage::isSoftOff(void) const {
	if (!isSoft()) {
		return false;
	}
	if (getP2() < 64) {
		return true;
	} else {
		return false;
	}
}



//////////////////////////////
//
// MidiMessage::isTimbre -- Returns true of a patch change message
//    (command nibble 0xc0).
//

bool MidiMessage::isTimbre(void) const {
	if (((*this)[0] & 0xf0) != 0xc0) {
		return false;
	} else if (size() != 2) {
		return false;
	} else {
		return true;
	}
}


bool MidiMessage::isPatchChange(void) const {
	return isTimbre();
}



//////////////////////////////
//
// MidiMessage::isPressure -- Returns true of a channel pressure message
//    (command nibble 0xd0).
//

bool MidiMessage::isPressure(void) const {
	if (((*this)[0] & 0xf0) != 0xd0) {
		return false;
	} else if (size() != 2) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isPitchbend -- Returns true of a pitch-bend message
//    (command nibble 0xe0).
//

bool MidiMessage::isPitchbend(void) const {
	if (((*this)[0] & 0xf0) != 0xe0) {
		return false;
	} else if (size() != 3) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isEmpty -- Returns true if size of data array is zero.
//

bool MidiMessage::isEmpty(void) const {
	return empty();
}



///////////////////////////////
//
// MidiMessage::getMetaType -- returns the meta-message type for the
//     MidiMessage.  If the message is not a meta message, then returns
//     -1.
//

int MidiMessage::getMetaType(void) const {
	if (!isMetaMessage()) {
		return -1;
	} else {
		return (int)(*this)[1];
	}
}



//////////////////////////////
//
// MidiMessage::isText -- Returns true if message is a meta
//      message describing some text (meta message type 0x01).
//

bool MidiMessage::isText(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x01) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isCopyright -- Returns true if message is a meta
//      message describing a copyright notice (meta message type 0x02).
//      Copyright messages should be at absolute tick position 0
//      (and be the first event in the track chunk as well), but this
//      function does not check for those requirements.
//

bool MidiMessage::isCopyright(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x02) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isTrackName -- Returns true if message is a meta
//      message describing a track name (meta message type 0x03).
//

bool MidiMessage::isTrackName(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x03) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isInstrumentName -- Returns true if message is a
//      meta message describing an instrument name (for the track)
//      (meta message type 0x04).
//

bool MidiMessage::isInstrumentName(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x04) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isLyricText -- Returns true if message is a meta message
//      describing some lyric text (for karaoke MIDI files)
//      (meta message type 0x05).
//

bool MidiMessage::isLyricText(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x05) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isMarkerText -- Returns true if message is a meta message
//      describing a marker text (meta message type 0x06).
//

bool MidiMessage::isMarkerText(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x06) {
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isTempo -- Returns true if message is a meta message
//      describing tempo (meta message type 0x51).
//

bool MidiMessage::isTempo(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x51) {
		return false;
	} else if (size() != 6) {
		// Meta tempo message can only be 6 bytes long.
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isTimeSignature -- Returns true if message is
//      a meta message describing a time signature (meta message
//      type 0x58).
//

bool MidiMessage::isTimeSignature(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x58) {
		return false;
	} else if (size() != 7) {
		// Meta time signature message can only be 7 bytes long:
		// FF 58 <size> <top> <bot-log-2> <clocks-per-beat> <32nds>
		return false;
	} else {
		return true;
	}
}



//////////////////////////////
//
// MidiMessage::isKeySignature -- Returns true if message is
//      a meta message describing a key signature (meta message
//      type 0x59).
//

bool MidiMessage::isKeySignature(void) const {
	if (!isMetaMessage()) {
		return false;
	} else if ((*this)[1] != 0x59) {
		return false;
	} else if (size() != 5) {
		// Meta key signature message can only be 5 bytes long:
		// FF 59 <size> <accid> <mode>
		return false;
	} else {
		return true;
	}
}


//////////////////////////////
//
// MidiMessage::isEndOfTrack -- Returns true if message is a meta message
//      for end-of-track (meta message type 0x2f).
//

bool MidiMessage::isEndOfTrack(void) const {
	return getMetaType() == 0x2f ? 1 : 0;
}



//////////////////////////////
//
// MidiMessage::getP0 -- Return index 1 byte, or -1 if it doesn't exist.
//

int MidiMessage::getP0(void) const {
	return size() < 1 ? -1 : (*this)[0];
}



//////////////////////////////
//
// MidiMessage::getP1 -- Return index 1 byte, or -1 if it doesn't exist.
//

int MidiMessage::getP1(void) const {
	return size() < 2 ? -1 : (*this)[1];
}



//////////////////////////////
//
// MidiMessage::getP2 -- Return index 2 byte, or -1 if it doesn't exist.
//

int MidiMessage::getP2(void) const {
	return size() < 3 ? -1 : (*this)[2];
}



//////////////////////////////
//
// MidiMessage::getP3 -- Return index 3 byte, or -1 if it doesn't exist.
//

int MidiMessage::getP3(void) const {
	return size() < 4 ? -1 : (*this)[3];
}



//////////////////////////////
//
// MidiMessage::getKeyNumber -- Return the key number (such as 60 for
//    middle C).  If the message does not have a note parameter, then
//    return -1;  if the key is invalid (above 127 in value), then
//    limit to the range 0 to 127.
//

int MidiMessage::getKeyNumber(void) const {
	if (isNote() || isAftertouch()) {
		int output = getP1();
		if (output < 0) {
			return output;
		} else {
			return 0xff & output;
		}
	} else {
		return -1;
	}
}



//////////////////////////////
//
// MidiMessage::getVelocity -- Return the key velocity.  If the message
//   is not a note-on or a note-off, then return -1.  If the value is
//   out of the range 0-127, then chop off the high-bits.
//

int MidiMessage::getVelocity(void) const {
	if (isNote()) {
		int output = getP2();
		if (output < 0) {
			return output;
		} else {
			return 0xff & output;
		}
	} else {
		return -1;
	}
}



//////////////////////////////
//
// MidiMessage::getControllerNumber -- Return the controller number (such as 1
//   for modulation wheel).  If the message does not have a controller number
//   parameter, then return -1.  If the controller number is invalid (above 127
//   in value), then limit the range to to 0-127.
//

int MidiMessage::getControllerNumber(void) const {
	if (isController()) {
		int output = getP1();
		if (output < 0) {
			// -1 means no P1, although isController() is false in such a case.
			return output;
		} else {
			return 0x7f & output;
		}
	} else {
		return -1;
	}
}



//////////////////////////////
//
// MidiMessage::getControllerValue -- Return the controller value.  If the
//   message is not a control change message, then return -1.  If the value is
//   out of the range 0-127, then chop off the high-bits.
//

int MidiMessage::getControllerValue(void) const {
	if (isController()) {
		int output = getP2();
		if (output < 0) {
			// -1 means no P2, although isController() is false in such a case.
			return output;
		} else {
			return 0x7f & output;
		}
	} else {
		return -1;
	}
}



//////////////////////////////
//
// MidiMessage::setP0 -- Set the command byte.
//   If the MidiMessage is too short, add extra spaces to
//   allow for P0.  The value should be in the range from
//   128 to 255, but this function will not babysit you.
//

void MidiMessage::setP0(int value) {
	if (getSize() < 1) {
		resize(1);
	}
	(*this)[0] = static_cast<uchar>(value);
}



//////////////////////////////
//
// MidiMessage::setP1 -- Set the first parameter value.
//   If the MidiMessage is too short, add extra spaces to
//   allow for P1.  The command byte will be undefined if
//   it was added.  The value should be in the range from
//   0 to 127, but this function will not babysit you.
//

void MidiMessage::setP1(int value) {
	if (getSize() < 2) {
		resize(2);
	}
	(*this)[1] = static_cast<uchar>(value);
}



//////////////////////////////
//
// MidiMessage::setP2 -- Set the second paramater value.
//     If the MidiMessage is too short, add extra spaces
//     to allow for P2.  The command byte and/or the P1 value
//     will be undefined if extra space needs to be added and
//     those slots are created.  The value should be in the range
//     from 0 to 127, but this function will not babysit you.
//

void MidiMessage::setP2(int value) {
	if (getSize() < 3) {
		resize(3);
	}
	(*this)[2] = static_cast<uchar>(value);
}



//////////////////////////////
//
// MidiMessage::setP3 -- Set the third paramater value.
//     If the MidiMessage is too short, add extra spaces
//     to allow for P3.  The command byte and/or the P1/P2 values
//     will be undefined if extra space needs to be added and
//     those slots are created.  The value should be in the range
//     from 0 to 127, but this function will not babysit you.
//

void MidiMessage::setP3(int value) {
	if (getSize() < 4) {
		resize(4);
	}
	(*this)[3] = static_cast<uchar>(value);
}



//////////////////////////////
//
// MidiMessage::setKeyNumber -- Set the note on/off key number (or
//    aftertouch key).  Ignore if not note or aftertouch message.
//    Limits the input value to the range from 0 to 127.
//

void MidiMessage::setKeyNumber(int value) {
	if (isNote() || isAftertouch()) {
		setP1(value & 0xff);
	} else {
		// don't do anything since this is not a note-related message.
	}
}



//////////////////////////////
//
// MidiMessage::setVelocity -- Set the note on/off velocity; ignore
//   if note a note message.  Limits the input value to the range
//   from 0 to 127.
//

void MidiMessage::setVelocity(int value) {
	if (isNote()) {
		setP2(value & 0xff);
	} else {
		// don't do anything since this is not a note-related message.
	}
}



//////////////////////////////
//
// MidiMessage::getCommandNibble -- Returns the top 4 bits of the (*this)[0]
//    entry, or -1 if there is not (*this)[0].
//

int MidiMessage::getCommandNibble(void) const {
	if (size() < 1) {
		return -1;
	} else {
		return (*this)[0] & 0xf0;
	}
}



//////////////////////////////
//
// MidiMessage::getCommandByte -- Return the command byte or -1 if not
//    allocated.
//

int MidiMessage::getCommandByte(void) const {
	if (size() < 1) {
		return -1;
	} else {
		return (*this)[0];
	}
}



//////////////////////////////
//
// MidiMessage::getChannelNibble -- Returns the bottom 4 bites of the
//      (*this)[0] entry, or -1 if there is not (*this)[0].  Should be refined
//      to return -1 if the top nibble is 0xf0, since those commands are
//      not channel specific.
//

int MidiMessage::getChannelNibble(void) const {
	if (size() < 1) {
		return -1;
	} else {
		return (*this)[0] & 0x0f;
	}
}


int MidiMessage::getChannel(void) const {
	return getChannelNibble();
}



//////////////////////////////
//
// MidiMessage::setCommandByte --
//

void MidiMessage::setCommandByte(int value) {
	if (size() < 1) {
		resize(1);
	} else {
		(*this)[0] = (uchar)(value & 0xff);
	}
}

void MidiMessage::setCommand(int value) {
	setCommandByte(value);
}



//////////////////////////////
//
// MidiMessage::setCommand -- Set the command byte and parameter bytes
//   for a MidiMessage.  The size of the message will be adjusted to
//   the number of input parameters.
//

void MidiMessage::setCommand(int value, int p1) {
	this->resize(2);
	(*this)[0] = (uchar)value;
	(*this)[1] = (uchar)p1;
}


void MidiMessage::setCommand(int value, int p1, int p2) {
	this->resize(3);
	(*this)[0] = (uchar)value;
	(*this)[1] = (uchar)p1;
	(*this)[2] = (uchar)p2;
}



//////////////////////////////
//
// MidiMessage::setCommandNibble --
//

void MidiMessage::setCommandNibble(int value) {
	if (this->size() < 1) {
		this->resize(1);
	}
	if (value <= 0x0f) {
		(*this)[0] = ((*this)[0] & 0x0f) | ((uchar)((value << 4) & 0xf0));
	} else {
		(*this)[0] = ((*this)[0] & 0x0f) | ((uchar)(value & 0xf0));
	}
}




//////////////////////////////
//
// MidiMessage::setChannelNibble --
//

void MidiMessage::setChannelNibble(int value) {
	if (this->size() < 1) {
		this->resize(1);
	}
	(*this)[0] = ((*this)[0] & 0xf0) | ((uchar)(value & 0x0f));
}


void MidiMessage::setChannel(int value) {
	setChannelNibble(value);
}



//////////////////////////////
//
// MidiMessage::setParameters -- Set the second and optionally the
//     third MIDI byte of a MIDI message.  The command byte will not
//     be altered, and will be set to 0 if it currently does not exist.
//

void MidiMessage::setParameters(int p1) {
	int oldsize = (int)size();
	resize(2);
	(*this)[1] = (uchar)p1;
	if (oldsize < 1) {
		(*this)[0] = 0;
	}
}


void MidiMessage::setParameters(int p1, int p2) {
	int oldsize = (int)size();
	resize(3);
	(*this)[1] = (uchar)p1;
	(*this)[2] = (uchar)p2;
	if (oldsize < 1) {
		(*this)[0] = 0;
	}
}


//////////////////////////////
//
// MidiMessage::setMessage --  Set the contents of MIDI bytes to the
//   input list of bytes.
//

void MidiMessage::setMessage(const std::vector<uchar>& message) {
	this->resize(message.size());
	for (int i=0; i<(int)this->size(); i++) {
		(*this)[i] = message[i];
	}
}


void MidiMessage::setMessage(const std::vector<char>& message) {
	resize(message.size());
	for (int i=0; i<(int)size(); i++) {
		(*this)[i] = (uchar)message[i];
	}
}


void MidiMessage::setMessage(const std::vector<int>& message) {
	resize(message.size());
	for (int i=0; i<(int)size(); i++) {
		(*this)[i] = (uchar)message[i];
	}
}



//////////////////////////////
//
// MidiMessage::setSpelling -- Encode a MidiPlus accidental state for a note.
//    For example, if a note's key number is 60, the enharmonic pitch name
//    could be any of these possibilities:
//        C, B-sharp, D-double-flat
//    MIDI note 60 is ambiguous as to which of these names are intended,
//    so MIDIPlus allows these mappings to be preserved for later recovery.
//    See Chapter 5 (pp. 99-104) of Beyond MIDI (1997).
//
//    The first parameter is the diatonic pitch number (or pitch class
//    if the octave is set to 0):
//       octave * 7 + 0 = C pitches
//       octave * 7 + 1 = D pitches
//       octave * 7 + 2 = E pitches
//       octave * 7 + 3 = F pitches
//       octave * 7 + 4 = G pitches
//       octave * 7 + 5 = A pitches
//       octave * 7 + 6 = B pitches
//
//    The second parameter is the semitone alteration (accidental).
//    0 = natural state, 1 = sharp, 2 = double sharp, -1 = flat,
//    -2 = double flat.
//
//    Only note-on messages can be processed (other messages will be
//    silently ignored).
//

void MidiMessage::setSpelling(int base7, int accidental) {
	if (!isNoteOn()) {
		return;
	}
	// The bottom two bits of the attack velocity are used for the
	// spelling, so need to make sure the velocity will not accidentally
	// be set to zero (and make the note-on a note-off).
	if (getVelocity() < 4) {
		setVelocity(4);
	}
	int dpc = base7 % 7;
	uchar spelling = 0;

	// Table 5.1, page 101 in Beyond MIDI (1997)
	// http://beyondmidi.ccarh.org/beyondmidi-600dpi.pdf
	switch (dpc) {

		case 0:
			switch (accidental) {
				case -2: spelling = 1; break; // Cbb
				case -1: spelling = 1; break; // Cb
				case  0: spelling = 2; break; // C
				case +1: spelling = 2; break; // C#
				case +2: spelling = 3; break; // C##
			}
			break;

		case 1:
			switch (accidental) {
				case -2: spelling = 1; break; // Dbb
				case -1: spelling = 1; break; // Db
				case  0: spelling = 2; break; // D
				case +1: spelling = 3; break; // D#
				case +2: spelling = 3; break; // D##
			}
			break;

		case 2:
			switch (accidental) {
				case -2: spelling = 1; break; // Ebb
				case -1: spelling = 2; break; // Eb
				case  0: spelling = 2; break; // E
				case +1: spelling = 3; break; // E#
				case +2: spelling = 3; break; // E##
			}
			break;

		case 3:
			switch (accidental) {
				case -2: spelling = 1; break; // Fbb
				case -1: spelling = 1; break; // Fb
				case  0: spelling = 2; break; // F
				case +1: spelling = 2; break; // F#
				case +2: spelling = 3; break; // F##
				case +3: spelling = 3; break; // F###
			}
			break;

		case 4:
			switch (accidental) {
				case -2: spelling = 1; break; // Gbb
				case -1: spelling = 1; break; // Gb
				case  0: spelling = 2; break; // G
				case +1: spelling = 2; break; // G#
				case +2: spelling = 3; break; // G##
			}
			break;

		case 5:
			switch (accidental) {
				case -2: spelling = 1; break; // Abb
				case -1: spelling = 1; break; // Ab
				case  0: spelling = 2; break; // A
				case +1: spelling = 3; break; // A#
				case +2: spelling = 3; break; // A##
			}
			break;

		case 6:
			switch (accidental) {
				case -2: spelling = 1; break; // Bbb
				case -1: spelling = 2; break; // Bb
				case  0: spelling = 2; break; // B
				case +1: spelling = 3; break; // B#
				case +2: spelling = 3; break; // B##
			}
			break;

	}

	uchar vel = static_cast<uchar>(getVelocity());
	// suppress any previous content in the first two bits:
	vel = vel & 0xFC;
	// insert the spelling code:
	vel = vel | spelling;
	setVelocity(vel);
}



//////////////////////////////
//
// MidiMessage::getSpelling -- Return the diatonic pitch class and accidental
//    for a note-on's key number.  The MIDI file must be encoded with MIDIPlus
//    pitch spelling codes for this function to return valid data; otherwise,
//    it will return a neutral fixed spelling for each MIDI key.
//
//    The first parameter will be filled in with the base-7 diatonic pitch:
//        pc + octave * 7
//     where pc is the numbers 0 through 6 representing the pitch classes
//     C through B, the octave is MIDI octave (not the scientific pitch
//     octave which is one less than the MIDI octave, such as C4 = middle C).
//     The second number is the accidental for the base-7 pitch.
//

void MidiMessage::getSpelling(int& base7, int& accidental) {
	if (!isNoteOn()) {
		return;
	}
	base7 = -123456;
	accidental = 123456;
	int base12   = getKeyNumber();
	int octave   = base12 / 12;
	int base12pc = base12 - octave * 12;
	int base7pc  = 0;
	int spelling = 0x03 & getVelocity();

	// Table 5.1, page 101 in Beyond MIDI (1997)
	// http://beyondmidi.ccarh.org/beyondmidi-600dpi.pdf
	switch (base12pc) {

		case 0:
			switch (spelling) {
				        case 1: base7pc = 1; accidental = -2; break;  // Dbb
				case 0: case 2: base7pc = 0; accidental =  0; break;  // C
				        case 3: base7pc = 6; accidental = +1; octave--; break;  // B#
			}
			break;

		case 1:
			switch (spelling) {
				        case 1: base7pc = 1; accidental = -1; break;  // Db
				case 0: case 2: base7pc = 0; accidental = +1; break;  // C#
				        case 3: base7pc = 6; accidental = +2; octave--; break;  // B##
			}
			break;

		case 2:
			switch (spelling) {
				        case 1: base7pc = 2; accidental = -2; break;  // Ebb
				case 0: case 2: base7pc = 1; accidental =  0; break;  // D
				        case 3: base7pc = 0; accidental = +2; break;  // C##
			}
			break;

		case 3:
			switch (spelling) {
				        case 1: base7pc = 3; accidental = -2; break;  // Fbb
				case 0: case 2: base7pc = 2; accidental = -1; break;  // Eb
				        case 3: base7pc = 1; accidental = +1; break;  // D#
			}
			break;

		case 4:
			switch (spelling) {
				        case 1: base7pc = 3; accidental = -1; break;  // Fb
				case 0: case 2: base7pc = 2; accidental =  0; break;  // E
				        case 3: base7pc = 1; accidental = +2; break;  // D##
			}
			break;

		case 5:
			switch (spelling) {
				        case 1: base7pc = 4; accidental = -2; break;  // Gbb
				case 0: case 2: base7pc = 3; accidental =  0; break;  // F
				        case 3: base7pc = 2; accidental = +1; break;  // E#
			}
			break;

		case 6:
			switch (spelling) {
				        case 1: base7pc = 4; accidental = -1; break;  // Gb
				case 0: case 2: base7pc = 3; accidental = +1; break;  // F#
				        case 3: base7pc = 2; accidental = +2; break;  // E##
			}
			break;

		case 7:
			switch (spelling) {
				        case 1: base7pc = 5; accidental = -2; break;  // Abb
				case 0: case 2: base7pc = 4; accidental =  0; break;  // G
				        case 3: base7pc = 3; accidental = +2; break;  // F##
			}
			break;

		case 8:
			switch (spelling) {
				        case 1: base7pc = 5; accidental = -1; break;  // Ab
				case 0: case 2: base7pc = 4; accidental = +1; break;  // G#
				        case 3: base7pc = 3; accidental = +3; break;  // F###
			}
			break;

		case 9:
			switch (spelling) {
				        case 1: base7pc = 6; accidental = -2; break;  // Bbb
				case 0: case 2: base7pc = 5; accidental =  0; break;  // A
				        case 3: base7pc = 4; accidental = +2; break;  // G##
			}
			break;

		case 10:
			switch (spelling) {
				        case 1: base7pc = 0; accidental = -2; octave++; break;  // Cbb
				case 0: case 2: base7pc = 6; accidental = -1; break;  // Bb
				        case 3: base7pc = 5; accidental = +1; break;  // A#
			}
			break;

		case 11:
			switch (spelling) {
				        case 1: base7pc = 0; accidental = -1; octave++; break;  // Cb
				case 0: case 2: base7pc = 6; accidental =  0; break;  // B
				        case 3: base7pc = 5; accidental = +2; break;  // A##
			}
			break;

	}

	base7 = base7pc + 7 * octave;
}



//////////////////////////////
//
// MidiMessage::getMetaContent -- Returns the bytes of the meta
//   message after the length (which is a variable-length-value).
//

std::string MidiMessage::getMetaContent(void) const {
	std::string output;
	if (!isMetaMessage()) {
		return output;
	}
	int start = 3;
	if (operator[](2) > 0x7f) {
		start++;
		if (operator[](3) > 0x7f) {
			start++;
			if (operator[](4) > 0x7f) {
				start++;
				if (operator[](5) > 0x7f) {
					start++;
					// maximum of 5 bytes in VLV, so last must be < 0x80
				}
			}
		}
	}
	output.reserve(this->size());
	for (int i=start; i<(int)this->size(); i++) {
		output.push_back(operator[](i));
	}
	return output;
}



//////////////////////////////
//
// MidiMessage::setMetaContent - Set the content of a meta-message.  This
//    function handles the size of the message starting at byte 3 in the
//    message, and it does not alter the meta message type.  The message
//    must be a meta-message before calling this function and be assigned
//    a meta-message type.
//

void MidiMessage::setMetaContent(const std::string& content) {
	if (this->size() < 2) {
		// invalid message, so ignore request
		return;
	}
	if (operator[](0) != 0xFF) {
		// not a meta message, so ignore request
		return;
	}
	this->resize(2);

	// add the size of the meta message data (VLV)
	int dsize = (int)content.size();
	std::vector<uchar> vlv = intToVlv(dsize);
	for (uchar item : vlv) {
		this->push_back(item);
	}
	std::copy(content.begin(), content.end(), std::back_inserter(*this));
}



//////////////////////////////
//
// MidiMessage::setMetaTempo -- Input tempo is in quarter notes per minute
//   (meta message #0x51).
//

void MidiMessage::setMetaTempo(double tempo) {
	int microseconds = (int)(60.0 / tempo * 1000000.0 + 0.5);
	setTempoMicroseconds(microseconds);
}



//////////////////////////////
//
// MidiMessage::setTempo -- Alias for MidiMessage::setMetaTempo().
//

void MidiMessage::setTempo(double tempo) {
	setMetaTempo(tempo);
}



//////////////////////////////
//
// MidiMessage::setTempoMicroseconds -- Set the tempo in terms
//   of microseconds per quarter note.
//

void MidiMessage::setTempoMicroseconds(int microseconds) {
	resize(6);
	(*this)[0] = 0xff;
	(*this)[1] = 0x51;
	(*this)[2] = 3;
	(*this)[3] = (microseconds >> 16) & 0xff;
	(*this)[4] = (microseconds >>  8) & 0xff;
	(*this)[5] = (microseconds >>  0) & 0xff;
}



//////////////////////////////
//
// MidiMessage::makeKeySignature -- create a key signature meta message
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

void MidiMessage::makeKeySignature(int fifths, bool mode) {
    resize(5);
    (*this)[0] = 0xff;
    (*this)[1] = 0x59;
    (*this)[2] = 0x02;
    (*this)[3] = 0xff & fifths;
    (*this)[4] = 0xff & (int)mode;
}



//////////////////////////////
//
// MidiMessage::makeTimeSignature -- create a time signature meta message
//      (meta #0x58).  The "bottom" parameter should be a power of two;
//      otherwise, it will be forced to be the next highest power of two,
//      as MIDI time signatures must have a power of two in the denominator.
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

void MidiMessage::makeTimeSignature(int top, int bottom, int clocksPerClick,
		int num32ndsPerQuarter) {
	int base2 = 0;
	while (bottom >>= 1) base2++;
	resize(7);
	(*this)[0] = 0xff;
	(*this)[1] = 0x58;
	(*this)[2] = 4;
	(*this)[3] = 0xff & top;
	(*this)[4] = 0xff & base2;
	(*this)[5] = 0xff & clocksPerClick;
	(*this)[6] = 0xff & num32ndsPerQuarter;
}



///////////////////////////////////////////////////////////////////////////
//
// make functions to create various MIDI message --
//


//////////////////////////////
//
// MidiMessage::makeNoteOn -- create a note-on message.
//
// default value: channel = 0
//
// Note: The channel parameter used to be last, but makes more sense to
//   have it first...
//

void MidiMessage::makeNoteOn(int channel, int key, int velocity) {
	resize(3);
	(*this)[0] = 0x90 | (0x0f & channel);
	(*this)[1] = key & 0x7f;
	(*this)[2] = velocity & 0x7f;
}



//////////////////////////////
//
// MidiMessage::makeNoteOff -- create a note-off message.   If no
//   parameters are given, the current contents is presumed to be a
//   note-on message, which will be converted into a note-off message.
//
// default value: channel = 0
//
// Note: The channel parameter used to be last, but makes more sense to
//   have it first...
//


void MidiMessage::makeNoteOff(int channel, int key, int velocity) {
	resize(3);
	(*this)[0] = 0x80 | (0x0f & channel);
	(*this)[1] = key & 0x7f;
	(*this)[2] = velocity & 0x7f;
}


void MidiMessage::makeNoteOff(int channel, int key) {
	resize(3);
	(*this)[0] = 0x90 | (0x0f & channel);
	(*this)[1] = key & 0x7f;
	(*this)[2] = 0x00;
}

//
// MidiMessage::makeNoteOff(void) -- create a 0x90 note message with
//      The key and velocity set to 0.
//

void MidiMessage::makeNoteOff(void) {
	if (!isNoteOn()) {
		resize(3);
		(*this)[0] = 0x90;
		(*this)[1] = 0;
		(*this)[2] = 0;
	} else {
		(*this)[2] = 0;
	}
}



/////////////////////////////
//
// MidiMessage::makePatchChange -- Create a patch-change message.
//

void MidiMessage::makePatchChange(int channel, int patchnum) {
	resize(0);
	push_back(0xc0 | (0x0f & channel));
	push_back(0x7f & patchnum);
}

//
// MidiMessage::makeTimbre -- alias for MidiMessage::makePatchChange().
//

void MidiMessage::makeTimbre(int channel, int patchnum) {
	makePatchChange(channel, patchnum);
}


/////////////////////////////
//
// MidiMessage::makeController -- Create a controller message.
//

void MidiMessage::makeController(int channel, int num, int value) {
	resize(0);
	push_back(0xb0 | (0x0f & channel));
	push_back(0x7f & num);
	push_back(0x7f & value);
}



/////////////////////////////
//
// MidiMessage::makePitchBend -- Create a pitch-bend message.  lsb is
//     least-significant 7 bits of the 14-bit range, and msb is the
//     most-significant 7 bits of the 14-bit range.  The range depth
//     is determined by a setting in the synthesizer.  Typically it is
//     +/- two semitones by default.  See MidiFile::setPitchBendRange()
//     to change the default (or change to the typical default).
//

void MidiMessage::makePitchBend(int channel, int lsb, int msb) {
	resize(0);
	push_back(0xe0 | (0x0e & channel));
	push_back(0x7f & lsb);
	push_back(0x7f & msb);
}

//
// value is a 14-bit number, where 0 is the lowest pitch of the range, and
//    2^15-1 is the highest pitch of the range.
//

void MidiMessage::makePitchBend(int channel, int value) {
	resize(0);
	int lsb = value & 0x7f;
	int msb = (value >> 7) & 0x7f;
	push_back(0xe0 | (0x7f & channel));
	push_back(lsb);
	push_back(msb);
}

//
// Input value is a number between -1.0 and +1.0.
//

void MidiMessage::makePitchBendDouble(int channel, double value) {
	// value is in the range from -1 for minimum and 2^18 - 1 for the maximum
	resize(0);
	double dvalue = (value + 1.0) * (pow(2.0, 15.0));
	if (dvalue < 0.0) {
		dvalue = 0.0;
	}
	if (dvalue > pow(2.0, 15.0) - 1.0) {
		dvalue = pow(2.0, 15.0) - 1.0;
	}
	ulong uivalue = (ulong)dvalue;
	uchar lsb = uivalue & 0x7f;
	uchar msb = (uivalue >> 7) & 0x7f;
	push_back(0xe0 | (0x7f & channel));
	push_back(lsb);
	push_back(msb);
}



/////////////////////////////
//
// MidiMessage::makeSustain -- Create a sustain pedal message.
//   Value in 0-63 range is a sustain off.  Value in the
//   64-127 value is a sustain on.
//

void MidiMessage::makeSustain(int channel, int value) {
	makeController(channel, 64, value);
}

//
// MidiMessage::makeSustain -- Alias for MidiMessage::makeSustain().
//

void MidiMessage::makeSustainPedal(int channel, int value) {
	makeSustain(channel, value);
}



/////////////////////////////
//
// MidiMessage::makeSustainOn -- Create sustain-on controller message.
//

void MidiMessage::makeSustainOn(int channel) {
	makeController(channel, 64, 127);
}

//
// MidiMessage::makeSustainPedalOn -- Alias for MidiMessage::makeSustainOn().
//

void MidiMessage::makeSustainPedalOn(int channel) {
	makeSustainOn(channel);
}



/////////////////////////////
//
// MidiMessage::makeSustainOff -- Create a sustain-off controller message.
//

void MidiMessage::makeSustainOff(int channel) {
	makeController(channel, 64, 0);
}

//
// MidiMessage::makeSustainPedalOff -- Alias for MidiMessage::makeSustainOff().
//

void MidiMessage::makeSustainPedalOff(int channel) {
	makeSustainOff(channel);
}



//////////////////////////////
//
// MidiMessage::makeMetaMessage -- Create a Meta event with the
//   given text string as the parameter.  The length of the string should
//   is a VLV.  If the length is larger than 127 byte, then the length
//   will contain more than one byte.
//

void MidiMessage::makeMetaMessage(int mnum, const std::string& data) {
	resize(0);
	push_back(0xff);
	push_back(mnum & 0x7f); // max meta-message number is 0x7f.
	setMetaContent(data);
}



//////////////////////////////
//
// MidiMessage::makeText -- Create a metaevent text message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeText(const std::string& text) {
	makeMetaMessage(0x01, text);
}



//////////////////////////////
//
// MidiMessage::makeCopyright -- Create a metaevent copyright message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeCopyright(const std::string& text) {
	makeMetaMessage(0x02, text);
}



//////////////////////////////
//
// MidiMessage::makeTrackName -- Create a metaevent track name message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeTrackName(const std::string& name) {
	makeMetaMessage(0x03, name);
}



//////////////////////////////
//
// MidiMessage::makeTrackName -- Create a metaevent instrument name message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeInstrumentName(const std::string& name) {
	makeMetaMessage(0x04, name);
}



//////////////////////////////
//
// MidiMessage::makeLyric -- Create a metaevent lyrics/text message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeLyric(const std::string& text) {
	makeMetaMessage(0x05, text);
}



//////////////////////////////
//
// MidiMessage::makeMarker -- Create a metaevent marker message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeMarker(const std::string& text) {
	makeMetaMessage(0x06, text);
}



//////////////////////////////
//
// MidiMessage::makeCue -- Create a metaevent cue-point message.
//    This is not a real MIDI message, but rather a pretend message for use
//    within Standard MIDI Files.
//

void MidiMessage::makeCue(const std::string& text) {
	makeMetaMessage(0x07, text);
}


//////////////////////////////
//
// MidiMessage::intToVlv -- Convert an integer into a VLV byte sequence.
//

std::vector<uchar> MidiMessage::intToVlv(int value) {
	std::vector<uchar> output;
	if (value < 128) {
		output.push_back((uchar)value);
	} else {
		// calculate VLV bytes and insert into message
		uchar byte1 = value & 0x7f;
		uchar byte2 = (value >>  7) & 0x7f;
		uchar byte3 = (value >> 14) & 0x7f;
		uchar byte4 = (value >> 21) & 0x7f;
		uchar byte5 = (value >> 28) & 0x7f;
		if (byte5) {
			byte4 |= 0x80;
		}
		if (byte4) {
			byte4 |= 0x80;
			byte3 |= 0x80;
		}
		if (byte3) {
			byte3 |= 0x80;
			byte2 |= 0x80;
		}
		if (byte2) {
			byte2 |= 0x80;
		}
		if (byte5) { output.push_back(byte5); }
		if (byte4) { output.push_back(byte4); }
		if (byte3) { output.push_back(byte3); }
		if (byte2) { output.push_back(byte2); }
		output.push_back(byte1);
	}

	return output;
}



//////////////////////////////
//
// MidiMessage::makeSysExMessage -- Add F0 at start and F7 at end (do not include
//    in data, but they will be double-checked for and ignored if found.
//

void MidiMessage::makeSysExMessage(const std::vector<uchar>& data) {
	int startindex = 0;
	int endindex = (int)data.size() - 1;
	if (data.size() > 0) {
		if (data[0] == 0xf0) {
			startindex++;
		}
	}
	if (data.size() > 0) {
		if (data.back() == 0xf7) {
			endindex--;
		}
	}

	this->clear();
	this->reserve(data.size() + 7);

	this->push_back((uchar)0xf0);

	int msize = endindex - startindex + 2;
	std::vector<uchar> vlv = intToVlv(msize);
	for (uchar item : vlv) {
		this->push_back(item);
	}
	for (int i=startindex; i<=endindex; i++) {
		this->push_back(data.at(i));
	}
	this->push_back((uchar)0xf7);
}



//////////////////////////////
//
// MidiMessage::frequencyToSemitones -- convert from frequency in Hertz to
//     semitones (MIDI key numbers with fractional values).  Returns 0.0
//     if too low, and returns 127.0 if too high.
//

double MidiMessage::frequencyToSemitones(double frequency, double a4frequency) {
	if (frequency < 1) {
		return 0.0;
	}
	if (a4frequency <= 0) {
		return 0.0;
	}
	double semitones = 69.0 + 12.0 * log2(frequency/a4frequency);
	if (semitones >= 128.0) {
		return 127.0;
	} else if (semitones < 0.0) {
		return 0.0;
	}
	return semitones;
}



//////////////////////////////
//
// MidiMessage::makeMts2_KeyTuningsByFrequency -- Map a list of key numbers to specific pitches by frequency.
//

void MidiMessage::makeMts2_KeyTuningsByFrequency(int key, double frequency, int program) {
	std::vector<std::pair<int, double>> mapping;
	mapping.push_back(std::make_pair(key, frequency));
	this->makeMts2_KeyTuningsByFrequency(mapping, program);
}


void MidiMessage::makeMts2_KeyTuningByFrequency(int key, double frequency, int program) {
	this->makeMts2_KeyTuningsByFrequency(key, frequency, program);
}


void MidiMessage::makeMts2_KeyTuningsByFrequency(std::vector<std::pair<int, double>>& mapping, int program) {
	std::vector<std::pair<int, double>> semimap(mapping.size());
	for (int i=0; i<(int)mapping.size(); i++) {
		semimap[i].first = mapping[i].first;
		semimap[i].second = MidiMessage::frequencyToSemitones(mapping[i].second);
	}
	this->makeMts2_KeyTuningsBySemitone(semimap, program);
}



//////////////////////////////
//
// MidiMessage::makeMts2_KeyTuningsBySemitone -- Map a list of key numbers to specific pitches by absolute
//    semitones (MIDI key numbers with fractional values).
//

void MidiMessage::makeMts2_KeyTuningsBySemitone(int key, double semitone, int program) {
	std::vector<std::pair<int, double>> semimap;
	semimap.push_back(std::make_pair(key, semitone));
	this->makeMts2_KeyTuningsBySemitone(semimap, program);
}


void MidiMessage::makeMts2_KeyTuningBySemitone(int key, double semitone, int program) {
	this->makeMts2_KeyTuningsBySemitone(key, semitone, program);
}


void MidiMessage::makeMts2_KeyTuningsBySemitone(std::vector<std::pair<int, double>>& mapping, int program) {
	if (program < 0) {
		program = 0;
	} else if (program > 127) {
		program = 127;
	}
	std::vector<uchar> data;
	data.reserve(mapping.size() * 4 + 10);
	data.push_back((uchar)0x7f);  // real-time sysex
	data.push_back((uchar)0x7f);  // all devices
	data.push_back((uchar)0x08);  // sub-ID#1 (MIDI Tuning)
	data.push_back((uchar)0x02);  // sub-ID#2 (note change)
	data.push_back((uchar)program);  // tuning program number (0 - 127)
	std::vector<uchar> vlv = intToVlv((int)mapping.size());
	for (uchar item : vlv) {
		data.push_back(item);
	}
	for (auto &item : mapping) {
		int keynum = item.first;
		if (keynum < 0) {
			keynum = 0;
		} else if (keynum > 127) {
			keynum = 127;
		}
		data.push_back((uchar)keynum);
		double semitones = item.second;
		int sint = (int)semitones;
		if (sint < 0) {
			sint = 0;
		} else if (sint > 127) {
			sint = 127;
		}
		data.push_back((uchar)sint);
		double fraction = semitones - sint;
		int value = int(fraction * (1 << 14));
		uchar lsb = value & 0x7f;
		uchar msb = (value >> 7) & 0x7f;
		data.push_back(msb);
		data.push_back(lsb);
    }
    this->makeSysExMessage(data);
}



//////////////////////////////
//
// MidiMessage::makeMts9_TemperamentByCentsDeviationFromET --
//

void MidiMessage::makeMts9_TemperamentByCentsDeviationFromET (std::vector<double>& mapping, int referencePitchClass, int channelMask) {
	if (mapping.size() != 12) {
		std::cerr << "Error: input mapping must have a size of 12." << std::endl;
		return;
	}
	if (referencePitchClass < 0) {
		std::cerr << "Error: Cannot have a negative reference pitch class" << std::endl;
		return;
	}

	std::vector<uchar> data;
	data.reserve(24 + 7);

	data.push_back((uchar)0x7f);  // real-time sysex
	data.push_back((uchar)0x7f);  // all devices
	data.push_back((uchar)0x08);  // sub-ID#1 (MIDI Tuning)
	data.push_back((uchar)0x09);  // sub-ID#2 (note change)

	uchar MMSB = (channelMask >> 14) & 0x3;
	uchar MSB  = (channelMask >> 7)  & 0x7f;
	uchar LSB  = channelMask & 0x7f;

	data.push_back(MMSB);
	data.push_back(MSB);
	data.push_back(LSB);

	for (int i=0; i<(int)mapping.size(); i++) {
		int ii = (i - referencePitchClass + 48) % 12;
		double value = mapping.at(ii) / 100.0;

		if (value > 1.0) {
			value = 1.0;
		}
		if (value < -1.0) {
			value = -1.0;
		}

		int intval = (int)(((1 << 13)-0.5)  * (value + 1.0) + 0.5);
		uchar LSB = intval & 0x7f;
		uchar MSB = (intval >>  7) & 0x7f;
		data.push_back(MSB);
		data.push_back(LSB);
	}
	this->makeSysExMessage(data);
}



//////////////////////////////
//
// MidiMessage::makeEqualTemperament --
//

void MidiMessage::makeTemperamentEqual(int referencePitchClass, int channelMask) {
	std::vector<double> temperament(12, 0.0);
	this->makeMts9_TemperamentByCentsDeviationFromET(temperament, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentBad -- Detune by random amounts from equal temperament.
//

void MidiMessage::makeTemperamentBad(double maxDeviationCents, int referencePitchClass, int channelMask) {
	if (maxDeviationCents < 0.0) {
		maxDeviationCents = -maxDeviationCents;
	}
	if (maxDeviationCents > 100.0) {
		maxDeviationCents = 100.0;
	}
	std::vector<double> temperament(12);
	for (double &item : temperament) {
		item = ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * maxDeviationCents;
	}
	this->makeMts9_TemperamentByCentsDeviationFromET(temperament, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentPythagorean -- Default reference pitch is 2 (D)
//

void MidiMessage::makeTemperamentPythagorean(int referencePitchClass, int channelMask) {
	std::vector<double> temperament(12);
	double x = 1200.0 * log2(3.0 / 2.0);
	temperament[1]  = x * -5 + 3500; // -9.775 cents
	temperament[8]  = x * -4 + 2800; // -7.820 cents
	temperament[3]  = x * -3 + 2100; // -5.865 cents
	temperament[10] = x * -2 + 1400; // -3.910 cents
	temperament[5]  = x * -1 + 700;  // -1.955 cents
	temperament[0]  = 0.0;           //  0     cents
	temperament[7]  = x * 1 - 700;   //  1.955 cents
	temperament[2]  = x * 2 - 1400;  //  3.910 cents
	temperament[9]  = x * 3 - 2100;  //  5.865 cents
	temperament[4]  = x * 4 - 2800;  //  7.820 cents
	temperament[11] = x * 5 - 3500;  //  9.775 cents
	temperament[6]  = x * 6 - 4200;  // 11.730 cents
	this->makeMts9_TemperamentByCentsDeviationFromET(temperament, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentMeantone -- Default type is 1/4-comma meantone.
//

void MidiMessage::makeTemperamentMeantone(double fraction, int referencePitchClass, int channelMask) {
	std::vector<double> temperament(12);
	double x = 1200.0 * log2((3.0/2.0)*pow(81.0/80.0, -fraction));
	temperament[1]  = x * -5 + 3500; //  17.107 cents (for fraction = 0.25)
	temperament[8]  = x * -4 + 2800; //  13.686 cents (for fraction = 0.25)
	temperament[3]  = x * -3 + 2100; //  10.265 cents (for fraction = 0.25)
	temperament[10] = x * -2 + 1400; //   6.843 cents (for fraction = 0.25)
	temperament[5]  = x * -1 + 700;  //   3.422 cents (for fraction = 0.25)
	temperament[0]  = 0.0;           //   0     cents
	temperament[7]  = x *  1 - 700;  //  -3.422 cents (for fraction = 0.25)
	temperament[2]  = x *  2 - 1400; //  -6.843 cents (for fraction = 0.25)
	temperament[9]  = x *  3 - 2100; // -10.265 cents (for fraction = 0.25)
	temperament[4]  = x *  4 - 2800; // -13.686 cents (for fraction = 0.25)
	temperament[11] = x *  5 - 3500; // -17.107 cents (for fraction = 0.25)
	temperament[6]  = x *  6 - 4200; // -20.529 cents (for fraction = 0.25)
	this->makeMts9_TemperamentByCentsDeviationFromET(temperament, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentMeantoneCommaQuarter -- 1/4-comma meantone
//

void MidiMessage::makeTemperamentMeantoneCommaQuarter(int referencePitchClass, int channelMask) {
	this->makeTemperamentMeantone(1.0 / 4.0, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentMeantoneCommaThird -- 1/3-comma meantone
//

void MidiMessage::makeTemperamentMeantoneCommaThird(int referencePitchClass, int channelMask) {
	this->makeTemperamentMeantone(1.0 / 3.0, referencePitchClass, channelMask);
}



//////////////////////////////
//
// MidiMessage::makeTemperamentMeantoneCommaHalf -- 1/2-comma meantone
//

void MidiMessage::makeTemperamentMeantoneCommaHalf(int referencePitchClass, int channelMask) {
	this->makeTemperamentMeantone(1.0 / 2.0, referencePitchClass, channelMask);
}



//////////////////////////////
//
// operator<<(MidiMessage) -- Print MIDI messages as text.  0x80 and above
//    are printed as hex, below as dec (will look strange for meta messages
//    and system exclusives which could be dealt with later).
//

std::ostream& operator<<(std::ostream& out, MidiMessage& message) {
	for (int i=0; i<(int)message.size(); i++) {
		if (message[i] >= 0x80) {
			out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)message[i];
			out << std::dec << std::setw(0) << std::setfill(' ');
		} else {
			out << (int)message[i];
		}
		if (i<(int)message.size() - 1) {
			out << ' ';
		}
	}
	return out;
}


} // end namespace smf



