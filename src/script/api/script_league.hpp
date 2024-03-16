/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_league.hpp Everything to manipulate league tables. */

#ifndef SCRIPT_LEAGUE_HPP
#define SCRIPT_LEAGUE_HPP

#include "script_company.hpp"
#include "script_text.hpp"
#include "../../league_type.h"

/**
 * Class that handles league table related functions.
 *
 * To create a league table:
 * 1. Create the league table
 * 2. Create league table elements that will be shown in the table in the order of their rating (higher=better).
 *
 * @api game
 */
class ScriptLeagueTable : public ScriptObject {
public:
	/**
	 * The league table IDs.
	 */
	enum LeagueTableID {
		LEAGUE_TABLE_INVALID = ::INVALID_LEAGUE_TABLE, ///< An invalid league table id.
	};

	/**
	 * The league table element IDs.
	 */
	enum LeagueTableElementID {
		LEAGUE_TABLE_ELEMENT_INVALID = ::INVALID_LEAGUE_TABLE_ELEMENT, ///< An invalid league table element id.
	};

	/**
	 * The type of a link.
	 */
	enum LinkType : uint8_t {
		LINK_NONE = ::LT_NONE,             ///< No link
		LINK_TILE = ::LT_TILE,             ///< Link a tile
		LINK_INDUSTRY = ::LT_INDUSTRY,     ///< Link an industry
		LINK_TOWN = ::LT_TOWN,             ///< Link a town
		LINK_COMPANY = ::LT_COMPANY,       ///< Link a company
		LINK_STORY_PAGE = ::LT_STORY_PAGE, ///< Link a story page
	};

	/**
	 * Check whether this is a valid league table ID.
	 * @param table_id The LeagueTableID to check.
	 * @return true iff this league table is valid.
	 */
	static bool IsValidLeagueTable(LeagueTableID table_id);

	/**
	 * Check whether this is a valid league table element ID.
	 * @param element_id The LeagueTableElementID to check.
	 * @return true iff this league table element is valid.
	 */
	static bool IsValidLeagueTableElement(LeagueTableElementID element_id);

	/**
	 * Create a new league table.
	 * @param title League table title (can be either a raw string, or ScriptText object).
	 * @param header The optional header text for the table (null is allowed).
	 * @param footer The optional footer text for the table (null is allowed).
	 * @return The new LeagueTableID, or LEAGUE_TABLE_INVALID if it failed.
	 * @pre ScriptCompanyMode::IsDeity.
	 * @pre title != null && len(title) != 0.
	 */
	static LeagueTableID New(Text *title, Text *header, Text *footer);

	/**
	 * Create a new league table element.
	 * @param table Id of the league table this element belongs to.
	 * @param rating Value that elements are ordered by.
	 * @param company Company to show the color blob for or INVALID_COMPANY.
	 * @param text Text of the element (can be either a raw string, or ScriptText object).
	 * @param score String representation of the score associated with the element (can be either a raw string, or ScriptText object).
	 * @param link_type Type of the referenced object.
	 * @param link_target Id of the referenced object.
	 * @return The new LeagueTableElementID, or LEAGUE_TABLE_ELEMENT_INVALID if it failed.
	 * @pre ScriptCompanyMode::IsDeity.
	 * @pre IsValidLeagueTable(table).
	 * @pre text != null && len(text) != 0.
	 * @pre score != null && len(score) != 0.
	 * @pre IsValidLink(Link(link_type, link_target)).
	 */
	static LeagueTableElementID NewElement(LeagueTableID table, SQInteger rating, ScriptCompany::CompanyID company, Text *text, Text *score, LinkType link_type, LinkTargetID link_target);

	/**
	 * Update the attributes of a league table element.
	 * @param element Id of the element to update
	 * @param company Company to show the color blob for or INVALID_COMPANY.
	 * @param text Text of the element (can be either a raw string, or ScriptText object).
	 * @param link_type Type of the referenced object.
	 * @param link_target Id of the referenced object.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity.
	 * @pre IsValidLeagueTableElement(element).
	 * @pre text != null && len(text) != 0.
	 * @pre IsValidLink(Link(link_type, link_target)).
	 */
	static bool UpdateElementData(LeagueTableElementID element, ScriptCompany::CompanyID company, Text *text, LinkType link_type, LinkTargetID link_target);

	/**
	 * Create a new league table element.
	 * @param element Id of the element to update
	 * @param rating Value that elements are ordered by.
	 * @param score String representation of the score associated with the element (can be either a raw string, or ScriptText object).
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity.
	 * @pre IsValidLeagueTableElement(element).
	 * @pre score != null && len(score) != 0.
	 */
	static bool UpdateElementScore(LeagueTableElementID element, SQInteger rating, Text *score);


	/**
	 * Remove a league table element.
	 * @param element Id of the element to update
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity.
	 * @pre IsValidLeagueTableElement(element).
	 */
	static bool RemoveElement(LeagueTableElementID element);
};


#endif /* SCRIPT_LEAGUE_HPP */
