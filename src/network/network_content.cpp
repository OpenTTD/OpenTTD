/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content.cpp Content sending/receiving part of the network protocol. */

#if defined(ENABLE_NETWORK)

#include "../stdafx.h"
#include "../rev.h"
#include "../ai/ai.hpp"
#include "../window_func.h"
#include "../gui.h"
#include "../base_media_base.h"
#include "../settings_type.h"
#include "network_content.h"

#include "table/strings.h"

#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

extern bool HasScenario(const ContentInfo *ci, bool md5sum);
ClientNetworkContentSocketHandler _network_content_client;

/** Wrapper function for the HasProc */
static bool HasGRFConfig(const ContentInfo *ci, bool md5sum)
{
	return FindGRFConfig(BSWAP32(ci->unique_id), md5sum ? FGCM_EXACT : FGCM_ANY, md5sum ? ci->md5sum : NULL) != NULL;
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
	p->Recv_string(ci->description, lengthof(ci->description),  true);

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
			proc = BaseGraphics::HasSet;
			break;

		case CONTENT_TYPE_BASE_MUSIC:
			proc = BaseMusic::HasSet;
			break;

		case CONTENT_TYPE_BASE_SOUNDS:
			proc = BaseSounds::HasSet;
			break;

		case CONTENT_TYPE_AI:
		case CONTENT_TYPE_AI_LIBRARY:
			proc = AI::HasAI; break;
			break;

		case CONTENT_TYPE_SCENARIO:
		case CONTENT_TYPE_HEIGHTMAP:
			proc = HasScenario;
			break;

		default:
			break;
	}

	if (proc != NULL) {
		if (proc(ci, true)) {
			ci->state = ContentInfo::ALREADY_HERE;
		} else {
			ci->state = ContentInfo::UNSELECTED;
			if (proc(ci, false)) ci->upgrade = true;
		}
	} else {
		ci->state = ContentInfo::UNSELECTED;
	}

	/* Something we don't have and has filesize 0 does not exist in te system */
	if (ci->state == ContentInfo::UNSELECTED && ci->filesize == 0) ci->state = ContentInfo::DOES_NOT_EXIST;

	/* Do we already have a stub for this? */
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		ContentInfo *ici = *iter;
		if (ici->type == ci->type && ici->unique_id == ci->unique_id &&
				memcmp(ci->md5sum, ici->md5sum, sizeof(ci->md5sum)) == 0) {
			/* Preserve the name if possible */
			if (StrEmpty(ci->name)) strecpy(ci->name, ici->name, lastof(ci->name));
			if (ici->IsSelected()) ci->state = ici->state;

			/*
			 * As ici might be selected by the content window we cannot delete that.
			 * However, we want to keep most of the values of ci, except the values
			 * we (just) already preserved.
			 * So transfer data and ownership of allocated memory from ci to ici.
			 */
			ici->TransferFrom(ci);
			delete ci;

			this->OnReceiveContentInfo(ici);
			return true;
		}
	}

	/* Missing content info? Don't list it */
	if (ci->filesize == 0) {
		delete ci;
		return true;
	}

	*this->infos.Append() = ci;

	/* Incoming data means that we might need to reconsider dependencies */
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		this->CheckDependencyState(*iter);
	}

	this->OnReceiveContentInfo(ci);

	return true;
}

void ClientNetworkContentSocketHandler::RequestContentList(ContentType type)
{
	if (type == CONTENT_TYPE_END) {
		this->RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
		this->RequestContentList(CONTENT_TYPE_BASE_MUSIC);
		this->RequestContentList(CONTENT_TYPE_BASE_SOUNDS);
		this->RequestContentList(CONTENT_TYPE_SCENARIO);
		this->RequestContentList(CONTENT_TYPE_HEIGHTMAP);
#ifdef ENABLE_AI
		this->RequestContentList(CONTENT_TYPE_AI);
		this->RequestContentList(CONTENT_TYPE_AI_LIBRARY);
#endif /* ENABLE_AI */
		this->RequestContentList(CONTENT_TYPE_NEWGRF);
		return;
	}

	this->Connect();

	Packet *p = new Packet(PACKET_CONTENT_CLIENT_INFO_LIST);
	p->Send_uint8 ((byte)type);
	p->Send_uint32(_openttd_newgrf_version);

	this->SendPacket(p);
}

