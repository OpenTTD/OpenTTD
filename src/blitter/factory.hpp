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


/**
 * The base factory, keeping track of all blitters.
 */
class BlitterFactory {
private:
	const std::string name;        ///< The name of the blitter factory.
	const std::string description; ///< The description of the blitter.

	typedef std::map<std::string, BlitterFactory *> Blitters; ///< Map of blitter factories.

	/**
	 * Get the map with currently known blitters.
	 * @return The known blitters.
	 */
	static Blitters &GetBlitters()
	{
		static Blitters &s_blitters = *new Blitters();
		return s_blitters;
	}

	/**
	 * Get the currently active blitter.
	 * @return The currently active blitter.
	 */
	static Blitter **GetActiveBlitter()
	{
		static Blitter *s_blitter = nullptr;
		return &s_blitter;
	}

protected:
	/**
	 * Construct the blitter, and register it.
	 * @param name        The name of the blitter.
	 * @param description A longer description for the blitter.
	 * @param usable      Whether the blitter is usable (on the current computer). For example for disabling SSE blitters when the CPU can't handle them.
	 * @pre name != nullptr.
	 * @pre description != nullptr.
	 * @pre There is no blitter registered with this name.
	 */
	BlitterFactory(const char *name, const char *description, bool usable = true) :
			name(name), description(description)
	{
		if (usable) {
			/*
			 * Only add when the blitter is usable. Do not bail out or
			 * do more special things since the blitters are always
			 * instantiated upon start anyhow and freed upon shutdown.
			 */
			std::pair<Blitters::iterator, bool> P = GetBlitters().insert(Blitters::value_type(this->name, this));
			assert(P.second);
		} else {
			DEBUG(driver, 1, "Not registering blitter %s as it is not usable", name);
		}
	}

public:
	virtual ~BlitterFactory()
	{
		GetBlitters().erase(this->name);
		if (GetBlitters().empty()) delete &GetBlitters();
	}

	/**
	 * Find the requested blitter and return his class.
	 * @param name the blitter to select.
	 * @post Sets the blitter so GetCurrentBlitter() returns it too.
	 */
	static Blitter *SelectBlitter(const std::string &name)
	{
		BlitterFactory *b = GetBlitterFactory(name);
		if (b == nullptr) return nullptr;

		Blitter *newb = b->CreateInstance();
		delete *GetActiveBlitter();
		*GetActiveBlitter() = newb;

		DEBUG(driver, 1, "Successfully %s blitter '%s'", name.empty() ? "probed" : "loaded", newb->GetName());
		return newb;
	}

	/**
	 * Get the blitter factory with the given name.
	 * @param name the blitter factory to select.
	 * @return The blitter factory, or nullptr when there isn't one with the wanted name.
	 */
	static BlitterFactory *GetBlitterFactory(const std::string &name)
	{
#if defined(DEDICATED)
		const char *default_blitter = "null";
#elif defined(WITH_COCOA)
		const char *default_blitter = "32bpp-anim";
#else
		const char *default_blitter = "8bpp-optimized";
#endif
		if (GetBlitters().size() == 0) return nullptr;
		const char *bname = name.empty() ? default_blitter : name.c_str();

		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactory *b = (*it).second;
			if (strcasecmp(bname, b->name.c_str()) == 0) {
				return b;
			}
		}
		return nullptr;
	}

	/**
	 * Get the current active blitter (always set by calling SelectBlitter).
	 */
	static Blitter *GetCurrentBlitter()
	{
		return *GetActiveBlitter();
	}

	/**
	 * Fill a buffer with information about the blitters.
	 * @param p The buffer to fill.
	 * @param last The last element of the buffer.
	 * @return p The location till where we filled the buffer.
	 */
	static char *GetBlittersInfo(char *p, const char *last)
	{
		p += seprintf(p, last, "List of blitters:\n");
		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactory *b = (*it).second;
			p += seprintf(p, last, "%18s: %s\n", b->name.c_str(), b->GetDescription().c_str());
		}
		p += seprintf(p, last, "\n");

		return p;
	}

	/**
	 * Get the long, human readable, name for the Blitter-class.
	 */
	const std::string &GetName() const
	{
		return this->name;
	}

	/**
	 * Get a nice description of the blitter-class.
	 */
	const std::string &GetDescription() const
	{
		return this->description;
	}

	/**
	 * Create an instance of this Blitter-class.
	 */
	virtual Blitter *CreateInstance() = 0;
};

extern std::string _ini_blitter;
extern bool _blitter_autodetected;

#endif /* BLITTER_FACTORY_HPP */
