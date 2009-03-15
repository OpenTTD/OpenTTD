/* $Id$ */

/** @file host.cpp Functions related to getting host specific data (IPs). */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "os_abstraction.h"
#include "../../core/alloc_func.hpp"

/**
 * Internal implementation for finding the broadcast IPs.
 * This function is implemented multiple times for multiple targets.
 * @param broadcast the list of broadcasts to write into.
 * @param limit     the maximum number of items to add.
 */
static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit);

#if defined(PSP)
static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit) // PSP implementation
{
	return 0;
}

#elif defined(BEOS_NET_SERVER) /* doesn't have neither getifaddrs or net/if.h */
/* Based on Andrew Bachmann's netstat+.c. Big thanks to him! */
int _netstat(int fd, char **output, int verbose);

int seek_past_header(char **pos, const char *header)
{
	char *new_pos = strstr(*pos, header);
	if (new_pos == 0) {
		return B_ERROR;
	}
	*pos += strlen(header) + new_pos - *pos + 1;
	return B_OK;
}

static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit) // BEOS implementation
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) {
		DEBUG(net, 0, "[core] error creating socket");
		return 0;
	}

	char *output_pointer = NULL;
	int output_length = _netstat(sock, &output_pointer, 1);
	if (output_length < 0) {
		DEBUG(net, 0, "[core] error running _netstat");
		return 0;
	}

	int index;
	char **output = &output_pointer;
	if (seek_past_header(output, "IP Interfaces:") == B_OK) {
		while (index != limit) {

			uint32 n, fields, read;
			uint8 i1, i2, i3, i4, j1, j2, j3, j4;
			struct in_addr inaddr;
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
				inaddr.s_addr = htonl(ip | ~netmask);
				broadcast[index] = inaddr.s_addr;
				index++;
			}
			if (read < 0) {
				break;
			}
			*output += read;
		}
		closesocket(sock);
	}

	return index;
}

#elif defined(HAVE_GETIFADDRS)
static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit) // GETIFADDRS implementation
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) return 0;

	int index = 0;
	for (ifa = ifap; ifa != NULL && index != limit; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
		if (ifa->ifa_broadaddr == NULL) continue;
		if (ifa->ifa_broadaddr->sa_family != AF_INET) continue;

		broadcast[index] = ((struct sockaddr_in*)ifa->ifa_broadaddr)->sin_addr.s_addr;
		index++;
	}
	freeifaddrs(ifap);

	return index;
}

#elif defined(WIN32)
static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit) // Win32 implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return 0;

	DWORD len = 0;
	INTERFACE_INFO *ifo = AllocaM(INTERFACE_INFO, limit);
	memset(ifo, 0, limit * sizeof(*ifo));
	if ((WSAIoctl(sock, SIO_GET_INTERFACE_LIST, NULL, 0, &ifo[0], limit * sizeof(*ifo), &len, NULL, NULL)) != 0) {
		closesocket(sock);
		return 0;
	}

	int index = 0;
	for (uint j = 0; j < len / sizeof(*ifo) && index != limit; j++) {
		if (ifo[j].iiFlags & IFF_LOOPBACK) continue;
		if (!(ifo[j].iiFlags & IFF_BROADCAST)) continue;

		/* iiBroadcast is unusable, because it always seems to be set to 255.255.255.255. */
		broadcast[index++] = ifo[j].iiAddress.AddressIn.sin_addr.s_addr | ~ifo[j].iiNetmask.AddressIn.sin_addr.s_addr;
	}

	closesocket(sock);
	return index;
}

#else /* not HAVE_GETIFADDRS */

#include "../../string_func.h"

static int NetworkFindBroadcastIPsInternal(uint32 *broadcast, int limit) // !GETIFADDRS implementation
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return 0;

	char buf[4 * 1024]; // Arbitrary buffer size
	struct ifconf ifconf;

	ifconf.ifc_len = sizeof(buf);
	ifconf.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifconf) == -1) {
		closesocket(sock);
		return 0;
	}

	const char *buf_end = buf + ifconf.ifc_len;
	int index = 0;
	for (const char *p = buf; p < buf_end && index != limit;) {
		const struct ifreq *req = (const struct ifreq*)p;

		if (req->ifr_addr.sa_family == AF_INET) {
			struct ifreq r;

			strecpy(r.ifr_name, req->ifr_name, lastof(r.ifr_name));
			if (ioctl(sock, SIOCGIFFLAGS, &r) != -1 &&
					r.ifr_flags & IFF_BROADCAST &&
					ioctl(sock, SIOCGIFBRDADDR, &r) != -1) {
				broadcast[index++] = ((struct sockaddr_in*)&r.ifr_broadaddr)->sin_addr.s_addr;
			}
		}

		p += sizeof(struct ifreq);
#if defined(AF_LINK) && !defined(SUNOS)
		p += req->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif
	}

	closesocket(sock);

	return index;
}
#endif /* all NetworkFindBroadcastIPsInternals */

/**
 * Find the IPs to broadcast.
 * @param broadcast the list of broadcasts to write into.
 * @param limit     the maximum number of items to add.
 */
void NetworkFindBroadcastIPs(uint32 *broadcast, int limit)
{
	int count = NetworkFindBroadcastIPsInternal(broadcast, limit);

	/* Make sure the list is terminated. */
	broadcast[count] = 0;

	/* Now display to the debug all the detected ips */
	DEBUG(net, 3, "Detected broadcast addresses:");
	for (int i = 0; broadcast[i] != 0; i++) {
		DEBUG(net, 3, "%d) %s", i, inet_ntoa(*(struct in_addr *)&broadcast[i])); // inet_ntoa(inaddr));
	}
}


/**
 * Resolve a hostname to an ip.
 * @param hsotname the hostname to resolve
 * @return the IP belonging to that hostname, or 0 on failure.
 */
uint32 NetworkResolveHost(const char *hostname)
{
	/* Is this an IP address? */
	in_addr_t ip = inet_addr(hostname);

	if (ip != INADDR_NONE) return ip;

	/* No, try to resolve the name */
	struct in_addr addr;
#if !defined(PSP)
	struct hostent *he = gethostbyname(hostname);
	if (he == NULL) {
		DEBUG(net, 0, "[NET] Cannot resolve %s", hostname);
		return 0;
	}
	addr = *(struct in_addr *)he->h_addr_list[0];
#else
	int rid = -1;
	char buf[1024];

	/* Create a resolver */
	if (sceNetResolverCreate(&rid, buf, sizeof(buf)) < 0) {
		DEBUG(net, 0, "[NET] Error connecting resolver");
		return 0;
	}

	/* Try to resolve the name */
	if (sceNetResolverStartNtoA(rid, hostname, &addr, 2, 3) < 0) {
		DEBUG(net, 0, "[NET] Cannot resolve %s", hostname);
		sceNetResolverDelete(rid);
		return 0;
	}
	sceNetResolverDelete(rid);
#endif /* PSP */

	DEBUG(net, 1, "[NET] Resolved %s to %s", hostname, inet_ntoa(addr));
	ip = addr.s_addr;
	return ip;
}

#endif /* ENABLE_NETWORK */