void ClientNetworkContentSocketHandler::RequestContentList(uint count, const ContentID *content_ids)
{
	this->Connect();

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

		this->SendPacket(p);
		count -= p_count;
		content_ids += p_count;
	}
}

void ClientNetworkContentSocketHandler::RequestContentList(ContentVector *cv, bool send_md5sum)
{
	if (cv == NULL) return;

	this->Connect();

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

	this->SendPacket(p);

	for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) {
		ContentInfo *ci = *iter;
		bool found = false;
		for (ContentIterator iter2 = this->infos.Begin(); iter2 != this->infos.End(); iter2++) {
			ContentInfo *ci2 = *iter2;
			if (ci->type == ci2->type && ci->unique_id == ci2->unique_id &&
					(!send_md5sum || memcmp(ci->md5sum, ci2->md5sum, sizeof(ci->md5sum)) == 0)) {
				found = true;
				break;
			}
		}
		if (!found) {
			*this->infos.Append() = ci;
		} else {
			delete ci;
		}
	}
}

void ClientNetworkContentSocketHandler::DownloadSelectedContent(uint &files, uint &bytes, bool fallback)
{
	bytes = 0;

	ContentIDList content;
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		const ContentInfo *ci = *iter;
		if (!ci->IsSelected() || ci->state == ContentInfo::ALREADY_HERE) continue;

		*content.Append() = ci->id;
		bytes += ci->filesize;
	}

	files = content.Length();

	/* If there's nothing to download, do nothing. */
	if (files == 0) return;

	if (_settings_client.network.no_http_content_downloads || fallback) {
		this->DownloadSelectedContentFallback(content);
	} else {
		this->DownloadSelectedContentHTTP(content);
	}
}

void ClientNetworkContentSocketHandler::DownloadSelectedContentHTTP(const ContentIDList &content)
{
	uint count = content.Length();

	/* Allocate memory for the whole request.
	 * Requests are "id\nid\n..." (as strings), so assume the maximum ID,
	 * which is uint32 so 10 characters long. Then the newlines and
	 * multiply that all with the count and then add the '\0'. */
	uint bytes = (10 + 1) * count + 1;
	char *content_request = MallocT<char>(bytes);
	const char *lastof = content_request + bytes - 1;

	char *p = content_request;
	for (const ContentID *id = content.Begin(); id != content.End(); id++) {
		p += seprintf(p, lastof, "%d\n", *id);
	}

	this->http_response_index = -1;

	NetworkAddress address(NETWORK_CONTENT_MIRROR_HOST, NETWORK_CONTENT_MIRROR_PORT);
	new NetworkHTTPContentConnecter(address, this, NETWORK_CONTENT_MIRROR_URL, content_request);
	/* NetworkHTTPContentConnecter takes over freeing of content_request! */
}

