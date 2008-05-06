/* $Id$ */

/** @file countedobj.cpp Support for reference counted objects. */

#include "../stdafx.h"

#include "countedptr.hpp"

int32 SimpleCountedObject::AddRef()
{
	return ++m_ref_cnt;
}

int32 SimpleCountedObject::Release()
{
	int32 res = --m_ref_cnt;
	assert(res >= 0);
	if (res == 0) {
		FinalRelease();
		delete this;
	}
	return res;
}
