/** @file demands.cpp Definition of demand calculating link graph handler. */

#include "../stdafx.h"
#include "demands.h"
#include <queue>

#include "../safeguards.h"

typedef std::queue<NodeID> NodeList;

/**
 * Scale various things according to symmetric/asymmetric distribution.
 */
class Scaler {
public:
	void SetDemands(LinkGraphJob &job, NodeID from, NodeID to, uint demand_forw);
};

/**
 * Scaler for symmetric distribution.
 */
class SymmetricScaler : public Scaler {
public:
	/**
	 * Constructor.
	 * @param mod_size Size modifier to be used. Determines how much demands
	 *                 increase with the supply of the remote station.
	 */
	inline SymmetricScaler(uint mod_size) : mod_size(mod_size), supply_sum(0),
		demand_per_node(0)
	{}

	/**
	 * Count a node's supply into the sum of supplies.
	 * @param node Node.
	 */
	inline void AddNode(const Node &node)
	{
		this->supply_sum += node.Supply();
	}

	/**
	 * Calculate the mean demand per node using the sum of supplies.
	 * @param num_demands Number of accepting nodes.
	 */
	inline void SetDemandPerNode(uint num_demands)
	{
		this->demand_per_node = max(this->supply_sum / num_demands, 1U);
	}

	/**
	 * Get the effective supply of one node towards another one. In symmetric
	 * distribution the supply of the other node is weighed in.
	 * @param from The supplying node.
	 * @param to The receiving node.
	 * @return Effective supply.
	 */
	inline uint EffectiveSupply(const Node &from, const Node &to)
	{
		return max(from.Supply() * max(1U, to.Supply()) * this->mod_size / 100 / this->demand_per_node, 1U);
	}

	/**
	 * Check if there is any acceptance left for this node. In symmetric distribution
	 * nodes only accept anything if they also supply something. So if
	 * undelivered_supply == 0 at the node there isn't any demand left either.
	 * @param to Node to be checked.
	 * @return If demand is left.
	 */
	inline bool HasDemandLeft(const Node &to)
	{
		return (to.Supply() == 0 || to.UndeliveredSupply() > 0) && to.Demand() > 0;
	}

	void SetDemands(LinkGraphJob &job, NodeID from, NodeID to, uint demand_forw);

private:
	uint mod_size;        ///< Size modifier. Determines how much demands increase with the supply of the remote station.
	uint supply_sum;      ///< Sum of all supplies in the component.
	uint demand_per_node; ///< Mean demand associated with each node.
};

/**
 * A scaler for asymmetric distribution.
 */
class AsymmetricScaler : public Scaler {
public:
	/**
	 * Nothing to do here.
	 * @param unused.
	 */
	inline void AddNode(const Node &)
	{
	}

	/**
	 * Nothing to do here.
	 * @param unused.
	 */
	inline void SetDemandPerNode(uint)
	{
	}

	/**
	 * Get the effective supply of one node towards another one.
	 * @param from The supplying node.
	 * @param unused.
	 */
	inline uint EffectiveSupply(const Node &from, const Node &)
	{
		return from.Supply();
	}

	/**
	 * Check if there is any acceptance left for this node. In asymmetric distribution
	 * nodes always accept as long as their demand > 0.
	 * @param to The node to be checked.
	 */
	inline bool HasDemandLeft(const Node &to) { return to.Demand() > 0; }
};

/**
 * Set the demands between two nodes using the given base demand. In symmetric mode
 * this sets demands in both directions.
 * @param job The link graph job.
 * @param from_id The supplying node.
 * @param to_id The receiving node.
 * @param demand_forw Demand calculated for the "forward" direction.
 */
void SymmetricScaler::SetDemands(LinkGraphJob &job, NodeID from_id, NodeID to_id, uint demand_forw)
{
	if (job[from_id].Demand() > 0) {
		uint demand_back = demand_forw * this->mod_size / 100;
		uint undelivered = job[to_id].UndeliveredSupply();
		if (demand_back > undelivered) {
			demand_back = undelivered;
			demand_forw = max(1U, demand_back * 100 / this->mod_size);
		}
		this->Scaler::SetDemands(job, to_id, from_id, demand_back);
	}

	this->Scaler::SetDemands(job, from_id, to_id, demand_forw);
}

/**
 * Set the demands between two nodes using the given base demand. In asymmetric mode
 * this only sets demand in the "forward" direction.
 * @param job The link graph job.
 * @param from_id The supplying node.
 * @param to_id The receiving node.
 * @param demand_forw Demand calculated for the "forward" direction.
 */
