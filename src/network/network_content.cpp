/* $Id$ */

/** @file network_content.cpp Content sending/receiving part of the network protocol. */

#if defined(ENABLE_NETWORK)

#include "../stdafx.h"
#include "../debug.h"
#include "../rev.h"
#include "../core/alloc_func.hpp"
#include "../core/math_func.hpp"
#include "../newgrf_config.h"
#include "../fileio_func.h"
#include "../string_func.h"
#include "../ai/ai.hpp"
#include "../window_func.h"
#include "../gui.h"
#include "core/host.h"
#include "network_content.h"

#include "table/strings.h"

#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

extern bool TarListAddFile(const char *filename);
extern bool HasGraphicsSet(const ContentInfo *ci, bool md5sum);
static ClientNetworkContentSocketHandler *_network_content_client;

/** Wrapper function for the HasProc */
static bool HasGRFConfig(const ContentInfo *ci, bool md5sum)
{
	return FindGRFConfig(BSWAP32(ci->unique_id), md5sum ? ci->md5sum : NULL) != NULL;
}

/**
 * Check whether a function piece of content is locally known.
 * Matches on the unique ID and possibly the MD5 checksum.
 * @param ci     the content info to search for
 * @param md5sum also match the MD5 checksum?
 * @return true iff it's known
 */
typedef bool (*HasProc)(const ContentInfo *ci, bool md5sum);

DEF_CONTENT_RECEIVE_COMMAND(Client, PACKET_CONTENT_SERVER_INFO)
{
	ContentInfo *ci = new ContentInfo();
	ci->type     = (ContentType)p->Recv_uint8();
	ci->id       = (ContentID)p->Recv_uint32();
	ci->filesize = p->Recv_uint32();

	p->Recv_string(ci->name, lengthof(ci->name));
	p->Recv_string(ci->version, lengthof(ci->name));
	p->Recv_string(ci->url, lengthof(ci->url));
	p->Recv_string(ci->description, lengthof(ci->description));

	ci->unique_id = p->Recv_uint32();
	for (uint j = 0; j < sizeof(ci->md5sum); j++) {
		ci->md5sum[j] = p->Recv_uint8();
	}

	ci->dependency_count = p->Recv_uint8();
	ci->dependencies = MallocT<ContentID>(ci->dependency_count);
	for (uint i = 0; i < ci->dependency_count; i++) ci->dependencies[i] = (ContentID)p->Recv_uint32();

	ci->tag_count = p->Recv_uint8();
	ci->tags = MallocT<char[32]>(ci->tag_count);
	for (uint i = 0; i < ci->tag_count; i++) p->Recv_string(ci->tags[i], lengthof(*ci->tags));

	if (!ci->IsValid()) {
		delete ci;
		this->Close();
		return false;
	}

	/* Find the appropriate check function */
	HasProc proc = NULL;
	switch (ci->type) {
		case CONTENT_TYPE_NEWGRF:
			proc = HasGRFConfig;
			break;

		case CONTENT_TYPE_BASE_GRAPHICS:
			proc = HasGraphicsSet;
			break;

		case CONTENT_TYPE_AI:
		case CONTENT_TYPE_AI_LIBRARY:
			proc = AI::HasAI; break;
			break;

		default:
			break;
	}

	if (proc != NULL) {
		if (proc(ci, true)) {
			ci->state = ContentInfo::ALREADY_HERE;
		} else {
			ci->state = ContentInfo::UNSELECTED;
			if (proc(ci, false)) ci->update = true;
		}
	} else {
		ci->state = ContentInfo::UNSELECTED;
	}

	/* Something we don't have and has filesize 0 does not exist in te system */
	if (ci->state == ContentInfo::UNSELECTED && ci->filesize == 0) ci->state = ContentInfo::DOES_NOT_EXIST;

	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); iter++) {
		(*iter)->OnReceiveContentInfo(ci);
	}

	return true;
}

void ClientNetworkContentSocketHandler::RequestContentList(ContentType type)
{
	Packet *p = new Packet(PACKET_CONTENT_CLIENT_INFO_LIST);
	p->Send_uint8 ((byte)type);
	p->Send_uint32(_openttd_newgrf_version);

	this->Send_Packet(p);
}

void ClientNetworkContentSocketHandler::RequestContentList(uint count, const ContentID *content_ids)
{
	while (count > 0) {
		/* We can "only" send a limited number of IDs in a single packet.
		 * A packet begins with the packet size and a byte for the type.
		 * Then this packet adds a byte for the content type and a uint16
		 * for the count in this packet. The rest of the packet can be
		 * used for the IDs. */
		uint p_count = min(count, (SEND_MTU - sizeof(PacketSize) - sizeof(byte) - sizeof(byte) - sizeof(uint16)) / sizeof(uint32));

		Packet *p = new Packet(PACKET_CONTENT_CLIENT_INFO_ID);
		p->Send_uint16(p_count);

		for (uint i = 0; i < p_count; i++) {
			p->Send_uint32(content_ids[i]);
		}

		this->Send_Packet(p);
		count -= p_count;
		content_ids += count;
	}
}

