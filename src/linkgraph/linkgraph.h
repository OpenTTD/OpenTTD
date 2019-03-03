/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph.h Declaration of link graph classes used for cargo distribution. */

#ifndef LINKGRAPH_H
#define LINKGRAPH_H

#include "../core/pool_type.hpp"
#include "../core/smallmap_type.hpp"
#include "../core/smallmatrix_type.hpp"
#include "../station_base.h"
#include "../cargotype.h"
#include "../date_func.h"
#include "linkgraph_type.h"

struct SaveLoad;
class LinkGraph;

/**
 * Type of the pool for link graph components. Each station can be in at up to
 * 32 link graphs. So we allow for plenty of them to be created.
 */
typedef Pool<LinkGraph, LinkGraphID, 32, 0xFFFF> LinkGraphPool;
/** The actual pool with link graphs. */
extern LinkGraphPool _link_graph_pool;

/**
 * A connected component of a link graph. Contains a complete set of stations
 * connected by links as nodes and edges. Each component also holds a copy of
 * the link graph settings at the time of its creation. The global settings
 * might change between the creation and join time so we can't rely on them.
 */
class LinkGraph : public LinkGraphPool::PoolItem<&_link_graph_pool> {
public:

	/**
	 * Node of the link graph. contains all relevant information from the associated
	 * station. It's copied so that the link graph job can work on its own data set
	 * in a separate thread.
	 */
	struct BaseNode {
		uint supply;             ///< Supply at the station.
		uint demand;             ///< Acceptance at the station.
		StationID station;       ///< Station ID.
		TileIndex xy;            ///< Location of the station referred to by the node.
		Date last_update;        ///< When the supply was last updated.
		void Init(TileIndex xy = INVALID_TILE, StationID st = INVALID_STATION, uint demand = 0);
	};

	/**
	 * An edge in the link graph. Corresponds to a link between two stations or at
	 * least the distance between them. Edges from one node to itself contain the
	 * ID of the opposite Node of the first active edge (i.e. not just distance) in
	 * the column as next_edge.
	 */
	struct BaseEdge {
		uint capacity;                 ///< Capacity of the link.
		uint usage;                    ///< Usage of the link.
		Date last_unrestricted_update; ///< When the unrestricted part of the link was last updated.
		Date last_restricted_update;   ///< When the restricted part of the link was last updated.
		NodeID next_edge;              ///< Destination of next valid edge starting at the same source node.
		void Init();
	};

	/**
	 * Wrapper for an edge (const or not) allowing retrieval, but no modification.
	 * @tparam Tedge Actual edge class, may be "const BaseEdge" or just "BaseEdge".
	 */
	template<typename Tedge>
	class EdgeWrapper {
	protected:
		Tedge &edge; ///< Actual edge to be used.

	public:

		/**
		 * Wrap a an edge.
		 * @param edge Edge to be wrapped.
		 */
		EdgeWrapper (Tedge &edge) : edge(edge) {}

		/**
		 * Get edge's capacity.
		 * @return Capacity.
		 */
		uint Capacity() const { return this->edge.capacity; }

		/**
		 * Get edge's usage.
		 * @return Usage.
		 */
		uint Usage() const { return this->edge.usage; }

		/**
		 * Get the date of the last update to the edge's unrestricted capacity.
		 * @return Last update.
		 */
		Date LastUnrestrictedUpdate() const { return this->edge.last_unrestricted_update; }

		/**
		 * Get the date of the last update to the edge's restricted capacity.
		 * @return Last update.
		 */
		Date LastRestrictedUpdate() const { return this->edge.last_restricted_update; }

		/**
		 * Get the date of the last update to any part of the edge's capacity.
		 * @return Last update.
		 */
		Date LastUpdate() const { return max(this->edge.last_unrestricted_update, this->edge.last_restricted_update); }
	};

	/**
	 * Wrapper for a node (const or not) allowing retrieval, but no modification.
	 * @tparam Tedge Actual node class, may be "const BaseNode" or just "BaseNode".
	 * @tparam Tedge Actual edge class, may be "const BaseEdge" or just "BaseEdge".
	 */
	template<typename Tnode, typename Tedge>
	class NodeWrapper {
	protected:
		Tnode &node;  ///< Node being wrapped.
		Tedge *edges; ///< Outgoing edges for wrapped node.
		NodeID index; ///< ID of wrapped node.