void ClientNetworkContentSocketHandler::DownloadSelectedContentFallback(const ContentIDList &content)
{
	uint count = content.Length();
	const ContentID *content_ids = content.Begin();
	this->Connect();

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

		this->SendPacket(p);
		count -= p_count;
		content_ids += p_count;
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
		case CONTENT_TYPE_BASE_MUSIC:    dir = GM_DIR;         break;
		case CONTENT_TYPE_BASE_SOUNDS:   dir = DATA_DIR;       break;
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
	FILE *ftmp = fopen(GetFullFilename(ci, true), "rb");
	gzFile fin = gzdopen(fileno(ftmp), "rb");
	FILE *fout = fopen(GetFullFilename(ci, false), "wb");

	if (fin == NULL || fout == NULL) {
		ret = false;
	} else {
		byte buff[8192];
		while (1) {
			int read = gzread(fin, buff, sizeof(buff));
			if (read == 0) {
				/* If gzread() returns 0, either the end-of-file has been
				 * reached or an underlying read error has occurred.
				 *
				 * gzeof() can't be used, because:
				 * 1.2.5 - it is safe, 1 means 'everything was OK'
				 * 1.2.3.5, 1.2.4 - 0 or 1 is returned 'randomly'
				 * 1.2.3.3 - 1 is returned for truncated archive
				 *
				 * So we use gzerror(). When proper end of archive
				 * has been reached, then:
				 * errnum == Z_STREAM_END in 1.2.3.3,
				 * errnum == 0 in 1.2.4 and 1.2.5 */
				int errnum;
				gzerror(fin, &errnum);
				if (errnum != 0 && errnum != Z_STREAM_END) ret = false;
				break;
			}
			if (read < 0 || (size_t)read != fwrite(buff, 1, read, fout)) {
				/* If gzread() returns -1, there was an error in archive */
				ret = false;
				break;
			}
			/* DO NOT DO THIS! It will fail to detect broken archive with 1.2.3.3!
			 * if (read < sizeof(buff)) break; */
		}
	}

	if (fin != NULL) {
		/* Closes ftmp too! */
		gzclose(fin);
	} else if (ftmp != NULL) {
		/* In case the gz stream was opened correctly this will
		 * be closed by gzclose. */
		fclose(ftmp);
	}
	if (fout != NULL) fclose(fout);

	return ret;
#else
	NOT_REACHED();
#endif /* defined(WITH_ZLIB) */
}

DEF_CONTENT_RECEIVE_COMMAND(Client, PACKET_CONTENT_SERVER_CONTENT)
{
	if (this->curFile == NULL) {
		delete this->curInfo;
		/* When we haven't opened a file this must be our first packet with metadata. */
		this->curInfo = new ContentInfo;
		this->curInfo->type     = (ContentType)p->Recv_uint8();
		this->curInfo->id       = (ContentID)p->Recv_uint32();
		this->curInfo->filesize = p->Recv_uint32();
		p->Recv_string(this->curInfo->filename, lengthof(this->curInfo->filename));

		if (!this->BeforeDownload()) {
			this->Close();
			return false;
		}
	} else {
		/* We have a file opened, thus are downloading internal content */
		size_t toRead = (size_t)(p->size - p->pos);
		if (fwrite(p->buffer + p->pos, 1, toRead, this->curFile) != toRead) {
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD, STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE, WL_ERROR);
			this->Close();
			fclose(this->curFile);
			this->curFile = NULL;

			return false;
		}

		this->OnDownloadProgress(this->curInfo, (uint)toRead);

		if (toRead == 0) this->AfterDownload();
	}

	return true;
}

/**
 * Handle the opening of the file before downloading.
 * @return false on any error.
 */
bool ClientNetworkContentSocketHandler::BeforeDownload()
{
	if (!this->curInfo->IsValid()) {
		delete this->curInfo;
		this->curInfo = NULL;
		return false;
	}

	if (this->curInfo->filesize != 0) {
		/* The filesize is > 0, so we are going to download it */
		const char *filename = GetFullFilename(this->curInfo, true);
		if (filename == NULL || (this->curFile = fopen(filename, "wb")) == NULL) {
			/* Unless that fails ofcourse... */
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD, STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE, WL_ERROR);
			return false;
		}
	}
	return true;
}

/**
 * Handle the closing and extracting of a file after
 * downloading it has been done.
 */
