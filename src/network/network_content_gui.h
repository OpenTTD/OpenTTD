/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content_gui.h User interface for downloading files. */

#ifndef NETWORK_CONTENT_GUI_H
#define NETWORK_CONTENT_GUI_H

#include "network_content.h"
#include "../window_gui.h"
#include "../widgets/network_content_widget.h"

/** Base window for showing the download status of content */
class BaseNetworkContentDownloadStatusWindow : public Window, ContentCallback {
protected:
	uint total_bytes;      ///< Number of bytes to download
	uint downloaded_bytes; ///< Number of bytes downloaded
	uint total_files;      ///< Number of files to download
	uint downloaded_files; ///< Number of files downloaded

	uint32_t cur_id;    ///< The current ID of the downloaded file
	std::string name; ///< The current name of the downloaded file

public:
	/**
	 * Create the window with the given description.
	 * @param desc  The description of the window.
	 */
	BaseNetworkContentDownloadStatusWindow(WindowDesc *desc);

	void Close([[maybe_unused]] int data = 0) override;
	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnDownloadProgress(const ContentInfo *ci, int bytes) override;
};

void BuildContentTypeStringList();

#endif /* NETWORK_CONTENT_GUI_H */
