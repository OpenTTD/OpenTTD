/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraphjob.h Declaration of link graph job classes used for cargo distribution. */

#ifndef LINKGRAPHJOB_H
#define LINKGRAPHJOB_H

#include "../thread.h"
#include "linkgraph.h"
#include <atomic>

class LinkGraphJob;
class Path;
typedef std::list<Path *> PathList;

/** Type of the pool for link graph jobs. */
typedef Pool<LinkGraphJob, LinkGraphJobID, 32, 0xFFFF> LinkGraphJobPool;
/** The actual pool with link graph jobs. */
extern LinkGraphJobPool _link_graph_job_pool;

/**
 * Class for calculation jobs to be run on link graphs.
 */
class LinkGraphJob : public LinkGraphJobPool::PoolItem<&_link_graph_job_pool>{
public:
	/**
	 * Demand between two nodes.
	 */
	struct DemandAnnotation {
		uint demand;             ///< Transport demand between the nodes.
		uint unsatisfied_demand; ///< Demand over this edge that hasn't been satisfied yet.
	};

	/**
	 * Annotation for a link graph edge.
	 */
	struct EdgeAnnotation {
		const LinkGraph::BaseEdge &base; ///< Reference to the edge that is annotated.

		uint flow;               ///< Planned flow over this edge.

		EdgeAnnotation(const LinkGraph::BaseEdge &base) : base(base), flow(0) {}

		/**
		 * Get the total flow on the edge.
		 * @return Flow.
		 */
		uint Flow() const { return this->flow; }

		/**
		 * Add some flow.
		 * @param flow Flow to be added.
		 */
		void AddFlow(uint flow) { this->flow += flow; }

		/**
		 * Remove some flow.
		 * @param flow Flow to be removed.
		 */
		void RemoveFlow(uint flow)
		{
			assert(flow <= this->flow);
			this->flow -= flow;
		}

		friend inline bool operator <(NodeID dest, const EdgeAnnotation &rhs)
		{
			return dest < rhs.base.dest_node;
		}
	};

	/**
	 * Annotation for a link graph node.
	 */
	struct NodeAnnotation {
		const LinkGraph::BaseNode &base; ///< Reference to the node that is annotated.

		uint undelivered_supply; ///< Amount of supply that hasn't been distributed yet.
		PathList paths;          ///< Paths through this node, sorted so that those with flow == 0 are in the back.
		FlowStatMap flows;       ///< Planned flows to other nodes.

		std::vector<EdgeAnnotation>   edges;   ///< Annotations for all edges originating at this node.
		std::vector<DemandAnnotation> demands; ///< Annotations for the demand to all other nodes.

		NodeAnnotation(const LinkGraph::BaseNode &node, size_t size) : base(node), undelivered_supply(node.supply), paths(), flows()
		{
			this->edges.reserve(node.edges.size());
			for (auto &e : node.edges) this->edges.emplace_back(e);
			this->demands.resize(size);
		}

		/**
		 * Retrieve an edge starting at this node.
		 * @param to Remote end of the edge.
		 * @return Edge between this node and "to".
		 */
		EdgeAnnotation &operator[](NodeID to)
		{
			auto it = std::find_if(this->edges.begin(), this->edges.end(), [=] (const EdgeAnnotation &e) { return e.base.dest_node == to; });
			assert(it != this->edges.end());
			return *it;
		}

		/**
		 * Retrieve an edge starting at this node.
		 * @param to Remote end of the edge.
		 * @return Edge between this node and "to".
		 */
		const EdgeAnnotation &operator[](NodeID to) const
		{
			auto it = std::find_if(this->edges.begin(), this->edges.end(), [=] (const EdgeAnnotation &e) { return e.base.dest_node == to; });
			assert(it != this->edges.end());
			return *it;
		}

		/**
		 * Get the transport demand between end the points of the edge.
		 * @return Demand.
		 */
		uint DemandTo(NodeID to) const { return this->demands[to].demand; }

		/**
		 * Get the transport demand that hasn't been satisfied by flows, yet.
		 * @return Unsatisfied demand.
		 */
		uint UnsatisfiedDemandTo(NodeID to) const { return this->demands[to].unsatisfied_demand; }

