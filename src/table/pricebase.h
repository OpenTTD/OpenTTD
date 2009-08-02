/* $Id$ */

/** @file pricebase.h Price Bases */

static PriceBaseSpec _price_base_specs[NUM_PRICES] = {
	{    100, PC_NONE        }, ///< station_value
	{    100, PC_CONSTRUCTION}, ///< build_rail
	{     95, PC_CONSTRUCTION}, ///< build_road
	{     65, PC_CONSTRUCTION}, ///< build_signals
	{    275, PC_CONSTRUCTION}, ///< build_bridge
	{    600, PC_CONSTRUCTION}, ///< build_train_depot
	{    500, PC_CONSTRUCTION}, ///< build_road_depot
	{    700, PC_CONSTRUCTION}, ///< build_ship_depot
	{    450, PC_CONSTRUCTION}, ///< build_tunnel
	{    200, PC_CONSTRUCTION}, ///< train_station_track
	{    180, PC_CONSTRUCTION}, ///< train_station_length
	{    600, PC_CONSTRUCTION}, ///< build_airport
	{    200, PC_CONSTRUCTION}, ///< build_bus_station
	{    200, PC_CONSTRUCTION}, ///< build_truck_station
	{    350, PC_CONSTRUCTION}, ///< build_dock
	{ 400000, PC_CONSTRUCTION}, ///< build_railvehicle
	{   2000, PC_CONSTRUCTION}, ///< build_railwagon
	{ 700000, PC_CONSTRUCTION}, ///< aircraft_base
	{  14000, PC_CONSTRUCTION}, ///< roadveh_base
	{  65000, PC_CONSTRUCTION}, ///< ship_base
	{     20, PC_CONSTRUCTION}, ///< build_trees
	{    250, PC_CONSTRUCTION}, ///< terraform
	{     20, PC_CONSTRUCTION}, ///< clear_grass
	{     40, PC_CONSTRUCTION}, ///< clear_roughland
	{    200, PC_CONSTRUCTION}, ///< clear_rocks
	{    500, PC_CONSTRUCTION}, ///< clear_fields
	{     20, PC_CONSTRUCTION}, ///< remove_trees
	{    -70, PC_CONSTRUCTION}, ///< remove_rail
	{     10, PC_CONSTRUCTION}, ///< remove_signals
	{     50, PC_CONSTRUCTION}, ///< clear_bridge
	{     80, PC_CONSTRUCTION}, ///< remove_train_depot
	{     80, PC_CONSTRUCTION}, ///< remove_road_depot
	{     90, PC_CONSTRUCTION}, ///< remove_ship_depot
	{     30, PC_CONSTRUCTION}, ///< clear_tunnel
	{  10000, PC_CONSTRUCTION}, ///< clear_water
	{     50, PC_CONSTRUCTION}, ///< remove_rail_station
	{     30, PC_CONSTRUCTION}, ///< remove_airport
	{     50, PC_CONSTRUCTION}, ///< remove_bus_station
	{     50, PC_CONSTRUCTION}, ///< remove_truck_station
	{     55, PC_CONSTRUCTION}, ///< remove_dock
	{   1600, PC_CONSTRUCTION}, ///< remove_house
	{     40, PC_CONSTRUCTION}, ///< remove_road
	{   5600, PC_RUNNING     }, ///< running_rail[0] steam
	{   5200, PC_RUNNING     }, ///< running_rail[1] diesel
	{   4800, PC_RUNNING     }, ///< running_rail[2] electric
	{   9600, PC_RUNNING     }, ///< aircraft_running
	{   1600, PC_RUNNING     }, ///< roadveh_running
	{   5600, PC_RUNNING     }, ///< ship_running
	{1000000, PC_CONSTRUCTION}, ///< build_industry
};
