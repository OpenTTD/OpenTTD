/* $Id$ */

/** @file ai_cargo.cpp Implementation of AICargo. */

#include "ai_cargo.hpp"
#include "../../cargotype.h"
#include "../../economy_func.h"
#include "../../core/alloc_func.hpp"
#include "../../core/bitmath_func.hpp"
#include "../../newgrf_cargo.h"

/* static */ bool AICargo::IsValidCargo(CargoID cargo_type)
{
	return (cargo_type < NUM_CARGO && ::GetCargo(cargo_type)->IsValid());
}

/* static */ char *AICargo::GetCargoLabel(CargoID cargo_type)
{
	if (!IsValidCargo(cargo_type)) return NULL;
	const CargoSpec *cargo = ::GetCargo(cargo_type);

	/* cargo->label is a uint32 packing a 4 character non-terminated string,
	 * like "PASS", "COAL", "OIL_". New ones can be defined by NewGRFs */
	char *cargo_label = MallocT<char>(sizeof(cargo->label) + 1);
	for (uint i = 0; i < sizeof(cargo->label); i++) {
		cargo_label[i] = GB(cargo->label, (uint8)(sizeof(cargo->label) - i - 1) * 8, 8);
	}
	cargo_label[sizeof(cargo->label)] = '\0';
	return cargo_label;
}

/* static */ bool AICargo::IsFreight(CargoID cargo_type)
{
	if (!IsValidCargo(cargo_type)) return false;
	const CargoSpec *cargo = ::GetCargo(cargo_type);
	return cargo->is_freight;
}

/* static */ bool AICargo::HasCargoClass(CargoID cargo_type, CargoClass cargo_class)
{
	if (!IsValidCargo(cargo_type)) return false;
	return ::IsCargoInClass(cargo_type, (::CargoClass)cargo_class);
}

/* static */ AICargo::TownEffect AICargo::GetTownEffect(CargoID cargo_type)
{
	if (!IsValidCargo(cargo_type)) return TE_NONE;

	return (AICargo::TownEffect)GetCargo(cargo_type)->town_effect;
}

/* static */ Money AICargo::GetCargoIncome(CargoID cargo_type, uint32 distance, uint32 days_in_transit)
{
	if (!IsValidCargo(cargo_type)) return -1;
	return ::GetTransportedGoodsIncome(1, distance, Clamp(days_in_transit * 2 / 5, 0, 255), cargo_type);
}
