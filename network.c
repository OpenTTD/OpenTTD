#include "stdafx.h"
#include "ttd.h"
#include "gui.h"
#include "command.h"
#include "player.h"
#include "console.h"

#if defined(WIN32)
#	include <windows.h>
#	include <winsock.h>

# pragma comment (lib, "ws2_32.lib")
# define ENABLE_NETWORK
# define GET_LAST_ERROR() WSAGetLastError()
# define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#if defined(UNIX)
// Make compatible with WIN32 names
#	define SOCKET int
#	define INVALID_SOCKET -1
// we need different defines for MorphOS and AmigaOS
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
#	define ioctlsocket ioctl
# define closesocket close
# define GET_LAST_ERROR() errno
#endif
// Need this for FIONREAD on solaris
#	define BSD_COMP
#	include <unistd.h>
#	include <sys/ioctl.h>

// Socket stuff
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
# 	include <errno.h>
# 	include <sys/time.h>
// NetDB
#   include <netdb.h>

# ifndef TCP_NODELAY
#  define TCP_NODELAY 0x0001
# endif

#endif


#if defined(__MORPHOS__) || defined(__AMIGA__)
#	include <exec/types.h>
#	include <proto/exec.h> 		// required for Open/CloseLibrary()
#	if defined(__MORPHOS__)
#		include <sys/filio.h> 	// FION#? defines
#	else // __AMIGA__
#		include	<proto/socket.h>
#	endif

// make source compatible with bsdsocket.library functions
# define closesocket(s)     						CloseSocket(s)
# define GET_LAST_ERROR() 							Errno()
#	define ioctlsocket(s,request,status)  IoctlSocket((LONG)s,(ULONG)request,(char*)status)

struct Library *SocketBase = NULL;

#if !defined(__MORPHOS__)
// usleep() implementation
#include <devices/timer.h>
#include <dos/dos.h>

struct Device       *TimerBase    = NULL;
struct MsgPort      *TimerPort    = NULL;
struct timerequest  *TimerRequest = NULL;
#endif

#endif /* __MORPHOS__ || __AMIGA__ */


#define SEND_MTU 1460

#if defined(ENABLE_NETWORK)

// sent from client -> server whenever the client wants to exec a command.
// send from server -> client when another player execs a command.
typedef struct CommandPacket {
	byte packet_length;
	byte packet_type;
	uint16 cmd;
	uint32 p1,p2;
	TileIndex tile;
	byte player;// player id, this is checked by the server.
	byte when;  // offset from the current max_frame value minus 1. this is set by the server.
	uint32 dp[8];
} CommandPacket;

assert_compile(sizeof(CommandPacket) == 16 + 32);

#define COMMAND_PACKET_BASE_SIZE (sizeof(CommandPacket) - 8 * sizeof(uint32))

// sent from server -> client periodically to tell the client about the current tick in the server
// and how far the client may progress.
typedef struct SyncPacket {
	byte packet_length;
	byte packet_type;
	byte frames; // how many more frames may the client execute? this is relative to the old value of max.
	byte server; // where is the server currently executing? this is negatively relative to the old value of max.
	uint32 random_seed_1; // current random state at server. used to detect out of sync.
	uint32 random_seed_2;
} SyncPacket;

assert_compile(sizeof(SyncPacket) == 12);

// sent from server -> client as an acknowledgement that the server received the command.
// the command will be executed at the current value of "max".
typedef struct AckPacket {
	byte packet_length;
	byte packet_type;
} AckPacket;

typedef struct FilePacketHdr {
	byte packet_length;
	byte packet_type;
	byte unused[2];
} FilePacketHdr;

assert_compile(sizeof(FilePacketHdr) == 4);

// sent from server to client when the client has joined.
typedef struct WelcomePacket {
	byte packet_length;
	byte packet_type;
	byte unused[2];
} WelcomePacket;

typedef struct Packet Packet;
struct Packet {
	Packet *next; // this one has to be the first element.
	uint siz;
	byte buf[SEND_MTU]; // packet payload
};

typedef struct ClientState {
	int socket;
	bool inactive; // disable sending of commands/syncs to client
	bool writable;
	uint xmitpos;

	uint eaten;
	Packet *head, **last;

	uint buflen;											// receive buffer len
	byte buf[256];										// receive buffer
} ClientState;


static uint _not_packet;

typedef struct QueuedCommand QueuedCommand;
struct QueuedCommand {
	QueuedCommand *next;
	CommandPacket cp;
	CommandCallback *callback;
	uint32 cmd;
	int32 frame;
};

typedef struct CommandQueue CommandQueue;
struct CommandQueue {
	QueuedCommand *head, **last;
};

#define MAX_CLIENTS (MAX_PLAYERS + 1)

// packets waiting to be executed, for each of the players.
// this list is sorted in frame order, so the item on the front will be executed first.
static CommandQueue _command_queue;

// in the client, this is the list of commands that have not yet been acked.
// when it is acked, it will be moved to the appropriate position at the end of the player queue.
static CommandQueue _ack_queue;

static ClientState _clients[MAX_CLIENTS];
static int _num_clients;

// keep a history of the 16 most recent seeds to be able to capture out of sync errors.
static uint32 _my_seed_list[16][2];

typedef struct FutureSeeds {
	int32 frame;
	uint32 seed[2];
} FutureSeeds;

// remember some future seeds that the server sent to us.
static FutureSeeds _future_seed[8];
static uint _num_future_seed;

static SOCKET _listensocket; // tcp socket

static SOCKET _udp_client_socket; // udp server socket
static SOCKET _udp_server_socket; // udp client socket

typedef struct UDPPacket {
	byte command_code;
	byte data_len;
	byte command_check;
	byte data[255];
} UDPPacket;

enum {
	NET_UDPCMD_SERVERSEARCH = 1,
	NET_UDPCMD_SERVERACTIVE,
	NET_UDPCMD_GETSERVERINFO,
	NET_UDPCMD_SERVERINFO,
};

void NetworkUDPSend(bool client, struct sockaddr_in recv,struct UDPPacket packet);

uint32 _network_ip_list[10]; // network ip list

// this is set to point to the savegame
static byte *_transmit_file;
static size_t _transmit_file_size;

static FILE *_recv_file;

