/* $Id$ */

/** @file signal_type.h Types and classes related to signals. */

#ifndef SIGNAL_TYPE_H
#define SIGNAL_TYPE_H

/** Variant of the signal, i.e. how does the signal look? */
enum SignalVariant {
	SIG_ELECTRIC  = 0, ///< Light signal
	SIG_SEMAPHORE = 1  ///< Old-fashioned semaphore signal
};


/** Type of signal, i.e. how does the signal behave? */
enum SignalType {
	SIGTYPE_NORMAL  = 0, ///< normal signal
	SIGTYPE_ENTRY   = 1, ///< presignal block entry
	SIGTYPE_EXIT    = 2, ///< presignal block exit
	SIGTYPE_COMBO   = 3  ///< presignal inter-block
};


#endif /* SIGNAL_TYPE_H */
