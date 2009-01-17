/* $Id$ */

/** @file network_content.h Part of the network protocol handling content distribution. */

#ifndef NETWORK_CONTENT_H
#define NETWORK_CONTENT_H

#if defined(ENABLE_NETWORK)

#include "core/tcp_content.h"
#include "../core/smallvec_type.hpp"

/** Vector with content info */
typedef SmallVector<ContentInfo *, 16> ContentVector;
/** Iterator for the content vector */
typedef ContentInfo **ContentIterator;

/** Callbacks for notifying others about incoming data */
struct ContentCallback {
	/**
	 * We received a content info.
	 * @param ci the content info
	 */
	virtual void OnReceiveContentInfo(ContentInfo *ci) {}

	/**
	 * We have progress in the download of a file
	 * @param ci the content info of the file
	 * @param bytes the number of bytes downloaded since the previous call
	 */
	virtual void OnDownloadProgress(ContentInfo *ci, uint bytes) {}

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
class ClientNetworkContentSocketHandler : public NetworkContentSocketHandler {
protected:
	SmallVector<ContentCallback *, 2> callbacks; ///< Callbacks to notify "the world"

	FILE *curFile;        ///< Currently downloaded file
	ContentInfo *curInfo; ///< Information about the currently downloaded file

	friend ClientNetworkContentSocketHandler *NetworkContent_Connect(ContentCallback *cb);
	friend void NetworkContent_Disconnect(ContentCallback *cb);

	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_INFO);
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_CONTENT);

	ClientNetworkContentSocketHandler(SOCKET s, const struct sockaddr_in *sin);
	~ClientNetworkContentSocketHandler();
public:
	void RequestContentList(ContentType type);
	void RequestContentList(uint count, const ContentID *content_ids);
	void RequestContentList(ContentVector *cv, bool send_md5sum = true);

	void RequestContent(uint count, const uint32 *content_ids);
};

ClientNetworkContentSocketHandler *NetworkContent_Connect(ContentCallback *cb);
void NetworkContent_Disconnect(ContentCallback *cb);
void NetworkContentLoop();

void ShowNetworkContentListWindow(ContentVector *cv = NULL, ContentType type = CONTENT_TYPE_END);

#else
static inline void ShowNetworkContentListWindow() {}
#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CONTENT_H */
