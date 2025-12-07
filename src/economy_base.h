/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file economy_base.h Base classes related to the economy. */

#ifndef ECONOMY_BASE_H
#define ECONOMY_BASE_H

#include "cargopacket.h"

/** Type of pool to store cargo payments in; little over 1 million. */
using CargoPaymentPool = Pool<CargoPayment, CargoPaymentID, 512>;
/** The actual pool to store cargo payments in. */
extern CargoPaymentPool _cargo_payment_pool;

/**
 * Helper class to perform the cargo payment.
 */
struct CargoPayment : CargoPaymentPool::PoolItem<&_cargo_payment_pool> {
	/* CargoPaymentID index member of CargoPaymentPool is 4 bytes. */
	StationID current_station = StationID::Invalid(); ///< NOSAVE: The current station

	Vehicle *front = nullptr; ///< The front vehicle to do the payment of
	Money route_profit = 0; ///< The amount of money to add/remove from the bank account
	Money visual_profit = 0; ///< The visual profit to show
	Money visual_transfer = 0; ///< The transfer credits to be shown

	/** Constructor for pool saveload */
	CargoPayment() {}
	CargoPayment(Vehicle *front);
	~CargoPayment();

	Money PayTransfer(CargoType cargo, const CargoPacket *cp, uint count, TileIndex current_tile);
	void PayFinalDelivery(CargoType cargo, const CargoPacket *cp, uint count, TileIndex current_tile);
};

#endif /* ECONOMY_BASE_H */