typedef struct NetworkGameInfo {	
	char server_name[40];			// name of the game
	char server_revision[8];	// server game version
	byte server_lang;					// langid
	byte players_max;					// max players allowed on server
	byte players_on;					// current count of players on server
	uint16 game_date;					// current date
	char game_password[10];		// should fit ... 10 chars
	char map_name[40];				// map which is played ["random" for a randomized map]
	uint map_width;						// map width / 8
	uint map_height;					// map height / 8
	byte map_set;							// graphical set
} NetworkGameInfo;

typedef struct NetworkGameList {
	NetworkGameInfo item;
	uint32 ip;
	uint16 port;
	char * _next;
} NetworkGameList;

static NetworkGameInfo _network_game;
static NetworkGameList * _network_game_list = NULL;

/* multi os compatible sleep function */
void CSleep(int milliseconds) {
#if defined(WIN32)
Sleep(milliseconds);
#endif
#if defined(UNIX)
#if !defined(__BEOS__) && !defined(__MORPHOS__) && !defined(__AMIGAOS__) 
usleep(milliseconds*1000);
#endif
#ifdef __BEOS__
snooze(milliseconds*1000);
#endif
#if defined(__MORPHOS__) 
usleep(milliseconds*1000);
#endif
#if defined(__AMIGAOS__) && !defined(__MORPHOS__) 
{ 
	ULONG signals;
	ULONG TimerSigBit = 1 << TimerPort->mp_SigBit;

	// send IORequest
	TimerRequest->tr_node.io_Command = TR_ADDREQUEST;
	TimerRequest->tr_time.tv_secs    = (milliseconds * 1000) / 1000000;
	TimerRequest->tr_time.tv_micro   = (milliseconds * 1000) % 1000000;
	SendIO((struct IORequest *)TimerRequest);

	if ( !((signals = Wait(TimerSigBit|SIGBREAKF_CTRL_C)) & TimerSigBit) ) {
		AbortIO((struct IORequest *)TimerRequest);
	}
	WaitIO((struct IORequest *)TimerRequest);
}
#endif // __AMIGAOS__ && !__MORPHOS__
#endif
}

//////////////////////////////////////////////////////////////////////

// ****************************** //
// * Network Error Handlers     * //
// ****************************** //

static void NetworkHandleSaveGameError() {
		_networking_sync = false;
		_networking_queuing = true;
		_switch_mode = SM_MENU;
		_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
}

static void NetworkHandleConnectionLost() {
		_networking_sync = false;
		_networking_queuing = true;
		_switch_mode = SM_MENU;
		_switch_mode_errorstr = STR_NETWORK_ERR_LOSTCONNECTION;
}
static void NetworkHandleDeSync() {
	printf("fatal error: network sync error at frame %i\n",_frame_counter);
		{
			int i;
			for (i=15; i>=0; i--) printf("frame %i: [0]=%i, [1]=%i\n",_frame_counter-(i+1),_my_seed_list[i][0],_my_seed_list[i][1]);
			for (i=0; i<8; i++) printf("frame %i: [0]=%i, [1]=%i\n",_frame_counter+i,_future_seed[i].seed[0],_future_seed[i].seed[1]);
		}
		_networking_sync = false;
		_networking_queuing = true;
		_switch_mode = SM_MENU;
		_switch_mode_errorstr = STR_NETWORK_ERR_DESYNC;
}

// ****************************** //
// * TCP Packets and Handlers   * //
// ****************************** //

static QueuedCommand *AllocQueuedCommand(CommandQueue *nq)
{
	QueuedCommand *qp = (QueuedCommand*)calloc(sizeof(QueuedCommand), 1);
	assert(qp);
	*nq->last = qp;
	nq->last = &qp->next;
	return qp;
}

// go through the player queues for each player and see if there are any pending commands
// that should be executed this frame. if there are, execute them.
void NetworkProcessCommands()
{
	CommandQueue *nq;
	QueuedCommand *qp;

	// queue mode ?
	if (_networking_queuing)
		return;

	nq = &_command_queue;
	while ( (qp=nq->head) && (!_networking_sync || qp->frame <= _frame_counter)) {
		// unlink it.
		if (!(nq->head = qp->next)) nq->last = &nq->head;

		if (qp->frame < _frame_counter && _networking_sync) {
			error("!qp->cp.frame < _frame_counter, %d < %d\n", qp->frame, _frame_counter);
		}

		// run the command
		_current_player = qp->cp.player;
		memcpy(_decode_parameters, qp->cp.dp, (qp->cp.packet_length - COMMAND_PACKET_BASE_SIZE));
		DoCommandP(qp->cp.tile, qp->cp.p1, qp->cp.p2, qp->callback, qp->cmd | CMD_DONT_NETWORK);
		free(qp);
	}

	if (!_networking_server) {
		// remember the random seed so we can check if we're out of sync.
		_my_seed_list[_frame_counter & 15][0] = _sync_seed_1;
		_my_seed_list[_frame_counter & 15][1] = _sync_seed_2;

		while (_num_future_seed) {
			assert(_future_seed[0].frame >= _frame_counter);
			if (_future_seed[0].frame != _frame_counter) break;
			if (_future_seed[0].seed[0] != _sync_seed_1 ||_future_seed[0].seed[1] != _sync_seed_2) NetworkHandleDeSync();
			memcpy_overlapping(_future_seed, _future_seed + 1, --_num_future_seed * sizeof(FutureSeeds));
		}
	}
}

// send a packet to a client
static void SendBytes(ClientState *cs, void *bytes, uint len)
{
	byte *b = (byte*)bytes;
	uint n;
	Packet *p;

	assert(len != 0);

	// see if there's space in the last packet?
	if (!cs->head || (p = (Packet*)cs->last, p->siz == sizeof(p->buf)))
		p = NULL;

	do {
		if (!p) {
			// need to allocate a new packet buffer.
			p = (Packet*)malloc(sizeof(Packet));

			// insert at the end of the linked list.
			*cs->last = p;
			cs->last = &p->next;
			p->next = NULL;
			p->siz = 0;
		}

		// copy bytes to packet.
		n = minu(sizeof(p->buf) - p->siz, len);
		memcpy(p->buf + p->siz, b, n);
		p->siz += n;
		b += n;
		p = NULL;
	} while (len -= n);
}

