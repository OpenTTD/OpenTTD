/* $Id$ */

/**
 * @file host.h Resolving of hostnames/IPs
 */

#ifndef NETWORK_CORE_HOST_H

void NetworkFindBroadcastIPs(uint32 *broadcast, int limit);
uint32 NetworkResolveHost(const char *hostname);

#endif /* NETWORK_CORE_HOST_H */
