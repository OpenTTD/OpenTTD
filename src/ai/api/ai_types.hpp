/* $Id$ */

/** @file ai_types.hpp Defines all the types of the game, like VehicleID, .... */

#ifndef AI_TYPES_HPP
#define AI_TYPES_HPP

#include "../../core/overflowsafe_type.hpp"
#include "../../company_type.h"

/* Define all types here, so we don't have to include the whole _type.h maze */
typedef uint BridgeType;     //!< Internal name, not of any use for you.
typedef byte CargoID;        //!< The ID of a cargo.
class CommandCost;           //!< The cost of a command.
typedef uint16 EngineID;     //!< The ID of an engine.
typedef uint16 GroupID;      //!< The ID of a group.
typedef uint16 IndustryID;   //!< The ID of an industry.
typedef uint8 IndustryType;  //!< The ID of an industry-type.
typedef OverflowSafeInt64 Money; //!< Money, stored in a 32bit/64bit safe way. For AIs money is always in pounds.
typedef uint16 SignID;       //!< The ID of a sign.
typedef uint16 StationID;    //!< The ID of a station.
typedef uint16 StringID;     //!< The ID of a string.
typedef uint32 TileIndex;    //!< The ID of a tile (just named differently).
typedef uint16 TownID;       //!< The ID of a town.
typedef uint16 VehicleID;    //!< The ID of a vehicle.
typedef uint16 WaypointID;   //!< The ID of a waypoint.

/* Types we defined ourself, as the OpenTTD core doesn't have them (yet) */
typedef uint AIErrorType;    //!< The types of errors inside the NoAI framework.
typedef BridgeType BridgeID; //!< The ID of a bridge.
typedef uint16 SubsidyID;    //!< The ID of a subsidy.

#ifndef _SQUIRREL_H_
/* Life becomes easier when we can tell about a function it needs the VM, but
 *  without really including 'squirrel.h'. */
typedef struct SQVM *HSQUIRRELVM;  //!< Pointer to Squirrel Virtual Machine.
typedef int SQInteger;             //!< Squirrel Integer.
typedef struct SQObject HSQOBJECT; //!< Squirrel Object (fake declare)
#endif

#endif /* AI_TYPES_HPP */
