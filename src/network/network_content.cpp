/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_content.cpp Content sending/receiving part of the network protocol. */

#include "../stdafx.h"
#include "../rev.h"
#include "../ai/ai.hpp"
#include "../game/game.hpp"
#include "../window_func.h"
#include "../error.h"
#include "../fileio_func.h"
#include "../base_media_base.h"
#include "../base_media_graphics.h"
#include "../base_media_music.h"
#include "../base_media_sounds.h"
#include "../settings_type.h"
#include "../strings_func.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"
#include "../core/string_consumer.hpp"
#include "network_content.h"

#include "table/strings.h"

#if defined(WITH_ZLIB)
#	include <zlib.h>
#	if defined(_WIN32)
		/* Required for: dup, fileno, close */
#		include <io.h>
#	endif
#endif

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#include "../safeguards.h"

extern bool HasScenario(const ContentInfo &ci, bool md5sum);

/** The client we use to connect to the server. */
ClientNetworkContentSocketHandler _network_content_client;

/** Wrapper function for the HasProc */
static bool HasGRFConfig(const ContentInfo &ci, bool md5sum)
{
	return FindGRFConfig(std::byteswap(ci.unique_id), md5sum ? FGCM_EXACT : FGCM_ANY, md5sum ? &ci.md5sum : nullptr) != nullptr;
}

/**
 * Check whether a function piece of content is locally known.
 * Matches on the unique ID and possibly the MD5 checksum.
 * @param ci     the content info to search for
 * @param md5sum also match the MD5 checksum?
 * @return true iff it's known
 */
using HasContentProc = bool(const ContentInfo &ci, bool md5sum);

/**
 * Get the has-content check function for the given content type.
 * @param type Content type to get check function for.
 * @return Check function pointer.
 */
static HasContentProc *GetHasContentProcforContentType(ContentType type)
{
	switch (type) {
		case CONTENT_TYPE_NEWGRF: return HasGRFConfig;
		case CONTENT_TYPE_BASE_GRAPHICS: return BaseGraphics::HasSet;
		case CONTENT_TYPE_BASE_MUSIC: return BaseMusic::HasSet;
		case CONTENT_TYPE_BASE_SOUNDS: return BaseSounds::HasSet;
		case CONTENT_TYPE_AI: return AI::HasAI;
		case CONTENT_TYPE_AI_LIBRARY: return AI::HasAILibrary;
		case CONTENT_TYPE_GAME: return Game::HasGame;
		case CONTENT_TYPE_GAME_LIBRARY: return Game::HasGameLibrary;
		case CONTENT_TYPE_SCENARIO: return HasScenario;
		case CONTENT_TYPE_HEIGHTMAP: return HasScenario;
		default: return nullptr;
	}
}

bool ClientNetworkContentSocketHandler::Receive_SERVER_INFO(Packet &p)
{
	auto ci = std::make_unique<ContentInfo>();
	ci->type     = (ContentType)p.Recv_uint8();
	ci->id       = (ContentID)p.Recv_uint32();
	ci->filesize = p.Recv_uint32();

	ci->name        = p.Recv_string(NETWORK_CONTENT_NAME_LENGTH);
	ci->version     = p.Recv_string(NETWORK_CONTENT_VERSION_LENGTH);
	ci->url         = p.Recv_string(NETWORK_CONTENT_URL_LENGTH);
	ci->description = p.Recv_string(NETWORK_CONTENT_DESC_LENGTH, {StringValidationSetting::ReplaceWithQuestionMark, StringValidationSetting::AllowNewline});

	ci->unique_id = p.Recv_uint32();
	p.Recv_bytes(ci->md5sum);

	uint dependency_count = p.Recv_uint8();
	ci->dependencies.reserve(dependency_count);
	for (uint i = 0; i < dependency_count; i++) {
		ContentID dependency_cid = (ContentID)p.Recv_uint32();
		ci->dependencies.push_back(dependency_cid);
		this->reverse_dependency_map.emplace(dependency_cid, ci->id);
	}

	uint tag_count = p.Recv_uint8();
	ci->tags.reserve(tag_count);
	for (uint i = 0; i < tag_count; i++) ci->tags.push_back(p.Recv_string(NETWORK_CONTENT_TAG_LENGTH));

	if (!ci->IsValid()) {
		this->CloseConnection();
		return false;
	}

	HasContentProc *proc = GetHasContentProcforContentType(ci->type);
	if (proc != nullptr) {
		if (proc(*ci, true)) {
			ci->state = ContentInfo::State::AlreadyHere;
		} else {
			ci->state = ContentInfo::State::Unselected;
			if (proc(*ci, false)) ci->upgrade = true;
		}
	} else {
		ci->state = ContentInfo::State::Unselected;
	}

	/* Something we don't have and has filesize 0 does not exist in the system */
	if (ci->state == ContentInfo::State::Unselected && ci->filesize == 0) ci->state = ContentInfo::State::DoesNotExist;

	/* Do we already have a stub for this? */
	for (const auto &ici : this->infos) {
		if (ici->type == ci->type && ici->unique_id == ci->unique_id && ci->md5sum == ici->md5sum) {
			/* Preserve the name if possible */
			if (ci->name.empty()) ci->name = ici->name;
			if (ici->IsSelected()) ci->state = ici->state;

			/*
			 * As ici might be selected by the content window we cannot delete that.
			 * However, we want to keep most of the values of ci, except the values
			 * we (just) already preserved.
			 */
			*ici = *ci;

			this->OnReceiveContentInfo(*ici);
			return true;
		}
	}

	/* Missing content info? Don't list it */
	if (ci->filesize == 0) {
		return true;
	}

	ContentInfo *info = this->infos.emplace_back(std::move(ci)).get();

	/* Incoming data means that we might need to reconsider dependencies */
	ConstContentVector parents;
	this->ReverseLookupTreeDependency(parents, info);
	for (const ContentInfo *ici : parents) {
		this->CheckDependencyState(*ici);
	}

	this->OnReceiveContentInfo(*info);

	return true;
}

