/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file geometry_type.hpp All geometry types in OpenTTD. */

#ifndef GEOMETRY_TYPE_HPP
#define GEOMETRY_TYPE_HPP

#if defined(__APPLE__)
	/* Mac OS X already has both Rect and Point declared */
#	define Rect OTTD_Rect
#	define Point OTTD_Point
#endif /* __APPLE__ */

/**
 * Determine where to position a centred object.
 * @param min The top or left coordinate.
 * @param max The bottom or right coordinate.
 * @param size The height or width of the object to draw.
 * @return Offset of where to position the object.
 */
inline int CentreBounds(int min, int max, int size)
{
	return (min + max - size + 1) / 2;
}

/** A coordinate with two dimensons. */
template <typename T>
struct Coord2D {
	T x = 0; ///< X coordinate.
	T y = 0; ///< Y coordinate.

	constexpr Coord2D() = default;
	constexpr Coord2D(T x, T y) : x(x), y(y) {}
};

/** A coordinate with three dimensions. */
template <typename T>
struct Coord3D {
	T x = 0; ///< X coordinate.
	T y = 0; ///< Y coordinate.
	T z = 0; ///< Z coordinate.

	constexpr Coord3D() = default;
	constexpr Coord3D(T x, T y, T z) : x(x), y(y), z(z) {}
};

/** Coordinates of a point in 2D */
using Point = Coord2D<int>;

/** Dimensions (a width and height) of a rectangle in 2D */
struct Dimension {
	uint width;
	uint height;

	constexpr Dimension() : width(0), height(0) {}
	constexpr Dimension(uint w, uint h) : width(w), height(h) {}

	bool operator< (const Dimension &other) const
	{
		int x = this->width - other.width;
		if (x != 0) return x < 0;
		return this->height < other.height;
	}

	bool operator== (const Dimension &other) const
	{
		return this->width == other.width && this->height == other.height;
	}
};

/** Padding dimensions to apply to each side of a Rect. */
struct RectPadding {
	uint8_t left = 0;
	uint8_t top = 0;
	uint8_t right = 0;
	uint8_t bottom = 0;

	static const RectPadding zero;

	/**
	 * Get total horizontal padding of RectPadding.
	 * @return total horizontal padding.
	 */
	constexpr uint Horizontal() const { return this->left + this->right; }

	/**
	 * Get total vertical padding of RectPadding.
	 * @return total vertical padding.
	 */
	constexpr uint Vertical() const { return this->top + this->bottom; }
};

inline const RectPadding RectPadding::zero{};

