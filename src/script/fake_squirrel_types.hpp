/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fake_squirrel_types.hpp Provides definitions for some squirrel types to prevent including squirrel.h in header files.*/

#ifndef FAKE_SQUIRREL_TYPES_HPP
#define FAKE_SQUIRREL_TYPES_HPP

#ifndef _SQUIRREL_H_
/* Life becomes easier when we can tell about a function it needs the VM, but
 *  without really including 'squirrel.h'. */
typedef struct SQVM *HSQUIRRELVM;  //!< Pointer to Squirrel Virtual Machine.
typedef int SQInteger;             //!< Squirrel Integer.
typedef struct SQObject HSQOBJECT; //!< Squirrel Object (fake declare)
#endif

#endif /* FAKE_SQUIRREL_TYPES_HPP */
