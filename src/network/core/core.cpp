/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core.cpp Functions used to initialize/shut down the core network
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "os_abstraction.h"
#include "packet.h"


#ifdef __MORPHOS__
/* the library base is required here */
struct Library *SocketBase = NULL;
#endif

/**
 * Initializes the network core (as that is needed for some platforms
 * @return true if the core has been initialized, false otherwise
 */
bool NetworkCoreInitialize()
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
		return false;
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
					/* free ressources... */
					DEBUG(net, 0, "[core] can't initialize timer, network unavailable");
					return false;
				}
			}
		}
	}
#endif /* __AMIGA__ */
#endif /* __MORPHOS__ / __AMIGA__ */

/* Let's load the network in windows */
#ifdef WIN32
	{
		WSADATA wsa;
		DEBUG(net, 3, "[core] loading windows socket library");
		if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0) {
			DEBUG(net, 0, "[core] WSAStartup failed, network unavailable");
			return false;
		}
	}
#endif /* WIN32 */

	return true;
}

/**
 * Shuts down the network core (as that is needed for some platforms
 */
void NetworkCoreShutdown()
{
#if defined(__MORPHOS__) || defined(__AMIGA__)
	/* free allocated resources */
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


/**
 * Serializes the GRFIdentifier (GRF ID and MD5 checksum) to the packet
 * @param p   the packet to write the data to
 * @param grf the GRFIdentifier to serialize
 */
void NetworkSocketHandler::SendGRFIdentifier(Packet *p, const GRFIdentifier *grf)
{
	uint j;
	p->Send_uint32(grf->grfid);
	for (j = 0; j < sizeof(grf->md5sum); j++) {
		p->Send_uint8 (grf->md5sum[j]);
	}
}

/**
 * Deserializes the GRFIdentifier (GRF ID and MD5 checksum) from the packet
 * @param p   the packet to read the data from
 * @param grf the GRFIdentifier to deserialize
 */
void NetworkSocketHandler::ReceiveGRFIdentifier(Packet *p, GRFIdentifier *grf)
{
	uint j;
	grf->grfid = p->Recv_uint32();
	for (j = 0; j < sizeof(grf->md5sum); j++) {
		grf->md5sum[j] = p->Recv_uint8();
	}
}

#endif /* ENABLE_NETWORK */