// client:
//   add it to the client's ack queue, and send the command to the server
// server:
//   add it to the server's player queue, and send it to all clients.
void NetworkSendCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback)
{
	int nump;
	QueuedCommand *qp;
	ClientState *cs;

	qp = AllocQueuedCommand(_networking_server ? &_command_queue : &_ack_queue);
	qp->cp.packet_type = 0;
	qp->cp.tile = tile;
	qp->cp.p1 = p1;
	qp->cp.p2 = p2;
	qp->cp.cmd = (uint16)cmd;
	qp->cp.player = _local_player;
	qp->cp.when = 0;
	qp->cmd = cmd;
	qp->callback = callback;

	// so the server knows when to execute it.
	qp->frame = _frame_counter_max;

	// calculate the amount of extra bytes.
	nump = 8;
	while ( nump != 0 && ((uint32*)_decode_parameters)[nump-1] == 0) nump--;
	qp->cp.packet_length = COMMAND_PACKET_BASE_SIZE + nump * sizeof(uint32);
	if (nump != 0) memcpy(qp->cp.dp, _decode_parameters, nump * sizeof(uint32));

#if defined(TTD_BIG_ENDIAN)
	// need to convert the command to little endian before sending it.
	{
		CommandPacket cp;
		cp = qp->cp;
		cp.cmd = TO_LE16(cp.cmd);
		cp.tile = TO_LE16(cp.tile);
		cp.p1 = TO_LE32(cp.p1);
		cp.p2 = TO_LE32(cp.p2);
		for(cs=_clients; cs->socket != INVALID_SOCKET; cs++) if (!cs->inactive) SendBytes(cs, &cp, cp.packet_length);
	}
#else
	// send it to the peers
	for(cs=_clients; cs->socket != INVALID_SOCKET; cs++) if (!cs->inactive) SendBytes(cs, &qp->cp, qp->cp.packet_length);

#endif
}

// client:
//   server sends a command from another player that we should execute.
//   put it in the command queue.
//
// server:
//   client sends a command that it wants to execute.
//   fill the when field so the client knows when to execute it.
//   put it in the appropriate player queue.
//   send it to all other clients.
//   send an ack packet to the actual client.

static void HandleCommandPacket(ClientState *cs, CommandPacket *np)
{
	QueuedCommand *qp;
	ClientState *c;
	AckPacket ap;

	printf("net: cmd size %d\n", np->packet_length);

	assert(np->packet_length >= COMMAND_PACKET_BASE_SIZE);

	// put it into the command queue
	qp = AllocQueuedCommand(&_command_queue);
	qp->cp = *np;

	qp->frame = _frame_counter_max;

	qp->callback = NULL;

	// extra params
	memcpy(&qp->cp.dp, np->dp, np->packet_length - COMMAND_PACKET_BASE_SIZE);

	ap.packet_type = 2;
	ap.packet_length = 2;

	// send it to the peers
	if (_networking_server) {
		for(c=_clients; c->socket != INVALID_SOCKET; c++) {
			if (c == cs) {
				SendBytes(c, &ap, 2);
			} else {
				if (!cs->inactive) SendBytes(c, &qp->cp, qp->cp.packet_length);
			}
		}
	}

// convert from little endian to big endian?
#if defined(TTD_BIG_ENDIAN)
	qp->cp.cmd = TO_LE16(qp->cp.cmd);
	qp->cp.tile = TO_LE16(qp->cp.tile);
	qp->cp.p1 = TO_LE32(qp->cp.p1);
	qp->cp.p2 = TO_LE32(qp->cp.p2);
#endif

	qp->cmd = qp->cp.cmd;
}

// sent from server -> client periodically to tell the client about the current tick in the server
// and how far the client may progress.
static void HandleSyncPacket(SyncPacket *sp)
{
	uint32 s1,s2;

	_frame_counter_srv = _frame_counter_max - sp->server;
	_frame_counter_max += sp->frames;

	printf("net: sync max=%d  cur=%d  server=%d\n", _frame_counter_max, _frame_counter, _frame_counter_srv);

	// queueing only?
	if (_networking_queuing || _frame_counter == 0)
		return;

	s1 = TO_LE32(sp->random_seed_1);
	s2 = TO_LE32(sp->random_seed_2);

	if (_frame_counter_srv <= _frame_counter) {
		// we are ahead of the server check if the seed is in our list.
		if (_frame_counter_srv + 16 > _frame_counter) {
			// the random seed exists in our array check it.
			if (s1 != _my_seed_list[_frame_counter_srv & 0xF][0] || s2 != _my_seed_list[_frame_counter_srv & 0xF][1]) NetworkHandleDeSync();
		}
	} else {
		// the server's frame has not been executed yet. store the server's seed in a list.
		if (_num_future_seed < lengthof(_future_seed)) {
			_future_seed[_num_future_seed].frame = _frame_counter_srv;
			_future_seed[_num_future_seed].seed[0] = s1;
			_future_seed[_num_future_seed].seed[1] = s2;
			_num_future_seed++;
		}
	}
}

// sent from server -> client as an acknowledgement that the server received the command.
// the command will be executed at the current value of "max".
static void HandleAckPacket()
{
	QueuedCommand *q;
	// move a packet from the ack queue to the end of this player's queue.
	q = _ack_queue.head;
	assert(q);
	if (!(_ack_queue.head = q->next)) _ack_queue.last = &_ack_queue.head;
	q->next = NULL;
	q->frame = _frame_counter_max;

	*_command_queue.last = q;
	_command_queue.last = &q->next;

	printf("net: ack\n");
}

static void HandleFilePacket(FilePacketHdr *fp)
{
	int n = fp->packet_length - sizeof(FilePacketHdr);
	if (n == 0) {
		assert(_networking_queuing);
		assert(!_networking_sync);
		// eof
		if (_recv_file) { fclose(_recv_file); _recv_file = NULL; }

		// attempt loading the game.
		_game_mode = GM_NORMAL;
		if (SaveOrLoad("networkc.tmp", SL_LOAD) != SL_OK) {
				NetworkCoreDisconnect();
				NetworkHandleSaveGameError();
				return;
				}
		// sync to server.
		_networking_queuing = false;
		NetworkStartSync(false);

		if (_network_playas == 0) {
			// send a command to make a new player
			_local_player = 0;
			NetworkSendCommand(0, 0, 0, CMD_PLAYER_CTRL, NULL);
			_local_player = OWNER_SPECTATOR;
		} else {
			// take control over an existing company
			if (DEREF_PLAYER(_network_playas-1)->is_active)
				_local_player = _network_playas-1;
			else
				_local_player = OWNER_SPECTATOR;
		}

	} else {
		if(!_recv_file) {
			_recv_file = fopen("networkc.tmp", "wb");
			if (!_recv_file) error("can't open savefile");
		}
		fwrite( (char*)fp + sizeof(*fp), n, 1, _recv_file);
	}
}

