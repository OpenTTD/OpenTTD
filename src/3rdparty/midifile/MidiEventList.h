//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat Feb 14 21:55:38 PST 2015
// Last Modified: Sat Apr 21 10:52:19 PDT 2018 Removed using namespace std;
// Filename:      midifile/include/MidiEventList.h
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 noexpandtab
//
// Description:   A class that stores a MidiEvents for a MidiFile track.
//

#ifndef _MIDIEVENTLIST_H_INCLUDED
#define _MIDIEVENTLIST_H_INCLUDED

#include "MidiEvent.h"

#include <vector>


namespace smf {

class MidiEventList {
	public:
		                 MidiEventList      (void);
		                 MidiEventList      (const MidiEventList& other);
		                 MidiEventList      (MidiEventList&& other);

		                ~MidiEventList      ();

		MidiEventList&   operator=          (MidiEventList& other);
		MidiEvent&       operator[]         (int index);
		const MidiEvent& operator[]         (int index) const;

		MidiEvent&       back               (void);
		const MidiEvent& back               (void) const;
		MidiEvent&       last               (void);
		const MidiEvent& last               (void) const;
		MidiEvent&       getEvent           (int index);
		const MidiEvent& getEvent           (int index) const;
		void             clear              (void);
		void             reserve            (int rsize);
		int              getEventCount      (void) const;
		int              getSize            (void) const;
		int              size               (void) const;
		void             removeEmpties      (void);
		int              linkNotePairs      (void);
		int              linkEventPairs     (void);
		void             clearLinks         (void);
		void             clearSequence      (void);
		int              markSequence       (int sequence = 1);

		int              push               (MidiEvent& event);
		int              push_back          (MidiEvent& event);
		int              append             (MidiEvent& event);

		// careful when using these, intended for internal use in MidiFile class:
		void             detach             (void);
		int              push_back_no_copy  (MidiEvent* event);

		// access to the list of MidiEvents for sorting with an external function:
		MidiEvent**      data               (void);

	protected:
		std::vector<MidiEvent*> list;

	private:
		void             sort                (void);

	// MidiFile class calls sort()
	friend class MidiFile;
};


int eventcompare(const void* a, const void* b);

} // end of namespace smf

#endif /* _MIDIEVENTLIST_H_INCLUDED */



