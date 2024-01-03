/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file countedobj.cpp Support for reference counted objects. */

#include "../stdafx.h"

#include "countedptr.hpp"

#include "../safeguards.h"

int32_t SimpleCountedObject::AddRef()
{
	return ++m_ref_cnt;
}

int32_t SimpleCountedObject::Release()
{
	int32_t res = --m_ref_cnt;
	assert(res >= 0);
	if (res == 0) {
		try {
			FinalRelease(); // may throw, for example ScriptTest/ExecMode
		} catch (...) {
			delete this;
			throw;
		}
		delete this;
	}
	return res;
}
