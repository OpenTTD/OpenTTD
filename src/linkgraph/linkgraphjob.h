/* $Id$ */

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
#include <list>

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
private:
	/**
	 * Annotation for a link graph edge.
	 */
	struct EdgeAnnotation {
		uint demand;             ///< Transport demand between the nodes.
		uint unsatisfied_demand; ///< Demand over this edge that hasn't been satisfied yet.
		uint flow;               ///< Planned flow over this edge.
		void Init();
	};

	/**
	 * Annotation for a link graph node.
	 */
	struct NodeAnnotation {
		uint undelivered_supply; ///< Amount of supply that hasn't been distributed yet.
		PathList paths;          ///< Paths through this node, sorted so that those with flow == 0 are in the back.
		FlowStatMap flows;       ///< Planned flows to other nodes.
		void Init(uint supply);
	};

	typedef std::vector<NodeAnnotation> NodeAnnotationVector;
	typedef SmallMatrix<EdgeAnnotation> EdgeAnnotationMatrix;

	friend const SaveLoad *GetLinkGraphJobDesc();
	friend class LinkGraphSchedule;

protected:
	const LinkGraph link_graph;       ///< Link graph to by analyzed. Is copied when job is started and mustn't be modified later.
	const LinkGraphSettings settings; ///< Copy of _settings_game.linkgraph at spawn time.
	std::thread thread;               ///< Thread the job is running in or a default-constructed thread if it's running in the main thread.
	Date join_date;                   ///< Date when the job is to be joined.
	NodeAnnotationVector nodes;       ///< Extra node data necessary for link graph calculation.
	EdgeAnnotationMatrix edges;       ///< Extra edge data necessary for link graph calculation.

	void EraseFlows(NodeID from);
	void JoinThread();
	void SpawnThread();

