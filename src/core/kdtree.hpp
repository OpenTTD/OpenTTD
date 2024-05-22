/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file kdtree.hpp K-d tree template specialised for 2-dimensional Manhattan geometry */

#ifndef KDTREE_HPP
#define KDTREE_HPP

#include "../stdafx.h"

/**
 * K-dimensional tree, specialised for 2-dimensional space.
 * This is not intended as a primary storage of data, but as an index into existing data.
 * Usually the type stored by this tree should be an index into an existing array.
 *
 * This implementation assumes Manhattan distances are used.
 *
 * Be careful when using this in game code, depending on usage pattern, the tree shape may
 * end up different for different clients in multiplayer, causing iteration order to differ
 * and possibly having elements returned in different order. The using code should be designed
 * to produce the same result regardless of iteration order.
 *
 * The element type T must be less-than comparable for FindNearest to work.
 *
 * @tparam T       Type stored in the tree, should be cheap to copy.
 * @tparam TxyFunc Functor type to extract coordinate from a T value and dimension index (0 or 1).
 * @tparam CoordT  Type of coordinate values extracted via TxyFunc.
 * @tparam DistT   Type to use for representing distance values.
 */
template <typename T, typename TxyFunc, typename CoordT, typename DistT>
class Kdtree {
	/** Type of a node in the tree */
	struct node {
		T      element;  ///< Element stored at node
		size_t left;     ///< Index of node to the left, INVALID_NODE if none
		size_t right;    ///< Index of node to the right, INVALID_NODE if none

		node(T element) : element(element), left(INVALID_NODE), right(INVALID_NODE) { }
	};

	static const size_t INVALID_NODE = SIZE_MAX; ///< Index value indicating no-such-node

	std::vector<node> nodes;       ///< Pool of all nodes in the tree
	std::vector<size_t> free_list; ///< List of dead indices in the nodes vector
	size_t root;                   ///< Index of root node
	TxyFunc xyfunc;                ///< Functor to extract a coordinate from an element
	size_t unbalanced;             ///< Number approximating how unbalanced the tree might be

	/** Create one new node in the tree, return its index in the pool */
	size_t AddNode(const T &element)
	{
		if (this->free_list.empty()) {
			this->nodes.emplace_back(element);
			return this->nodes.size() - 1;
		} else {
			size_t newidx = this->free_list.back();
			this->free_list.pop_back();
			this->nodes[newidx] = node{ element };
			return newidx;
		}
	}

	/** Find a coordinate value to split a range of elements at */
	template <typename It>
	CoordT SelectSplitCoord(It begin, It end, int level)
	{
		It mid = begin + (end - begin) / 2;
		std::nth_element(begin, mid, end, [&](T a, T b) { return this->xyfunc(a, level % 2) < this->xyfunc(b, level % 2); });
		return this->xyfunc(*mid, level % 2);
	}

	/** Construct a subtree from elements between begin and end iterators, return index of root */
	template <typename It>
	size_t BuildSubtree(It begin, It end, int level)
	{
		ptrdiff_t count = end - begin;

		if (count == 0) {
			return INVALID_NODE;
		} else if (count == 1) {
			return this->AddNode(*begin);
		} else if (count > 1) {
			CoordT split_coord = SelectSplitCoord(begin, end, level);
			It split = std::partition(begin, end, [&](T v) { return this->xyfunc(v, level % 2) < split_coord; });
			size_t newidx = this->AddNode(*split);
			this->nodes[newidx].left = this->BuildSubtree(begin, split, level + 1);
			this->nodes[newidx].right = this->BuildSubtree(split + 1, end, level + 1);
			return newidx;
		} else {
			NOT_REACHED();
		}
	}

	/** Rebuild the tree with all existing elements, optionally adding or removing one more */
	bool Rebuild(const T *include_element, const T *exclude_element)
	{
		size_t initial_count = this->Count();
		if (initial_count < 8) return false; // arbitrary value for "not worth rebalancing"

		T root_element = this->nodes[this->root].element;
		std::vector<T> elements = this->FreeSubtree(this->root);
		elements.push_back(root_element);

		if (include_element != nullptr) {
			elements.push_back(*include_element);
			initial_count++;
		}
		if (exclude_element != nullptr) {
			typename std::vector<T>::iterator removed = std::remove(elements.begin(), elements.end(), *exclude_element);
			elements.erase(removed, elements.end());
			initial_count--;
		}

		this->Build(elements.begin(), elements.end());
		assert(initial_count == this->Count());
		return true;
	}