static void CloseClient(ClientState *cs)
{
	Packet *p, *next;

	DEBUG(misc,1) ("[NET][TCP] closed client connection");

	assert(cs->socket != INVALID_SOCKET);

	closesocket(cs->socket);

	// free buffers
	for(p = cs->head; p; p=next) {
		next = p->next;
		free(p);
	}

	// copy up structs...
	while ((cs+1)->socket != INVALID_SOCKET) {
		*cs = *(cs+1);
		cs++;
	}
	cs->socket = INVALID_SOCKET;

	if (_networking_server) _network_game.players_on--;

	_num_clients--;
}

#define NETWORK_BUFFER_SIZE 4096
static bool ReadPackets(ClientState *cs)
{
	byte network_buffer[NETWORK_BUFFER_SIZE];
	uint pos,size;
	unsigned long recv_bytes;

	size = cs->buflen;

	for(;;) {
		if (size != 0) memcpy(network_buffer, cs->buf, size);

		recv_bytes = recv(cs->socket, (char*)network_buffer + size, sizeof(network_buffer) - size, 0);
		if ( recv_bytes == (unsigned long)-1) {
			int err = GET_LAST_ERROR();
			if (err == EWOULDBLOCK) break;
			printf("net: recv() failed with error %d\n", err);
			CloseClient(cs);
			return false;
		}
		// no more bytes for now?
		if (recv_bytes == 0)
			break;

		size += recv_bytes; // number of bytes read.
		pos = 0;
		while (size >= 2) {
			byte *packet = network_buffer + pos;
			// whole packet not there yet?
			if (size < packet[0]) break;
			size -= packet[0];
			pos += packet[0];

			switch(packet[1]) {
			case 0:
				HandleCommandPacket(cs, (CommandPacket*)packet);
				break;
			case 1:
				assert(_networking_sync || _networking_queuing);
				assert(!_networking_server);
				HandleSyncPacket((SyncPacket*)packet);
				break;
			case 2:
				assert(!_networking_server);
				HandleAckPacket();
				break;
			case 3:
				HandleFilePacket((FilePacketHdr*)packet);
				break;
			default:
				error("unknown packet type");
			}
		}

		assert(size < sizeof(cs->buf));

		memcpy(cs->buf, network_buffer + pos, size);
	}

	cs->buflen = size;

	return true;
}


static bool SendPackets(ClientState *cs)
{
	Packet *p;
	int n;
	uint nskip = cs->eaten, nsent = nskip;

	// try sending as much as possible.
	for(p=cs->head; p ;p = p->next) {
		if (p->siz) {
			assert(nskip < p->siz);

			n = send(cs->socket, p->buf + nskip, p->siz - nskip, 0);
			if (n == -1) {
				int err = GET_LAST_ERROR();
				if (err == EWOULDBLOCK) break;
				printf("net: send() failed with error %d\n", err);
				CloseClient(cs);
				return false;
			}
			nsent += n;
			// send was not able to send it all? then we assume that the os buffer is full and break.
			if (nskip + n != p->siz)
				break;
			nskip = 0;
		}
	}

	// nsent bytes in the linked list are not invalid. free as many buffers as possible.
	// don't actually free the last buffer.
	while (nsent) {
		p = cs->head;
		assert(p->siz != 0);

		// some bytes of the packet are still unsent.
		if ( (int)(nsent - p->siz) < 0)
			break;
		nsent -= p->siz;
		p->siz = 0;
		if (p->next) {
			cs->head = p->next;
			free(p);
		}
	}

	cs->eaten = nsent;

	return true;
}

// transmit the file..
static void SendXmit(ClientState *cs)
{
	uint pos, n;
	FilePacketHdr hdr;
	int p;

	// if too many unsent bytes left in buffer, don't send more.
	if (cs->head && cs->head->next)
		return;

	pos = cs->xmitpos - 1;

	p = 20;
	do {
		// compute size of data to xmit
		n = minu(_transmit_file_size - pos, 248);

		hdr.packet_length = n + sizeof(hdr);
		hdr.packet_type = 3;
		hdr.unused[0] = hdr.unused[1] = 0;
		SendBytes(cs, &hdr, sizeof(hdr));

		if (n == 0) {
			pos = -1; // eof
			break;
		}
		SendBytes(cs, _transmit_file + pos, n);
		pos += n;
	} while (--p);

	cs->xmitpos = pos + 1;

	printf("net: client xmit at %d\n", pos + 1);
}

static ClientState *AllocClient(SOCKET s)
{
	ClientState *cs;

	if (_num_clients == MAX_CLIENTS)
		return NULL;

	if (_networking_server) _network_game.players_on++;

	cs = &_clients[_num_clients++];
	memset(cs, 0, sizeof(*cs));
	cs->last = &cs->head;
	cs->socket = s;
	return cs;
}


static void NetworkAcceptClients()
{
	struct sockaddr_in sin;
	SOCKET s;
	ClientState *cs;
#ifndef __MORPHOS__
	int sin_len;
#else
	LONG sin_len; // for some reason we need a 'LONG' under MorphOS
#endif

	assert(_listensocket != INVALID_SOCKET);

	for(;;) {
		sin_len = sizeof(sin);
		s = accept(_listensocket, (struct sockaddr*)&sin, &sin_len);
		if (s == INVALID_SOCKET) return;

		// set nonblocking mode for client socket
		{ unsigned long blocking = 1; ioctlsocket(s, FIONBIO, &blocking); }

		printf("net: got client from %s\n", inet_ntoa(sin.sin_addr));

		// set nodelay
		{int b = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&b, sizeof(b));}

		cs = AllocClient(s);
		if (cs == NULL) {
			// no more clients allowed?
			closesocket(s);
			continue;
		}

		if (_networking_sync) {
			// a new client has connected. it needs a snapshot.
			cs->inactive = true;
		}
	}

	// when a new client has joined. it needs different information depending on if it's at the game menu or in an active game.
	// Game menu:
	//  - list of players already in the game (name, company name, face, color)
	//  - list of game settings and patch settings
	// Active game:
	//  - the state of the world (includes player name, company name, player face, player color)
	//  - list of the patch settings

	// Networking can be in several "states".
	//  * not sync - games don't need to be in sync, and frame counter is not synced. for example intro screen. all commands are executed immediately.
	//  * sync - games are in sync
}


