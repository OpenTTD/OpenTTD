/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_list.cpp Implementation of AIList. */

#include "ai_list.hpp"

void AIList::AddItem(int32 item, int32 value)
{
	AIAbstractList::AddItem(item);
	this->SetValue(item, value);
}

void AIList::ChangeItem(int32 item, int32 value)
{
	this->SetValue(item, value);
}

void AIList::RemoveItem(int32 item)
{
	AIAbstractList::RemoveItem(item);
}

SQInteger AIList::_set(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;
	if (sq_gettype(vm, 3) != OT_INTEGER || sq_gettype(vm, 3) == OT_NULL) {
		return sq_throwerror(vm, _SC("you can only assign integers to this list"));
	}

	SQInteger idx, val;
	sq_getinteger(vm, 2, &idx);
	if (sq_gettype(vm, 3) == OT_NULL) {
		this->RemoveItem(idx);
		return 0;
	}

	sq_getinteger(vm, 3, &val);
	if (!this->HasItem(idx)) {
		this->AddItem(idx, val);
		return 0;
	}

	this->ChangeItem(idx, val);
	return 0;
}
