/* $Id$ */

/** @file blitter.hpp */

#ifndef BLITTER_HPP
#define BLITTER_HPP

#include "../spriteloader/spriteloader.hpp"
#include "../spritecache.h"
#include <string>
#include <map>

enum BlitterMode {
	BM_NORMAL,
	BM_COLOUR_REMAP,
	BM_TRANSPARENT,
};

/**
 * How all blitters should look like. Extend this class to make your own.
 */
class Blitter {
public:
	struct BlitterParams {
		const void *sprite;      ///< Pointer to the sprite how ever the encoder stored it
		const byte *remap;       ///< XXX -- Temporary storage for remap array

		int skip_left, skip_top; ///< How much pixels of the source to skip on the left and top (based on zoom of dst)
		int width, height;       ///< The width and height in pixels that needs to be drawn to dst
		int sprite_width;        ///< Real width of the sprite
		int sprite_height;       ///< Real height of the sprite
		int left, top;           ///< The offset in the 'dst' in pixels to start drawing

		void *dst;               ///< Destination buffer
		int pitch;               ///< The pitch of the destination buffer
	};

	typedef void *AllocatorProc(size_t size);

	/**
	 * Get the screen depth this blitter works for.
	 *  This is either: 8, 16, 24 or 32.
	 */
	virtual uint8 GetScreenDepth() = 0;

	/**
	 * Draw an image to the screen, given an amount of params defined above.
	 */
	virtual void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) = 0;

	/**
	 * Convert a sprite from the loader to our own format.
	 */
	virtual Sprite *Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator) = 0;

	/**
	 * Get the renderer this class depends on.
	 */
	virtual const char *GetRenderer() = 0;

	virtual ~Blitter() { }
};

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

	virtual ~BlitterFactoryBase() { if (this->name != NULL) GetBlitters().erase(this->name); free(this->name); }

	/**
	 * Find the requested blitter and return his class.
	 * @param name the blitter to select.
	 * @post Sets the blitter so GetCurrentBlitter() returns it too.
	 */
	static Blitter *SelectBlitter(const char *name)
	{
		if (GetBlitters().size() == 0) return NULL;

		Blitters::iterator it = GetBlitters().begin();
		for (; it != GetBlitters().end(); it++) {
			BlitterFactoryBase *b = (*it).second;
			if (strcasecmp(name, b->name) == 0) {
				Blitter *newb = b->CreateInstance();
				*GetActiveBlitter() = newb;
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

#endif /* BLITTER_HPP */
