/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music.cpp The songs that OpenTTD knows. */

#include "stdafx.h"
#include "music.h"

const SongSpecs _origin_songs_specs[] = {
	{"gm_tt00.gm", "Tycoon DELUXE Theme"},
	{"gm_tt02.gm", "Easy Driver"},
	{"gm_tt03.gm", "Little Red Diesel"},
	{"gm_tt17.gm", "Cruise Control"},
	{"gm_tt07.gm", "Don't Walk!"},
	{"gm_tt09.gm", "Fell Apart On Me"},
	{"gm_tt04.gm", "City Groove"},
	{"gm_tt19.gm", "Funk Central"},
	{"gm_tt06.gm", "Stoke It"},
	{"gm_tt12.gm", "Road Hog"},
	{"gm_tt05.gm", "Aliens Ate My Railway"},
	{"gm_tt01.gm", "Snarl Up"},
	{"gm_tt18.gm", "Stroll On"},
	{"gm_tt10.gm", "Can't Get There From Here"},
	{"gm_tt08.gm", "Sawyer's Tune"},
	{"gm_tt13.gm", "Hold That Train!"},
	{"gm_tt21.gm", "Movin' On"},
	{"gm_tt15.gm", "Goss Groove"},
	{"gm_tt16.gm", "Small Town"},
	{"gm_tt14.gm", "Broomer's Oil Rag"},
	{"gm_tt20.gm", "Jammit"},
	{"gm_tt11.gm", "Hard Drivin'"},
};

assert_compile(NUM_SONGS_AVAILABLE == lengthof(_origin_songs_specs));
