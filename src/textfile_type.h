/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textfile_type.h Types related to textfiles. */

#ifndef TEXTFILE_TYPE_H
#define TEXTFILE_TYPE_H

/** Additional text files accompanying Tar archives */
enum TextfileType {
	TFT_BEGIN,

	TFT_README = TFT_BEGIN, ///< NewGRF readme
	TFT_CHANGELOG,          ///< NewGRF changelog
	TFT_LICENSE,            ///< NewGRF license

	TFT_END,
};
DECLARE_POSTFIX_INCREMENT(TextfileType)

#endif /* TEXTFILE_TYPE_H */
