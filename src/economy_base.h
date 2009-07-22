/* $Id$ */

/** @file economy_base.h Base classes related to the economy. */

#ifndef ECONOMY_BASE_H
#define ECONOMY_BASE_H

#include "cargopacket.h"
#include "vehicle_type.h"
#include "company_type.h"

/** Type of pool to store cargo payments in. */
typedef Pool<CargoPayment, CargoPaymentID, 512, 64000> CargoPaymentPool;
/** The actual pool to store cargo payments in. */
extern CargoPaymentPool _cargo_payment_pool;

/**
 * Helper class to perform the cargo payment.
 */
struct CargoPayment : CargoPaymentPool::PoolItem<&_cargo_payment_pool> {
	Vehicle *front;      ///< The front vehicle to do the payment of
	Money route_profit;  ///< The amount of money to add/remove from the bank account
	Money visual_profit; ///< The visual profit to show

	/* Unsaved variables */
	Company *owner;            ///< The owner of the vehicle
	StationID current_station; ///< The current station
	CargoID ct;                ///< The currently handled cargo type

	/** Constructor for pool saveload */
	CargoPayment() {}
	CargoPayment(Vehicle *front);
	~CargoPayment();

	void PayTransfer(CargoPacket *cp, uint count);
	void PayFinalDelivery(CargoPacket *cp, uint count);

	/**
	 * Sets the currently handled cargo type.
	 * @param ct the cargo type to handle from now on.
	 */
	void SetCargo(CargoID ct) { this->ct = ct; }
};

#define FOR_ALL_CARGO_PAYMENTS_FROM(var, start) FOR_ALL_ITEMS_FROM(CargoPayment, cargo_payment_index, var, start)
#define FOR_ALL_CARGO_PAYMENTS(var) FOR_ALL_CARGO_PAYMENTS_FROM(var, 0)

#endif /* ECONOMY_BASE_H */