	public:

		/**
		 * Wrap a node.
		 * @param node Node to be wrapped.
		 * @param edges Outgoing edges for node to be wrapped.
		 * @param index ID of node to be wrapped.
		 */
		NodeWrapper(Tnode &node, Tedge *edges, NodeID index) : node(node),
			edges(edges), index(index) {}

		/**
		 * Get supply of wrapped node.
		 * @return Supply.
		 */
		uint Supply() const { return this->node.supply; }

		/**
		 * Get demand of wrapped node.
		 * @return Demand.
		 */
		uint Demand() const { return this->node.demand; }

		/**
		 * Get ID of station belonging to wrapped node.
		 * @return ID of node's station.
		 */
		StationID Station() const { return this->node.station; }

		/**
		 * Get node's last update.
		 * @return Last update.
		 */
		Date LastUpdate() const { return this->node.last_update; }

		/**
		 * Get the location of the station associated with the node.
		 * @return Location of the station.
		 */
		TileIndex XY() const { return this->node.xy; }
	};

	/**
	 * Base class for iterating across outgoing edges of a node. Only the real
	 * edges (those with capacity) are iterated. The ones with only distance
	 * information are skipped.
	 * @tparam Tedge Actual edge class. May be "BaseEdge" or "const BaseEdge".
	 * @tparam Titer Actual iterator class.
	 */
	template <class Tedge, class Tedge_wrapper, class Titer>
	class BaseEdgeIterator {
	protected:
		Tedge *base;    ///< Array of edges being iterated.
		NodeID current; ///< Current offset in edges array.

		/**
		 * A "fake" pointer to enable operator-> on temporaries. As the objects
		 * returned from operator* aren't references but real objects, we have
		 * to return something that implements operator->, but isn't a pointer
		 * from operator->. A fake pointer.
		 */
		class FakePointer : public SmallPair<NodeID, Tedge_wrapper> {
		public:

			/**
			 * Construct a fake pointer from a pair of NodeID and edge.
			 * @param pair Pair to be "pointed" to (in fact shallow-copied).
			 */
			FakePointer(const SmallPair<NodeID, Tedge_wrapper> &pair) : SmallPair<NodeID, Tedge_wrapper>(pair) {}

			/**
			 * Retrieve the pair by operator->.
			 * @return Pair being "pointed" to.
			 */
			SmallPair<NodeID, Tedge_wrapper> *operator->() { return this; }
		};

	public:
		/**
		 * Constructor.
		 * @param base Array of edges to be iterated.
		 * @param current ID of current node (to locate the first edge).
		 */
		BaseEdgeIterator (Tedge *base, NodeID current) :
			base(base),
			current(current == INVALID_NODE ? current : base[current].next_edge)
		{}

		/**
		 * Prefix-increment.
		 * @return This.
		 */
		Titer &operator++()
		{
			this->current = this->base[this->current].next_edge;
			return static_cast<Titer &>(*this);
		}

		/**
		 * Postfix-increment.
		 * @return Version of this before increment.
		 */
		Titer operator++(int)
		{
			Titer ret(static_cast<Titer &>(*this));
			this->current = this->base[this->current].next_edge;
			return ret;
		}

		/**
		 * Compare with some other edge iterator. The other one may be of a
		 * child class.
		 * @tparam Tother Class of other iterator.
		 * @param other Instance of other iterator.
		 * @return If the iterators have the same edge array and current node.
		 */
		template<class Tother>
		bool operator==(const Tother &other)
		{
			return this->base == other.base && this->current == other.current;
		}

		/**
		 * Compare for inequality with some other edge iterator. The other one
		 * may be of a child class.
		 * @tparam Tother Class of other iterator.
		 * @param other Instance of other iterator.
		 * @return If either the edge arrays or the current nodes differ.
		 */
		template<class Tother>
		bool operator!=(const Tother &other)
		{
			return this->base != other.base || this->current != other.current;
		}

		/**
		 * Dereference with operator*.
		 * @return Pair of current target NodeID and edge object.
		 */
		SmallPair<NodeID, Tedge_wrapper> operator*() const
		{
			return SmallPair<NodeID, Tedge_wrapper>(this->current, Tedge_wrapper(this->base[this->current]));
		}

		/**
		 * Dereference with operator->.
		 * @return Fake pointer to Pair of current target NodeID and edge object.
		 */
		FakePointer operator->() const {
			return FakePointer(this->operator*());
		}
	};

