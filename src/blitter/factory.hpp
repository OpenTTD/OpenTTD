/* $Id$ */

/** @file factory.hpp Factory to 'query' all available blitters. */

#ifndef BLITTER_FACTORY_HPP
#define BLITTER_FACTORY_HPP

#include "base.hpp"
#include "../debug.h"
#include "../string_func.h"
#include <string>
#include <map>

/**
 * The base factory, keeping track of all blitters.
 */
class BlitterFactoryBase {
private:
	char *name;
	typedef std::map<std::string, BlitterFactoryBase *> Blitters;

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
		free(this->name);
	}

	/**
	 * Find the requested blitter and return his class.
	 * @param name the blitter to select.
	 * @post Sets the blitter so GetCurrentBlitter() returns it too.
	 */
	static Blitter *SelectBlitter(const char *name)
	{
		const char *default_blitter = "8bpp-optimized";

		if (GetBlitters().size() == 0) return NULL;
		const char *bname = (StrEmpty(name)) ? default_blitter : name;

		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactoryBase *b = (*it).second;
			if (strcasecmp(bname, b->name) == 0) {
				Blitter *newb = b->CreateInstance();
				delete *GetActiveBlitter();
				*GetActiveBlitter() = newb;

				DEBUG(driver, 1, "Successfully %s blitter '%s'",StrEmpty(name) ? "probed" : "loaded", bname);
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
		p += snprintf(p, last - p, "List of blitters:\n");
		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactoryBase *b = (*it).second;
			p += snprintf(p, last - p, "%18s: %s\n", b->name, b->GetDescription());
		}
		p += snprintf(p, last - p, "\n");

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

extern char _ini_blitter[32];

#endif /* BLITTER_FACTORY_HPP */
