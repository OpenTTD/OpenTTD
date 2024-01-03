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
#include "../station_base.h"
#include "../cargotype.h"
#include "../timer/timer_game_calendar.h"
#include "../saveload/saveload.h"
#include "linkgraph_type.h"
#include <utility>

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
	 * An edge in the link graph. Corresponds to a link between two stations.
	 */
	struct BaseEdge {
		uint capacity;                 ///< Capacity of the link.
		uint usage;                    ///< Usage of the link.
		uint64_t travel_time_sum;        ///< Sum of the travel times of the link, in ticks.
		TimerGameCalendar::Date last_unrestricted_update; ///< When the unrestricted part of the link was last updated.
		TimerGameCalendar::Date last_restricted_update;   ///< When the restricted part of the link was last updated.
		NodeID dest_node;              ///< Destination of the edge.

		BaseEdge(NodeID dest_node = INVALID_NODE);

		/**
		 * Get edge's average travel time.
		 * @return Travel time, in ticks.
		 */
		uint32_t TravelTime() const { return this->travel_time_sum / this->capacity; }

		/**
		 * Get the date of the last update to any part of the edge's capacity.
		 * @return Last update.
		 */
		TimerGameCalendar::Date LastUpdate() const { return std::max(this->last_unrestricted_update, this->last_restricted_update); }

		void Update(uint capacity, uint usage, uint32_t time, EdgeUpdateMode mode);
		void Restrict() { this->last_unrestricted_update = CalendarTime::INVALID_DATE; }
		void Release() { this->last_restricted_update = CalendarTime::INVALID_DATE; }

		/** Comparison operator based on \c dest_node. */
		bool operator <(const BaseEdge &rhs) const
		{
			return this->dest_node < rhs.dest_node;
		}

		bool operator <(NodeID rhs) const
		{
			return this->dest_node < rhs;
		}

		friend inline bool operator <(NodeID lhs, const LinkGraph::BaseEdge &rhs)
		{
			return lhs < rhs.dest_node;
		}
	};

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
		TimerGameCalendar::Date last_update;        ///< When the supply was last updated.

		std::vector<BaseEdge> edges; ///< Sorted list of outgoing edges from this node.

		BaseNode(TileIndex xy = INVALID_TILE, StationID st = INVALID_STATION, uint demand = 0);

		/**
		 * Update the node's supply and set last_update to the current date.
		 * @param supply Supply to be added.
		 */
		void UpdateSupply(uint supply)
		{
			this->supply += supply;
			this->last_update = TimerGameCalendar::date;
		}

		/**
		 * Update the node's location on the map.
		 * @param xy New location.
		 */
		void UpdateLocation(TileIndex xy)
		{
			this->xy = xy;
		}

		/**
		 * Set the node's demand.
		 * @param demand New demand for the node.
		 */
		void SetDemand(uint demand)
		{
			this->demand = demand;
		}

		void AddEdge(NodeID to, uint capacity, uint usage, uint32_t time, EdgeUpdateMode mode);
		void UpdateEdge(NodeID to, uint capacity, uint usage, uint32_t time, EdgeUpdateMode mode);
		void RemoveEdge(NodeID to);

		/**
		 * Check if an edge to a destination is present.
		 * @param dest Wanted edge destination.
		 * @return True if an edge is present.
		 */
		bool HasEdgeTo(NodeID dest) const
		{
			return std::binary_search(this->edges.begin(), this->edges.end(), dest);
		}

		BaseEdge &operator[](NodeID to)
		{
			assert(this->HasEdgeTo(to));
			return *GetEdge(to);
		}

		const BaseEdge &operator[](NodeID to) const
		{
			assert(this->HasEdgeTo(to));
			return *GetEdge(to);
		}

	private:
		std::vector<BaseEdge>::iterator GetEdge(NodeID dest)
		{
			return std::lower_bound(this->edges.begin(), this->edges.end(), dest);
		}

		std::vector<BaseEdge>::const_iterator GetEdge(NodeID dest) const
		{
			return std::lower_bound(this->edges.begin(), this->edges.end(), dest);
		}
	};

	typedef std::vector<BaseNode> NodeVector;

	/** Minimum effective distance for timeout calculation. */
	static const uint MIN_TIMEOUT_DISTANCE = 32;

	/** Number of days before deleting links served only by vehicles stopped in depot. */
	static constexpr TimerGameCalendar::Date STALE_LINK_DEPOT_TIMEOUT = 1024;

	/** Minimum number of days between subsequent compressions of a LG. */
	static constexpr TimerGameCalendar::Date COMPRESSION_INTERVAL = 256;

	/**
	 * Scale a value from a link graph of age orig_age for usage in one of age
	 * target_age. Make sure that the value stays > 0 if it was > 0 before.
	 * @param val Value to be scaled.
	 * @param target_age Age of the target link graph.
	 * @param orig_age Age of the original link graph.
	 * @return scaled value.
	 */
	inline static uint Scale(uint val, TimerGameCalendar::Date target_age, TimerGameCalendar::Date orig_age)
	{
		return val > 0 ? std::max(1U, val * target_age.base() / orig_age.base()) : 0;
	}

	/** Bare constructor, only for save/load. */
	LinkGraph() : cargo(CT_INVALID), last_compression(0) {}
	/**
	 * Real constructor.
	 * @param cargo Cargo the link graph is about.
	 */
	LinkGraph(CargoID cargo) : cargo(cargo), last_compression(TimerGameCalendar::date) {}

	void Init(uint size);
	void ShiftDates(TimerGameCalendar::Date interval);
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
	inline BaseNode &operator[](NodeID num) { return this->nodes[num]; }

	/**
	 * Get a const reference to a node with the specified id.
	 * @param num ID of the node.
	 * @return the Requested node.
	 */
	inline const BaseNode &operator[](NodeID num) const { return this->nodes[num]; }

	/**
	 * Get the current size of the component.
	 * @return Size.
	 */
	inline NodeID Size() const { return (NodeID)this->nodes.size(); }

	/**
	 * Get date of last compression.
	 * @return Date of last compression.
	 */
	inline TimerGameCalendar::Date LastCompression() const { return this->last_compression; }

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
		return base * 30 / (TimerGameCalendar::date - this->last_compression + 1).base();
	}

	NodeID AddNode(const Station *st);
	void RemoveNode(NodeID id);

protected:
	friend SaveLoadTable GetLinkGraphDesc();
	friend SaveLoadTable GetLinkGraphJobDesc();
	friend class SlLinkgraphNode;
	friend class SlLinkgraphEdge;
	friend class LinkGraphJob;

	CargoID cargo;         ///< Cargo of this component's link graph.
	TimerGameCalendar::Date last_compression; ///< Last time the capacities and supplies were compressed.
	NodeVector nodes;      ///< Nodes in the component.
};

#endif /* LINKGRAPH_H */
