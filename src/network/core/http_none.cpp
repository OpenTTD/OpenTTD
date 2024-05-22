/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file http_emscripten.cpp Emscripten-based implementation for HTTP requests.
 */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../rev.h"
#include "../network_internal.h"

#include "http.h"

#include "../../safeguards.h"

/* static */ void NetworkHTTPSocketHandler::Connect(const std::string &, HTTPCallback *callback, const std::string)
{
	/* No valid HTTP backend was compiled in, so we fail all HTTP requests. */
	callback->OnFailure();
}

/* static */ void NetworkHTTPSocketHandler::HTTPReceive()
{
}

void NetworkHTTPInitialize()
{
}

void NetworkHTTPUninitialize()
{
}
