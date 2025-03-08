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

#ifdef _WIN32
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast) // Win32 implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return;

	std::vector<INTERFACE_INFO> ifos;
	ifos.resize(2);

	for (;;) {
		DWORD len = 0;
		if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, nullptr, 0, ifos.data(), static_cast<DWORD>(ifos.size() * sizeof(INTERFACE_INFO)), &len, nullptr, nullptr) == 0) {
			ifos.resize(len / sizeof(INTERFACE_INFO));
			break;
		}
		if (WSAGetLastError() != WSAEFAULT) {
			closesocket(sock);
			return;
		}
		ifos.resize(ifos.size() * 2);
	}

	for (auto &ifo : ifos) {
		if (ifo.iiFlags & IFF_LOOPBACK) continue;
		if (!(ifo.iiFlags & IFF_BROADCAST)) continue;

		sockaddr_storage address{};
		/* iiBroadcast is unusable, because it always seems to be set to 255.255.255.255. */
		memcpy(&address, &ifo.iiAddress.Address, sizeof(sockaddr));
		reinterpret_cast<sockaddr_in*>(&address)->sin_addr.s_addr = ifo.iiAddress.AddressIn.sin_addr.s_addr | ~ifo.iiNetmask.AddressIn.sin_addr.s_addr;
		NetworkAddress addr(address, sizeof(sockaddr));
		if (std::none_of(broadcast->begin(), broadcast->end(), [&addr](NetworkAddress const &elem) -> bool { return elem == addr; })) broadcast->push_back(addr);
	}

	closesocket(sock);
}

#else /* not WIN32 */
static void NetworkFindBroadcastIPsInternal(NetworkAddressList *broadcast)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) return;

	for (ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
		if (ifa->ifa_broadaddr == nullptr) continue;
		if (ifa->ifa_broadaddr->sa_family != AF_INET) continue;

		NetworkAddress addr(ifa->ifa_broadaddr, sizeof(sockaddr));
		if (std::none_of(broadcast->begin(), broadcast->end(), [&addr](NetworkAddress const &elem) -> bool { return elem == addr; })) broadcast->push_back(std::move(addr));
	}
	freeifaddrs(ifap);
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
