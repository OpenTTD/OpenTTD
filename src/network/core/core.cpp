/* $Id$ */

/**
 * @file core.cpp Functions used to initialize/shut down the core network
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../company_base.h"
#include "../../strings_func.h"
#include "../../string_func.h"
#include "../../date_func.h"
#include "os_abstraction.h"
#include "core.h"
#include "packet.h"
#include "../network_func.h"

#include "table/strings.h"

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
#endif // __AMIGA__
#endif // __MORPHOS__ / __AMIGA__

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
void NetworkSocketHandler::Send_GRFIdentifier(Packet *p, const GRFIdentifier *grf)
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
void NetworkSocketHandler::Recv_GRFIdentifier(Packet *p, GRFIdentifier *grf)
{
	uint j;
	grf->grfid = p->Recv_uint32();
	for (j = 0; j < sizeof(grf->md5sum); j++) {
		grf->md5sum[j] = p->Recv_uint8();
	}
}

void NetworkSocketHandler::Send_CompanyInformation(Packet *p, const Company *c, const NetworkCompanyStats *stats)
{
	/* Grab the company name */
	char company_name[NETWORK_COMPANY_NAME_LENGTH];
	SetDParam(0, c->index);
	GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

	/* Get the income */
	Money income = 0;
	if (_cur_year - 1 == c->inaugurated_year) {
		/* The company is here just 1 year, so display [2], else display[1] */
		for (uint i = 0; i < lengthof(c->yearly_expenses[2]); i++) {
			income -= c->yearly_expenses[2][i];
		}
	} else {
		for (uint i = 0; i < lengthof(c->yearly_expenses[1]); i++) {
			income -= c->yearly_expenses[1][i];
		}
	}

	/* Send the information */
	p->Send_uint8 (c->index);
	p->Send_string(company_name);
	p->Send_uint32(c->inaugurated_year);
	p->Send_uint64(c->old_economy[0].company_value);
	p->Send_uint64(c->money);
	p->Send_uint64(income);
	p->Send_uint16(c->old_economy[0].performance_history);

	/* Send 1 if there is a passord for the company else send 0 */
	p->Send_bool  (!StrEmpty(_network_company_states[c->index].password));

	for (int i = 0; i < NETWORK_VEHICLE_TYPES; i++) {
		p->Send_uint16(stats->num_vehicle[i]);
	}

	for (int i = 0; i < NETWORK_STATION_TYPES; i++) {
		p->Send_uint16(stats->num_station[i]);
	}
}

#endif /* ENABLE_NETWORK */
