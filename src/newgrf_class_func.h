/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_class_func.h Implementation of the NewGRF class' functions. */

#include "newgrf_class.h"

#include "table/strings.h"

#define DEFINE_NEWGRF_CLASS_METHOD(type) \
	template <typename Tspec, typename Tid, Tid Tmax> \
	type NewGRFClass<Tspec, Tid, Tmax>

/** Instantiate the array. */
template <typename Tspec, typename Tid, Tid Tmax>
NewGRFClass<Tspec, Tid, Tmax> NewGRFClass<Tspec, Tid, Tmax>::classes[Tmax];

DEFINE_NEWGRF_CLASS_METHOD(void)::Reset()
{
	for (Tid i = (Tid)0; i < Tmax; i++) {
		classes[i].global_id = 0;
		classes[i].name      = STR_EMPTY;
		classes[i].count     = 0;

		free(classes[i].spec);
		classes[i].spec = NULL;
	}

	InsertDefaults();
}

DEFINE_NEWGRF_CLASS_METHOD(Tid)::Allocate(uint32 global_id)
{
	for (Tid i = (Tid)0; i < Tmax; i++) {
		if (classes[i].global_id == global_id) {
			/* ClassID is already allocated, so reuse it. */
			return i;
		} else if (classes[i].global_id == 0) {
			/* This class is empty, so allocate it to the global id. */
			classes[i].global_id = global_id;
			return i;
		}
	}

	grfmsg(2, "ClassAllocate: already allocated %d classes, using default", Tmax);
	return (Tid)0;
}

DEFINE_NEWGRF_CLASS_METHOD(void)::SetName(Tid cls_id, StringID name)
{
	assert(cls_id < Tmax);
	classes[cls_id].name = name;
}

DEFINE_NEWGRF_CLASS_METHOD(void)::Assign(Tspec *spec)
{
	assert(spec->cls_id < Tmax);
	NewGRFClass<Tspec, Tid, Tmax> *cls = &classes[spec->cls_id];

	uint i = cls->count++;
	cls->spec = ReallocT(cls->spec, cls->count);

	cls->spec[i] = spec;
}

DEFINE_NEWGRF_CLASS_METHOD(StringID)::GetName(Tid cls_id)
{
	assert(cls_id < Tmax);
	return classes[cls_id].name;
}

DEFINE_NEWGRF_CLASS_METHOD(uint)::GetCount()
{
	uint i;
	for (i = 0; i < Tmax && classes[i].global_id != 0; i++) {}
	return i;
}

DEFINE_NEWGRF_CLASS_METHOD(uint)::GetCount(Tid cls_id)
{
	assert(cls_id < Tmax);
	return classes[cls_id].count;
}

DEFINE_NEWGRF_CLASS_METHOD(const Tspec *)::Get(Tid cls_id, uint index)
{
	assert(cls_id < Tmax);
	if (index < classes[cls_id].count) return classes[cls_id].spec[index];

	/* If the custom spec isn't defined any more, then the GRF file probably was not loaded. */
	return NULL;
}

DEFINE_NEWGRF_CLASS_METHOD(const Tspec *)::GetByGrf(uint32 grfid, byte local_id, int *index)
{
	uint j;

	for (Tid i = (Tid)0; i < Tmax; i++) {
		for (j = 0; j < classes[i].count; j++) {
			const Tspec *spec = classes[i].spec[j];
			if (spec == NULL) continue;
			if (spec->grf_prop.grffile->grfid == grfid && spec->grf_prop.local_id == local_id) {
				if (index != NULL) *index = j;
				return spec;
			}
		}
	}

	return NULL;
}

#undef DEFINE_NEWGRF_CLASS_METHOD

/** Force instantiation of the methods so we don't get linker errors. */
#define INSTANTIATE_NEWGRF_CLASS_METHODS(name, Tspec, Tid, Tmax) \
	template void name::Reset(); \
	template Tid name::Allocate(uint32 global_id); \
	template void name::SetName(Tid cls_id, StringID name); \
	template void name::Assign(Tspec *spec); \
	template StringID name::GetName(Tid cls_id); \
	template uint name::GetCount(); \
	template uint name::GetCount(Tid cls_id); \
	template const Tspec *name::Get(Tid cls_id, uint index); \
	template const Tspec *name::GetByGrf(uint32 grfid, byte localidx, int *index);