	/**
	 * A constant edge class.
	 */
	typedef EdgeWrapper<const BaseEdge> ConstEdge;

	/**
	 * An updatable edge class.
	 */
	class Edge : public EdgeWrapper<BaseEdge> {
	public:
		/**
		 * Constructor
		 * @param edge Edge to be wrapped.
		 */
		Edge(BaseEdge &edge) : EdgeWrapper<BaseEdge>(edge) {}
		void Update(uint capacity, uint usage, EdgeUpdateMode mode);
		void Restrict() { this->edge.last_unrestricted_update = INVALID_DATE; }
		void Release() { this->edge.last_restricted_update = INVALID_DATE; }
	};

	/**
	 * An iterator for const edges. Cannot be typedef'ed because of
	 * template-reference to ConstEdgeIterator itself.
	 */
	class ConstEdgeIterator : public BaseEdgeIterator<const BaseEdge, ConstEdge, ConstEdgeIterator> {
	public:
		/**
		 * Constructor.
		 * @param edges Array of edges to be iterated over.
		 * @param current ID of current edge's end node.
		 */
		ConstEdgeIterator(const BaseEdge *edges, NodeID current) :
			BaseEdgeIterator<const BaseEdge, ConstEdge, ConstEdgeIterator>(edges, current) {}
	};

	/**
	 * An iterator for non-const edges. Cannot be typedef'ed because of
	 * template-reference to EdgeIterator itself.
	 */
	class EdgeIterator : public BaseEdgeIterator<BaseEdge, Edge, EdgeIterator> {
	public:
		/**
		 * Constructor.
		 * @param edges Array of edges to be iterated over.
		 * @param current ID of current edge's end node.
		 */
		EdgeIterator(BaseEdge *edges, NodeID current) :
			BaseEdgeIterator<BaseEdge, Edge, EdgeIterator>(edges, current) {}
	};

	/**
	 * Constant node class. Only retrieval operations are allowed on both the
	 * node itself and its edges.
	 */
	class ConstNode : public NodeWrapper<const BaseNode, const BaseEdge> {
	public:
		/**
		 * Constructor.
		 * @param lg LinkGraph to get the node from.
		 * @param node ID of the node.
		 */
		ConstNode(const LinkGraph *lg, NodeID node) :
			NodeWrapper<const BaseNode, const BaseEdge>(lg->nodes[node], lg->edges[node], node)
		{}

		/**
		 * Get a ConstEdge. This is not a reference as the wrapper objects are
		 * not actually persistent.
		 * @param to ID of end node of edge.
		 * @return Constant edge wrapper.
		 */
		ConstEdge operator[](NodeID to) const { return ConstEdge(this->edges[to]); }

		/**
		 * Get an iterator pointing to the start of the edges array.
		 * @return Constant edge iterator.
		 */
		ConstEdgeIterator Begin() const { return ConstEdgeIterator(this->edges, this->index); }

		/**
		 * Get an iterator pointing beyond the end of the edges array.
		 * @return Constant edge iterator.
		 */
		ConstEdgeIterator End() const { return ConstEdgeIterator(this->edges, INVALID_NODE); }
	};

	/**
	 * Updatable node class. The node itself as well as its edges can be modified.
	 */
	class Node : public NodeWrapper<BaseNode, BaseEdge> {
	public:
		/**
		 * Constructor.
		 * @param lg LinkGraph to get the node from.
		 * @param node ID of the node.
		 */
		Node(LinkGraph *lg, NodeID node) :
			NodeWrapper<BaseNode, BaseEdge>(lg->nodes[node], lg->edges[node], node)
		{}

		/**
		 * Get an Edge. This is not a reference as the wrapper objects are not
		 * actually persistent.
		 * @param to ID of end node of edge.
		 * @return Edge wrapper.
		 */
		Edge operator[](NodeID to) { return Edge(this->edges[to]); }

		/**
		 * Get an iterator pointing to the start of the edges array.
		 * @return Edge iterator.
		 */
		EdgeIterator Begin() { return EdgeIterator(this->edges, this->index); }

		/**
		 * Get an iterator pointing beyond the end of the edges array.
		 * @return Constant edge iterator.
		 */
		EdgeIterator End() { return EdgeIterator(this->edges, INVALID_NODE); }

