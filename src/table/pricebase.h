/* $Id$ */

/** @file pricebase.h Price Bases */

static PriceBaseSpec _price_base_specs[NUM_PRICES] = {
	{    100, PCAT_NONE        }, ///< station_value
	{    100, PCAT_CONSTRUCTION}, ///< build_rail
	{     95, PCAT_CONSTRUCTION}, ///< build_road
	{     65, PCAT_CONSTRUCTION}, ///< build_signals
	{    275, PCAT_CONSTRUCTION}, ///< build_bridge
	{    600, PCAT_CONSTRUCTION}, ///< build_train_depot
	{    500, PCAT_CONSTRUCTION}, ///< build_road_depot
	{    700, PCAT_CONSTRUCTION}, ///< build_ship_depot
	{    450, PCAT_CONSTRUCTION}, ///< build_tunnel
	{    200, PCAT_CONSTRUCTION}, ///< train_station_track
	{    180, PCAT_CONSTRUCTION}, ///< train_station_length
	{    600, PCAT_CONSTRUCTION}, ///< build_airport
	{    200, PCAT_CONSTRUCTION}, ///< build_bus_station
	{    200, PCAT_CONSTRUCTION}, ///< build_truck_station
	{    350, PCAT_CONSTRUCTION}, ///< build_dock
	{ 400000, PCAT_CONSTRUCTION}, ///< build_railvehicle
	{   2000, PCAT_CONSTRUCTION}, ///< build_railwagon
	{ 700000, PCAT_CONSTRUCTION}, ///< aircraft_base
	{  14000, PCAT_CONSTRUCTION}, ///< roadveh_base
	{  65000, PCAT_CONSTRUCTION}, ///< ship_base
	{     20, PCAT_CONSTRUCTION}, ///< build_trees
	{    250, PCAT_CONSTRUCTION}, ///< terraform
	{     20, PCAT_CONSTRUCTION}, ///< clear_grass
	{     40, PCAT_CONSTRUCTION}, ///< clear_roughland
	{    200, PCAT_CONSTRUCTION}, ///< clear_rocks
	{    500, PCAT_CONSTRUCTION}, ///< clear_fields
	{     20, PCAT_CONSTRUCTION}, ///< remove_trees
	{    -70, PCAT_CONSTRUCTION}, ///< remove_rail
	{     10, PCAT_CONSTRUCTION}, ///< remove_signals
	{     50, PCAT_CONSTRUCTION}, ///< clear_bridge
	{     80, PCAT_CONSTRUCTION}, ///< remove_train_depot
	{     80, PCAT_CONSTRUCTION}, ///< remove_road_depot
	{     90, PCAT_CONSTRUCTION}, ///< remove_ship_depot
	{     30, PCAT_CONSTRUCTION}, ///< clear_tunnel
	{  10000, PCAT_CONSTRUCTION}, ///< clear_water
	{     50, PCAT_CONSTRUCTION}, ///< remove_rail_station
	{     30, PCAT_CONSTRUCTION}, ///< remove_airport
	{     50, PCAT_CONSTRUCTION}, ///< remove_bus_station
	{     50, PCAT_CONSTRUCTION}, ///< remove_truck_station
	{     55, PCAT_CONSTRUCTION}, ///< remove_dock
	{   1600, PCAT_CONSTRUCTION}, ///< remove_house
	{     40, PCAT_CONSTRUCTION}, ///< remove_road
	{   5600, PCAT_RUNNING     }, ///< running_rail[0] steam
	{   5200, PCAT_RUNNING     }, ///< running_rail[1] diesel
	{   4800, PCAT_RUNNING     }, ///< running_rail[2] electric
	{   9600, PCAT_RUNNING     }, ///< aircraft_running
	{   1600, PCAT_RUNNING     }, ///< roadveh_running
	{   5600, PCAT_RUNNING     }, ///< ship_running
	{1000000, PCAT_CONSTRUCTION}, ///< build_industry
};
