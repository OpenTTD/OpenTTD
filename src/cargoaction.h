/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargoaction.h Actions to be applied to cargo packets. */

#ifndef CARGOACTION_H
#define CARGOACTION_H

#include "cargopacket.h"

/**
 * Abstract action of removing cargo from a vehicle or a station.
 * @tparam Tsource CargoList subclass to remove cargo from.
 */
template<class Tsource>
class CargoRemoval {
protected:
	Tsource *source; ///< Source of the cargo.
	uint max_move;   ///< Maximum amount of cargo to be removed with this action.
	uint Preprocess(CargoPacket *cp);
	bool Postprocess(CargoPacket *cp, uint remove);
public:
	CargoRemoval(Tsource *source, uint max_move) : source(source), max_move(max_move) {}

	/**
	 * Returns how much more cargo can be removed with this action.
	 * @return Amount of cargo this action can still remove.
	 */
	uint MaxMove() { return this->max_move; }

	bool operator()(CargoPacket *cp);
};

/** Action of final delivery of cargo. */
class CargoDelivery : public CargoRemoval<VehicleCargoList> {
protected:
	CargoPayment *payment; ///< Payment object where payments will be registered.
public:
	CargoDelivery(VehicleCargoList *source, uint max_move, CargoPayment *payment) :
			CargoRemoval<VehicleCargoList>(source, max_move), payment(payment) {}
	bool operator()(CargoPacket *cp);
};

/**
 * Abstract action for moving cargo from one list to another.
 * @tparam Tsource CargoList subclass to remove cargo from.
 * @tparam Tdest CargoList subclass to add cargo to.
 */
template<class Tsource, class Tdest>
class CargoMovement {
protected:
	Tsource *source;    ///< Source of the cargo.
	Tdest *destination; ///< Destination for the cargo.
	uint max_move;      ///< Maximum amount of cargo to be moved with this action.
	CargoPacket *Preprocess(CargoPacket *cp);
public:
	CargoMovement(Tsource *source, Tdest *destination, uint max_move) : source(source), destination(destination), max_move(max_move) {}

	/**
	 * Returns how much more cargo can be moved with this action.
	 * @return Amount of cargo this action can still move.
	 */
	uint MaxMove() { return this->max_move; }
};

/** Action of transferring cargo from a vehicle to a station. */
class CargoTransfer : public CargoMovement<VehicleCargoList, StationCargoList> {
protected:
	CargoPayment *payment;  ///< Payment object for registering transfer credits.
public:
	CargoTransfer(VehicleCargoList *source, StationCargoList *destination, uint max_move, CargoPayment *payment) :
			CargoMovement<VehicleCargoList, StationCargoList>(source, destination, max_move), payment(payment) {}
	bool operator()(CargoPacket *cp);
};

/** Action of loading cargo from a station onto a vehicle. */
class CargoLoad : public CargoMovement<StationCargoList, VehicleCargoList> {
protected:
	TileIndex load_place; ///< TileIndex to be saved in the packets' loaded_at_xy.
public:
	CargoLoad(StationCargoList *source, VehicleCargoList *destination, uint max_move, TileIndex load_place) :
			CargoMovement<StationCargoList, VehicleCargoList>(source, destination, max_move), load_place(load_place) {}
	bool operator()(CargoPacket *cp);
};

/** Action of reserving cargo from a station to be loaded onto a vehicle. */
class CargoReservation: public CargoLoad {
public:
	CargoReservation(StationCargoList *source, VehicleCargoList *destination, uint max_move, TileIndex load_place) :
			CargoLoad(source, destination, max_move, load_place) {}
	bool operator()(CargoPacket *cp);
};

/** Action of returning previously reserved cargo from the vehicle to the station. */
class CargoReturn: public CargoMovement<VehicleCargoList, StationCargoList> {
public:
	CargoReturn(VehicleCargoList *source, StationCargoList *destination, uint max_move) :
			CargoMovement<VehicleCargoList, StationCargoList>(source, destination, max_move) {}
	bool operator()(CargoPacket *cp);
};

/** Action of shifting cargo from one vehicle to another. */
class CargoShift : public CargoMovement<VehicleCargoList, VehicleCargoList> {
public:
	CargoShift(VehicleCargoList *source, VehicleCargoList *destination, uint max_move) :
			CargoMovement<VehicleCargoList, VehicleCargoList>(source, destination, max_move) {}
	bool operator()(CargoPacket *cp);
};

#endif /* CARGOACTION_H */
