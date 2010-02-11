/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file factory.hpp Factory to 'query' all available blitters. */

#ifndef BLITTER_FACTORY_HPP
#define BLITTER_FACTORY_HPP

#include "base.hpp"
#include "../debug.h"
#include "../string_func.h"
#include "../core/string_compare_type.hpp"
#include <map>

#if defined(WITH_COCOA)
bool QZ_CanDisplay8bpp();
#endif /* defined(WITH_COCOA) */

/**
 * The base factory, keeping track of all blitters.
 */
class BlitterFactoryBase {
private:
	const char *name;

	typedef std::map<const char *, BlitterFactoryBase *, StringCompare> Blitters;

	static Blitters &GetBlitters()
	{
		static Blitters &s_blitters = *new Blitters();
		return s_blitters;
	}

	static Blitter **GetActiveBlitter()
	{
		static Blitter *s_blitter = NULL;
		return &s_blitter;
	}

protected:
	/**
	 * Register a blitter internally, based on his name.
	 * @param name the name of the blitter.
	 * @note an assert() will be trigger if 2 blitters with the same name try to register.
	 */
	void RegisterBlitter(const char *name)
	{
		/* Don't register nameless Blitters */
		if (name == NULL) return;

		this->name = strdup(name);

		std::pair<Blitters::iterator, bool> P = GetBlitters().insert(Blitters::value_type(name, this));
		assert(P.second);
	}

public:
	BlitterFactoryBase() :
		name(NULL)
	{}

	virtual ~BlitterFactoryBase()
	{
		if (this->name == NULL) return;
		GetBlitters().erase(this->name);
		if (GetBlitters().empty()) delete &GetBlitters();
		free((void *)this->name);
	}

	/**
	 * Find the requested blitter and return his class.
	 * @param name the blitter to select.
	 * @post Sets the blitter so GetCurrentBlitter() returns it too.
	 */
	static Blitter *SelectBlitter(const char *name)
	{
#if defined(DEDICATED)
		const char *default_blitter = "null";
#else
		const char *default_blitter = "8bpp-optimized";

#if defined(WITH_COCOA)
		/* Some people reported lack of fullscreen support in 8 bpp mode.
		 * While we prefer 8 bpp since it's faster, we will still have to test for support. */
		if (!QZ_CanDisplay8bpp()) {
			/* The main display can't go to 8 bpp fullscreen mode.
			 * We will have to switch to 32 bpp by default. */
			default_blitter = "32bpp-anim";
		}
#endif /* defined(WITH_COCOA) */
#endif /* defined(DEDICATED) */
		if (GetBlitters().size() == 0) return NULL;
		const char *bname = (StrEmpty(name)) ? default_blitter : name;

		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactoryBase *b = (*it).second;
			if (strcasecmp(bname, b->name) == 0) {
				Blitter *newb = b->CreateInstance();
				delete *GetActiveBlitter();
				*GetActiveBlitter() = newb;

				DEBUG(driver, 1, "Successfully %s blitter '%s'", StrEmpty(name) ? "probed" : "loaded", bname);
				return newb;
			}
		}
		return NULL;
	}

	/**
	 * Get the current active blitter (always set by calling SelectBlitter).
	 */
	static Blitter *GetCurrentBlitter()
	{
		return *GetActiveBlitter();
	}


	static char *GetBlittersInfo(char *p, const char *last)
	{
		p += seprintf(p, last, "List of blitters:\n");
		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactoryBase *b = (*it).second;
			p += seprintf(p, last, "%18s: %s\n", b->name, b->GetDescription());
		}
		p += seprintf(p, last, "\n");

		return p;
	}

	/**
	 * Get a nice description of the blitter-class.
	 */
	virtual const char *GetDescription() = 0;

	/**
	 * Create an instance of this Blitter-class.
	 */
	virtual Blitter *CreateInstance() = 0;
};

/**
 * A template factory, so ->GetName() works correctly. This because else some compiler will complain.
 */
template <class T>
class BlitterFactory: public BlitterFactoryBase {
public:
	BlitterFactory() { this->RegisterBlitter(((T *)this)->GetName()); }

	/**
	 * Get the long, human readable, name for the Blitter-class.
	 */
	const char *GetName();
};

extern char *_ini_blitter;

#endif /* BLITTER_FACTORY_HPP */