static void SendQueuedCommandsToNewClient(ClientState *cs)
{
	// send the commands in the server queue to the new client.
	QueuedCommand *qp;
	SyncPacket sp;
	int32 frame;

	printf("net: sending queued commands to client\n");

	sp.packet_length = sizeof(sp);
	sp.packet_type = 1;
	sp.random_seed_1 = sp.random_seed_2 = 0;
	sp.server = 0;

	frame = _frame_counter;

	for(qp=_command_queue.head; qp; qp = qp->next) {
		printf("net: sending cmd to be executed at %d (old %d)\n", qp->frame, frame);
		if (qp->frame > frame) {
			assert(qp->frame <= _frame_counter_max);
			sp.frames = qp->frame - frame;
			frame = qp->frame;
			SendBytes(cs, &sp, sizeof(sp));
		}
		SendBytes(cs, &qp->cp, qp->cp.packet_length);
	}

	if (frame < _frame_counter_max) {
		printf("net: sending queued sync %d (%d)\n", _frame_counter_max, frame);
		sp.frames = _frame_counter_max - frame;
		SendBytes(cs, &sp, sizeof(sp));
	}

}

// ************************** //
// * TCP Networking         * //
// ************************** //

unsigned long NetworkResolveHost(const char *hostname) {
	struct hostent* remotehost;

	if ((hostname[0]<0x30) || (hostname[0]>0x39)) {
		// seems to be an hostname [first character is no number]
		remotehost = gethostbyname(hostname);
		if (remotehost == NULL) {
			DEBUG(misc, 2) ("[NET][IP] cannot resolve %s", hostname);
			return 0;
		} else {
			DEBUG(misc,2) ("[NET][IP] resolved %s to %s",hostname, inet_ntoa(*(struct in_addr *) remotehost->h_addr_list[0]));
			return inet_addr(inet_ntoa(*(struct in_addr *) remotehost->h_addr_list[0]));
			}
	} else {
		// seems to be an ip [first character is a number]
		return inet_addr(hostname);
		}

}

bool NetworkConnect(const char *hostname, int port)
{
	SOCKET s;
	struct sockaddr_in sin;
	int b;

	DEBUG(misc, 1) ("[NET][TCP] Connecting to %s %d", hostname, port);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) error("socket() failed");

	b = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&b, sizeof(b));
	
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = NetworkResolveHost(hostname);
	sin.sin_port = htons(port);

	if (connect(s, (struct sockaddr*) &sin, sizeof(sin)) != 0) {
		NetworkClose(true);
		return false;
		}

	// set nonblocking mode for socket..
	{ unsigned long blocking = 1; ioctlsocket(s, FIONBIO, &blocking); }

	// in client mode, only the first client field is used. it's pointing to the server.
	AllocClient(s);

	// queue packets.. because we're waiting for the savegame.
	_networking_queuing = true;
	_frame_counter_max = 0;

	return true;
}

void NetworkListen()
{
		
	SOCKET ls;
	struct sockaddr_in sin;
	int port;

	port = _network_server_port;

	DEBUG(misc, 1) ("[NET][TCP] listening on port %d", port);

	ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls == INVALID_SOCKET)
		error("socket() on listen socket failed");
	
	// reuse the socket
	{
		int reuse = 1; if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == -1)
			error("setsockopt() on listen socket failed");
	}

	// set nonblocking mode for socket
	{ unsigned long blocking = 1; ioctlsocket(ls, FIONBIO, &blocking); }

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);

	if (bind(ls, (struct sockaddr*)&sin, sizeof(sin)) != 0)
		error("bind() failed");

	if (listen(ls, 1) != 0)
		error("listen() failed");

	_listensocket = ls;
}

void NetworkReceive()
{
	ClientState *cs;
	int n;
	fd_set read_fd, write_fd;
	struct timeval tv;
	
	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	for(cs=_clients;cs->socket != INVALID_SOCKET; cs++) {
		FD_SET(cs->socket, &read_fd);
		FD_SET(cs->socket, &write_fd);
	}

	// take care of listener port
	if (_networking_server) {
		FD_SET(_listensocket, &read_fd);
	}

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	n = WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif
	if (n == -1) NetworkHandleConnectionLost();

	// accept clients..
	if (_networking_server && FD_ISSET(_listensocket, &read_fd))
		NetworkAcceptClients();

	// read stuff from clients
	for(cs=_clients;cs->socket != INVALID_SOCKET; cs++) {
		cs->writable = !!FD_ISSET(cs->socket, &write_fd);
		if (FD_ISSET(cs->socket, &read_fd)) {
			if (!ReadPackets(cs))
				cs--;
		}
	}

	// if we're a server, and any client needs a snapshot, create a snapshot and send all commands from the server queue to the client.
	if (_networking_server && _transmit_file == NULL) {
		bool didsave = false;

		for(cs=_clients;cs->socket != INVALID_SOCKET; cs++) {
			if (cs->inactive) {
				cs->inactive = false;
				// found a client waiting for a snapshot. make a snapshot.
				if (!didsave) {
					char filename[256];
					sprintf(filename, "%snetwork.tmp",  _path.autosave_dir);
					didsave = true;
					if (SaveOrLoad(filename, SL_SAVE) != SL_OK) error("network savedump failed");
					_transmit_file = ReadFileToMem(filename, &_transmit_file_size, 500000);
					if (_transmit_file == NULL) error("network savedump failed to load");
				}
				// and start sending the file..
				cs->xmitpos = 1;

				// send queue of commands to client.
				SendQueuedCommandsToNewClient(cs);
			}
		}
	}
}