/**
 * Request the content list for the given type.
 * @param type The content type to request the list for.
 */
void ClientNetworkContentSocketHandler::RequestContentList(ContentType type)
{
	if (type == CONTENT_TYPE_END) {
		this->RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
		this->RequestContentList(CONTENT_TYPE_BASE_MUSIC);
		this->RequestContentList(CONTENT_TYPE_BASE_SOUNDS);
		this->RequestContentList(CONTENT_TYPE_SCENARIO);
		this->RequestContentList(CONTENT_TYPE_HEIGHTMAP);
		this->RequestContentList(CONTENT_TYPE_AI);
		this->RequestContentList(CONTENT_TYPE_AI_LIBRARY);
		this->RequestContentList(CONTENT_TYPE_GAME);
		this->RequestContentList(CONTENT_TYPE_GAME_LIBRARY);
		this->RequestContentList(CONTENT_TYPE_NEWGRF);
		return;
	}

	this->Connect();

	auto p = std::make_unique<Packet>(this, PACKET_CONTENT_CLIENT_INFO_LIST);
	p->Send_uint8 ((uint8_t)type);
	p->Send_uint32(0xffffffff);
	p->Send_uint8 (1);
	p->Send_string("vanilla");
	p->Send_string(_openttd_content_version);

	/* Patchpacks can extend the list with one. In BaNaNaS metadata you can
	 * add a branch in the 'compatibility' list, to filter on this. If you want
	 * your patchpack to be mentioned in the BaNaNaS web-interface, create an
	 * issue on https://github.com/OpenTTD/bananas-api asking for this.

	p->Send_string("patchpack"); // Or what-ever the name of your patchpack is.
	p->Send_string(_openttd_content_version_patchpack);

	 */

	this->SendPacket(std::move(p));
}

/**
 * Request the content list for a given number of content IDs.
 * @param count The number of IDs to request.
 * @param content_ids The unique identifiers of the content to request information about.
 */
void ClientNetworkContentSocketHandler::RequestContentList(std::span<const ContentID> content_ids)
{
	/* We can "only" send a limited number of IDs in a single packet.
	 * A packet begins with the packet size and a byte for the type.
	 * Then this packet adds a uint16_t for the count in this packet.
	 * The rest of the packet can be used for the IDs. */
	static constexpr size_t MAX_CONTENT_IDS_PER_PACKET = (TCP_MTU - sizeof(PacketSize) - sizeof(uint8_t) - sizeof(uint16_t)) / sizeof(uint32_t);

	if (content_ids.empty()) return;

	this->Connect();

	for (auto it = std::begin(content_ids); it != std::end(content_ids); /* nothing */) {
		auto last = std::ranges::next(it, MAX_CONTENT_IDS_PER_PACKET, std::end(content_ids));

		auto p = std::make_unique<Packet>(this, PACKET_CONTENT_CLIENT_INFO_ID, TCP_MTU);
		p->Send_uint16(std::distance(it, last));

		for (; it != last; ++it) {
			p->Send_uint32(*it);
		}

		this->SendPacket(std::move(p));
	}
}

