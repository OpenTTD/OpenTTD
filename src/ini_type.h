/* $Id$ */

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
	IniItem *next; ///< The next item in this group
	char *name;    ///< The name of this item
	char *value;   ///< The value of this item
	char *comment; ///< The comment associated with this item

	IniItem(struct IniGroup *parent, const char *name, const char *last = NULL);
	~IniItem();

	void SetValue(const char *value);
};

/** A group within an ini file. */
struct IniGroup {
	IniGroup *next;      ///< the next group within this file
	IniGroupType type;   ///< type of group
	IniItem *item;       ///< the first item in the group
	IniItem **last_item; ///< the last item in the group
	char *name;          ///< name of group
	char *comment;       ///< comment for group

	IniGroup(struct IniLoadFile *parent, const char *name, const char *last = NULL);
	~IniGroup();

	IniItem *GetItem(const char *name, bool create);
	void Clear();
};

/** Ini file that only supports loading. */
struct IniLoadFile {
	IniGroup *group;                      ///< the first group in the ini
	IniGroup **last_group;                ///< the last group in the ini
	char *comment;                        ///< last comment in file
	const char * const *list_group_names; ///< NULL terminated list with group names that are lists
	const char * const *seq_group_names;  ///< NULL terminated list with group names that are sequences.

	IniLoadFile(const char * const *list_group_names = NULL, const char * const *seq_group_names = NULL);
	virtual ~IniLoadFile();

	IniGroup *GetGroup(const char *name, size_t len = 0, bool create_new = true);
	void RemoveGroup(const char *name);

	void LoadFromDisk(const char *filename, Subdirectory subdir);

	/**
	 * Open the INI file.
	 * @param filename Name of the INI file.
	 * @param subdir The subdir to load the file from.
	 * @param[out] size Size of the opened file.
	 * @return File handle of the opened file, or \c NULL.
	 */
	virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) = 0;

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
	IniFile(const char * const *list_group_names = NULL);

	bool SaveToDisk(const char *filename);

	virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size);
	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post);
};

#endif /* INI_TYPE_H */
