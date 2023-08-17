/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_base.h %LeagueTable base class. */

#ifndef LEAGUE_BASE_H
#define LEAGUE_BASE_H

#include "company_type.h"
#include "goal_type.h"
#include "league_type.h"
#include "core/pool_type.hpp"

bool IsValidLink(Link link);

typedef Pool<LeagueTableElement, LeagueTableElementID, 64, 64000> LeagueTableElementPool;
extern LeagueTableElementPool _league_table_element_pool;

typedef Pool<LeagueTable, LeagueTableID, 4, 256> LeagueTablePool;
extern LeagueTablePool _league_table_pool;


/**
 * Struct about league table elements.
 * Each LeagueTable is composed of one or more elements. Elements are sorted by their rating (higher=better).
 **/
struct LeagueTableElement : LeagueTableElementPool::PoolItem<&_league_table_element_pool> {
	LeagueTableID table;  ///< Id of the table which this element belongs to
	int64_t rating;         ///< Value that determines ordering of elements in the table (higher=better)
	CompanyID company;    ///< Company Id to show the color blob for or INVALID_COMPANY
	std::string text;     ///< Text of the element
	std::string score;    ///< String representation of the score associated with the element
	Link link;            ///< What opens when element is clicked

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	LeagueTableElement() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~LeagueTableElement() { }
};


/** Struct about custom league tables */
struct LeagueTable : LeagueTablePool::PoolItem<&_league_table_pool> {
	std::string title;  ///< Title of the table
	std::string header;  ///< Text to show above the table
	std::string footer;  ///< Text to show below the table

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	LeagueTable() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~LeagueTable() { }
};

#endif /* LEAGUE_BASE_H */
