/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file callback_table.h Table with all command callbacks. */

#ifndef CALLBACK_TABLE_H
#define CALLBACK_TABLE_H

#include "command_type.h"

extern CommandCallback * const _callback_table[];
extern const int _callback_table_count;

#endif /* CALLBACK_TABLE_H */