	/** Insert one element in the tree somewhere below node_idx */
	void InsertRecursive(const T &element, size_t node_idx, int level)
	{
		/* Dimension index of current level */
		int dim = level % 2;
		/* Node reference */
		node &n = this->nodes[node_idx];

		/* Coordinate of element splitting at this node */
		CoordT nc = this->xyfunc(n.element, dim);
		/* Coordinate of the new element */
		CoordT ec = this->xyfunc(element, dim);
		/* Which side to insert on */
		size_t &next = (ec < nc) ? n.left : n.right;

		if (next == INVALID_NODE) {
			/* New leaf */
			size_t newidx = this->AddNode(element);
			/* Vector may have been reallocated at this point, n and next are invalid */
			node &nn = this->nodes[node_idx];
			if (ec < nc) nn.left = newidx; else nn.right = newidx;
		} else {
			this->InsertRecursive(element, next, level + 1);
		}
	}

	/**
	 * Free all children of the given node
	 * @return Collection of elements that were removed from tree.
	 */
	std::vector<T> FreeSubtree(size_t node_idx)
	{
		std::vector<T> subtree_elements;
		node &n = this->nodes[node_idx];

		/* We'll be appending items to the free_list, get index of our first item */
		size_t first_free = this->free_list.size();
		/* Prepare the descent with our children */
		if (n.left != INVALID_NODE) this->free_list.push_back(n.left);
		if (n.right != INVALID_NODE) this->free_list.push_back(n.right);
		n.left = n.right = INVALID_NODE;

		/* Recursively free the nodes being collected */
		for (size_t i = first_free; i < this->free_list.size(); i++) {
			node &fn = this->nodes[this->free_list[i]];
			subtree_elements.push_back(fn.element);
			if (fn.left != INVALID_NODE) this->free_list.push_back(fn.left);
			if (fn.right != INVALID_NODE) this->free_list.push_back(fn.right);
			fn.left = fn.right = INVALID_NODE;
		}

		return subtree_elements;
	}

	/**
	 * Find and remove one element from the tree.
	 * @param element   The element to search for
	 * @param node_idx  Sub-tree to search in
	 * @param level     Current depth in the tree
	 * @return New root node index of the sub-tree processed
	 */
	size_t RemoveRecursive(const T &element, size_t node_idx, int level)
	{
		/* Node reference */
		node &n = this->nodes[node_idx];

		if (n.element == element) {
			/* Remove this one */
			this->free_list.push_back(node_idx);
			if (n.left == INVALID_NODE && n.right == INVALID_NODE) {
				/* Simple case, leaf, new child node for parent is "none" */
				return INVALID_NODE;
			} else {
				/* Complex case, rebuild the sub-tree */
				std::vector<T> subtree_elements = this->FreeSubtree(node_idx);
				return this->BuildSubtree(subtree_elements.begin(), subtree_elements.end(), level);;
			}
		} else {
			/* Search in a sub-tree */
			/* Dimension index of current level */
			int dim = level % 2;
			/* Coordinate of element splitting at this node */
			CoordT nc = this->xyfunc(n.element, dim);
			/* Coordinate of the element being removed */
			CoordT ec = this->xyfunc(element, dim);
			/* Which side to remove from */
			size_t next = (ec < nc) ? n.left : n.right;
			assert(next != INVALID_NODE); // node must exist somewhere and must be found before a leaf is reached
			/* Descend */
			size_t new_branch = this->RemoveRecursive(element, next, level + 1);
			if (new_branch != next) {
				/* Vector may have been reallocated at this point, n and next are invalid */
				node &nn = this->nodes[node_idx];
				if (ec < nc) nn.left = new_branch; else nn.right = new_branch;
			}
			return node_idx;
		}
	}


	DistT ManhattanDistance(const T &element, CoordT x, CoordT y) const
	{
		return abs((DistT)this->xyfunc(element, 0) - (DistT)x) + abs((DistT)this->xyfunc(element, 1) - (DistT)y);
	}

