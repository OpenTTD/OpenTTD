/* $Id$ */

/** @file economy_base.h Base classes related to the economy. */

#ifndef ECONOMY_BASE_H
#define ECONOMY_BASE_H

#include "cargopacket.h"

DECLARE_OLD_POOL(CargoPayment, CargoPayment, 9, 125)

/**
 * Helper class to perform the cargo payment.
 */
struct CargoPayment : PoolItem<CargoPayment, CargoPaymentID, &_CargoPayment_pool> {
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

	inline bool IsValid() const { return this->front != NULL; }
};

#define FOR_ALL_CARGO_PAYMENTS_FROM(v, start) for (v = GetCargoPayment(start); v != NULL; v = (v->index + 1U < GetCargoPaymentPoolSize()) ? GetCargoPayment(v->index + 1) : NULL) if (v->IsValid())
#define FOR_ALL_CARGO_PAYMENTS(var) FOR_ALL_CARGO_PAYMENTS_FROM(var, 0)

/**
 * Whether this savegame is a savegame with cargo payment or not.
 * This is important to know when loading a savegame.
 */
extern bool _cargo_payment_savegame;

#endif /* ECONOMY_BASE_H */
