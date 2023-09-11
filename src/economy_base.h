/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_base.h Base classes related to the economy. */

#ifndef ECONOMY_BASE_H
#define ECONOMY_BASE_H

#include "cargopacket.h"
#include "company_type.h"

/** Type of pool to store cargo payments in; little over 1 million. */
typedef Pool<CargoPayment, CargoPaymentID, 512, 0xFF000> CargoPaymentPool;
/** The actual pool to store cargo payments in. */
extern CargoPaymentPool _cargo_payment_pool;

/**
 * Helper class to perform the cargo payment.
 */
struct CargoPayment : CargoPaymentPool::PoolItem<&_cargo_payment_pool> {
	/* CargoPaymentID index member of CargoPaymentPool is 4 bytes. */
	StationID current_station; ///< NOSAVE: The current station
	CargoID ct; ///< NOSAVE: The currently handled cargo type
	Company *owner; ///< NOSAVE: The owner of the vehicle

	Vehicle *front;        ///< The front vehicle to do the payment of
	Money route_profit;    ///< The amount of money to add/remove from the bank account
	Money visual_profit;   ///< The visual profit to show
	Money visual_transfer; ///< The transfer credits to be shown

	/** Constructor for pool saveload */
	CargoPayment() {}
	CargoPayment(Vehicle *front);
	~CargoPayment();

	Money PayTransfer(const CargoPacket *cp, uint count, TileIndex current_tile);
	void PayFinalDelivery(const CargoPacket *cp, uint count, TileIndex current_tile);

	/**
	 * Sets the currently handled cargo type.
	 * @param ct the cargo type to handle from now on.
	 */
	void SetCargo(CargoID ct) { this->ct = ct; }
};

#endif /* ECONOMY_BASE_H */