public:

	/**
	 * A job edge. Wraps a link graph edge and an edge annotation. The
	 * annotation can be modified, the edge is constant.
	 */
	class Edge : public LinkGraph::ConstEdge {
	private:
		EdgeAnnotation &anno; ///< Annotation being wrapped.
	public:
		/**
		 * Constructor.
		 * @param edge Link graph edge to be wrapped.
		 * @param anno Annotation to be wrapped.
		 */
		Edge(const LinkGraph::BaseEdge &edge, EdgeAnnotation &anno) :
				LinkGraph::ConstEdge(edge), anno(anno) {}

		/**
		 * Get the transport demand between end the points of the edge.
		 * @return Demand.
		 */
		uint Demand() const { return this->anno.demand; }

		/**
		 * Get the transport demand that hasn't been satisfied by flows, yet.
		 * @return Unsatisfied demand.
		 */
		uint UnsatisfiedDemand() const { return this->anno.unsatisfied_demand; }

		/**
		 * Get the total flow on the edge.
		 * @return Flow.
		 */
		uint Flow() const { return this->anno.flow; }

		/**
		 * Add some flow.
		 * @param flow Flow to be added.
		 */
		void AddFlow(uint flow) { this->anno.flow += flow; }

		/**
		 * Remove some flow.
		 * @param flow Flow to be removed.
		 */
		void RemoveFlow(uint flow)
		{
			assert(flow <= this->anno.flow);
			this->anno.flow -= flow;
		}

		/**
		 * Add some (not yet satisfied) demand.
		 * @param demand Demand to be added.
		 */
		void AddDemand(uint demand)
		{
			this->anno.demand += demand;
			this->anno.unsatisfied_demand += demand;
		}

		/**
		 * Satisfy some demand.
		 * @param demand Demand to be satisfied.
		 */
		void SatisfyDemand(uint demand)
		{
			assert(demand <= this->anno.unsatisfied_demand);
			this->anno.unsatisfied_demand -= demand;
		}
	};

	/**
	 * Iterator for job edges.
	 */
	class EdgeIterator : public LinkGraph::BaseEdgeIterator<const LinkGraph::BaseEdge, Edge, EdgeIterator> {
		EdgeAnnotation *base_anno; ///< Array of annotations to be (indirectly) iterated.
	public:
		/**
		 * Constructor.
		 * @param base Array of edges to be iterated.
		 * @param base_anno Array of annotations to be iterated.
		 * @param current Start offset of iteration.
		 */
		EdgeIterator(const LinkGraph::BaseEdge *base, EdgeAnnotation *base_anno, NodeID current) :
				LinkGraph::BaseEdgeIterator<const LinkGraph::BaseEdge, Edge, EdgeIterator>(base, current),
				base_anno(base_anno) {}

		/**
		 * Dereference.
		 * @return Pair of the edge currently pointed to and the ID of its
		 *         other end.
		 */
		SmallPair<NodeID, Edge> operator*() const
		{
			return SmallPair<NodeID, Edge>(this->current, Edge(this->base[this->current], this->base_anno[this->current]));
		}

		/**
		 * Dereference. Has to be repeated here as operator* is different than
		 * in LinkGraph::EdgeWrapper.
		 * @return Fake pointer to pair of NodeID/Edge.
		 */
		FakePointer operator->() const {
			return FakePointer(this->operator*());
		}
	};

	/**
	 * Link graph job node. Wraps a constant link graph node and a modifiable
	 * node annotation.
	 */
	class Node : public LinkGraph::ConstNode {
	private:
		NodeAnnotation &node_anno;  ///< Annotation being wrapped.
		EdgeAnnotation *edge_annos; ///< Edge annotations belonging to this node.
	public:

		/**
		 * Constructor.
		 * @param lgj Job to take the node from.
		 * @param node ID of the node.
		 */
		Node (LinkGraphJob *lgj, NodeID node) :
			LinkGraph::ConstNode(&lgj->link_graph, node),
			node_anno(lgj->nodes[node]), edge_annos(lgj->edges[node])
		{}

		/**
		 * Retrieve an edge starting at this node. Mind that this returns an
		 * object, not a reference.
		 * @param to Remote end of the edge.
		 * @return Edge between this node and "to".
		 */
		Edge operator[](NodeID to) const { return Edge(this->edges[to], this->edge_annos[to]); }

		/**
		 * Iterator for the "begin" of the edge array. Only edges with capacity
		 * are iterated. The others are skipped.
		 * @return Iterator pointing to the first edge.
		 */
		EdgeIterator Begin() const { return EdgeIterator(this->edges, this->edge_annos, index); }

		/**
		 * Iterator for the "end" of the edge array. Only edges with capacity
		 * are iterated. The others are skipped.
		 * @return Iterator pointing beyond the last edge.
		 */
		EdgeIterator End() const { return EdgeIterator(this->edges, this->edge_annos, INVALID_NODE); }

		/**
		 * Get amount of supply that hasn't been delivered, yet.
		 * @return Undelivered supply.
		 */
		uint UndeliveredSupply() const { return this->node_anno.undelivered_supply; }

		/**
		 * Get the flows running through this node.
		 * @return Flows.
		 */
		FlowStatMap &Flows() { return this->node_anno.flows; }

		/**
		 * Get a constant version of the flows running through this node.
		 * @return Flows.
		 */
		const FlowStatMap &Flows() const { return this->node_anno.flows; }

		/**
		 * Get the paths this node is part of. Paths are always expected to be
		 * sorted so that those with flow == 0 are in the back of the list.
		 * @return Paths.
		 */
		PathList &Paths() { return this->node_anno.paths; }

		/**
		 * Get a constant version of the paths this node is part of.
		 * @return Paths.
		 */
		const PathList &Paths() const { return this->node_anno.paths; }

		/**
		 * Deliver some supply, adding demand to the respective edge.
		 * @param to Destination for supply.
		 * @param amount Amount of supply to be delivered.
		 */
		void DeliverSupply(NodeID to, uint amount)
		{
			this->node_anno.undelivered_supply -= amount;
			(*this)[to].AddDemand(amount);
		}
	};

	/**
	 * Bare constructor, only for save/load. link_graph, join_date and actually
	 * settings have to be brutally const-casted in order to populate them.
	 */
	LinkGraphJob() : settings(_settings_game.linkgraph),
			join_date(INVALID_DATE) {}

	LinkGraphJob(const LinkGraph &orig);
	~LinkGraphJob();

	void Init();

	/**
	 * Check if job is supposed to be finished.
	 * @return True if job should be finished by now, false if not.
	 */
	inline bool IsFinished() const { return this->join_date <= _date; }

	/**
	 * Get the date when the job should be finished.
	 * @return Join date.
	 */
	inline Date JoinDate() const { return join_date; }

	/**
	 * Change the join date on date cheating.
	 * @param interval Number of days to add.
	 */
	inline void ShiftJoinDate(int interval) { this->join_date += interval; }

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
	inline Node operator[](NodeID num) { return Node(this, num); }

	/**
	 * Get the size of the underlying link graph.
	 * @return Size.
	 */
	inline uint Size() const { return this->link_graph.Size(); }

	/**
	 * Get the cargo of the underlying link graph.
	 * @return Cargo.
	 */
	inline CargoID Cargo() const { return this->link_graph.Cargo(); }

	/**
	 * Get the date when the underlying link graph was last compressed.
	 * @return Compression date.
	 */
	inline Date LastCompression() const { return this->link_graph.LastCompression(); }

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

#define FOR_ALL_LINK_GRAPH_JOBS(var) FOR_ALL_ITEMS_FROM(LinkGraphJob, link_graph_job_index, var, 0)

/**
 * A leg of a path in the link graph. Paths can form trees by being "forked".
 */
class Path {
public:
	static Path *invalid_path;

	Path(NodeID n, bool source = false);

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
		return Clamp(free, PATH_CAP_MIN_FREE, PATH_CAP_MAX_FREE) * PATH_CAP_MULTIPLIER / max(total, 1U);
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
	 * Some boundaries to clamp agains in order to avoid integer overflows.
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
