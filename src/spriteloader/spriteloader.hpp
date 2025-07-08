/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spriteloader.hpp Base for loading sprites. */

#ifndef SPRITELOADER_HPP
#define SPRITELOADER_HPP

#include "../core/alloc_type.hpp"
#include "../core/enum_type.hpp"
#include "../gfx_type.h"
#include "sprite_file_type.hpp"

struct Sprite;

/** The different colour components a sprite can have. */
enum class SpriteComponent : uint8_t {
	RGB     = 0, ///< Sprite has RGB.
	Alpha   = 1, ///< Sprite has alpha.
	Palette = 2, ///< Sprite has palette data.
	End,
};
using SpriteComponents = EnumBitSet<SpriteComponent, uint8_t, SpriteComponent::End>;

/**
 * Key into sprite collections.
 */
class SpriteCollKey {
public:
	bool rtl;
	ZoomLevel zoom;

	inline constexpr SpriteCollKey(ZoomLevel zoom, bool rtl) : rtl(rtl), zoom(zoom) {}

	inline constexpr bool operator==(const SpriteCollKey &rhs) const = default;
	inline constexpr std::strong_ordering operator<=>(const SpriteCollKey &rhs) const = default;

	static SpriteCollKey Root(bool rtl) { return SpriteCollKey(ZoomLevel::Min, rtl); }
};

/**
 * Set of sprite collection keys.
 */
class SpriteCollKeys {
	ZoomLevels ltr, rtl;
public:
	inline constexpr SpriteCollKeys() = default;
	inline constexpr void Set(const SpriteCollKey &sck) { (sck.rtl ? this->rtl : this->ltr).Set(sck.zoom); }
	inline constexpr bool Test(const SpriteCollKey &sck) const { return (sck.rtl ? this->rtl : this->ltr).Test(sck.zoom); }
	inline constexpr bool AnyLtr() const { return this->ltr.Any(); }
	inline constexpr bool AnyRtl() const { return this->rtl.Any(); }
	inline constexpr bool NoLtr() const { return this->ltr.None(); }
	inline constexpr bool NoRtl() const { return this->rtl.None(); }
};

/**
 * Map sprite collection keys to data.
 */
template <class T>
class SpriteCollMap {
	std::array<T, to_underlying(ZoomLevel::End)> ltr{}, rtl{};
public:
	inline constexpr T &operator[](const SpriteCollKey &sck) { return (sck.rtl ? this->rtl : this->ltr)[to_underlying(sck.zoom)]; }
	inline constexpr const T &operator[](const SpriteCollKey &sck) const { return (sck.rtl ? this->rtl : this->ltr)[to_underlying(sck.zoom)]; }

	T &Root(bool rtl) { return (rtl ? this->rtl : this->ltr)[to_underlying(ZoomLevel::Min)]; }
	const T &Root(bool rtl) const { return (rtl ? this->rtl : this->ltr)[to_underlying(ZoomLevel::Min)]; }
};

/**
 * Range of sprite collection keys.
 */
class SpriteCollKeyRange {
public:
	class Iterator {
		ZoomLevel zoom_min, zoom_max;
		SpriteCollKey pos;
	public:
		using value_type = SpriteCollKey;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::bidirectional_iterator_tag;
		using pointer = void;
		using reference = void;

		Iterator(ZoomLevel zoom_min, ZoomLevel zoom_max, SpriteCollKey pos) : zoom_min(zoom_min), zoom_max(zoom_max), pos(pos) {}
		bool operator==(const Iterator &rhs) const { return this->pos == rhs.pos; }
		std::strong_ordering operator<=>(const Iterator &rhs) const { return this->pos <=> rhs.pos; }
		const SpriteCollKey &operator*() const { return this->pos; }

		Iterator& operator++()
		{
			if (!this->pos.rtl && this->pos.zoom == this->zoom_max) {
				this->pos.zoom = this->zoom_min;
				this->pos.rtl = true;
			} else {
				++this->pos.zoom;
			}
			return *this;
		}

		Iterator operator++(int)
		{
			Iterator result = *this;
			++*this;
			return result;
		}

		Iterator& operator--()
		{
			if (this->pos.zoom == this->zoom_min) {
				this->pos.zoom = this->zoom_max;
				this->pos.rtl = false;
			} else {
				--this->pos.zoom;
			}
			return *this;
		}

		Iterator operator--(int)
		{
			Iterator result = *this;
			--*this;
			return result;
		}
	};