void NetworkSend()
{
	ClientState *cs;
	void *free_xmit;

	// send sync packets?
	if (_networking_server && _networking_sync && !_pause) {
		if (++_not_packet >= _network_sync_freq) {
			SyncPacket sp;
			uint new_max;

			_not_packet = 0;

			new_max = max(_frame_counter + (int)_network_ahead_frames, _frame_counter_max);

			sp.packet_length = sizeof(sp);
			sp.packet_type = 1;
			sp.frames = new_max - _frame_counter_max;
			sp.server = _frame_counter_max - _frame_counter;
			sp.random_seed_1 = TO_LE32(_sync_seed_1);
			sp.random_seed_2 = TO_LE32(_sync_seed_2);
			_frame_counter_max = new_max;

			// send it to all the clients
			for(cs=_clients;cs->socket != INVALID_SOCKET; cs++) SendBytes(cs, &sp, sizeof(sp));
		}
	}

	free_xmit = _transmit_file;

	// send stuff to all clients
	for(cs=_clients;cs->socket != INVALID_SOCKET; cs++) {
		if (cs->xmitpos) {
			if (cs->writable)
				SendXmit(cs);
			free_xmit = NULL;
		}
		if (cs->writable)	{
			if (!SendPackets(cs)) cs--;
		}
	}

	// no clients left that xmit the file, free it.
	if (free_xmit) {
		_transmit_file = NULL;
		free(free_xmit);
	}
}


void NetworkInitialize()
{
	ClientState *cs;

	_command_queue.last = &_command_queue.head;
	_ack_queue.last = &_ack_queue.head;

	// invalidate all clients
	for(cs=_clients; cs != &_clients[MAX_CLIENTS]; cs++)
		cs->socket = INVALID_SOCKET;

}

void NetworkClose(bool client) {

	ClientState *cs;
	// invalidate all clients

	for(cs=_clients; cs != &_clients[MAX_CLIENTS]; cs++) if (cs->socket != INVALID_SOCKET) {
		CloseClient(cs);
		}

	if (!client) {
		// if in servermode --> close listener
		closesocket(_listensocket);
		_listensocket= INVALID_SOCKET;
		DEBUG(misc,1) ("[NET][TCP] closed listener on port %i", _network_server_port);
		}
}

void NetworkShutdown()
{
	_networking_server = false;	
	_networking = false;
	_networking_sync = false;
	_frame_counter = 0;
	_frame_counter_max = 0;
	_frame_counter_srv = 0;
}

// switch to synced mode.
void NetworkStartSync(bool fcreset)
{
	DEBUG(misc,3) ("[NET][SYNC] switching to synced game mode");
	_networking_sync = true;
	_frame_counter = 0;
	if (fcreset) {
		_frame_counter_max = 0;
		_frame_counter_srv = 0;
		}
	_num_future_seed = 0;
	_sync_seed_1 = _sync_seed_2 = 0;
	memset(_my_seed_list, 0, sizeof(_my_seed_list));

}

// ********************************* //
// * Network Core Console Commands * //
// ********************************* //

static _iconsole_var * NetworkConsoleCmdConnect(byte argc, byte* argv[], byte argt[]) {
	if (argc<2) return NULL;
	if (argc==2) {
		IConsolePrintF(_iconsole_color_default, "connecting to %s",argv[1]);
		NetworkCoreConnectGame(argv[1],_network_server_port);
	} else if (argc==3) {
		IConsolePrintF(_iconsole_color_default, "connecting to %s on port %s",argv[1],argv[2]);
		NetworkCoreConnectGame(argv[1],atoi(argv[2]));
		}
	return NULL;
}

// ************************** //
// * UDP Network Extensions * //
// ************************** //

void NetworkUDPListen(bool client)
{
	SOCKET udp;
	struct sockaddr_in sin;
	int port;

	if (client) { port = _network_client_port; } else { port = _network_server_port; };

	DEBUG(misc,1) ("[NET][UDP] listening on port %i", port);

	udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	// this disables network
	_network_available = !(udp == INVALID_SOCKET);
	
	// set nonblocking mode for socket
	{ unsigned long blocking = 1; ioctlsocket(udp, FIONBIO, &blocking); }

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);

	if (bind(udp, (struct sockaddr*)&sin, sizeof(sin)) != 0)
		DEBUG(misc,1) ("[NET][UDP] error: bind failed on port %i", port);
		

	// enable broadcasting
	{ unsigned long val=1; setsockopt(udp, SOL_SOCKET, SO_BROADCAST, (char *) &val , sizeof(val)); }
	// allow reusing
	{ unsigned long val=1; setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (char *) &val , sizeof(val)); }

	if (client) { _udp_client_socket = udp; } else { _udp_server_socket = udp; } ;

}

void NetworkUDPClose(bool client) {
	if (client) { 
		DEBUG(misc,1) ("[NET][UDP] closed listener on port %i", _network_client_port);
		closesocket(_udp_client_socket);
		_udp_client_socket = INVALID_SOCKET;
		} else {
		DEBUG(misc,1) ("[NET][UDP] closed listener on port %i", _network_server_port);
		closesocket(_udp_server_socket);
		_udp_server_socket = INVALID_SOCKET;
		};
	}

void NetworkUDPReceive(bool client) {
	struct sockaddr_in client_addr;
#ifndef __MORPHOS__
	int client_len;
#else
	LONG client_len; // for some reason we need a 'LONG' under MorphOS
#endif
	int nbytes;
	struct UDPPacket packet;
	int packet_len;
	
	SOCKET udp;
	if (client) udp=_udp_client_socket; else udp=_udp_server_socket;

	packet_len = sizeof(packet);
	client_len = sizeof(client_addr);	
	
	nbytes = recvfrom(udp, (char *) &packet, packet_len , 0, (struct sockaddr *) &client_addr, &client_len);
	if (nbytes>0) {
		if (packet.command_code==packet.command_check) switch (packet.command_code) {
		
 		case NET_UDPCMD_SERVERSEARCH:
 			if (!client) {
				packet.command_check=packet.command_code=NET_UDPCMD_SERVERINFO;
				memcpy(&packet.data,&_network_game,sizeof(_network_game));
				packet.data_len=sizeof(_network_game);
 				NetworkUDPSend(client,client_addr, packet);
 			}
 			break;
		case NET_UDPCMD_GETSERVERINFO:
			if (!client) {
				packet.command_check=packet.command_code=NET_UDPCMD_SERVERINFO;
				memcpy(&packet.data,&_network_game,sizeof(_network_game));
				packet.data_len=sizeof(_network_game);
				NetworkUDPSend(client,client_addr, packet);
			}
			break;
		case NET_UDPCMD_SERVERINFO:
 			if (client) {
				NetworkGameList * item;

				item = (NetworkGameList *) NetworkGameListAdd();
				item -> ip = inet_addr(inet_ntoa(client_addr.sin_addr));
				item -> port = ntohs(client_addr.sin_port);
				
				memcpy(item,&packet.data,packet.data_len);
 			}
 			break;
		}
	}
}