void ClientNetworkContentSocketHandler::AfterDownload()
{
	/* We read nothing; that's our marker for end-of-stream.
	 * Now gunzip the tar and make it known. */
	fclose(this->curFile);
	this->curFile = NULL;

	if (GunzipFile(this->curInfo)) {
		unlink(GetFullFilename(this->curInfo, true));

		TarScanner ts;
		ts.AddFile(GetFullFilename(this->curInfo, false), 0);

		if (this->curInfo->type == CONTENT_TYPE_BASE_MUSIC) {
			/* Music can't be in a tar. So extract the tar! */
			ExtractTar(GetFullFilename(this->curInfo, false));
			unlink(GetFullFilename(this->curInfo, false));
		}

		this->OnDownloadComplete(this->curInfo->id);
	} else {
		ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_EXTRACT, INVALID_STRING_ID, WL_ERROR);
	}
}

/* Also called to just clean up the mess. */
void ClientNetworkContentSocketHandler::OnFailure()
{
	/* If we fail, download the rest via the 'old' system. */
	uint files, bytes;
	this->DownloadSelectedContent(files, bytes, true);

	this->http_response.Reset();
	this->http_response_index = -2;

	if (this->curFile != NULL) {
		fclose(this->curFile);
		this->curFile = NULL;
	}
}

void ClientNetworkContentSocketHandler::OnReceiveData(const char *data, size_t length)
{
	assert(data == NULL || length != 0);

	/* Ignore any latent data coming from a connection we closed. */
	if (this->http_response_index == -2) return;

	if (this->http_response_index == -1) {
		if (data != NULL) {
			/* Append the rest of the response. */
			memcpy(this->http_response.Append((uint)length), data, length);
			return;
		} else {
			/* Make sure the response is properly terminated. */
			*this->http_response.Append() = '\0';

			/* And prepare for receiving the rest of the data. */
			this->http_response_index = 0;
		}
	}

	if (data != NULL) {
		/* We have data, so write it to the file. */
		if (fwrite(data, 1, length, this->curFile) != length) {
			/* Writing failed somehow, let try via the old method. */
			this->OnFailure();
		} else {
			/* Just received the data. */
			this->OnDownloadProgress(this->curInfo, (uint)length);
		}
		/* Nothing more to do now. */
		return;
	}

	if (this->curFile != NULL) {
		/* We've finished downloading a file. */
		this->AfterDownload();
	}

	if ((uint)this->http_response_index >= this->http_response.Length()) {
		/* It's not a real failure, but if there's
		 * nothing more to download it helps with
		 * cleaning up the stuff we allocated. */
		this->OnFailure();
		return;
	}

	delete this->curInfo;
	/* When we haven't opened a file this must be our first packet with metadata. */
	this->curInfo = new ContentInfo;

/** Check p for not being null and return calling OnFailure if that's not the case. */
#define check_not_null(p) { if ((p) == NULL) { this->OnFailure(); return; } }
/** Check p for not being null and then terminate, or return calling OnFailure. */
#define check_and_terminate(p) { check_not_null(p); *(p) = '\0'; }

	for (;;) {
		char *str = this->http_response.Begin() + this->http_response_index;
		char *p = strchr(str, '\n');
		check_and_terminate(p);

		/* Update the index for the next one */
		this->http_response_index += (int)strlen(str) + 1;

		/* Read the ID */
		p = strchr(str, ',');
		check_and_terminate(p);
		this->curInfo->id = (ContentID)atoi(str);

		/* Read the type */
		str = p + 1;
		p = strchr(str, ',');
		check_and_terminate(p);
		this->curInfo->type = (ContentType)atoi(str);

		/* Read the file size */
		str = p + 1;
		p = strchr(str, ',');
		check_and_terminate(p);
		this->curInfo->filesize = atoi(str);

		/* Read the URL */
		str = p + 1;
		/* Is it a fallback URL? If so, just continue with the next one. */
		if (strncmp(str, "ottd", 4) == 0) {
			if ((uint)this->http_response_index >= this->http_response.Length()) {
				/* Have we gone through all lines? */
				this->OnFailure();
				return;
			}
			continue;
		}

		p = strrchr(str, '/');
		check_not_null(p);
		p++; // Start after the '/'

		char tmp[MAX_PATH];
		if (strecpy(tmp, p, lastof(tmp)) == lastof(tmp)) {
			this->OnFailure();
			return;
		}
		/* Remove the extension from the string. */
		for (uint i = 0; i < 2; i++) {
			p = strrchr(tmp, '.');
			check_and_terminate(p);
		}

		/* Copy the string, without extension, to the filename. */
		strecpy(this->curInfo->filename, tmp, lastof(this->curInfo->filename));

		/* Request the next file. */
		if (!this->BeforeDownload()) {
			this->OnFailure();
			return;
		}

		NetworkHTTPSocketHandler::Connect(str, this);
		return;
	}

#undef check
#undef check_and_terminate
}

