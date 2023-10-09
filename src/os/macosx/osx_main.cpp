/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unix_main.cpp Main entry for Mac OSX. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../crashlog.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"

#include <time.h>
#include <signal.h>

#include "macos.h"

#include "../../safeguards.h"

void CocoaSetupAutoreleasePool();
void CocoaReleaseAutoreleasePool();

int CDECL main(int argc, char *argv[])
{
	/* Make sure our arguments contain only valid UTF-8 characters. */
	for (int i = 0; i < argc; i++) StrMakeValidInPlace(argv[i]);

	CocoaSetupAutoreleasePool();
	/* This is passed if we are launched by double-clicking */
	if (argc >= 2 && strncmp(argv[1], "-psn", 4) == 0) {
		argv[1] = nullptr;
		argc = 1;
	}

	CrashLog::InitialiseCrashLog();

	SetRandomSeed(time(nullptr));

	signal(SIGPIPE, SIG_IGN);

	int ret = openttd_main(argc, argv);

	CocoaReleaseAutoreleasePool();

	return ret;
}