/** Specification of a rectangle with absolute coordinates of all edges */
struct Rect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;

	/**
	 * Get width of Rect.
	 * @return width of Rect.
	 */
	inline int Width() const { return this->right - this->left + 1; }

	/**
	 * Get height of Rect.
	 * @return height of Rect.
	 */
	inline int Height() const { return this->bottom - this->top + 1; }

	/**
	 * Copy and shrink Rect by s pixels.
	 * @param s number of pixels to remove from each side of Rect.
	 * @return the new smaller Rect.
	 */
	[[nodiscard]] inline Rect Shrink(int s) const
	{
		return {this->left + s, this->top + s, this->right - s, this->bottom - s};
	}

	/**
	 * Copy and shrink Rect by h horizontal and v vertical pixels.
	 * @param h number of pixels to remove from left and right sides.
	 * @param v number of pixels to remove from top and bottom sides.
	 * @return the new smaller Rect.
	 */
	[[nodiscard]] inline Rect Shrink(int h, int v) const
	{
		return {this->left + h, this->top + v, this->right - h, this->bottom - v};
	}

	/**
	 * Copy and shrink Rect by pixels.
	 * @param left number of pixels to remove from left side.
	 * @param top number of pixels to remove from top side.
	 * @param right number of pixels to remove from right side.
	 * @param bottom number of pixels to remove from bottom side.
	 * @return the new smaller Rect.
	 */
	[[nodiscard]] inline Rect Shrink(int left, int top, int right, int bottom) const
	{
		return {this->left + left, this->top + top, this->right - right, this->bottom - bottom};
	}

	/**
	 * Copy and shrink Rect by a RectPadding.
	 * @param other RectPadding to remove from each side of Rect.
	 * @return the new smaller Rect.
	 */
	[[nodiscard]] inline Rect Shrink(const RectPadding &other) const
	{
		return {this->left + other.left, this->top + other.top, this->right - other.right, this->bottom - other.bottom};
	}

	/**
	 * Copy and shrink Rect by a different horizontal and vertical RectPadding.
	 * @param horz RectPadding to remove from left and right of Rect.
	 * @param vert RectPadding to remove from top and bottom of Rect.
	 * @return the new smaller Rect.
	 */
	[[nodiscard]] inline Rect Shrink(const RectPadding &horz, const RectPadding &vert) const
	{
		return {this->left + horz.left, this->top + vert.top, this->right - horz.right, this->bottom - vert.bottom};
	}

	/**
	 * Copy and expand Rect by s pixels.
	 * @param s number of pixels to add to each side of Rect.
	 * @return the new larger Rect.
	 */
	[[nodiscard]] inline Rect Expand(int s) const
	{
		return this->Shrink(-s);
	}

	/**
	 * Copy and expand Rect by a RectPadding.
	 * @param other RectPadding to add to each side of Rect.
	 * @return the new larger Rect.
	 */
	[[nodiscard]] inline Rect Expand(const RectPadding &other) const
	{
		return {this->left - other.left, this->top - other.top, this->right + other.right, this->bottom + other.bottom};
	}

	/**
	 * Copy and translate Rect by x,y pixels.
	 * @param x number of pixels to move horizontally.
	 * @param y number of pixels to move vertically.
	 * @return the new translated Rect.
	 */
	[[nodiscard]] inline Rect Translate(int x, int y) const
	{
		return {this->left + x, this->top + y, this->right + x, this->bottom + y};
	}

	/**
	 * Copy Rect and set its width.
	 * @param width width in pixels for new Rect.
	 * @param end   if set, set width at end of Rect, i.e. on right.
	 * @return the new resized Rect.
	 */
	[[nodiscard]] inline Rect WithWidth(int width, bool end) const
	{
		return end
			? this->WithX(this->right - width + 1, this->right)
			: this->WithX(this->left, this->left + width - 1);
	}

	/**
	 * Copy Rect and indent it from its position.
	 * @param indent offset in pixels for new Rect.
	 * @param end   if set, set indent at end of Rect, i.e. on right.
	 * @return the new resized Rect.
	 */
	[[nodiscard]] inline Rect Indent(int indent, bool end) const
	{
		return end
			? this->WithX(this->left, this->right - indent)
			: this->WithX(this->left + indent, this->right);
	}

	/**
	 * Copy Rect and set its height.
	 * @param height height in pixels for new Rect.
	 * @param end   if set, set height at end of Rect, i.e. at bottom.
	 * @return the new resized Rect.
	 */
	[[nodiscard]] inline Rect WithHeight(int height, bool end = false) const
	{
		return end
			? this->WithY(this->bottom - height + 1, this->bottom)
			: this->WithY(this->top, this->top + height - 1);
	}

	/**
	 * Test if a point falls inside this Rect.
	 * @param pt the point to test.
	 * @return true iff the point falls inside the Rect.
	 */
	inline bool Contains(const Point &pt) const
	{
		/* This is a local version of IsInsideMM, to avoid including math_func everywhere. */
		return (uint)(pt.x - this->left) <= (uint)(this->right - this->left) && (uint)(pt.y - this->top) <= (uint)(this->bottom - this->top);
	}

	/**
	 * Centre a vertical dimension within this Rect.
	 * @param height The vertical dimension.
	 * @return the new resized Rect.
	 */
	[[nodiscard]] inline Rect CentreToHeight(int height) const
	{
		int new_top = CentreBounds(this->top, this->bottom, height);
		return {this->left, new_top, this->right, new_top + height - 1};
	}

	/**
	 * Create a new Rect, replacing the left and right coordiates.
	 * @param new_left New left coordinate.
	 * @param new_right New right coordinate.
	 * @return The new Rect.
	 */
	[[nodiscard]] inline Rect WithX(int new_left, int new_right) const { return {new_left, this->top, new_right, this->bottom}; }

	/**
	 * Create a new Rect, replacing the top and bottom coordiates.
	 * @param new_top New top coordinate.
	 * @param new_bottom New bottom coordinate.
	 * @return The new Rect.
	 */
	[[nodiscard]] inline Rect WithY(int new_top, int new_bottom) const { return {this->left, new_top, this->right, new_bottom}; }

	/**
	 * Create a new Rect, replacing the left and right coordiates.
	 * @param other Rect containing the new left and right coordinates.
	 * @return The new Rect.
	 */
	[[nodiscard]] inline Rect WithX(const Rect &other) const { return this->WithX(other.left, other.right); }

	/**
	 * Create a new Rect, replacing the top and bottom coordiates.
	 * @param other Rect containing the new top and bottom coordinates.
	 * @return The new Rect.
	 */
	[[nodiscard]] inline Rect WithY(const Rect &other) const { return this->WithY(other.top, other.bottom); }
};

/**
 * Specification of a rectangle with an absolute top-left coordinate and a
 * (relative) width/height
 */
struct PointDimension {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

#endif /* GEOMETRY_TYPE_HPP */
