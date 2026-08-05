//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat Feb 14 21:40:14 PST 2015
// Last Modified: Sat Apr 21 10:52:19 PDT 2018 Removed using namespace std;
// Filename:      midifile/src/MidiEvent.cpp
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   A class which stores a MidiMessage and a timestamp
//                for the MidiFile class.
//

#include "MidiEvent.h"

#include <cstdlib>


namespace smf {

//////////////////////////////
//
// MidiEvent::MidiEvent -- Constructor classes
//

MidiEvent::MidiEvent(void) : MidiMessage() {
	clearVariables();
}


MidiEvent::MidiEvent(int command) : MidiMessage(command)  {
	clearVariables();
}


MidiEvent::MidiEvent(int command, int p1) : MidiMessage(command, p1) {
	clearVariables();
}


MidiEvent::MidiEvent(int command, int p1, int p2)
		: MidiMessage(command, p1, p2) {
	clearVariables();
}


MidiEvent::MidiEvent(int aTime, int aTrack, vector<uchar>& message)
		: MidiMessage(message) {
	track       = aTrack;
	tick        = aTime;
	seconds     = 0.0;
	seq         = 0;
	m_eventlink = NULL;
}


MidiEvent::MidiEvent(const MidiEvent& mfevent) : MidiMessage() {
	track   = mfevent.track;
	tick    = mfevent.tick;
	seconds = mfevent.seconds;
	seq     = mfevent.seq;
	m_eventlink = NULL;

	this->resize(mfevent.size());
	for (int i=0; i<(int)this->size(); i++) {
		(*this)[i] = mfevent[i];
	}
}



//////////////////////////////
//
// MidiEvent::~MidiEvent -- MidiFile Event destructor
//

MidiEvent::~MidiEvent() {
	track   = -1;
	tick    = -1;
	seconds = -1.0;
	seq     = -1;
	this->resize(0);
	m_eventlink = NULL;
}


//////////////////////////////
//
// MidiEvent::clearVariables --  Clear everything except MidiMessage data.
//

void MidiEvent::clearVariables(void) {
	track     = 0;
	tick      = 0;
	seconds   = 0.0;
	seq       = 0;
	m_eventlink = NULL;
}


//////////////////////////////
//
// MidiEvent::operator= -- Copy the contents of another MidiEvent.
//

MidiEvent& MidiEvent::operator=(const MidiEvent& mfevent) {
	if (this == &mfevent) {
		return *this;
	}
	tick    = mfevent.tick;
	track   = mfevent.track;
	seconds = mfevent.seconds;
	seq     = mfevent.seq;
	m_eventlink = NULL;
	this->resize(mfevent.size());
	for (int i=0; i<(int)this->size(); i++) {
		(*this)[i] = mfevent[i];
	}
	return *this;
}


MidiEvent& MidiEvent::operator=(const MidiMessage& message) {
	if (this == &message) {
		return *this;
	}
	clearVariables();
	this->resize(message.size());
	for (int i=0; i<(int)this->size(); i++) {
		(*this)[i] = message[i];
	}
	return *this;
}


MidiEvent& MidiEvent::operator=(const vector<uchar>& bytes) {
	clearVariables();
	this->resize(bytes.size());
	for (int i=0; i<(int)this->size(); i++) {
		(*this)[i] = bytes[i];
	}
	return *this;
}


MidiEvent& MidiEvent::operator=(const vector<char>& bytes) {
	clearVariables();
	setMessage(bytes);
	return *this;
}


MidiEvent& MidiEvent::operator=(const vector<int>& bytes) {
	clearVariables();
	setMessage(bytes);
	return *this;
}



//////////////////////////////
//
// MidiEvent::unlinkEvent -- Disassociate this event with another.
//   Also tell the other event to disassociate from this event.
//

void MidiEvent::unlinkEvent(void) {
	if (m_eventlink == NULL) {
		return;
	}
	MidiEvent* mev = m_eventlink;
	m_eventlink = NULL;
	mev->unlinkEvent();
}



//////////////////////////////
//
// MidiEvent::linkEvent -- Make a link between two messages.
//   Unlinking
//

void MidiEvent::linkEvent(MidiEvent* mev) {
	if (mev->m_eventlink != NULL) {
		// unlink other event if it is linked to something else;
		mev->unlinkEvent();
	}
	// if this is already linked to something else, then unlink:
	if (m_eventlink != NULL) {
		m_eventlink->unlinkEvent();
	}
	unlinkEvent();

	mev->m_eventlink = this;
	m_eventlink = mev;
}


void MidiEvent::linkEvent(MidiEvent& mev) {
	linkEvent(&mev);
}



//////////////////////////////
//
// MidiEvent::getLinkedEvent -- Returns a linked event.  Usually
//   this is the note-off message for a note-on message and vice-versa.
//   Returns null if there are no links.
//

MidiEvent* MidiEvent::getLinkedEvent(void) {
	return m_eventlink;
}


const MidiEvent* MidiEvent::getLinkedEvent(void) const {
	return m_eventlink;
}



//////////////////////////////
//
// MidiEvent::isLinked -- Returns true if there is an event which is not
//   NULL.  This function is similar to getLinkedEvent().
//

int MidiEvent::isLinked(void) const {
	return m_eventlink == NULL ? 0 : 1;
}



//////////////////////////////
//
// MidiEvent::getTickDuration --  For linked events (note-ons and note-offs),
//    return the absolute tick time difference between the two events.
//    The tick values are presumed to be in absolute tick mode rather than
//    delta tick mode.  Returns 0 if not linked.
//

int MidiEvent::getTickDuration(void) const {
	const MidiEvent* mev = getLinkedEvent();
	if (mev == NULL) {
		return 0;
	}
	int tick2 = mev->tick;
	if (tick2 > tick) {
		return tick2 - tick;
	} else {
		return tick - tick2;
	}
}



//////////////////////////////
//
// MidiEvent::getDurationInSeconds -- For linked events (note-ons and
//     note-offs), return the duration of the note in seconds.  The
//     seconds analysis must be done first; otherwise the duration will be
//     reported as zero.
//

double MidiEvent::getDurationInSeconds(void) const {
	const MidiEvent* mev = getLinkedEvent();
	if (mev == NULL) {
		return 0;
	}
	double seconds2 = mev->seconds;
	if (seconds2 > seconds) {
		return seconds2 - seconds;
	} else {
		return seconds - seconds2;
	}
}



//////////////////////////////
//
// operator<<(MidiMessage) -- Print tick value followed by MIDI bytes for event.
//     The tick value will be either relative or absolute depending on the state
//     of the MidiFile object containing it.
//

std::ostream& operator<<(std::ostream& out, MidiEvent& event) {
	out << event.tick << '(' << static_cast<MidiMessage&>(event) << ')';
	return out;
}


} // end namespace smf



