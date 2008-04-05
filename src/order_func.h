/* $Id$ */

/** @file order_func.h Functions related to orders. */

#ifndef ORDER_FUNC_H
#define ORDER_FUNC_H

#include "order_type.h"
#include "vehicle_type.h"
#include "tile_type.h"
#include "group_type.h"
#include "date_type.h"

struct BackuppedOrders {
	BackuppedOrders() : order(NULL), name(NULL) { }
	~BackuppedOrders() { free(order); free(name); }

	VehicleID clone;
	VehicleOrderID orderindex;
	GroupID group;
	Order *order;
	uint16 service_interval;
	char *name;
};

extern TileIndex _backup_orders_tile;
extern BackuppedOrders _backup_orders_data;

void BackupVehicleOrders(const Vehicle *v, BackuppedOrders *order = &_backup_orders_data);
void RestoreVehicleOrders(const Vehicle *v, const BackuppedOrders *order = &_backup_orders_data);

/* Functions */
void RemoveOrderFromAllVehicles(OrderType type, DestinationID destination);
void InvalidateVehicleOrder(const Vehicle *v);
bool VehicleHasDepotOrders(const Vehicle *v);
void CheckOrders(const Vehicle*);
void DeleteVehicleOrders(Vehicle *v);
bool ProcessOrders(Vehicle *v);

#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

/**
 * Get the service interval domain.
 * Get the new proposed service interval for the vehicle is indeed, clamped
 * within the given bounds. @see MIN_SERVINT_PERCENT ,etc.
 * @param index proposed service interval
 * @return service interval
 */
Date GetServiceIntervalClamped(uint index);

#endif /* ORDER_FUNC_H */