	SpriteCollKeyRange(ZoomLevel zoom_min, ZoomLevel zoom_max, bool has_rtl) : zoom_min(zoom_min), zoom_max(zoom_max), has_rtl(has_rtl) {}
	Iterator begin() const { return Iterator{this->zoom_min, this->zoom_max, SpriteCollKey{this->zoom_min, false}}; }
	Iterator end() const { return ++Iterator{this->zoom_min, this->zoom_max, SpriteCollKey{this->zoom_max, this->has_rtl}}; }

private:
	ZoomLevel zoom_min, zoom_max;
	bool has_rtl;
};

/** Interface for the loader of our sprites. */
class SpriteLoader {
public:
	/** Definition of a common pixel in OpenTTD's realm. */
	struct CommonPixel {
		uint8_t r = 0;  ///< Red-channel
		uint8_t g = 0;  ///< Green-channel
		uint8_t b = 0;  ///< Blue-channel
		uint8_t a = 0;  ///< Alpha-channel
		uint8_t m = 0;  ///< Remap-channel
	};

	/**
	 * Structure for passing information from the sprite loader to the blitter.
	 * You can only use this struct once at a time when using AllocateData to
	 * allocate the memory as that will always return the same memory address.
	 * This to prevent thousands of malloc + frees just to load a sprite.
	 */
	struct Sprite {
		uint16_t height;                   ///< Height of the sprite
		uint16_t width;                    ///< Width of the sprite
		int16_t x_offs;                    ///< The x-offset of where the sprite will be drawn
		int16_t y_offs;                    ///< The y-offset of where the sprite will be drawn
		SpriteComponents colours;   ///< The colour components of the sprite with useful information.
		SpriteLoader::CommonPixel *data; ///< The sprite itself

		/**
		 * Allocate the sprite data of this sprite.
		 * @param sck Key to allocate the data for.
		 * @param size the minimum size of the data field.
		 */
		void AllocateData(SpriteCollKey sck, size_t size) { this->data = Sprite::buffer[sck].ZeroAllocate(size); }
	private:
		/** Allocated memory to pass sprite data around */
		static SpriteCollMap<ReusableBuffer<SpriteLoader::CommonPixel>> buffer;
	};

	/**
	 * Type defining a collection of sprites, one for each key.
	 */
	using SpriteCollection = SpriteCollMap<Sprite>;

	/**
	 * Load a sprite from the disk and return a sprite struct which is the same for all loaders.
	 * @param[out] sprite The sprites to fill with data.
	 * @param file The file "descriptor" of the file we read from.
	 * @param file_pos    The position within the file the image begins.
	 * @param sprite_type The type of sprite we're trying to load.
	 * @param load_32bpp  True if 32bpp sprites should be loaded, false for a 8bpp sprite.
	 * @param control_flags Control flags, see SpriteCacheCtrlFlags.
	 * @param[out] avail_8bpp Available 8bpp sprites.
	 * @param[out] avail_32bpp Available 32bpp sprites.
	 * @return Available sprites matching \a load_32bpp.
	 */
	virtual SpriteCollKeys LoadSprite(SpriteLoader::SpriteCollection &sprite, SpriteFile &file, size_t file_pos, SpriteType sprite_type, bool load_32bpp, uint8_t control_flags, SpriteCollKeys &avail_8bpp, SpriteCollKeys &avail_32bpp) = 0;

	virtual ~SpriteLoader() = default;
};

/** Interface for something that can allocate memory for a sprite. */
class SpriteAllocator {
public:
	virtual ~SpriteAllocator() = default;

	/**
	 * Allocate memory for a sprite.
	 * @tparam T Type to return memory as.
	 * @param size Size of memory to allocate in bytes.
	 * @return Pointer to allocated memory.
	 */
	template <typename T>
	T *Allocate(size_t size)
	{
		return static_cast<T *>(this->AllocatePtr(size));
	}

protected:
	/**
	 * Allocate memory for a sprite.
	 * @param size Size of memory to allocate.
	 * @return Pointer to allocated memory.
	 */
	virtual void *AllocatePtr(size_t size) = 0;
};

/** Interface for something that can encode a sprite. */
class SpriteEncoder {
public:

	virtual ~SpriteEncoder() = default;

	/**
	 * Can the sprite encoder make use of RGBA sprites?
	 */
	virtual bool Is32BppSupported() = 0;

	/**
	 * Convert a sprite from the loader to our own format.
	 */
	virtual Sprite *Encode(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, bool has_rtl, SpriteAllocator &allocator) = 0;

	/**
	 * Get the value which the height and width on a sprite have to be aligned by.
	 * @return The needed alignment or 0 if any alignment is accepted.
	 */
	virtual uint GetSpriteAlignment()
	{
		return 0;
	}
};
#endif /* SPRITELOADER_HPP */
