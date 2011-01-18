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

/** Types of groups */
enum IniGroupType {
	IGT_VARIABLES = 0, ///< values of the form "landscape = hilly"
	IGT_LIST      = 1, ///< a list of values, seperated by \n and terminated by the next group block
};

/** A single "line" in an ini file. */
struct IniItem {
	IniItem *next; ///< The next item in this group
	char *name;    ///< The name of this item
	char *value;   ///< The value of this item
	char *comment; ///< The comment associated with this item

	IniItem(struct IniGroup *parent, const char *name, size_t len = 0);
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

	IniGroup(struct IniFile *parent, const char *name, size_t len = 0);
	~IniGroup();

	IniItem *GetItem(const char *name, bool create);
	void Clear();
};

/** The complete ini file. */
struct IniFile {
	IniGroup *group;                      ///< the first group in the ini
	IniGroup **last_group;                ///< the last group in the ini
	char *comment;                        ///< last comment in file
	const char * const *list_group_names; ///< NULL terminated list with group names that are lists

	IniFile(const char * const *list_group_names = NULL);
	~IniFile();

	IniGroup *GetGroup(const char *name, size_t len = 0);
	void RemoveGroup(const char *name);

	void LoadFromDisk(const char *filename);
	bool SaveToDisk(const char *filename);
};

#endif /* INI_TYPE_H */