/**
 * Create a socket handler with the given socket and (server) address.
 * @param s the socket to communicate over
 * @param sin the IP/port of the server
 */
ClientNetworkContentSocketHandler::ClientNetworkContentSocketHandler() :
	NetworkContentSocketHandler(),
	http_response_index(-2),
	curFile(NULL),
	curInfo(NULL),
	isConnecting(false)
{
}

/** Clear up the mess ;) */
ClientNetworkContentSocketHandler::~ClientNetworkContentSocketHandler()
{
	delete this->curInfo;
	if (this->curFile != NULL) fclose(this->curFile);

	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) delete *iter;
}

class NetworkContentConnecter : TCPConnecter {
public:
	NetworkContentConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	virtual void OnFailure()
	{
		_network_content_client.isConnecting = false;
		_network_content_client.OnConnect(false);
	}

	virtual void OnConnect(SOCKET s)
	{
		assert(_network_content_client.sock == INVALID_SOCKET);
		_network_content_client.isConnecting = false;
		_network_content_client.sock = s;
		_network_content_client.Reopen();
		_network_content_client.OnConnect(true);
	}
};

/**
 * Connect with the content server.
 */
void ClientNetworkContentSocketHandler::Connect()
{
	this->lastActivity = _realtime_tick;

	if (this->sock != INVALID_SOCKET || this->isConnecting) return;
	this->isConnecting = true;
	new NetworkContentConnecter(NetworkAddress(NETWORK_CONTENT_SERVER_HOST, NETWORK_CONTENT_SERVER_PORT, AF_UNSPEC));
}

/**
 * Disconnect from the content server.
 */
void ClientNetworkContentSocketHandler::Close()
{
	if (this->sock == INVALID_SOCKET) return;
	NetworkContentSocketHandler::Close();

	this->OnDisconnect();
}

/**
 * Check whether we received/can send some data from/to the content server and
 * when that's the case handle it appropriately
 */
void ClientNetworkContentSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET || this->isConnecting) return;

	if (this->lastActivity + IDLE_TIMEOUT < _realtime_tick) {
		this->Close();
		return;
	}

	if (this->CanSendReceive()) {
		this->ReceivePackets();
		this->lastActivity = _realtime_tick;
	}

	this->SendPackets();
}

/**
 * Download information of a given Content ID if not already tried
 * @param cid the ID to try
 */
void ClientNetworkContentSocketHandler::DownloadContentInfo(ContentID cid)
{
	/* When we tried to download it already, don't try again */
	if (this->requested.Contains(cid)) return;

	*this->requested.Append() = cid;
	assert(this->requested.Contains(cid));
	this->RequestContentList(1, &cid);
}

/**
 * Get the content info based on a ContentID
 * @param cid the ContentID to search for
 * @return the ContentInfo or NULL if not found
 */
ContentInfo *ClientNetworkContentSocketHandler::GetContent(ContentID cid)
{
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		ContentInfo *ci = *iter;
		if (ci->id == cid) return ci;
	}
	return NULL;
}


/**
 * Select a specific content id.
 * @param cid the content ID to select
 */
void ClientNetworkContentSocketHandler::Select(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci == NULL || ci->state != ContentInfo::UNSELECTED) return;

	ci->state = ContentInfo::SELECTED;
	this->CheckDependencyState(ci);
}

/**
 * Unselect a specific content id.
 * @param cid the content ID to deselect
 */
