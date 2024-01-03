/** @file mcf.h Declaration of Multi-Commodity-Flow solver */

#ifndef MCF_H
#define MCF_H

#include "linkgraphjob_base.h"

typedef std::vector<Path *> PathVector;

/**
 * Multi-commodity flow calculating base class.
 */
class MultiCommodityFlow {
protected:
	/**
	 * Constructor.
	 * @param job Link graph job being executed.
	 */
	MultiCommodityFlow(LinkGraphJob &job) : job(job),
			max_saturation(job.Settings().short_path_saturation)
	{}

	template<class Tannotation, class Tedge_iterator>
	void Dijkstra(NodeID from, PathVector &paths);

	uint PushFlow(Node &node, NodeID to, Path *path, uint accuracy, uint max_saturation);

	void CleanupPaths(NodeID source, PathVector &paths);

	LinkGraphJob &job;   ///< Job we're working with.
	uint max_saturation; ///< Maximum saturation for edges.
};

/**
 * First pass of the MCF calculation. Saturates shortest paths first, creates
 * new paths if needed, eliminates cycles. This calculation is of exponential
 * complexity in the number of nodes but the constant factors are sufficiently
 * small to make it usable for most real-life link graph components. You can
 * deal with performance problems that might occur here in multiple ways:
 * - The overall accuracy is used here to determine how much flow is assigned
 *   in each loop. The lower the accuracy, the more flow is assigned, the less
 *   loops it takes to assign all flow.
 * - The short_path_saturation setting determines when this pass stops. The
 *   lower you set it, the less flow will be assigned in this pass, the less
 *   time it will take.
 * - You can increase the recalculation interval to allow for longer running
 *   times without creating lags.
 */
class MCF1stPass : public MultiCommodityFlow {
private:
	bool EliminateCycles();
	bool EliminateCycles(PathVector &path, NodeID origin_id, NodeID next_id);
	void EliminateCycle(PathVector &path, Path *cycle_begin, uint flow);
	uint FindCycleFlow(const PathVector &path, const Path *cycle_begin);
public:
	MCF1stPass(LinkGraphJob &job);
};

/**
 * Second pass of the MCF calculation. Saturates paths with most capacity left
 * first and doesn't create any paths along edges that haven't been visited in
 * the first pass. This is why it doesn't have to do any cycle detection and
 * elimination. As cycle detection is the most intense problem in the first
 * pass this pass is cheaper. The accuracy is used here, too.
 */
class MCF2ndPass : public MultiCommodityFlow {
public:
	MCF2ndPass(LinkGraphJob &job);
};

/**
 * Link graph handler for MCF. Creates MultiCommodityFlow instance according to
 * the template parameter.
 */
template<class Tpass>
class MCFHandler : public ComponentHandler {
public:

	/**
	 * Run the calculation.
	 * @param graph Component to be calculated.
	 */
	void Run(LinkGraphJob &job) const override { Tpass pass(job); }
};

#endif /* MCF_H */
