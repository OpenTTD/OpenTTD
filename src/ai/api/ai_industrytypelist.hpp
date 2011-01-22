/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_industrytypelist.hpp List all available industry types. */

#ifndef AI_INDUSTRYTYPELIST_HPP
#define AI_INDUSTRYTYPELIST_HPP

#include "ai_industrytype.hpp"

/**
 * Creates a list of valid industry types.
 * @ingroup AIList
 */
class AIIndustryTypeList : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIIndustryTypeList"; }
	AIIndustryTypeList();
};


#endif /* AI_INDUSTRYTYPELIST_HPP */
