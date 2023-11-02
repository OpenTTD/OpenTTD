/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mock_environment.h Singleton instance to create a mock FontCache/SpriteCache environment. */

#ifndef MOCK_ENVIRONMENT_H
#define MOCK_ENVIRONMENT_H

#include "mock_fontcache.h"
#include "mock_spritecache.h"

/** Singleton class to set up the mock environemnt once. */
class MockEnvironment {
public:
	static MockEnvironment &Instance()
	{
		static MockEnvironment instance;
		return instance;
	}

	MockEnvironment(MockEnvironment const &) = delete;
	void operator=(MockEnvironment const &) = delete;

private:
	MockEnvironment()
	{
		/* Mock SpriteCache initialization is needed for some widget generators. */
		MockGfxLoadSprites();

		/* Mock FontCache initialization is needed for some NWidgetParts. */
		MockFontCache::InitializeFontCaches();
	}
};

#endif /* MOCK_ENVIRONMENT_H */
