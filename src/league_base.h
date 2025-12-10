/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file league_base.h %LeagueTable base class. */

#ifndef LEAGUE_BASE_H
#define LEAGUE_BASE_H

#include "company_type.h"
#include "goal_type.h"
#include "league_type.h"
#include "core/pool_type.hpp"
#include "strings_type.h"

bool IsValidLink(Link link);

using LeagueTableElementPool = Pool<LeagueTableElement, LeagueTableElementID, 64>;
extern LeagueTableElementPool _league_table_element_pool;

using LeagueTablePool = Pool<LeagueTable, LeagueTableID, 4>;
extern LeagueTablePool _league_table_pool;


/**
 * Struct about league table elements.
 * Each LeagueTable is composed of one or more elements. Elements are sorted by their rating (higher=better).
 **/
struct LeagueTableElement : LeagueTableElementPool::PoolItem<&_league_table_element_pool> {
	LeagueTableID table = LeagueTableID::Invalid(); ///< Id of the table which this element belongs to
	int64_t rating = 0; ///< Value that determines ordering of elements in the table (higher=better)
	CompanyID company = CompanyID::Invalid(); ///< Company Id to show the colour blob for or CompanyID::Invalid()
	EncodedString text{}; ///< Text of the element
	EncodedString score{}; ///< String representation of the score associated with the element
	Link link{}; ///< What opens when element is clicked

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	LeagueTableElement() { }
	LeagueTableElement(LeagueTableID table, int64_t rating, CompanyID company, const EncodedString &text, const EncodedString &score, const Link &link) :
		table(table), rating(rating), company(company), text(text), score(score), link(link) {}

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~LeagueTableElement() { }
};


/** Struct about custom league tables */
struct LeagueTable : LeagueTablePool::PoolItem<&_league_table_pool> {
	EncodedString title{}; ///< Title of the table
	EncodedString header{}; ///< Text to show above the table
	EncodedString footer{}; ///< Text to show below the table

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	LeagueTable() { }
	LeagueTable(const EncodedString &title, const EncodedString &header, const EncodedString &footer) : title(title), header(header), footer(footer) { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~LeagueTable() { }
};

#endif /* LEAGUE_BASE_H */