/**
 * Request the content list for a list of content.
 * @param cv List with unique IDs and MD5 checksums.
 * @param send_md5sum Whether we want a MD5 checksum matched set of files or not.
 */
void ClientNetworkContentSocketHandler::RequestContentList(ContentVector *cv, bool send_md5sum)
{
	if (cv == nullptr) return;

	this->Connect();

	assert(cv->size() < 255);
	assert(cv->size() < (TCP_MTU - sizeof(PacketSize) - sizeof(uint8_t) - sizeof(uint8_t)) /
			(sizeof(uint8_t) + sizeof(uint32_t) + (send_md5sum ? MD5_HASH_BYTES : 0)));

	auto p = std::make_unique<Packet>(this, send_md5sum ? PACKET_CONTENT_CLIENT_INFO_EXTID_MD5 : PACKET_CONTENT_CLIENT_INFO_EXTID, TCP_MTU);
	p->Send_uint8((uint8_t)cv->size());

	for (const auto &ci : *cv) {
		p->Send_uint8((uint8_t)ci->type);
		p->Send_uint32(ci->unique_id);
		if (!send_md5sum) continue;
		p->Send_bytes(ci->md5sum);
	}

	this->SendPacket(std::move(p));

	for (auto &ci : *cv) {
		bool found = false;
		for (const auto &ci2 : this->infos) {
			if (ci->type == ci2->type && ci->unique_id == ci2->unique_id &&
					(!send_md5sum || ci->md5sum == ci2->md5sum)) {
				found = true;
				break;
			}
		}
		if (!found) {
			this->infos.push_back(std::move(ci));
		}
	}
}

/**
 * Actually begin downloading the content we selected.
 * @param[out] files The number of files we are going to download.
 * @param[out] bytes The number of bytes we are going to download.
 * @param fallback Whether to use the fallback or not.
 */
void ClientNetworkContentSocketHandler::DownloadSelectedContent(uint &files, uint &bytes, bool fallback)
{
	bytes = 0;

	ContentIDList content;
	for (const auto &ci : this->infos) {
		if (!ci->IsSelected() || ci->state == ContentInfo::State::AlreadyHere) continue;

		content.push_back(ci->id);
		bytes += ci->filesize;
	}

	files = (uint)content.size();

	/* If there's nothing to download, do nothing. */
	if (files == 0) return;

	this->is_cancelled = false;

	if (fallback) {
		this->DownloadSelectedContentFallback(content);
	} else {
		this->DownloadSelectedContentHTTP(content);
	}
}

/**
 * Initiate downloading the content over HTTP.
 * @param content The content to download.
 */
void ClientNetworkContentSocketHandler::DownloadSelectedContentHTTP(const ContentIDList &content)
{
	std::string content_request;
	for (const ContentID &id : content) {
		format_append(content_request, "{}\n", id);
	}

	this->http_response_index = -1;

	NetworkHTTPSocketHandler::Connect(NetworkContentMirrorUriString(), this, std::move(content_request));
}

/**
 * Initiate downloading the content over the fallback protocol.
 * @param content The content to download.
 */
void ClientNetworkContentSocketHandler::DownloadSelectedContentFallback(const ContentIDList &content)
{
	uint count = (uint)content.size();
	const ContentID *content_ids = content.data();
	this->Connect();

	while (count > 0) {
		/* We can "only" send a limited number of IDs in a single packet.
		 * A packet begins with the packet size and a byte for the type.
		 * Then this packet adds a uint16_t for the count in this packet.
		 * The rest of the packet can be used for the IDs. */
		uint p_count = std::min<uint>(count, (TCP_MTU - sizeof(PacketSize) - sizeof(uint8_t) - sizeof(uint16_t)) / sizeof(uint32_t));

		auto p = std::make_unique<Packet>(this, PACKET_CONTENT_CLIENT_CONTENT, TCP_MTU);
		p->Send_uint16(p_count);

		for (uint i = 0; i < p_count; i++) {
			p->Send_uint32(content_ids[i]);
		}

		this->SendPacket(std::move(p));
		count -= p_count;
		content_ids += p_count;
	}
}

/**
 * Determine the full filename of a piece of content information
 * @param ci         the information to get the filename from
 * @param compressed should the filename end with .gz?
 * @return a statically allocated buffer with the filename or
 *         nullptr when no filename could be made.
 */
