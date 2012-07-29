/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_consist.cpp Properties for front vehicles/consists. */

#include "stdafx.h"
#include "base_consist.h"

/**
 * Copy properties of other BaseConsist.
 * @param src Source for copying
 */
void BaseConsist::CopyConsistPropertiesFrom(const BaseConsist *src)
{
	if (this == src) return;

	this->service_interval = src->service_interval;
	this->cur_real_order_index = src->cur_real_order_index;
}