void NetworkUDPBroadCast(bool client, struct UDPPacket packet) {
	int i=0, res;
	struct sockaddr_in out_addr;
	uint32 bcaddr;
	byte * bcptr;

	SOCKET udp;
	if (client) udp=_udp_client_socket; else udp=_udp_server_socket;

	while (_network_ip_list[i]!=0) {
		bcaddr=_network_ip_list[i];
		out_addr.sin_family = AF_INET;
		if (client) { out_addr.sin_port = htons(_network_server_port); } else { out_addr.sin_port = htons(_network_client_port); };
		bcptr = (byte *) &bcaddr;
		bcptr[3]=255;
		out_addr.sin_addr.s_addr = bcaddr;
		res=sendto(udp,(char *) &packet,sizeof(packet),0,(struct sockaddr *) &out_addr,sizeof(out_addr));
		if (res==-1) DEBUG(misc,1)("udp: broadcast error: %i",GET_LAST_ERROR());
		i++;
	}
	
}

void NetworkUDPSend(bool client, struct sockaddr_in recv,struct UDPPacket packet) {

	SOCKET udp;
	if (client) udp=_udp_client_socket; else udp=_udp_server_socket;
	
	sendto(udp,(char *) &packet,sizeof(packet),0,(struct sockaddr *) &recv,sizeof(recv));
}


bool NetworkUDPSearchGame(const byte ** _network_detected_serverip, unsigned short * _network_detected_serverport) {
	struct UDPPacket packet;
	int timeout=3000;
	
	NetworkGameListClear();

	DEBUG(misc,0) ("[NET][UDP] searching server");
	*_network_detected_serverip = "255.255.255.255";
	*_network_detected_serverport = 0;
	
	packet.command_check=packet.command_code=NET_UDPCMD_SERVERSEARCH;
	packet.data_len=0;
	NetworkUDPBroadCast(true, packet);
	while (timeout>=0) {
		CSleep(100);
		timeout-=100;
	    NetworkUDPReceive(true);

		if (_network_game_count>0) {
			NetworkGameList * item;
			item = (NetworkGameList *) NetworkGameListItem(0);
			*_network_detected_serverip=inet_ntoa(*(struct in_addr *) &item->ip);
			*_network_detected_serverport=item->port;
 			timeout=-1;
 			DEBUG(misc,0) ("[NET][UDP] server found on %s", *_network_detected_serverip);
 			}
	
		}

	return (*_network_detected_serverport>0);
		
}


// *************************** //
// * New Network Core System * //
// *************************** //

void NetworkIPListInit() {
	struct hostent* he = NULL;
	char hostname[250];
	uint32 bcaddr;
	int i=0;
		
	gethostname(hostname,250);
	DEBUG(misc,2) ("[NET][IP] init for host %s", hostname);
	he=gethostbyname((char *) hostname);

	if (he == NULL) {
		he = gethostbyname("localhost");
		}
	
	if (he == NULL) {
		bcaddr = inet_addr("127.0.0.1");
		he = gethostbyaddr(inet_ntoa(*(struct in_addr *) &bcaddr), sizeof(bcaddr), AF_INET);
		}

	if (he == NULL) {
		DEBUG(misc, 2) ("[NET][IP] cannot resolve %s", hostname);
	} else {
		while(he->h_addr_list[i]) { 
			bcaddr = inet_addr(inet_ntoa(*(struct in_addr *) he->h_addr_list[i]));
			_network_ip_list[i]=bcaddr;
			DEBUG(misc,2) ("[NET][IP] add %s",inet_ntoa(*(struct in_addr *) he->h_addr_list[i]));
			i++;
		}

	}
	_network_ip_list[i]=0;
	
}

/* *************************************************** */

void NetworkCoreInit() {

DEBUG(misc,3) ("[NET][Core] init()");
_network_available=true;

// [win32] winsock startup

#if defined(WIN32)
{
	WSADATA wsa;
	DEBUG(misc,3) ("[NET][Core] using windows socket library");
	if (WSAStartup(MAKEWORD(2,0), &wsa) != 0) {
		DEBUG(misc,3) ("[NET][Core] error: WSAStartup failed");
		_network_available=false;
		}
}
#else 

// [morphos/amigaos] bsd-socket startup

#if defined(__MORPHOS__) || defined(__AMIGA__)
{
	DEBUG(misc,3) ("[NET][Core] using bsd socket library");
	if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
		DEBUG(misc,3) ("[NET][Core] Couldn't open bsdsocket.library version 4.");
		_network_available=false;
		}

	#if !defined(__MORPHOS__)
	// for usleep() implementation (only required for legacy AmigaOS builds)
	if ( (TimerPort = CreateMsgPort()) ) {
		if ( (TimerRequest = (struct timerequest *) CreateIORequest(TimerPort, sizeof(struct timerequest))) ) {
			if ( OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) TimerRequest, 0) == 0 ) {
				if ( !(TimerBase = TimerRequest->tr_node.io_Device) ) {
					// free ressources... 
					DEBUG(misc,3) ("[NET][Core] Couldn't initialize timer.");
					_network_available=false;
				}
			}
		}
	}
	#endif 

}
#else

// [linux/macos] unix-socket startup

	DEBUG(misc,3) ("[NET][Core] using unix socket library");

#endif

#endif


if (_network_available) {
	DEBUG(misc,3) ("[NET][Core] OK: multiplayer available");
	// initiate network ip list
	NetworkIPListInit();
	IConsoleCmdRegister("connect",NetworkConsoleCmdConnect);
	} else {
	DEBUG(misc,3) ("[NET][Core] FAILED: multiplayer not available");
	}
}

/* *************************************************** */

void NetworkCoreShutdown() {

DEBUG(misc,3) ("[NET][Core] shutdown()");

#if defined(__MORPHOS__) || defined(__AMIGA__)
{	
	// free allocated ressources
	#if !defined(__MORPHOS__) 
  if (TimerBase)    { CloseDevice((struct IORequest *) TimerRequest); }
  if (TimerRequest) { DeleteIORequest(TimerRequest); }
  if (TimerPort)    { DeleteMsgPort(TimerPort); }
	#endif

	if (SocketBase) {
		CloseLibrary(SocketBase);
	}
}
#endif


#if defined(WIN32)
{
	WSACleanup();
}
#endif

}

/* *************************************************** */