inline void Scaler::SetDemands(LinkGraphJob &job, NodeID from_id, NodeID to_id, uint demand_forw)
{
	job[from_id].DeliverSupply(to_id, demand_forw);
}

/**
 * Do the actual demand calculation, called from constructor.
 * @param job Job to calculate the demands for.
 * @tparam Tscaler Scaler to be used for scaling demands.
 */
template<class Tscaler>
void DemandCalculator::CalcDemand(LinkGraphJob &job, Tscaler scaler)
{
	NodeList supplies;
	NodeList demands;
	uint num_supplies = 0;
	uint num_demands = 0;

	for (NodeID node = 0; node < job.Size(); node++) {
		scaler.AddNode(job[node]);
		if (job[node].Supply() > 0) {
			supplies.push(node);
			num_supplies++;
		}
		if (job[node].Demand() > 0) {
			demands.push(node);
			num_demands++;
		}
	}

	if (num_supplies == 0 || num_demands == 0) return;

	/* Mean acceptance attributed to each node. If the distribution is
	 * symmetric this is relative to remote supply, otherwise it is
	 * relative to remote demand. */
	scaler.SetDemandPerNode(num_demands);
	uint chance = 0;

	while (!supplies.empty() && !demands.empty()) {
		NodeID from_id = supplies.front();
		supplies.pop();

		for (uint i = 0; i < num_demands; ++i) {
			assert(!demands.empty());
			NodeID to_id = demands.front();
			demands.pop();
			if (from_id == to_id) {
				/* Only one node with supply and demand left */
				if (demands.empty() && supplies.empty()) return;

				demands.push(to_id);
				continue;
			}

			int32 supply = scaler.EffectiveSupply(job[from_id], job[to_id]);
			assert(supply > 0);

			/* Scale the distance by mod_dist around max_distance */
			int32 distance = this->max_distance - (this->max_distance -
					(int32)DistanceMaxPlusManhattan(job[from_id].XY(), job[to_id].XY())) *
					this->mod_dist / 100;

			/* Scale the accuracy by distance around accuracy / 2 */
			int32 divisor = this->accuracy * (this->mod_dist - 50) / 100 +
					this->accuracy * distance / this->max_distance + 1;

			assert(divisor > 0);

			uint demand_forw = 0;
			if (divisor <= supply) {
				/* At first only distribute demand if
				 * effective supply / accuracy divisor >= 1
				 * Others are too small or too far away to be considered. */
				demand_forw = supply / divisor;
			} else if (++chance > this->accuracy * num_demands * num_supplies) {
				/* After some trying, if there is still supply left, distribute
				 * demand also to other nodes. */
				demand_forw = 1;
			}

			demand_forw = min(demand_forw, job[from_id].UndeliveredSupply());

			scaler.SetDemands(job, from_id, to_id, demand_forw);

			if (scaler.HasDemandLeft(job[to_id])) {
				demands.push(to_id);
			} else {
				num_demands--;
			}

			if (job[from_id].UndeliveredSupply() == 0) break;
		}

		if (job[from_id].UndeliveredSupply() != 0) {
			supplies.push(from_id);
		} else {
			num_supplies--;
		}
	}
}

/**
 * Create the DemandCalculator and immediately do the calculation.
 * @param job Job to calculate the demands for.
 */
DemandCalculator::DemandCalculator(LinkGraphJob &job) :
	max_distance(DistanceMaxPlusManhattan(TileXY(0,0), TileXY(MapMaxX(), MapMaxY())))
{
	const LinkGraphSettings &settings = job.Settings();
	CargoID cargo = job.Cargo();

	this->accuracy = settings.accuracy;
	this->mod_dist = settings.demand_distance;
	if (this->mod_dist > 100) {
		/* Increase effect of mod_dist > 100 */
		int over100 = this->mod_dist - 100;
		this->mod_dist = 100 + over100 * over100;
	}

	switch (settings.GetDistributionType(cargo)) {
		case DT_SYMMETRIC:
			this->CalcDemand<SymmetricScaler>(job, SymmetricScaler(settings.demand_size));
			break;
		case DT_ASYMMETRIC:
			this->CalcDemand<AsymmetricScaler>(job, AsymmetricScaler());
			break;
		default:
			/* Nothing to do. */
			break;
	}
}
