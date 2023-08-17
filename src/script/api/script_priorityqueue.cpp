/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_priorityqueue.cpp Implementation of ScriptPriorityQueue. */

#include "../../stdafx.h"
#include "script_priorityqueue.hpp"
#include "script_error.hpp"
#include "../squirrel_helper.hpp"
#include "../script_instance.hpp"
#include "../../debug.h"

#include "../../safeguards.h"


static bool operator==(const ScriptPriorityQueue::PriorityItem &lhs, const HSQOBJECT &rhs)
{
	return lhs.second._type == rhs._type && lhs.second._unVal.raw == rhs._unVal.raw;
}


ScriptPriorityQueue::~ScriptPriorityQueue()
{
	/* Release reference to stored objects. */
	auto inst = ScriptObject::GetActiveInstance();
	if (!inst->InShutdown()) {
		for (auto &i : this->queue) inst->ReleaseSQObject(const_cast<HSQOBJECT *>(&i.second));
	}
}

SQInteger ScriptPriorityQueue::Insert(HSQUIRRELVM vm)
{
	HSQOBJECT item;
	SQInteger priority;
	sq_resetobject(&item);
	sq_getstackobj(vm, 2, &item);
	sq_getinteger(vm, 3, &priority);

	sq_addref(vm, &item); // Keep object alive.

	this->queue.emplace_back(priority, item);
	std::push_heap(this->queue.begin(), this->queue.end(), this->comp);

	return SQConvert::Return<bool>::Set(vm, true);
}

SQInteger ScriptPriorityQueue::Pop(HSQUIRRELVM vm)
{
	if (this->IsEmpty()) {
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_FAILED);
		sq_pushnull(vm);
		return 1;
	}

	HSQOBJECT item = this->queue.front().second;
	std::pop_heap(this->queue.begin(), this->queue.end(), this->comp);
	this->queue.pop_back();

	/* Store the object on the Squirrel stack before releasing it to make sure the ref count can't drop to zero. */
	auto ret = SQConvert::Return<HSQOBJECT>::Set(vm, item);
	sq_release(vm, &item);
	return ret;
}

SQInteger ScriptPriorityQueue::Peek(HSQUIRRELVM vm)
{
	if (this->IsEmpty()) {
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_FAILED);
		sq_pushnull(vm);
		return 1;
	}

	return SQConvert::Return<HSQOBJECT>::Set(vm, this->queue.front().second);
}

SQInteger ScriptPriorityQueue::Exists(HSQUIRRELVM vm)
{
	HSQOBJECT item;
	sq_resetobject(&item);
	sq_getstackobj(vm, 2, &item);

	return SQConvert::Return<bool>::Set(vm, std::find(this->queue.cbegin(), this->queue.cend(), item) != this->queue.cend());
}

SQInteger ScriptPriorityQueue::Clear(HSQUIRRELVM vm)
{
	/* Release reference to stored objects. */
	for (auto &i : this->queue) sq_release(vm, const_cast<HSQOBJECT *>(&i.second));
	this->queue.clear();

	return 0;
}

bool ScriptPriorityQueue::IsEmpty()
{
	return this->queue.empty();
}

SQInteger ScriptPriorityQueue::Count()
{
	return (SQInteger)this->queue.size();
}
