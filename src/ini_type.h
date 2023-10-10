/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini_type.h Types related to reading/writing '*.ini' files. */

#ifndef INI_TYPE_H
#define INI_TYPE_H

#include "fileio_type.h"

/** Types of groups */
enum IniGroupType {
	IGT_VARIABLES = 0, ///< Values of the form "landscape = hilly".
	IGT_LIST      = 1, ///< A list of values, separated by \n and terminated by the next group block.
	IGT_SEQUENCE  = 2, ///< A list of uninterpreted lines, terminated by the next group block.
};

/** A single "line" in an ini file. */
struct IniItem {
	IniItem *next;                    ///< The next item in this group
	std::string name;                 ///< The name of this item
	std::optional<std::string> value; ///< The value of this item
	std::string comment;              ///< The comment associated with this item

	IniItem(struct IniGroup *parent, const std::string &name);
	~IniItem();

	void SetValue(const std::string_view value);
};

/** A group within an ini file. */
struct IniGroup {
	IniGroup *next;      ///< the next group within this file
	IniGroupType type;   ///< type of group
	IniItem *item;       ///< the first item in the group
	IniItem **last_item; ///< the last item in the group
	std::string name;    ///< name of group
	std::string comment; ///< comment for group

	IniGroup(struct IniLoadFile *parent, const std::string &name);
	~IniGroup();

	IniItem *GetItem(const std::string &name) const;
	IniItem &GetOrCreateItem(const std::string &name);
	IniItem &CreateItem(const std::string &name);
	void RemoveItem(const std::string &name);
	void Clear();
};

/** Ini file that only supports loading. */
struct IniLoadFile {
	IniGroup *group;                      ///< the first group in the ini
	IniGroup **last_group;                ///< the last group in the ini
	std::string comment;                  ///< last comment in file
	const char * const *list_group_names; ///< nullptr terminated list with group names that are lists
	const char * const *seq_group_names;  ///< nullptr terminated list with group names that are sequences.

	IniLoadFile(const char * const *list_group_names = nullptr, const char * const *seq_group_names = nullptr);
	virtual ~IniLoadFile();

	IniGroup *GetGroup(const std::string &name, bool create_new = true);
	IniGroup &GetOrCreateGroup(const std::string &name);
	IniGroup &CreateGroup(const std::string &name);
	void RemoveGroup(const std::string &name);

	void LoadFromDisk(const std::string &filename, Subdirectory subdir);

	/**
	 * Open the INI file.
	 * @param filename Name of the INI file.
	 * @param subdir The subdir to load the file from.
	 * @param[out] size Size of the opened file.
	 * @return File handle of the opened file, or \c nullptr.
	 */
	virtual FILE *OpenFile(const std::string &filename, Subdirectory subdir, size_t *size) = 0;

	/**
	 * Report an error about the file contents.
	 * @param pre    Prefix text of the \a buffer part.
	 * @param buffer Part of the file with the error.
	 * @param post   Suffix text of the \a buffer part.
	 */
	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post) = 0;
};

/** Ini file that supports both loading and saving. */
struct IniFile : IniLoadFile {
	IniFile(const char * const *list_group_names = nullptr);

	bool SaveToDisk(const std::string &filename);

	FILE *OpenFile(const std::string &filename, Subdirectory subdir, size_t *size) override;
	void ReportFileError(const char * const pre, const char * const buffer, const char * const post) override;
};

#endif /* INI_TYPE_H */