		/**
		 * Satisfy some demand.
		 * @param demand Demand to be satisfied.
		 */
		void SatisfyDemandTo(NodeID to, uint demand)
		{
			assert(demand <= this->demands[to].unsatisfied_demand);
			this->demands[to].unsatisfied_demand -= demand;
		}

		/**
		 * Deliver some supply, adding demand to the respective edge.
		 * @param to Destination for supply.
		 * @param amount Amount of supply to be delivered.
		 */
		void DeliverSupply(NodeID to, uint amount)
		{
			this->undelivered_supply -= amount;
			this->demands[to].demand += amount;
			this->demands[to].unsatisfied_demand += amount;
		}
	};

private:
	typedef std::vector<NodeAnnotation> NodeAnnotationVector;

	friend SaveLoadTable GetLinkGraphJobDesc();
	friend class LinkGraphSchedule;

protected:
	const LinkGraph link_graph;        ///< Link graph to by analyzed. Is copied when job is started and mustn't be modified later.
	const LinkGraphSettings settings;  ///< Copy of _settings_game.linkgraph at spawn time.
	std::thread thread;                ///< Thread the job is running in or a default-constructed thread if it's running in the main thread.
	TimerGameCalendar::Date join_date; ///< Date when the job is to be joined.
	NodeAnnotationVector nodes;        ///< Extra node data necessary for link graph calculation.
	std::atomic<bool> job_completed;   ///< Is the job still running. This is accessed by multiple threads and reads may be stale.
	std::atomic<bool> job_aborted;     ///< Has the job been aborted. This is accessed by multiple threads and reads may be stale.

	void EraseFlows(NodeID from);
	void JoinThread();
	void SpawnThread();

public:
	/**
	 * Bare constructor, only for save/load. link_graph, join_date and actually
	 * settings have to be brutally const-casted in order to populate them.
	 */
	LinkGraphJob() : settings(_settings_game.linkgraph),
			join_date(CalendarTime::INVALID_DATE), job_completed(false), job_aborted(false) {}

	LinkGraphJob(const LinkGraph &orig);
	~LinkGraphJob();

	void Init();

	/**
	 * Check if job has actually finished.
	 * This is allowed to spuriously return an incorrect value.
	 * @return True if job has actually finished.
	 */
	inline bool IsJobCompleted() const { return this->job_completed.load(std::memory_order_acquire); }

	/**
	 * Check if job has been aborted.
	 * This is allowed to spuriously return false incorrectly, but is not allowed to incorrectly return true.
	 * @return True if job has been aborted.
	 */
	inline bool IsJobAborted() const { return this->job_aborted.load(std::memory_order_acquire); }

	/**
	 * Abort job.
	 * The job may exit early at the next available opportunity.
	 * After this method has been called the state of the job is undefined, and the only valid operation
	 * is to join the thread and discard the job data.
	 */
	inline void AbortJob() { this->job_aborted.store(true, std::memory_order_release); }

	/**
	 * Check if job is supposed to be finished.
	 * @return True if job should be finished by now, false if not.
	 */
	inline bool IsScheduledToBeJoined() const { return this->join_date <= TimerGameCalendar::date; }

	/**
	 * Get the date when the job should be finished.
	 * @return Join date.
	 */
	inline TimerGameCalendar::Date JoinDate() const { return join_date; }

	/**
	 * Change the join date on date cheating.
	 * @param interval Number of days to add.
	 */
	inline void ShiftJoinDate(TimerGameCalendar::Date interval) { this->join_date += interval; }

	/**
	 * Get the link graph settings for this component.
	 * @return Settings.
	 */
	inline const LinkGraphSettings &Settings() const { return this->settings; }

	/**
	 * Get a node abstraction with the specified id.
	 * @param num ID of the node.
	 * @return the Requested node.
	 */
	inline NodeAnnotation &operator[](NodeID num) { return this->nodes[num]; }

	/**
	 * Get the size of the underlying link graph.
	 * @return Size.
	 */
	inline NodeID Size() const { return this->link_graph.Size(); }

