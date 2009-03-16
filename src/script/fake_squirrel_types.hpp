/* $Id$ */

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