bool NetworkCoreConnectGame(const byte* b, unsigned short port)
{
	if (!_network_available) return false;

	if (strcmp(b,"auto")==0) {
		// do autodetect
		NetworkUDPSearchGame(&b, &port);
	}

	if (port==0) {
		// autodetection failed
		if (_networking_override) NetworkLobbyShutdown();
		ShowErrorMessage(-1, STR_NETWORK_ERR_NOSERVER, 0, 0);
		_switch_mode_errorstr = STR_NETWORK_ERR_NOSERVER;
		return false;
	}

	NetworkInitialize();
	_networking = NetworkConnect(b, port);
	if (_networking) {
		NetworkLobbyShutdown();
	} else {
		if (_networking_override) 
			NetworkLobbyShutdown();
		
		ShowErrorMessage(-1, STR_NETWORK_ERR_NOCONNECTION,0,0);
		_switch_mode_errorstr = STR_NETWORK_ERR_NOCONNECTION;
	}
	return _networking;
}

/* *************************************************** */

bool NetworkCoreStartGame()
{
	if (!_network_available) return false;
	NetworkLobbyShutdown();
	NetworkInitialize();
	NetworkListen();
	NetworkUDPListen(false);
	_networking_server = true;
	_networking = true;
	NetworkGameFillDefaults(); // clears the network game info
	_network_game.players_on++; // the serverplayer is online
	return true;
}

/* *************************************************** */

void NetworkCoreDisconnect()
{
	/* terminate server */
	if (_networking_server) {
		NetworkUDPClose(false);
		NetworkClose(false);
		} 

	/* terminate client connection */
	else if (_networking) {
		NetworkClose(true);
		}
	
	NetworkShutdown();
}

/* *************************************************** */

void NetworkCoreLoop(bool incomming) {


if (incomming) {

	// incomming

	if ( _udp_client_socket != INVALID_SOCKET ) NetworkUDPReceive(true);
	if ( _udp_server_socket != INVALID_SOCKET ) NetworkUDPReceive(false);

	if (_networking) {
		NetworkReceive();
		NetworkProcessCommands(); // to check if we got any new commands belonging to the current frame before we increase it.
		}

	} else {

	// outgoing

	if (_networking) {
		NetworkSend();
		}

	}

}

void NetworkLobbyInit() {
	DEBUG(misc,3) ("[NET][Lobby] init()");
	NetworkUDPListen(true);
}

void NetworkLobbyShutdown() {
	DEBUG(misc,3) ("[NET][Lobby] shutdown()");
	NetworkUDPClose(true);
}


// ******************************** //
// * Network Game List Extensions * //
// ******************************** //

void NetworkGameListClear() {
NetworkGameList * item;
NetworkGameList * next; 

DEBUG(misc,4) ("[NET][G-List] cleared server list");

item = _network_game_list;
while (item != NULL) {
	next = (NetworkGameList *) item -> _next;
	free (item);
	item = next;
	}
_network_game_list=NULL;
_network_game_count=0;
}

char * NetworkGameListAdd() {
NetworkGameList * item;
NetworkGameList * before; 

DEBUG(misc,4) ("[NET][G-List] added server to list");

item = _network_game_list;
before = item;
while (item != NULL) {
	before = item;
	item = (NetworkGameList *) item -> _next;
	}
item = malloc(sizeof(NetworkGameList));
item -> _next = NULL;
if (before == NULL) {
	_network_game_list = item;
	} else {
	before -> _next = (char *) item;
	}
_network_game_count++;
return (char *) item;
}

void NetworkGameListFromLAN() {
	struct UDPPacket packet;
	DEBUG(misc,2) ("[NET][G-List] searching server over lan");
	NetworkGameListClear();
	packet.command_check=packet.command_code=NET_UDPCMD_SERVERSEARCH;
	packet.data_len=0;
	NetworkUDPBroadCast(true,packet);
}

void NetworkGameListFromInternet() {
	DEBUG(misc,2) ("[NET][G-List] searching servers over internet");
	NetworkGameListClear();

	// **TODO** masterserver communication [internet protocol list]

}

char * NetworkGameListItem(uint16 index) {
NetworkGameList * item;
NetworkGameList * next; 
uint16 cnt = 0;

item = _network_game_list;

while ((item != NULL) && (cnt != index)) {
	next = (NetworkGameList *) item -> _next;
	item = next;
	cnt++;
	}

return (char *) item;
}

// *************************** //
// * Network Game Extensions * //
// *************************** //

void NetworkGameFillDefaults() {
	NetworkGameInfo * game = &_network_game;
#if defined(WITH_REV)
	extern char _openttd_revision[];
#endif
	
	DEBUG(misc,4) ("[NET][G-Info] setting defaults");

	ttd_strlcpy(game->server_name,"OpenTTD Game",13);
	game->game_password[0]='\0';
	game->map_name[0]='\0';
#if defined(WITH_REV)
	ttd_strlcpy(game->server_revision,_openttd_revision,strlen(_openttd_revision));
#else
	ttd_strlcpy(game->server_revision,"norev000",strlen("norev000"));
#endif
	game->game_date=0;

	game->map_height=0;
	game->map_width=0;
	game->map_set=0;

	game->players_max=8;
	game->players_on=0;
	
	game->server_lang=_dynlang.curr;
}

void NetworkGameChangeDate(uint16 newdate) {
	if (_networking_server) {
		_network_game.game_date = newdate;
		}
}

#else // not ENABLE_NETWORK

// stubs
void NetworkInitialize() {}
void NetworkShutdown() {}
void NetworkListen() {}
void NetworkConnect(const char *hostname, int port) {}
void NetworkReceive() {}
void NetworkSend() {}
void NetworkSendCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback) {}
void NetworkProcessCommands() {}
void NetworkStartSync(bool fcreset) {}
void NetworkCoreInit() { _network_available=false; };
void NetworkCoreShutdown() {};
void NetworkCoreDisconnect() {};
void NetworkCoreLoop(bool incomming) {};
bool NetworkCoreConnectGame(byte* b, unsigned short port) {return false;};
bool NetworkCoreStartGame() {return false;};
void NetworkLobbyShutdown() {};
void NetworkLobbyInit() {};
void NetworkGameListClear() {};
char * NetworkGameListAdd() {return NULL;};
void NetworkGameListFromLAN() {};
void NetworkGameListFromInternet() {};
void NetworkGameFillDefaults() {};
char * NetworkGameListItem(uint16 index) {return NULL;};
void NetworkGameChangeDate(uint16 newdate) {};

#endif