static std::string GetFullFilename(const ContentInfo &ci, bool compressed)
{
	Subdirectory dir = GetContentInfoSubDir(ci.type);
	if (dir == NO_DIRECTORY) return {};

	std::string buf = FioGetDirectory(SP_AUTODOWNLOAD_DIR, dir);
	buf += ci.filename;
	buf += compressed ? ".tar.gz" : ".tar";

	return buf;
}

/**
 * Gunzip a given file and remove the .gz if successful.
 * @param ci container with filename
 * @return true if the gunzip completed
 */
static bool GunzipFile(const ContentInfo &ci)
{
#if defined(WITH_ZLIB)
	bool ret = true;

	/* Need to open the file with fopen() to support non-ASCII on Windows. */
	auto ftmp = FileHandle::Open(GetFullFilename(ci, true), "rb");
	if (!ftmp.has_value()) return false;
	/* Duplicate the handle, and close the FILE*, to avoid double-closing the handle later. */
	int fdup = dup(fileno(*ftmp));
	gzFile fin = gzdopen(fdup, "rb");
	ftmp.reset();

	auto fout = FileHandle::Open(GetFullFilename(ci, false), "wb");

	if (fin == nullptr || !fout.has_value()) {
		ret = false;
	} else {
		uint8_t buff[8192];
		for (;;) {
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
			if (read < 0 || static_cast<size_t>(read) != fwrite(buff, 1, read, *fout)) {
				/* If gzread() returns -1, there was an error in archive */
				ret = false;
				break;
			}
			/* DO NOT DO THIS! It will fail to detect broken archive with 1.2.3.3!
			 * if (read < sizeof(buff)) break; */
		}
	}

	if (fin != nullptr) {
		gzclose(fin);
	} else if (fdup != -1) {
		/* Failing gzdopen does not close the passed file descriptor. */
		close(fdup);
	}

	return ret;
#else
	NOT_REACHED();
#endif /* defined(WITH_ZLIB) */
}

bool ClientNetworkContentSocketHandler::Receive_SERVER_CONTENT(Packet &p)
{
	if (!this->cur_file.has_value()) {
		/* When we haven't opened a file this must be our first packet with metadata. */
		this->cur_info = std::make_unique<ContentInfo>();
		this->cur_info->type     = (ContentType)p.Recv_uint8();
		this->cur_info->id       = (ContentID)p.Recv_uint32();
		this->cur_info->filesize = p.Recv_uint32();
		this->cur_info->filename = p.Recv_string(NETWORK_CONTENT_FILENAME_LENGTH);

		if (!this->BeforeDownload()) {
			this->CloseConnection();
			return false;
		}
	} else {
		/* We have a file opened, thus are downloading internal content */
		ssize_t to_read = p.RemainingBytesToTransfer();
		auto write_to_disk = [this](std::span<const uint8_t> buffer) {
			return fwrite(buffer.data(), 1, buffer.size(), *this->cur_file);
		};
		if (to_read != 0 && p.TransferOut(write_to_disk) != to_read) {
			CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD);
			ShowErrorMessage(
				GetEncodedString(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD),
				GetEncodedString(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE),
				WL_ERROR);
			this->CloseConnection();
			this->cur_file.reset();

			return false;
		}

		this->OnDownloadProgress(*this->cur_info, (int)to_read);

		if (to_read == 0) this->AfterDownload();
	}

	return true;
}

/**
 * Handle the opening of the file before downloading.
 * @return false on any error.
 */
