/* $Id$ */

/** @file economy_base.h Base classes related to the economy. */

#ifndef ECONOMY_BASE_H
#define ECONOMY_BASE_H

#include "cargopacket.h"

/**
 * Helper class to perform the cargo payment.
 */
struct CargoPayment {
	Vehicle *front;      ///< The front vehicle to do the payment of
	Money route_profit;  ///< The amount of money to add/remove from the bank account
	Money visual_profit; ///< The visual profit to show

	Company *owner;            ///< The owner of the vehicle
	StationID current_station; ///< The current station
	CargoID ct;                ///< The currently handled cargo type

	/** Constructor for pool saveload */
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

#endif /* ECONOMY_BASE_H */
