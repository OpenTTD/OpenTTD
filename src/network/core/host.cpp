/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file host.cpp Functions related to getting host specific data (IPs). */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "address.h"

#include "../../safeguards.h"

/**
 * Internal implementation for finding the broadcast IPs.
 * This function is implemented multiple times for multiple targets.
 * @param broadcast the list of broadcasts to write into.
 */
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast);

#if defined(BEOS_NET_SERVER) || defined(__HAIKU__) /* doesn't have neither getifaddrs or net/if.h */
/* Based on Andrew Bachmann's netstat+.c. Big thanks to him! */
extern "C" int _netstat(int fd, char **output, int verbose);

int seek_past_header(char **pos, const char *header)
{
	char *new_pos = strstr(*pos, header);
	if (new_pos == 0) {
		return B_ERROR;
	}
	*pos += strlen(header) + new_pos - *pos + 1;
	return B_OK;
}

static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // BEOS implementation
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) {
		DEBUG(net, 0, "[core] error creating socket");
		return;
	}

	char *output_pointer = NULL;
	int output_length = _netstat(sock, &output_pointer, 1);
	if (output_length < 0) {
		DEBUG(net, 0, "[core] error running _netstat");
		return;
	}

	char **output = &output_pointer;
	if (seek_past_header(output, "IP Interfaces:") == B_OK) {
		for (;;) {
			uint32 n;
			int fields, read;
			uint8 i1, i2, i3, i4, j1, j2, j3, j4;
			uint32 ip;
			uint32 netmask;

			fields = sscanf(*output, "%u: %hhu.%hhu.%hhu.%hhu, netmask %hhu.%hhu.%hhu.%hhu%n",
												&n, &i1, &i2, &i3, &i4, &j1, &j2, &j3, &j4, &read);
			read += 1;
			if (fields != 9) {
				break;
			}

			ip      = (uint32)i1 << 24 | (uint32)i2 << 16 | (uint32)i3 << 8 | (uint32)i4;
			netmask = (uint32)j1 << 24 | (uint32)j2 << 16 | (uint32)j3 << 8 | (uint32)j4;

			if (ip != INADDR_LOOPBACK && ip != INADDR_ANY) {
				sockaddr_storage address;
				memset(&address, 0, sizeof(address));
				((sockaddr_in*)&address)->sin_addr.s_addr = htonl(ip | ~netmask);
				NetworkAddress addr(address, sizeof(sockaddr));
				if (!broadcast->Contains(addr)) *broadcast->Append() = addr;
			}
			if (read < 0) {
				break;
			}
			*output += read;
		}
		closesocket(sock);
	}
}

#elif defined(HAVE_GETIFADDRS)
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // GETIFADDRS implementation
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) return;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
		if (ifa->ifa_broadaddr == NULL) continue;
		if (ifa->ifa_broadaddr->sa_family != AF_INET) continue;

		NetworkAddress addr(ifa->ifa_broadaddr, sizeof(sockaddr));
		if (!broadcast->Contains(addr)) *broadcast->Append() = addr;
	}
	freeifaddrs(ifap);
}

#elif defined(WIN32)
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // Win32 implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return;

	DWORD len = 0;
	int num = 2;
	INTERFACE_INFO *ifo = CallocT<INTERFACE_INFO>(num);

	for (;;) {
		if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, NULL, 0, ifo, num * sizeof(*ifo), &len, NULL, NULL) == 0) break;
		free(ifo);
		if (WSAGetLastError() != WSAEFAULT) {
			closesocket(sock);
			return;
		}
		num *= 2;
		ifo = CallocT<INTERFACE_INFO>(num);
	}

	for (uint j = 0; j < len / sizeof(*ifo); j++) {
		if (ifo[j].iiFlags & IFF_LOOPBACK) continue;
		if (!(ifo[j].iiFlags & IFF_BROADCAST)) continue;

		sockaddr_storage address;
		memset(&address, 0, sizeof(address));
		/* iiBroadcast is unusable, because it always seems to be set to 255.255.255.255. */
		memcpy(&address, &ifo[j].iiAddress.Address, sizeof(sockaddr));
		((sockaddr_in*)&address)->sin_addr.s_addr = ifo[j].iiAddress.AddressIn.sin_addr.s_addr | ~ifo[j].iiNetmask.AddressIn.sin_addr.s_addr;
		NetworkAddress addr(address, sizeof(sockaddr));
		if (!broadcast->Contains(addr)) *broadcast->Append() = addr;
	}

	free(ifo);
	closesocket(sock);
}

#else /* not HAVE_GETIFADDRS */

#include "../../string_func.h"

static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // !GETIFADDRS implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return;

	char buf[4 * 1024]; // Arbitrary buffer size
	struct ifconf ifconf;

	ifconf.ifc_len = sizeof(buf);
	ifconf.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifconf) == -1) {
		closesocket(sock);
		return;
	}

	const char *buf_end = buf + ifconf.ifc_len;
	for (const char *p = buf; p < buf_end;) {
		const struct ifreq *req = (const struct ifreq*)p;

		if (req->ifr_addr.sa_family == AF_INET) {
			struct ifreq r;

			strecpy(r.ifr_name, req->ifr_name, lastof(r.ifr_name));
			if (ioctl(sock, SIOCGIFFLAGS, &r) != -1 &&
					(r.ifr_flags & IFF_BROADCAST) &&
					ioctl(sock, SIOCGIFBRDADDR, &r) != -1) {
				NetworkAddress addr(&r.ifr_broadaddr, sizeof(sockaddr));
				if (!broadcast->Contains(addr)) *broadcast->Append() = addr;
			}
		}

		p += sizeof(struct ifreq);
#if defined(AF_LINK) && !defined(SUNOS)
		p += req->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif
	}

	closesocket(sock);
}
#endif /* all NetworkFindBroadcastIPsInternals */

/**
 * Find the IPv4 broadcast addresses; IPv6 uses a completely different
 * strategy for broadcasting.
 * @param broadcast the list of broadcasts to write into.
 */
void NetworkFindBroadcastIPs(NetworkAddressList *broadcast)
{
	NetworkFindBroadcastIPsInternal(broadcast);

	/* Now display to the debug all the detected ips */
	DEBUG(net, 3, "Detected broadcast addresses:");
	int i = 0;
	for (NetworkAddress *addr = broadcast->Begin(); addr != broadcast->End(); addr++) {
		addr->SetPort(NETWORK_DEFAULT_PORT);
		DEBUG(net, 3, "%d) %s", i++, addr->GetHostname());
	}
}

#endif /* ENABLE_NETWORK */
