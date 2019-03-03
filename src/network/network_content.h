/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content.h Part of the network protocol handling content distribution. */

#ifndef NETWORK_CONTENT_H
#define NETWORK_CONTENT_H

#include "core/tcp_content.h"
#include "core/tcp_http.h"

/** Vector with content info */
typedef std::vector<ContentInfo *> ContentVector;
/** Vector with constant content info */
typedef std::vector<const ContentInfo *> ConstContentVector;

/** Iterator for the content vector */
typedef ContentInfo **ContentIterator;
/** Iterator for the constant content vector */
typedef const ContentInfo * const * ConstContentIterator;

/** Callbacks for notifying others about incoming data */
struct ContentCallback {
	/**
	 * Callback for when the connection has finished
	 * @param success whether the connection was made or that we failed to make it
	 */
	virtual void OnConnect(bool success) {}

	/**
	 * Callback for when the connection got disconnected.
	 */
	virtual void OnDisconnect() {}

	/**
	 * We received a content info.
	 * @param ci the content info
	 */
	virtual void OnReceiveContentInfo(const ContentInfo *ci) {}

	/**
	 * We have progress in the download of a file
	 * @param ci the content info of the file
	 * @param bytes the number of bytes downloaded since the previous call
	 */
	virtual void OnDownloadProgress(const ContentInfo *ci, int bytes) {}

	/**
	 * We have finished downloading a file
	 * @param cid the ContentID of the downloaded file
	 */
	virtual void OnDownloadComplete(ContentID cid) {}

	/** Silentium */
	virtual ~ContentCallback() {}
};

/**
 * Socket handler for the content server connection
 */
class ClientNetworkContentSocketHandler : public NetworkContentSocketHandler, ContentCallback, HTTPCallback {
protected:
	typedef std::vector<ContentID> ContentIDList; ///< List of content IDs to (possibly) select.
	std::vector<ContentCallback *> callbacks; ///< Callbacks to notify "the world"
	ContentIDList requested;                     ///< ContentIDs we already requested (so we don't do it again)
	ContentVector infos;                         ///< All content info we received
	std::vector<char> http_response;       ///< The HTTP response to the requests we've been doing
	int http_response_index;                     ///< Where we are, in the response, with handling it

	FILE *curFile;        ///< Currently downloaded file
	ContentInfo *curInfo; ///< Information about the currently downloaded file
	bool isConnecting;    ///< Whether we're connecting
	uint32 lastActivity;  ///< The last time there was network activity

	friend class NetworkContentConnecter;

	bool Receive_SERVER_INFO(Packet *p) override;
	bool Receive_SERVER_CONTENT(Packet *p) override;

	ContentInfo *GetContent(ContentID cid);
	void DownloadContentInfo(ContentID cid);

	void OnConnect(bool success) override;
	void OnDisconnect() override;
	void OnReceiveContentInfo(const ContentInfo *ci) override;
	void OnDownloadProgress(const ContentInfo *ci, int bytes) override;
	void OnDownloadComplete(ContentID cid) override;

	void OnFailure() override;
	void OnReceiveData(const char *data, size_t length) override;

	bool BeforeDownload();
	void AfterDownload();

	void DownloadSelectedContentHTTP(const ContentIDList &content);
	void DownloadSelectedContentFallback(const ContentIDList &content);
public:
	/** The idle timeout; when to close the connection because it's idle. */
	static const int IDLE_TIMEOUT = 60 * 1000;

	ClientNetworkContentSocketHandler();
	~ClientNetworkContentSocketHandler();

	void Connect();
	void SendReceive();
	void Close() override;

	void RequestContentList(ContentType type);
	void RequestContentList(uint count, const ContentID *content_ids);
	void RequestContentList(ContentVector *cv, bool send_md5sum = true);

	void DownloadSelectedContent(uint &files, uint &bytes, bool fallback = false);

	void Select(ContentID cid);
	void Unselect(ContentID cid);
	void SelectAll();
	void SelectUpgrade();
	void UnselectAll();
	void ToggleSelectedState(const ContentInfo *ci);

	void ReverseLookupDependency(ConstContentVector &parents, const ContentInfo *child) const;
	void ReverseLookupTreeDependency(ConstContentVector &tree, const ContentInfo *child) const;
	void CheckDependencyState(ContentInfo *ci);

	/** Get the number of content items we know locally. */
	uint Length() const { return this->infos.size(); }
	/** Get the begin of the content inf iterator. */
	ConstContentIterator Begin() const { return this->infos.data(); }
	/** Get the nth position of the content inf iterator. */
	ConstContentIterator Get(uint32 index) const { return this->infos.data() + index; }
	/** Get the end of the content inf iterator. */
	ConstContentIterator End() const { return this->Begin() + this->Length(); }

	void Clear();

	/** Add a callback to this class */
	void AddCallback(ContentCallback *cb) { include(this->callbacks, cb); }
	/** Remove a callback */
	void RemoveCallback(ContentCallback *cb) { this->callbacks.erase(std::find(this->callbacks.begin(), this->callbacks.end(), cb)); }
};

extern ClientNetworkContentSocketHandler _network_content_client;

void ShowNetworkContentListWindow(ContentVector *cv = NULL, ContentType type1 = CONTENT_TYPE_END, ContentType type2 = CONTENT_TYPE_END);

void ShowMissingContentWindow(const struct GRFConfig *list);

#endif /* NETWORK_CONTENT_H */