void ClientNetworkContentSocketHandler::Unselect(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci == NULL || !ci->IsSelected()) return;

	ci->state = ContentInfo::UNSELECTED;
	this->CheckDependencyState(ci);
}

/** Select everything we can select */
void ClientNetworkContentSocketHandler::SelectAll()
{
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		ContentInfo *ci = *iter;
		if (ci->state == ContentInfo::UNSELECTED) {
			ci->state = ContentInfo::SELECTED;
			this->CheckDependencyState(ci);
		}
	}
}

/** Select everything that's an update for something we've got */
void ClientNetworkContentSocketHandler::SelectUpgrade()
{
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		ContentInfo *ci = *iter;
		if (ci->state == ContentInfo::UNSELECTED && ci->upgrade) {
			ci->state = ContentInfo::SELECTED;
			this->CheckDependencyState(ci);
		}
	}
}

/** Unselect everything that we've not downloaded so far. */
void ClientNetworkContentSocketHandler::UnselectAll()
{
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		ContentInfo *ci = *iter;
		if (ci->IsSelected() && ci->state != ContentInfo::ALREADY_HERE) ci->state = ContentInfo::UNSELECTED;
	}
}

/** Toggle the state of a content info and check its dependencies */
void ClientNetworkContentSocketHandler::ToggleSelectedState(const ContentInfo *ci)
{
	switch (ci->state) {
		case ContentInfo::SELECTED:
		case ContentInfo::AUTOSELECTED:
			this->Unselect(ci->id);
			break;

		case ContentInfo::UNSELECTED:
			this->Select(ci->id);
			break;

		default:
			break;
	}
}

/**
 * Reverse lookup the dependencies of (direct) parents over a given child.
 * @param parents list to store all parents in (is not cleared)
 * @param child   the child to search the parents' dependencies for
 */
void ClientNetworkContentSocketHandler::ReverseLookupDependency(ConstContentVector &parents, const ContentInfo *child) const
{
	for (ConstContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
		const ContentInfo *ci = *iter;
		if (ci == child) continue;

		for (uint i = 0; i < ci->dependency_count; i++) {
			if (ci->dependencies[i] == child->id) {
				*parents.Append() = ci;
				break;
			}
		}
	}
}

/**
 * Reverse lookup the dependencies of all parents over a given child.
 * @param tree  list to store all parents in (is not cleared)
 * @param child the child to search the parents' dependencies for
 */
void ClientNetworkContentSocketHandler::ReverseLookupTreeDependency(ConstContentVector &tree, const ContentInfo *child) const
{
	*tree.Append() = child;

	/* First find all direct parents */
	for (ConstContentIterator iter = tree.Begin(); iter != tree.End(); iter++) {
		ConstContentVector parents;
		this->ReverseLookupDependency(parents, *iter);

		for (ConstContentIterator piter = parents.Begin(); piter != parents.End(); piter++) {
			tree.Include(*piter);
		}
	}
}

/**
 * Check the dependencies (recursively) of this content info
 * @param ci the content info to check the dependencies of
 */
