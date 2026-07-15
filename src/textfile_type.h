/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file textfile_type.h Types related to textfiles. */

#ifndef TEXTFILE_TYPE_H
#define TEXTFILE_TYPE_H

#include "core/enum_type.hpp"

/** Additional text files accompanying Tar archives */
enum class TextfileType : uint8_t {
	ContentBegin, ///< This marker is used to generate the below three buttons in sequence by various of places in the code.

	Readme = TextfileType::ContentBegin, ///< Content readme
	Changelog, ///< Content changelog
	License, ///< Content license

	ContentEnd, ///< This marker is used to generate the above three buttons in sequence by various of places in the code.

	SurveyResult = TextfileType::ContentEnd, ///< Survey result (preview)
	GameManual, ///< Game manual/documentation file

	End, ///< End marker.
};
DECLARE_ENUM_AS_ADDABLE(TextfileType)

#endif /* TEXTFILE_TYPE_H */