	/** A data element and its distance to a searched-for point */
	using node_distance = std::pair<T, DistT>;
	/** Ordering function for node_distance objects, elements with equal distance are ordered by less-than comparison */
	static node_distance SelectNearestNodeDistance(const node_distance &a, const node_distance &b)
	{
		if (a.second < b.second) return a;
		if (b.second < a.second) return b;
		if (a.first < b.first) return a;
		if (b.first < a.first) return b;
		NOT_REACHED(); // a.first == b.first: same element must not be inserted twice
	}
	/** Search a sub-tree for the element nearest to a given point */
	node_distance FindNearestRecursive(CoordT xy[2], size_t node_idx, int level, DistT limit = std::numeric_limits<DistT>::max()) const
	{
		/* Dimension index of current level */
		int dim = level % 2;
		/* Node reference */
		const node &n = this->nodes[node_idx];

		/* Coordinate of element splitting at this node */
		CoordT c = this->xyfunc(n.element, dim);
		/* This node's distance to target */
		DistT thisdist = ManhattanDistance(n.element, xy[0], xy[1]);
		/* Assume this node is the best choice for now */
		node_distance best = std::make_pair(n.element, thisdist);

		/* Next node to visit */
		size_t next = (xy[dim] < c) ? n.left : n.right;
		if (next != INVALID_NODE) {
			/* Check if there is a better node down the tree */
			best = SelectNearestNodeDistance(best, this->FindNearestRecursive(xy, next, level + 1));
		}

		limit = std::min(best.second, limit);

		/* Check if the distance from current best is worse than distance from target to splitting line,
		 * if it is we also need to check the other side of the split. */
		size_t opposite = (xy[dim] >= c) ? n.left : n.right; // reverse of above
		if (opposite != INVALID_NODE && limit >= abs((int)xy[dim] - (int)c)) {
			node_distance other_candidate = this->FindNearestRecursive(xy, opposite, level + 1, limit);
			best = SelectNearestNodeDistance(best, other_candidate);
		}

		return best;
	}

	template <typename Outputter>
	void FindContainedRecursive(CoordT p1[2], CoordT p2[2], size_t node_idx, int level, const Outputter &outputter) const
	{
		/* Dimension index of current level */
		int dim = level % 2;
		/* Node reference */
		const node &n = this->nodes[node_idx];

		/* Coordinate of element splitting at this node */
		CoordT ec = this->xyfunc(n.element, dim);
		/* Opposite coordinate of element */
		CoordT oc = this->xyfunc(n.element, 1 - dim);

		/* Test if this element is within rectangle */
		if (ec >= p1[dim] && ec < p2[dim] && oc >= p1[1 - dim] && oc < p2[1 - dim]) outputter(n.element);

		/* Recurse left if part of rectangle is left of split */
		if (p1[dim] < ec && n.left != INVALID_NODE) this->FindContainedRecursive(p1, p2, n.left, level + 1, outputter);

		/* Recurse right if part of rectangle is right of split */
		if (p2[dim] > ec && n.right != INVALID_NODE) this->FindContainedRecursive(p1, p2, n.right, level + 1, outputter);
	}

	/** Debugging function, counts number of occurrences of an element regardless of its correct position in the tree */
	size_t CountValue(const T &element, size_t node_idx) const
	{
		if (node_idx == INVALID_NODE) return 0;
		const node &n = this->nodes[node_idx];
		return CountValue(element, n.left) + CountValue(element, n.right) + ((n.element == element) ? 1 : 0);
	}

	void IncrementUnbalanced(size_t amount = 1)
	{
		this->unbalanced += amount;
	}

	/** Check if the entire tree is in need of rebuilding */
	bool IsUnbalanced()
	{
		size_t count = this->Count();
		if (count < 8) return false;
		return this->unbalanced > this->Count() / 4;
	}

	/** Verify that the invariant is true for a sub-tree, assert if not */
	void CheckInvariant(size_t node_idx, int level, CoordT min_x, CoordT max_x, CoordT min_y, CoordT max_y)
	{
		if (node_idx == INVALID_NODE) return;

		const node &n = this->nodes[node_idx];
		CoordT cx = this->xyfunc(n.element, 0);
		CoordT cy = this->xyfunc(n.element, 1);

		assert(cx >= min_x);
		assert(cx < max_x);
		assert(cy >= min_y);
		assert(cy < max_y);

		if (level % 2 == 0) {
			// split in dimension 0 = x
			CheckInvariant(n.left,  level + 1, min_x, cx, min_y, max_y);
			CheckInvariant(n.right, level + 1, cx, max_x, min_y, max_y);
		} else {
			// split in dimension 1 = y
			CheckInvariant(n.left,  level + 1, min_x, max_x, min_y, cy);
			CheckInvariant(n.right, level + 1, min_x, max_x, cy, max_y);
		}
	}