void ClientNetworkContentSocketHandler::RequestContentList(ContentVector *cv, bool send_md5sum)
{
	if (cv == NULL) return;

	/* 20 is sizeof(uint32) + sizeof(md5sum (byte[16])) */
	assert(cv->Length() < 255);
	assert(cv->Length() < (SEND_MTU - sizeof(PacketSize) - sizeof(byte) - sizeof(uint8)) / (send_md5sum ? 20 : sizeof(uint32)));

	Packet *p = new Packet(send_md5sum ? PACKET_CONTENT_CLIENT_INFO_EXTID_MD5 : PACKET_CONTENT_CLIENT_INFO_EXTID);
	p->Send_uint8(cv->Length());

	for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) {
		const ContentInfo *ci = *iter;
		p->Send_uint8((byte)ci->type);
		p->Send_uint32(ci->unique_id);
		if (!send_md5sum) continue;

		for (uint j = 0; j < sizeof(ci->md5sum); j++) {
			p->Send_uint8(ci->md5sum[j]);
		}
	}

	this->Send_Packet(p);
}

void ClientNetworkContentSocketHandler::RequestContent(uint count, const uint32 *content_ids)
{
	while (count > 0) {
		/* We can "only" send a limited number of IDs in a single packet.
		* A packet begins with the packet size and a byte for the type.
		* Then this packet adds a uint16 for the count in this packet.
		* The rest of the packet can be used for the IDs. */
		uint p_count = min(count, (SEND_MTU - sizeof(PacketSize) - sizeof(byte) - sizeof(uint16)) / sizeof(uint32));

		Packet *p = new Packet(PACKET_CONTENT_CLIENT_CONTENT);
		p->Send_uint16(p_count);

		for (uint i = 0; i < p_count; i++) {
			p->Send_uint32(content_ids[i]);
		}

		this->Send_Packet(p);
		count -= p_count;
		content_ids += count;
	}
}

/**
 * Determine the full filename of a piece of content information
 * @param ci         the information to get the filename from
 * @param compressed should the filename end with .gz?
 * @return a statically allocated buffer with the filename or
 *         NULL when no filename could be made.
 */
static char *GetFullFilename(const ContentInfo *ci, bool compressed)
{
	Subdirectory dir;
	switch (ci->type) {
		default: return NULL;
		case CONTENT_TYPE_BASE_GRAPHICS: dir = DATA_DIR;       break;
		case CONTENT_TYPE_NEWGRF:        dir = DATA_DIR;       break;
		case CONTENT_TYPE_AI:            dir = AI_DIR;         break;
		case CONTENT_TYPE_AI_LIBRARY:    dir = AI_LIBRARY_DIR; break;
		case CONTENT_TYPE_SCENARIO:      dir = SCENARIO_DIR;   break;
		case CONTENT_TYPE_HEIGHTMAP:     dir = HEIGHTMAP_DIR;  break;
	}

	static char buf[MAX_PATH];
	FioGetFullPath(buf, lengthof(buf), SP_AUTODOWNLOAD_DIR, dir, ci->filename);
	strecat(buf, compressed ? ".tar.gz" : ".tar", lastof(buf));

	return buf;
}

/**
 * Gunzip a given file and remove the .gz if successful.
 * @param ci container with filename
 * @return true if the gunzip completed
 */
static bool GunzipFile(const ContentInfo *ci)
{
#if defined(WITH_ZLIB)
	bool ret = true;
	gzFile fin = gzopen(GetFullFilename(ci, true), "rb");
	FILE *fout = fopen(GetFullFilename(ci, false), "wb");

	if (fin == NULL || fout == NULL) {
		ret = false;
		goto exit;
	}

	byte buff[8192];
	while (!gzeof(fin)) {
		int read = gzread(fin, buff, sizeof(buff));
		if (read < 0 || (size_t)read != fwrite(buff, 1, read, fout)) {
			ret = false;
			break;
		}
	}

exit:
	if (fin != NULL) gzclose(fin);
	if (fout != NULL) fclose(fout);

	return ret;
#else
	NOT_REACHED();
#endif /* defined(WITH_ZLIB) */
}

