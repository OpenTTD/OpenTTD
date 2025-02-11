/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot_type.h Types related to screenshot providers. */

#ifndef SCREENSHOT_TYPE_H
#define SCREENSHOT_TYPE_H

#include "gfx_type.h"
#include "provider_manager.h"

/**
 * Callback function signature for generating lines of pixel data to be written to the screenshot file.
 * @param userdata Pointer to user data.
 * @param buf      Destination buffer.
 * @param y        Line number of the first line to write.
 * @param pitch    Number of pixels to write (1 byte for 8bpp, 4 bytes for 32bpp). @see Colour
 * @param n        Number of lines to write.
 */
using ScreenshotCallback = void(void *userdata, void *buf, uint y, uint pitch, uint n);

/** Base interface for a SoundLoader implementation. */
class ScreenshotProvider : public PriorityBaseProvider<ScreenshotProvider> {
public:
	ScreenshotProvider(std::string_view name, std::string_view description, int priority) : PriorityBaseProvider<ScreenshotProvider>(name, description, priority)
	{
		ProviderManager<ScreenshotProvider>::Register(*this);
	}

	virtual ~ScreenshotProvider()
	{
		ProviderManager<ScreenshotProvider>::Unregister(*this);
	}

	virtual bool MakeImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette) = 0;
};

#endif /* SCREENSHOT_TYPE_H */