		/**
		 * Update the node's supply and set last_update to the current date.
		 * @param supply Supply to be added.
		 */
		void UpdateSupply(uint supply)
		{
			this->node.supply += supply;
			this->node.last_update = _date;
		}

		/**
		 * Update the node's location on the map.
		 * @param xy New location.
		 */
		void UpdateLocation(TileIndex xy)
		{
			this->node.xy = xy;
		}

		/**
		 * Set the node's demand.
		 * @param demand New demand for the node.
		 */
		void SetDemand(uint demand)
		{
			this->node.demand = demand;
		}

		void AddEdge(NodeID to, uint capacity, uint usage, EdgeUpdateMode mode);
		void UpdateEdge(NodeID to, uint capacity, uint usage, EdgeUpdateMode mode);
		void RemoveEdge(NodeID to);
	};

	typedef std::vector<BaseNode> NodeVector;
	typedef SmallMatrix<BaseEdge> EdgeMatrix;

	/** Minimum effective distance for timeout calculation. */
	static const uint MIN_TIMEOUT_DISTANCE = 32;

	/** Minimum number of days between subsequent compressions of a LG. */
	static const uint COMPRESSION_INTERVAL = 256;

	/**
	 * Scale a value from a link graph of age orig_age for usage in one of age
	 * target_age. Make sure that the value stays > 0 if it was > 0 before.
	 * @param val Value to be scaled.
	 * @param target_age Age of the target link graph.
	 * @param orig_age Age of the original link graph.
	 * @return scaled value.
	 */
	inline static uint Scale(uint val, uint target_age, uint orig_age)
	{
		return val > 0 ? max(1U, val * target_age / orig_age) : 0;
	}

	/** Bare constructor, only for save/load. */
	LinkGraph() : cargo(INVALID_CARGO), last_compression(0) {}
	/**
	 * Real constructor.
	 * @param cargo Cargo the link graph is about.
	 */
	LinkGraph(CargoID cargo) : cargo(cargo), last_compression(_date) {}

	void Init(uint size);
	void ShiftDates(int interval);
	void Compress();
	void Merge(LinkGraph *other);

	/* Splitting link graphs is intentionally not implemented.
	 * The overhead in determining connectedness would probably outweigh the
	 * benefit of having to deal with smaller graphs. In real world examples
	 * networks generally grow. Only rarely a network is permanently split.
	 * Reacting to temporary splits here would obviously create performance
	 * problems and detecting the temporary or permanent nature of splits isn't
	 * trivial. */

	/**
	 * Get a node with the specified id.
	 * @param num ID of the node.
	 * @return the Requested node.
	 */
	inline Node operator[](NodeID num) { return Node(this, num); }

	/**
	 * Get a const reference to a node with the specified id.
	 * @param num ID of the node.
	 * @return the Requested node.
	 */
	inline ConstNode operator[](NodeID num) const { return ConstNode(this, num); }

	/**
	 * Get the current size of the component.
	 * @return Size.
	 */
	inline uint Size() const { return this->nodes.size(); }

	/**
	 * Get date of last compression.
	 * @return Date of last compression.
	 */
	inline Date LastCompression() const { return this->last_compression; }

	/**
	 * Get the cargo ID this component's link graph refers to.
	 * @return Cargo ID.
	 */
	inline CargoID Cargo() const { return this->cargo; }

	/**
	 * Scale a value to its monthly equivalent, based on last compression.
	 * @param base Value to be scaled.
	 * @return Scaled value.
	 */
	inline uint Monthly(uint base) const
	{
		return base * 30 / (_date - this->last_compression + 1);
	}

	NodeID AddNode(const Station *st);
	void RemoveNode(NodeID id);

protected:
	friend class LinkGraph::ConstNode;
	friend class LinkGraph::Node;
	friend const SaveLoad *GetLinkGraphDesc();
	friend const SaveLoad *GetLinkGraphJobDesc();
	friend void SaveLoad_LinkGraph(LinkGraph &lg);

	CargoID cargo;         ///< Cargo of this component's link graph.
	Date last_compression; ///< Last time the capacities and supplies were compressed.
	NodeVector nodes;      ///< Nodes in the component.
	EdgeMatrix edges;      ///< Edges in the component.
};

#define FOR_ALL_LINK_GRAPHS(var) FOR_ALL_ITEMS_FROM(LinkGraph, link_graph_index, var, 0)

#endif /* LINKGRAPH_H */