void ClientNetworkContentSocketHandler::CheckDependencyState(ContentInfo *ci)
{
	if (ci->IsSelected() || ci->state == ContentInfo::ALREADY_HERE) {
		/* Selection is easy; just walk all children and set the
		 * autoselected state. That way we can see what we automatically
		 * selected and thus can unselect when a dependency is removed. */
		for (uint i = 0; i < ci->dependency_count; i++) {
			ContentInfo *c = this->GetContent(ci->dependencies[i]);
			if (c == NULL) {
				this->DownloadContentInfo(ci->dependencies[i]);
			} else if (c->state == ContentInfo::UNSELECTED) {
				c->state = ContentInfo::AUTOSELECTED;
				this->CheckDependencyState(c);
			}
		}
		return;
	}

	if (ci->state != ContentInfo::UNSELECTED) return;

	/* For unselection we need to find the parents of us. We need to
	 * unselect them. After that we unselect all children that we
	 * depend on and are not used as dependency for us, but only when
	 * we automatically selected them. */
	ConstContentVector parents;
	this->ReverseLookupDependency(parents, ci);
	for (ConstContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
		const ContentInfo *c = *iter;
		if (!c->IsSelected()) continue;

		this->Unselect(c->id);
	}

	for (uint i = 0; i < ci->dependency_count; i++) {
		const ContentInfo *c = this->GetContent(ci->dependencies[i]);
		if (c == NULL) {
			DownloadContentInfo(ci->dependencies[i]);
			continue;
		}
		if (c->state != ContentInfo::AUTOSELECTED) continue;

		/* Only unselect when WE are the only parent. */
		parents.Clear();
		this->ReverseLookupDependency(parents, c);

		/* First check whether anything depends on us */
		int sel_count = 0;
		bool force_selection = false;
		for (ConstContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
			if ((*iter)->IsSelected()) sel_count++;
			if ((*iter)->state == ContentInfo::SELECTED) force_selection = true;
		}
		if (sel_count == 0) {
			/* Nothing depends on us */
			this->Unselect(c->id);
			continue;
		}
		/* Something manually selected depends directly on us */
		if (force_selection) continue;

		/* "Flood" search to find all items in the dependency graph*/
		parents.Clear();
		this->ReverseLookupTreeDependency(parents, c);

		/* Is there anything that is "force" selected?, if so... we're done. */
		for (ConstContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
			if ((*iter)->state != ContentInfo::SELECTED) continue;

			force_selection = true;
			break;
		}

		/* So something depended directly on us */
		if (force_selection) continue;

		/* Nothing depends on us, mark the whole graph as unselected.
		 * After that's done run over them once again to test their children
		 * to unselect. Don't do it immediately because it'll do exactly what
		 * we're doing now. */
		for (ConstContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
			const ContentInfo *c = *iter;
			if (c->state == ContentInfo::AUTOSELECTED) this->Unselect(c->id);
		}
		for (ConstContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
			this->CheckDependencyState(this->GetContent((*iter)->id));
		}
	}
}

/** Clear all downloaded content information. */
void ClientNetworkContentSocketHandler::Clear()
{
	for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) delete *iter;

	this->infos.Clear();
	this->requested.Clear();
}

/*** CALLBACK ***/

void ClientNetworkContentSocketHandler::OnConnect(bool success)
{
	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); /* nothing */) {
		ContentCallback *cb = *iter;
		cb->OnConnect(success);
		if (iter != this->callbacks.End() && *iter == cb) iter++;
	}
}

void ClientNetworkContentSocketHandler::OnDisconnect()
{
	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); /* nothing */) {
		ContentCallback *cb = *iter;
		cb->OnDisconnect();
		if (iter != this->callbacks.End() && *iter == cb) iter++;
	}
}

void ClientNetworkContentSocketHandler::OnReceiveContentInfo(const ContentInfo *ci)
{
	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); /* nothing */) {
		ContentCallback *cb = *iter;
		cb->OnReceiveContentInfo(ci);
		if (iter != this->callbacks.End() && *iter == cb) iter++;
	}
}

void ClientNetworkContentSocketHandler::OnDownloadProgress(const ContentInfo *ci, uint bytes)
{
	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); /* nothing */) {
		ContentCallback *cb = *iter;
		cb->OnDownloadProgress(ci, bytes);
		if (iter != this->callbacks.End() && *iter == cb) iter++;
	}
}

void ClientNetworkContentSocketHandler::OnDownloadComplete(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci != NULL) {
		ci->state = ContentInfo::ALREADY_HERE;
	}

	for (ContentCallback **iter = this->callbacks.Begin(); iter != this->callbacks.End(); /* nothing */) {
		ContentCallback *cb = *iter;
		cb->OnDownloadComplete(cid);
		if (iter != this->callbacks.End() && *iter == cb) iter++;
	}
}

#endif /* ENABLE_NETWORK */