bool ClientNetworkContentSocketHandler::BeforeDownload()
{
	if (!this->cur_info->IsValid()) {
		this->cur_info.reset();
		return false;
	}

	if (this->cur_info->filesize != 0) {
		/* The filesize is > 0, so we are going to download it */
		std::string filename = GetFullFilename(*this->cur_info, true);
		if (filename.empty() || !(this->cur_file = FileHandle::Open(filename, "wb")).has_value()) {
			/* Unless that fails of course... */
			CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD);
			ShowErrorMessage(
				GetEncodedString(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD),
				GetEncodedString(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_FILE_NOT_WRITABLE),
				WL_ERROR);
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
	this->cur_file.reset();

	if (GunzipFile(*this->cur_info)) {
		FioRemove(GetFullFilename(*this->cur_info, true));

		Subdirectory sd = GetContentInfoSubDir(this->cur_info->type);
		if (sd == NO_DIRECTORY) NOT_REACHED();

		TarScanner ts;
		std::string fname = GetFullFilename(*this->cur_info, false);
		ts.AddFile(sd, fname);

		if (this->cur_info->type == CONTENT_TYPE_BASE_MUSIC) {
			/* Music can't be in a tar. So extract the tar! */
			ExtractTar(fname, BASESET_DIR);
			FioRemove(fname);
		}

#ifdef __EMSCRIPTEN__
		EM_ASM(if (window["openttd_syncfs"]) openttd_syncfs());
#endif

		this->OnDownloadComplete(this->cur_info->id);
	} else {
		ShowErrorMessage(GetEncodedString(STR_CONTENT_ERROR_COULD_NOT_EXTRACT), {}, WL_ERROR);
	}
}

bool ClientNetworkContentSocketHandler::IsCancelled() const
{
	return this->is_cancelled;
}

/* Also called to just clean up the mess. */
void ClientNetworkContentSocketHandler::OnFailure()
{
	this->http_response.clear();
	this->http_response.shrink_to_fit();
	this->http_response_index = -2;

	if (this->cur_file.has_value()) {
		this->OnDownloadProgress(*this->cur_info, -1);

		this->cur_file.reset();
	}

	/* If we fail, download the rest via the 'old' system. */
	if (!this->is_cancelled) {
		uint files, bytes;

		this->DownloadSelectedContent(files, bytes, true);
	}
}

void ClientNetworkContentSocketHandler::OnReceiveData(std::unique_ptr<char[]> data, size_t length)
{
	assert(data.get() == nullptr || length != 0);

	/* Ignore any latent data coming from a connection we closed. */
	if (this->http_response_index == -2) {
		return;
	}

	if (this->http_response_index == -1) {
		if (data != nullptr) {
			/* Append the rest of the response. */
			this->http_response.insert(this->http_response.end(), data.get(), data.get() + length);
			return;
		} else {
			/* Make sure the response is properly terminated. */
			this->http_response.push_back('\0');

			/* And prepare for receiving the rest of the data. */
			this->http_response_index = 0;
		}
	}

	if (data != nullptr) {
		/* We have data, so write it to the file. */
		if (fwrite(data.get(), 1, length, *this->cur_file) != length) {
			/* Writing failed somehow, let try via the old method. */
			this->OnFailure();
		} else {
			/* Just received the data. */
			this->OnDownloadProgress(*this->cur_info, (int)length);
		}

		/* Nothing more to do now. */
		return;
	}

	if (this->cur_file.has_value()) {
		/* We've finished downloading a file. */
		this->AfterDownload();
	}

	if ((uint)this->http_response_index >= this->http_response.size()) {
		/* It's not a real failure, but if there's
		 * nothing more to download it helps with
		 * cleaning up the stuff we allocated. */
		this->OnFailure();
		return;
	}

	/* When we haven't opened a file this must be our first packet with metadata. */
	this->cur_info = std::make_unique<ContentInfo>();

	try {
		for (;;) {
			std::string_view buffer{this->http_response.data(), this->http_response.size()};
			buffer.remove_prefix(this->http_response_index);
			auto len = buffer.find('\n');
			if (len == std::string_view::npos) throw std::exception{};
			/* Update the index for the next one */
			this->http_response_index += static_cast<int>(len + 1);

			StringConsumer consumer{buffer.substr(0, len)};

			/* Read the ID */
			this->cur_info->id = static_cast<ContentID>(consumer.ReadIntegerBase<uint>(10));
			if (!consumer.ReadIf(",")) throw std::exception{};

			/* Read the type */
			this->cur_info->type = static_cast<ContentType>(consumer.ReadIntegerBase<uint>(10));
			if (!consumer.ReadIf(",")) throw std::exception{};

			/* Read the file size */
			this->cur_info->filesize = consumer.ReadIntegerBase<uint32_t>(10);
			if (!consumer.ReadIf(",")) throw std::exception{};

			/* Read the URL */
			auto url = consumer.GetLeftData();

			/* Is it a fallback URL? If so, just continue with the next one. */
			if (consumer.ReadIf("ottd")) {
				/* Have we gone through all lines? */
				if (static_cast<size_t>(this->http_response_index) >= this->http_response.size()) throw std::exception{};
				continue;
			}

			consumer.SkipUntilChar('/', StringConsumer::KEEP_SEPARATOR);
			std::string_view filename;
			/* Skip all but the last part. There must be at least one / though */
			do {
				if (!consumer.ReadIf("/")) throw std::exception{};
				filename = consumer.ReadUntilChar('/', StringConsumer::KEEP_SEPARATOR);
			} while (consumer.AnyBytesLeft());

			/* Remove the extension from the string. */
			for (uint i = 0; i < 2; i++) {
				auto pos = filename.find_last_of('.');
				if (pos == std::string::npos) throw std::exception{};
				filename = filename.substr(0, pos);
			}

			/* Copy the string, without extension, to the filename. */
			this->cur_info->filename = filename;

			/* Request the next file. */
			if (!this->BeforeDownload()) throw std::exception{};

			NetworkHTTPSocketHandler::Connect(url, this);
			break;
		}
	} catch (const std::exception&) {
		this->OnFailure();
	}
}

/** Connect to the content server. */
class NetworkContentConnecter : public TCPConnecter {
public:
	/**
	 * Initiate the connecting.
	 * @param address The address of the server.
	 */
	NetworkContentConnecter(std::string_view connection_string) : TCPConnecter(connection_string, NETWORK_CONTENT_SERVER_PORT) {}

	void OnFailure() override
	{
		_network_content_client.is_connecting = false;
		_network_content_client.OnConnect(false);
	}

	void OnConnect(SOCKET s) override
	{
		assert(_network_content_client.sock == INVALID_SOCKET);
		_network_content_client.last_activity = std::chrono::steady_clock::now();
		_network_content_client.is_connecting = false;
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
	if (this->sock != INVALID_SOCKET || this->is_connecting) return;

	this->is_cancelled = false;
	this->is_connecting = true;

	TCPConnecter::Create<NetworkContentConnecter>(NetworkContentServerConnectionString());
}

/**
 * Disconnect from the content server.
 */
NetworkRecvStatus ClientNetworkContentSocketHandler::CloseConnection(bool)
{
	NetworkContentSocketHandler::CloseConnection();

	if (this->sock == INVALID_SOCKET) return NETWORK_RECV_STATUS_OKAY;

	this->CloseSocket();
	this->OnDisconnect();

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Cancel the current download.
 */
void ClientNetworkContentSocketHandler::Cancel(void)
{
	this->is_cancelled = true;
	this->CloseConnection();
}

/**
 * Check whether we received/can send some data from/to the content server and
 * when that's the case handle it appropriately
 */
void ClientNetworkContentSocketHandler::SendReceive()
{
	if (this->sock == INVALID_SOCKET || this->is_connecting) return;

	/* Close the connection to the content server after inactivity; there can still be downloads pending via HTTP. */
	if (std::chrono::steady_clock::now() > this->last_activity + IDLE_TIMEOUT) {
		this->CloseConnection();
		return;
	}

	if (this->CanSendReceive()) {
		if (this->ReceivePackets()) {
			/* Only update activity once a packet is received, instead of every time we try it. */
			this->last_activity = std::chrono::steady_clock::now();
		}
	}

	this->SendPackets();
}

/** Timeout after queueing content for it to try to be requested. */
static constexpr auto CONTENT_QUEUE_TIMEOUT = std::chrono::milliseconds(100);

static TimeoutTimer<TimerWindow> _request_queue_timeout = {CONTENT_QUEUE_TIMEOUT, []() {
	_network_content_client.RequestQueuedContentInfo();
}};

/**
 * Download information of a given Content ID if not already tried
 * @param cid the ID to try
 */
void ClientNetworkContentSocketHandler::DownloadContentInfo(ContentID cid)
{
	/* When we tried to download it already, don't try again */
	if (std::ranges::find(this->requested, cid) != this->requested.end()) return;

	this->requested.push_back(cid);
	this->queued.push_back(cid);
	_request_queue_timeout.Reset();
}

/**
 * Send a content request for queued content info download.
 */
void ClientNetworkContentSocketHandler::RequestQueuedContentInfo()
{
	if (this->queued.empty()) return;

	/* Wait until we've briefly stopped receiving data (which will contain more content) before making the request. */
	if (std::chrono::steady_clock::now() <= this->last_activity + CONTENT_QUEUE_TIMEOUT) {
		_request_queue_timeout.Reset();
		return;
	}

	/* Move the queue locally so more ids can be queued for later. */
	ContentIDList queue;
	queue.swap(this->queued);

	/* Remove ids that have since been received since the request was queued. */
	queue.erase(std::remove_if(std::begin(queue), std::end(queue), [this](ContentID content_id) {
		return std::ranges::find(this->infos, content_id, &ContentInfo::id) != std::end(this->infos);
	}), std::end(queue));

	this->RequestContentList(queue);
}

/**
 * Get the content info based on a ContentID
 * @param cid the ContentID to search for
 * @return the ContentInfo or nullptr if not found
 */
ContentInfo *ClientNetworkContentSocketHandler::GetContent(ContentID cid) const
{
	for (const auto &ci : this->infos) {
		if (ci->id == cid) return ci.get();
	}
	return nullptr;
}


/**
 * Select a specific content id.
 * @param cid the content ID to select
 */
void ClientNetworkContentSocketHandler::Select(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci == nullptr || ci->state != ContentInfo::State::Unselected) return;

	ci->state = ContentInfo::State::Selected;
	this->CheckDependencyState(*ci);
}

/**
 * Unselect a specific content id.
 * @param cid the content ID to deselect
 */
void ClientNetworkContentSocketHandler::Unselect(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci == nullptr || !ci->IsSelected()) return;

	ci->state = ContentInfo::State::Unselected;
	this->CheckDependencyState(*ci);
}

/** Select everything we can select */
void ClientNetworkContentSocketHandler::SelectAll()
{
	for (const auto &ci : this->infos) {
		if (ci->state == ContentInfo::State::Unselected) {
			ci->state = ContentInfo::State::Selected;
			this->CheckDependencyState(*ci);
		}
	}
}

/** Select everything that's an update for something we've got */
void ClientNetworkContentSocketHandler::SelectUpgrade()
{
	for (const auto &ci : this->infos) {
		if (ci->state == ContentInfo::State::Unselected && ci->upgrade) {
			ci->state = ContentInfo::State::Selected;
			this->CheckDependencyState(*ci);
		}
	}
}

/** Unselect everything that we've not downloaded so far. */
void ClientNetworkContentSocketHandler::UnselectAll()
{
	for (const auto &ci : this->infos) {
		if (ci->IsSelected() && ci->state != ContentInfo::State::AlreadyHere) ci->state = ContentInfo::State::Unselected;
	}
}

/** Toggle the state of a content info and check its dependencies */
void ClientNetworkContentSocketHandler::ToggleSelectedState(const ContentInfo &ci)
{
	switch (ci.state) {
		case ContentInfo::State::Selected:
		case ContentInfo::State::Autoselected:
			this->Unselect(ci.id);
			break;

		case ContentInfo::State::Unselected:
			this->Select(ci.id);
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
void ClientNetworkContentSocketHandler::ReverseLookupDependency(ConstContentVector &parents, const ContentInfo &child) const
{
	auto range = this->reverse_dependency_map.equal_range(child.id);

	for (auto iter = range.first; iter != range.second; ++iter) {
		parents.push_back(GetContent(iter->second));
	}
}

/**
 * Reverse lookup the dependencies of all parents over a given child.
 * @param tree  list to store all parents in (is not cleared)
 * @param child the child to search the parents' dependencies for
 */
void ClientNetworkContentSocketHandler::ReverseLookupTreeDependency(ConstContentVector &tree, const ContentInfo *child) const
{
	tree.push_back(child);

	/* First find all direct parents. We can't use the "normal" iterator as
	 * we are including stuff into the vector and as such the vector's data
	 * store can be reallocated (and thus move), which means our iterating
	 * pointer gets invalid. So fall back to the indices. */
	for (size_t i = 0; i < tree.size(); i++) {
		ConstContentVector parents;
		this->ReverseLookupDependency(parents, *tree[i]);

		for (const ContentInfo *ci : parents) {
			include(tree, ci);
		}
	}
}

/**
 * Check the dependencies (recursively) of this content info
 * @param ci the content info to check the dependencies of
 */
void ClientNetworkContentSocketHandler::CheckDependencyState(const ContentInfo &ci)
{
	if (ci.IsSelected() || ci.state == ContentInfo::State::AlreadyHere) {
		/* Selection is easy; just walk all children and set the
		 * autoselected state. That way we can see what we automatically
		 * selected and thus can unselect when a dependency is removed. */
		for (auto &dependency : ci.dependencies) {
			ContentInfo *c = this->GetContent(dependency);
			if (c == nullptr) {
				this->DownloadContentInfo(dependency);
			} else if (c->state == ContentInfo::State::Unselected) {
				c->state = ContentInfo::State::Autoselected;
				this->CheckDependencyState(*c);
			}
		}
		return;
	}

	if (ci.state != ContentInfo::State::Unselected) return;

	/* For unselection we need to find the parents of us. We need to
	 * unselect them. After that we unselect all children that we
	 * depend on and are not used as dependency for us, but only when
	 * we automatically selected them. */
	ConstContentVector parents;
	this->ReverseLookupDependency(parents, ci);
	for (const ContentInfo *c : parents) {
		if (!c->IsSelected()) continue;

		this->Unselect(c->id);
	}

	for (auto &dependency : ci.dependencies) {
		const ContentInfo *c = this->GetContent(dependency);
		if (c == nullptr) {
			DownloadContentInfo(dependency);
			continue;
		}
		if (c->state != ContentInfo::State::Autoselected) continue;

		/* Only unselect when WE are the only parent. */
		parents.clear();
		this->ReverseLookupDependency(parents, *c);

		/* First check whether anything depends on us */
		int sel_count = 0;
		bool force_selection = false;
		for (const ContentInfo *parent_ci : parents) {
			if (parent_ci->IsSelected()) sel_count++;
			if (parent_ci->state == ContentInfo::State::Selected) force_selection = true;
		}
		if (sel_count == 0) {
			/* Nothing depends on us */
			this->Unselect(c->id);
			continue;
		}
		/* Something manually selected depends directly on us */
		if (force_selection) continue;

		/* "Flood" search to find all items in the dependency graph*/
		parents.clear();
		this->ReverseLookupTreeDependency(parents, c);

		/* Is there anything that is "force" selected?, if so... we're done. */
		for (const ContentInfo *parent_ci : parents) {
			if (parent_ci->state != ContentInfo::State::Selected) continue;

			force_selection = true;
			break;
		}

		/* So something depended directly on us */
		if (force_selection) continue;

		/* Nothing depends on us, mark the whole graph as unselected.
		 * After that's done run over them once again to test their children
		 * to unselect. Don't do it immediately because it'll do exactly what
		 * we're doing now. */
		for (const ContentInfo *parent : parents) {
			if (parent->state == ContentInfo::State::Autoselected) this->Unselect(parent->id);
		}
		for (const ContentInfo *parent : parents) {
			this->CheckDependencyState(*this->GetContent(parent->id));
		}
	}
}

/** Clear all downloaded content information. */
void ClientNetworkContentSocketHandler::Clear()
{
	this->infos.clear();
	this->requested.clear();
	this->queued.clear();
	this->reverse_dependency_map.clear();
}

/*** CALLBACK ***/

void ClientNetworkContentSocketHandler::OnConnect(bool success)
{
	for (size_t i = 0; i < this->callbacks.size(); /* nothing */) {
		ContentCallback *cb = this->callbacks[i];
		/* the callback may remove itself from this->callbacks */
		cb->OnConnect(success);
		if (i != this->callbacks.size() && this->callbacks[i] == cb) i++;
	}
}

void ClientNetworkContentSocketHandler::OnDisconnect()
{
	for (size_t i = 0; i < this->callbacks.size(); /* nothing */) {
		ContentCallback *cb = this->callbacks[i];
		cb->OnDisconnect();
		if (i != this->callbacks.size() && this->callbacks[i] == cb) i++;
	}
}

void ClientNetworkContentSocketHandler::OnReceiveContentInfo(const ContentInfo &ci)
{
	for (size_t i = 0; i < this->callbacks.size(); /* nothing */) {
		ContentCallback *cb = this->callbacks[i];
		/* the callback may add items and/or remove itself from this->callbacks */
		cb->OnReceiveContentInfo(ci);
		if (i != this->callbacks.size() && this->callbacks[i] == cb) i++;
	}
}

void ClientNetworkContentSocketHandler::OnDownloadProgress(const ContentInfo &ci, int bytes)
{
	for (size_t i = 0; i < this->callbacks.size(); /* nothing */) {
		ContentCallback *cb = this->callbacks[i];
		cb->OnDownloadProgress(ci, bytes);
		if (i != this->callbacks.size() && this->callbacks[i] == cb) i++;
	}
}

void ClientNetworkContentSocketHandler::OnDownloadComplete(ContentID cid)
{
	ContentInfo *ci = this->GetContent(cid);
	if (ci != nullptr) {
		ci->state = ContentInfo::State::AlreadyHere;
	}

	for (size_t i = 0; i < this->callbacks.size(); /* nothing */) {
		ContentCallback *cb = this->callbacks[i];
		/* the callback may remove itself from this->callbacks */
		cb->OnDownloadComplete(cid);
		if (i != this->callbacks.size() && this->callbacks[i] == cb) i++;
	}
}