DEF_CONTENT_RECEIVE_COMMAND(Client, PACKET_CONTENT_SERVER_CONTENT)
{
	if (this->curFile == NULL) {
		/* When we haven't opened a file this must be our first packet with metadata. */
		this->curInfo = new ContentInfo;
		this->curInfo->type     = (ContentType)p->Recv_uint8();
		this->curInfo->id       = (ContentID)p->Recv_uint32();
		this->curInfo->filesize = p->Recv_uint32();
		p->Recv_string(this->curInfo->filename, lengthof(this->curInfo->filename));

		if (!this->curInfo->IsValid()) {
			delete this->curInfo;
			this->curInfo = NULL;
			this->Close();
			return false;
		}

		if (this->curInfo->filesize != 0) {
			/* The filesize is > 0, so we are going to download it */
			const char *filename = GetFullFilename(this->curInfo, true);
			if (filename == NULL) {
				/* Unless that fails ofcourse... */
				DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
				ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD, STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE, 0, 0);
				this->Close();
				return false;
			}

			this->curFile = fopen(filename, "wb");
		}
	} else {
		/* We have a file opened, thus are downloading internal content */
		size_t toRead = (size_t)(p->size - p->pos);
		if (fwrite(p->buffer + p->pos, 1, toRead, this->curFile) != toRead) {
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD, STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE, 0, 0);
			this->Close();
			fclose(this->curFile);
			this->curFile = NULL;

			return false;
		}

		/* Tell the rest we downloaded some bytes */
		for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); iter++) {
			(*iter)->OnDownloadProgress(this->curInfo, toRead);
		}

		if (toRead == 0) {
			/* We read nothing; that's our marker for end-of-stream.
			 * Now gunzip the tar and make it known. */
			fclose(this->curFile);
			this->curFile = NULL;

			if (GunzipFile(this->curInfo)) {
				unlink(GetFullFilename(this->curInfo, true));

				TarListAddFile(GetFullFilename(this->curInfo, false));

				for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); iter++) {
					(*iter)->OnDownloadComplete(this->curInfo->id);
				}
			} else {
				ShowErrorMessage(INVALID_STRING_ID, STR_CONTENT_ERROR_COULD_NOT_EXTRACT, 0, 0);
			}
		}
	}

	/* We ended this file, so clean up the mess */
	if (this->curFile == NULL) {
		delete this->curInfo;
		this->curInfo = NULL;
	}

	return true;
}

/**
 * Create a socket handler with the given socket and (server) address.
 * @param s the socket to communicate over
 * @param sin the IP/port of the server
 */
ClientNetworkContentSocketHandler::ClientNetworkContentSocketHandler(SOCKET s, const struct sockaddr_in *sin) :
	NetworkContentSocketHandler(s, sin),
	curFile(NULL),
	curInfo(NULL)
{
}

/** Clear up the mess ;) */
ClientNetworkContentSocketHandler::~ClientNetworkContentSocketHandler()
{
	delete this->curInfo;
	if (this->curFile != NULL) fclose(this->curFile);
}

/**
 * Connect to the content server if needed, return the socket handler and
 * add the callback to the socket handler.
 * @param cb the callback to add
 * @return the socket handler or NULL is connecting failed
 */
ClientNetworkContentSocketHandler *NetworkContent_Connect(ContentCallback *cb)
{
	/* If there's no connection or when it's broken, we try to reconnect.
	 * Otherwise we just add ourselves and return immediatelly */
	if (_network_content_client != NULL && !_network_content_client->has_quit) {
		*_network_content_client->callbacks.Append() = cb;
		return _network_content_client;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) return NULL;

	if (!SetNoDelay(s)) DEBUG(net, 1, "Setting TCP_NODELAY failed");

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = NetworkResolveHost(NETWORK_CONTENT_SERVER_HOST);
	sin.sin_port = htons(NETWORK_CONTENT_SERVER_PORT);

	/* We failed to connect for which reason what so ever */
	if (connect(s, (struct sockaddr*)&sin, sizeof(sin)) != 0) return NULL;

	if (!SetNonBlocking(s)) DEBUG(net, 0, "Setting non-blocking mode failed"); // XXX should this be an error?

	if (_network_content_client != NULL) {
		if (_network_content_client->sock != INVALID_SOCKET) closesocket(_network_content_client->sock);
		_network_content_client->sock = s;

		/* Clean up the mess that could've been left behind */
		_network_content_client->has_quit = false;
		delete _network_content_client->curInfo;
		_network_content_client->curInfo = NULL;
		if (_network_content_client->curFile != NULL) fclose(_network_content_client->curFile);
		_network_content_client->curFile = NULL;
	} else {
		_network_content_client = new ClientNetworkContentSocketHandler(s, &sin);
	}
	*_network_content_client->callbacks.Append() = cb;
	return _network_content_client;
}

/**
 * Remove yourself from the network content callback list and when
 * that list is empty the connection will be closed.
 */
void NetworkContent_Disconnect(ContentCallback *cb)
{
	_network_content_client->callbacks.Erase(_network_content_client->callbacks.Find(cb));

	if (_network_content_client->callbacks.Length() == 0) {
		delete _network_content_client;
		_network_content_client = NULL;
	}
}

/**
 * Check whether we received/can send some data from/to the content server and
 * when that's the case handle it appropriately
 */
void NetworkContentLoop()
{
	if (_network_content_client == NULL) return;

	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	FD_SET(_network_content_client->sock, &read_fd);
	FD_SET(_network_content_client->sock, &write_fd);

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif
	if (FD_ISSET(_network_content_client->sock, &read_fd)) _network_content_client->Recv_Packets();
	_network_content_client->writable = !!FD_ISSET(_network_content_client->sock, &write_fd);
	_network_content_client->Send_Packets();
}

#endif /* ENABLE_NETWORK */
