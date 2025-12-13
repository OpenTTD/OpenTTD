/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bootstrap_emscripten_gui.cpp Barely used user interface for bootstrapping OpenTTD, i.e. downloading the required content. */

#if defined(__EMSCRIPTEN__)

#include "stdafx.h"
#include "base_media_base.h"
#include "network/network_content.h"
#include "openttd.h"

#include <emscripten.h>

#include "safeguards.h"

class BootstrapEmscripten : public ContentCallback {
	bool downloading = false;
	uint total_files = 0;
	uint total_bytes = 0;
	uint downloaded_bytes = 0;

public:
	BootstrapEmscripten()
	{
		_network_content_client.AddCallback(this);
		_network_content_client.Connect();
	}

	~BootstrapEmscripten()
	{
		_network_content_client.RemoveCallback(this);
	}

	void OnConnect(bool success) override
	{
		if (!success) {
			EM_ASM({ if (window["openttd_bootstrap_failed"]) openttd_bootstrap_failed(); });
			return;
		}

		/* Once connected, request the metadata. */
		_network_content_client.RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
	}

	void OnReceiveContentInfo(const ContentInfo &ci) override
	{
		if (this->downloading) return;

		/* And once the metadata is received, start downloading it. */
		_network_content_client.Select(ci.id);
		_network_content_client.DownloadSelectedContent(this->total_files, this->total_bytes);
		this->downloading = true;

		EM_ASM({ if (window["openttd_bootstrap"]) openttd_bootstrap($0, $1); }, this->downloaded_bytes, this->total_bytes);
	}

	void OnDownloadProgress(const ContentInfo &, int bytes) override
	{
		/* A negative value means we are resetting; for example, when retrying or using a fallback. */
		if (bytes < 0) {
			this->downloaded_bytes = 0;
		} else {
			this->downloaded_bytes += bytes;
		}

		EM_ASM({ if (window["openttd_bootstrap"]) openttd_bootstrap($0, $1); }, this->downloaded_bytes, this->total_bytes);
	}

	void OnDownloadComplete(ContentID) override
	{
		/* _exit_game is used to break out of the outer video driver's MainLoop. */
		_exit_game = true;

		delete this;
	}
};

void HandleBootstrapGui()
{
	new BootstrapEmscripten();
}

#endif /* __EMSCRIPTEN__ */
