/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "os_abstraction.h"

#ifdef __MORPHOS__
/* the library base is required here */
struct Library *SocketBase = NULL;
#endif

/**
 * Initializes the network core (as that is needed for some platforms
 */
void NetworkCoreInitialize(void)
{
#if defined(__MORPHOS__) || defined(__AMIGA__)
	/*
	 *  IMPORTANT NOTE: SocketBase needs to be initialized before we use _any_
	 *  network related function, else: crash.
	 */
	DEBUG(net, 3, "[core] loading bsd socket library");
	SocketBase = OpenLibrary("bsdsocket.library", 4);
	if (SocketBase == NULL) {
		DEBUG(net, 0, "[core] can't open bsdsocket.library version 4, network unavailable");
		_network_available = false;
		return;
	}

#if defined(__AMIGA__)
	/* for usleep() implementation (only required for legacy AmigaOS builds) */
	TimerPort = CreateMsgPort();
	if (TimerPort != NULL) {
		TimerRequest = (struct timerequest*)CreateIORequest(TimerPort, sizeof(struct timerequest);
		if (TimerRequest != NULL) {
			if (OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest*)TimerRequest, 0) == 0) {
				TimerBase = TimerRequest->tr_node.io_Device;
				if (TimerBase == NULL) {
					// free ressources...
					DEBUG(net, 0, "[core] can't initialize timer, network unavailable");
					_network_available = false;
					return;
				}
			}
		}
	}
#endif // __AMIGA__
#endif // __MORPHOS__ / __AMIGA__

/* Let's load the network in windows */
#ifdef WIN32
	{
		WSADATA wsa;
		DEBUG(net, 3, "[core] loading windows socket library");
		if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0) {
			DEBUG(net, 0, "[core] WSAStartup failed, network unavailable");
			_network_available = false;
			return;
		}
	}
#endif /* WIN32 */
}

/**
 * Shuts down the network core (as that is needed for some platforms
 */
void NetworkCoreShutdown(void)
{
#if defined(__MORPHOS__) || defined(__AMIGA__)
	/* free allocated ressources */
#if defined(__AMIGA__)
	if (TimerBase    != NULL) CloseDevice((struct IORequest*)TimerRequest); // XXX This smells wrong
	if (TimerRequest != NULL) DeleteIORequest(TimerRequest);
	if (TimerPort    != NULL) DeleteMsgPort(TimerPort);
#endif

	if (SocketBase != NULL) CloseLibrary(SocketBase);
#endif

#if defined(WIN32)
	WSACleanup();
#endif
}

#endif /* ENABLE_NETWORK */
