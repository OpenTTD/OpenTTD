/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file host.cpp Functions related to getting host specific data (IPs). */

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

#if defined(HAVE_GETIFADDRS)
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // GETIFADDRS implementation
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) return;

	for (ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
		if (ifa->ifa_broadaddr == nullptr) continue;
		if (ifa->ifa_broadaddr->sa_family != AF_INET) continue;

		NetworkAddress addr(ifa->ifa_broadaddr, sizeof(sockaddr));
		if (std::none_of(broadcast->begin(), broadcast->end(), [&addr](NetworkAddress const& elem) -> bool { return elem == addr; })) broadcast->push_back(addr);
	}
	freeifaddrs(ifap);
}

#elif defined(_WIN32)
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // Win32 implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return;

	DWORD len = 0;
	int num = 2;
	INTERFACE_INFO *ifo = CallocT<INTERFACE_INFO>(num);

	for (;;) {
		if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, nullptr, 0, ifo, num * sizeof(*ifo), &len, nullptr, nullptr) == 0) break;
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
		if (std::none_of(broadcast->begin(), broadcast->end(), [&addr](NetworkAddress const& elem) -> bool { return elem == addr; })) broadcast->push_back(addr);
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
				if (std::none_of(broadcast->begin(), broadcast->end(), [&addr](NetworkAddress const& elem) -> bool { return elem == addr; })) broadcast->push_back(addr);
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
	Debug(net, 3, "Detected broadcast addresses:");
	int i = 0;
	for (NetworkAddress &addr : *broadcast) {
		addr.SetPort(NETWORK_DEFAULT_PORT);
		Debug(net, 3, "  {}) {}", i++, addr.GetHostname());
	}
}
