/* $Id$ */

/** @file renderer.hpp */

#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <string>
#include <map>

class Renderer {
public:
	virtual ~Renderer() { }

	/**
	 * Move the destination pointer the requested amount x and y, keeping in mind
	 *  any pitch and bpp of the renderer.
	 * @param video The destination pointer (video-buffer) to scroll.
	 * @param x How much you want to scroll to the right.
	 * @param y How much you want to scroll to the bottom.
	 * @return A new destination pointer moved the the requested place.
	 */
	virtual void *MoveTo(const void *video, int x, int y) = 0;

	/**
	 * Draw a pixel with a given color on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param x The x position within video-buffer.
	 * @param y The y position within video-buffer.
	 * @param color A 8bpp mapping color.
	 */
	virtual void SetPixel(void *video, int x, int y, uint8 color) = 0;

	/**
	 * Draw a pixel with a given color on the video-buffer if there is currently a black pixel.
	 * @param video The destination pointer (video-buffer).
	 * @param x The x position within video-buffer.
	 * @param y The y position within video-buffer.
	 * @param color A 8bpp mapping color.
	 */
	virtual void SetPixelIfEmpty(void *video, int x, int y, uint8 color) = 0;

	/**
	 * Make a single horizontal line in a single color on the video-buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param width The lenght of the line.
	 * @param color A 8bpp mapping color.
	 */
	virtual void SetHorizontalLine(void *video, int width, uint8 color) = 0;

	/**
	 * Copy from a buffer to the screen.
	 * @param video The destionation pointer (video-buffer).
	 * @param src The buffer from which the data will be read.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @param src_pitch The pitch (byte per line) of the source buffer.
	 */
	virtual void CopyFromBuffer(void *video, const void *src, int width, int height, int src_pitch) = 0;

	/**
	 * Copy from the screen to a buffer.
	 * @param video The destination pointer (video-buffer).
	 * @param dst The buffer in which the data will be stored.
	 * @param width The width of the buffer.
	 * @param height The height of the buffer.
	 * @param dst_pitch The pitch (byte per line) of the destination buffer.
	 */
	virtual void CopyToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) = 0;

	/**
	 * Move the videobuffer some places (via memmove).
	 * @param video_dst The destination pointer (video-buffer).
	 * @param video_src The source pointer (video-buffer).
	 * @param width The width of the buffer to move.
	 * @param height The height of the buffer to move.
	 */
	virtual void MoveBuffer(void *video_dst, const void *video_src, int width, int height) = 0;

	/**
	 * Calculate how much memory there is needed for an image of this size in the video-buffer.
	 * @param width The width of the buffer-to-be.
	 * @param height The height of the buffer-to-be.
	 * @return The size needed for the buffer.
	 */
	virtual int BufferSize(int width, int height) = 0;
};

/**
 * The factory, keeping track of all renderers.
 */
class RendererFactoryBase {
private:
	char *name;
	typedef std::map<std::string, RendererFactoryBase *> Renderers;

	static Renderers &GetRenderers()
	{
		static Renderers &s_renderers = *new Renderers();
		return s_renderers;
	}

protected:
	/**
	 * Register a renderer internally, based on his bpp.
	 * @param name the name of the renderer.
	 * @note an assert() will be trigger if 2 renderers with the same bpp try to register.
	 */
	void RegisterRenderer(const char *name)
	{
		/* Don't register nameless Renderers */
		if (name == NULL) return;

		this->name = strdup(name);
		std::pair<Renderers::iterator, bool> P = GetRenderers().insert(Renderers::value_type(name, this));
		assert(P.second);
	}

public:
	RendererFactoryBase() :
		name(NULL)
	{ }

	virtual ~RendererFactoryBase() { if (this->name != NULL) GetRenderers().erase(this->name); free(this->name); }

	/**
	 * Find the requested renderer and return his class-instance.
	 * @param name the renderer to select.
	 */
	static Renderer *SelectRenderer(const char *name)
	{
		if (GetRenderers().size() == 0) return NULL;

		Renderers::iterator it = GetRenderers().begin();
		for (; it != GetRenderers().end(); it++) {
			RendererFactoryBase *r = (*it).second;
			if (strcasecmp(name, r->name) == 0) {
				return r->CreateInstance();
			}
		}
		return NULL;
	}

	/**
	 * Create an instance of this Renderer-class.
	 */
	virtual Renderer *CreateInstance() = 0;
};

/**
 * A template factory, so ->GetBpp() works correctly. This because else some compiler will complain.
 */
template <class T>
class RendererFactory: public RendererFactoryBase {
public:
	RendererFactory() { this->RegisterRenderer(((T *)this)->GetName()); }

	/**
	 * Get the name for this renderer.
	 */
	const char *GetName();
};


#endif /* RENDERER_HPP */
