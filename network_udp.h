#ifndef NETWORK_LAN_H
#define NETWORK_LAN_H

void NetworkUDPInitialize(void);
bool NetworkUDPListen(uint32 host, uint16 port);
void NetworkUDPReceive(void);
void NetworkUDPSearchGame(void);
void NetworkUDPQueryServer(const byte* host, unsigned short port);

#endif /* NETWORK_LAN_H */