	/** Verify the invariant for the entire tree, does nothing unless KDTREE_DEBUG is defined */
	void CheckInvariant()
	{
#ifdef KDTREE_DEBUG
		CheckInvariant(this->root, 0, std::numeric_limits<CoordT>::min(), std::numeric_limits<CoordT>::max(), std::numeric_limits<CoordT>::min(), std::numeric_limits<CoordT>::max());
#endif
	}

public:
	/** Construct a new Kdtree with the given xyfunc */
	Kdtree(TxyFunc xyfunc) : root(INVALID_NODE), xyfunc(xyfunc), unbalanced(0) { }

	/**
	 * Clear and rebuild the tree from a new sequence of elements,
	 * @tparam It    Iterator type for element sequence.
	 * @param  begin First element in sequence.
	 * @param  end   One past last element in sequence.
	 */
	template <typename It>
	void Build(It begin, It end)
	{
		this->nodes.clear();
		this->free_list.clear();
		this->unbalanced = 0;
		if (begin == end) return;
		this->nodes.reserve(end - begin);

		this->root = this->BuildSubtree(begin, end, 0);
		CheckInvariant();
	}

	/**
	 * Clear the tree.
	 */
	void Clear()
	{
		this->nodes.clear();
		this->free_list.clear();
		this->unbalanced = 0;
		return;
	}

	/**
	 * Reconstruct the tree with the same elements, letting it be fully balanced.
	 */
	void Rebuild()
	{
		this->Rebuild(nullptr, nullptr);
	}

	/**
	 * Insert a single element in the tree.
	 * Repeatedly inserting single elements may cause the tree to become unbalanced.
	 * Undefined behaviour if the element already exists in the tree.
	 */
	void Insert(const T &element)
	{
		if (this->Count() == 0) {
			this->root = this->AddNode(element);
		} else {
			if (!this->IsUnbalanced() || !this->Rebuild(&element, nullptr)) {
				this->InsertRecursive(element, this->root, 0);
				this->IncrementUnbalanced();
			}
			CheckInvariant();
		}
	}

	/**
	 * Remove a single element from the tree, if it exists.
	 * Since elements are stored in interior nodes as well as leaf nodes, removing one may
	 * require a larger sub-tree to be re-built. Because of this, worst case run time is
	 * as bad as a full tree rebuild.
	 */
	void Remove(const T &element)
	{
		size_t count = this->Count();
		if (count == 0) return;
		if (!this->IsUnbalanced() || !this->Rebuild(nullptr, &element)) {
			/* If the removed element is the root node, this modifies this->root */
			this->root = this->RemoveRecursive(element, this->root, 0);
			this->IncrementUnbalanced();
		}
		CheckInvariant();
	}

	/** Get number of elements stored in tree */
	size_t Count() const
	{
		assert(this->free_list.size() <= this->nodes.size());
		return this->nodes.size() - this->free_list.size();
	}

	/**
	 * Find the element closest to given coordinate, in Manhattan distance.
	 * For multiple elements with the same distance, the one comparing smaller with
	 * a less-than comparison is chosen.
	 */
	T FindNearest(CoordT x, CoordT y) const
	{
		assert(this->Count() > 0);

		CoordT xy[2] = { x, y };
		return this->FindNearestRecursive(xy, this->root, 0).first;
	}

	/**
	 * Find all items contained within the given rectangle.
	 * @note Start coordinates are inclusive, end coordinates are exclusive. x1<x2 && y1<y2 is a precondition.
	 * @param x1 Start first coordinate, points found are greater or equals to this.
	 * @param y1 Start second coordinate, points found are greater or equals to this.
	 * @param x2 End first coordinate, points found are less than this.
	 * @param y2 End second coordinate, points found are less than this.
	 * @param outputter Callback used to return values from the search.
	 */
	template <typename Outputter>
	void FindContained(CoordT x1, CoordT y1, CoordT x2, CoordT y2, const Outputter &outputter) const
	{
		assert(x1 < x2);
		assert(y1 < y2);

		if (this->Count() == 0) return;

		CoordT p1[2] = { x1, y1 };
		CoordT p2[2] = { x2, y2 };
		this->FindContainedRecursive(p1, p2, this->root, 0, outputter);
	}

	/**
	 * Find all items contained within the given rectangle.
	 * @note End coordinates are exclusive, x1<x2 && y1<y2 is a precondition.
	 */
	std::vector<T> FindContained(CoordT x1, CoordT y1, CoordT x2, CoordT y2) const
	{
		std::vector<T> result;
		this->FindContained(x1, y1, x2, y2, [&result](T e) {result.push_back(e); });
		return result;
	}
};

#endif