	/**
	 * Get the cargo of the underlying link graph.
	 * @return Cargo.
	 */
	inline CargoID Cargo() const { return this->link_graph.Cargo(); }

	/**
	 * Get the date when the underlying link graph was last compressed.
	 * @return Compression date.
	 */
	inline TimerGameCalendar::Date LastCompression() const { return this->link_graph.LastCompression(); }

	/**
	 * Get the ID of the underlying link graph.
	 * @return Link graph ID.
	 */
	inline LinkGraphID LinkGraphIndex() const { return this->link_graph.index; }

	/**
	 * Get a reference to the underlying link graph. Only use this for save/load.
	 * @return Link graph.
	 */
	inline const LinkGraph &Graph() const { return this->link_graph; }
};

/**
 * A leg of a path in the link graph. Paths can form trees by being "forked".
 */
class Path {
public:
	static Path *invalid_path;

	Path(NodeID n, bool source = false);
	virtual ~Path() = default;

	/** Get the node this leg passes. */
	inline NodeID GetNode() const { return this->node; }

	/** Get the overall origin of the path. */
	inline NodeID GetOrigin() const { return this->origin; }

	/** Get the parent leg of this one. */
	inline Path *GetParent() { return this->parent; }

	/** Get the overall capacity of the path. */
	inline uint GetCapacity() const { return this->capacity; }

	/** Get the free capacity of the path. */
	inline int GetFreeCapacity() const { return this->free_capacity; }

	/**
	 * Get ratio of free * 16 (so that we get fewer 0) /
	 * max(total capacity, 1) (so that we don't divide by 0).
	 * @param free Free capacity.
	 * @param total Total capacity.
	 * @return free * 16 / max(total, 1).
	 */
	inline static int GetCapacityRatio(int free, uint total)
	{
		return Clamp(free, PATH_CAP_MIN_FREE, PATH_CAP_MAX_FREE) * PATH_CAP_MULTIPLIER / std::max(total, 1U);
	}

	/**
	 * Get capacity ratio of this path.
	 * @return free capacity * 16 / (total capacity + 1).
	 */
	inline int GetCapacityRatio() const
	{
		return Path::GetCapacityRatio(this->free_capacity, this->capacity);
	}

	/** Get the overall distance of the path. */
	inline uint GetDistance() const { return this->distance; }

	/** Reduce the flow on this leg only by the specified amount. */
	inline void ReduceFlow(uint f) { this->flow -= f; }

	/** Increase the flow on this leg only by the specified amount. */
	inline void AddFlow(uint f) { this->flow += f; }

	/** Get the flow on this leg. */
	inline uint GetFlow() const { return this->flow; }

	/** Get the number of "forked off" child legs of this one. */
	inline uint GetNumChildren() const { return this->num_children; }

	/**
	 * Detach this path from its parent.
	 */
	inline void Detach()
	{
		if (this->parent != nullptr) {
			this->parent->num_children--;
			this->parent = nullptr;
		}
	}

	uint AddFlow(uint f, LinkGraphJob &job, uint max_saturation);
	void Fork(Path *base, uint cap, int free_cap, uint dist);

protected:

	/**
	 * Some boundaries to clamp against in order to avoid integer overflows.
	 */
	enum PathCapacityBoundaries {
		PATH_CAP_MULTIPLIER = 16,
		PATH_CAP_MIN_FREE = (INT_MIN + 1) / PATH_CAP_MULTIPLIER,
		PATH_CAP_MAX_FREE = (INT_MAX - 1) / PATH_CAP_MULTIPLIER
	};

	uint distance;     ///< Sum(distance of all legs up to this one).
	uint capacity;     ///< This capacity is min(capacity) fom all edges.
	int free_capacity; ///< This capacity is min(edge.capacity - edge.flow) for the current run of Dijkstra.
	uint flow;         ///< Flow the current run of the mcf solver assigns.
	NodeID node;       ///< Link graph node this leg passes.
	NodeID origin;     ///< Link graph node this path originates from.
	uint num_children; ///< Number of child legs that have been forked from this path.
	Path *parent;      ///< Parent leg of this one.
};

#endif /* LINKGRAPHJOB_H */
