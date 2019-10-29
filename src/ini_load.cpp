/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini_load.cpp Definition of the #IniLoadFile class, related to reading and storing '*.ini' files. */

#include "stdafx.h"
#include "core/alloc_func.hpp"
#include "core/mem_func.hpp"
#include "ini_type.h"
#include "string_func.h"

#include "safeguards.h"

/**
 * Construct a new in-memory item of an Ini file.
 * @param parent the group we belong to
 * @param name   the name of the item
 * @param last   the last element of the name of the item
 */
IniItem::IniItem(IniGroup *parent, const char *name, const char *last) : next(nullptr), value(nullptr), comment(nullptr)
{
	this->name = stredup(name, last);
	str_validate(this->name, this->name + strlen(this->name));

	*parent->last_item = this;
	parent->last_item = &this->next;
}

/** Free everything we loaded. */
IniItem::~IniItem()
{
	free(this->name);
	free(this->value);
	free(this->comment);

	delete this->next;
}

/**
 * Replace the current value with another value.
 * @param value the value to replace with.
 */
void IniItem::SetValue(const char *value)
{
	free(this->value);
	this->value = stredup(value);
}

/**
 * Construct a new in-memory group of an Ini file.
 * @param parent the file we belong to
 * @param name   the name of the group
 * @param last   the last element of the name of the group
 */
IniGroup::IniGroup(IniLoadFile *parent, const char *name, const char *last) : next(nullptr), type(IGT_VARIABLES), item(nullptr), comment(nullptr)
{
	this->name = stredup(name, last);
	str_validate(this->name, this->name + strlen(this->name));

	this->last_item = &this->item;
	*parent->last_group = this;
	parent->last_group = &this->next;

	if (parent->list_group_names != nullptr) {
		for (uint i = 0; parent->list_group_names[i] != nullptr; i++) {
			if (strcmp(this->name, parent->list_group_names[i]) == 0) {
				this->type = IGT_LIST;
				return;
			}
		}
	}
	if (parent->seq_group_names != nullptr) {
		for (uint i = 0; parent->seq_group_names[i] != nullptr; i++) {
			if (strcmp(this->name, parent->seq_group_names[i]) == 0) {
				this->type = IGT_SEQUENCE;
				return;
			}
		}
	}
}

/** Free everything we loaded. */
IniGroup::~IniGroup()
{
	free(this->name);
	free(this->comment);

	delete this->item;
	delete this->next;
}

/**
 * Get the item with the given name, and if it doesn't exist
 * and create is true it creates a new item.
 * @param name   name of the item to find.
 * @param create whether to create an item when not found or not.
 * @return the requested item or nullptr if not found.
 */
IniItem *IniGroup::GetItem(const char *name, bool create)
{
	for (IniItem *item = this->item; item != nullptr; item = item->next) {
		if (strcmp(item->name, name) == 0) return item;
	}

	if (!create) return nullptr;

	/* otherwise make a new one */
	return new IniItem(this, name, nullptr);
}

/**
 * Clear all items in the group
 */
void IniGroup::Clear()
{
	delete this->item;
	this->item = nullptr;
	this->last_item = &this->item;
}

/**
 * Construct a new in-memory Ini file representation.
 * @param list_group_names A \c nullptr terminated list with group names that should be loaded as lists instead of variables. @see IGT_LIST
 * @param seq_group_names  A \c nullptr terminated list with group names that should be loaded as lists of names. @see IGT_SEQUENCE
 */
IniLoadFile::IniLoadFile(const char * const *list_group_names, const char * const *seq_group_names) :
		group(nullptr),
		comment(nullptr),
		list_group_names(list_group_names),
		seq_group_names(seq_group_names)
{
	this->last_group = &this->group;
}

/** Free everything we loaded. */
IniLoadFile::~IniLoadFile()
{
	free(this->comment);
	delete this->group;
}

/**
 * Get the group with the given name. If it doesn't exist
 * and \a create_new is \c true create a new group.
 * @param name name of the group to find.
 * @param len  the maximum length of said name (\c 0 means length of the string).
 * @param create_new Allow creation of group if it does not exist.
 * @return The requested group if it exists or was created, else \c nullptr.
 */
IniGroup *IniLoadFile::GetGroup(const char *name, size_t len, bool create_new)
{
	if (len == 0) len = strlen(name);

	/* does it exist already? */
	for (IniGroup *group = this->group; group != nullptr; group = group->next) {
		if (!strncmp(group->name, name, len) && group->name[len] == 0) {
			return group;
		}
	}

	if (!create_new) return nullptr;

	/* otherwise make a new one */
	IniGroup *group = new IniGroup(this, name, name + len - 1);
	group->comment = stredup("\n");
	return group;
}

/**
 * Remove the group with the given name.
 * @param name name of the group to remove.
 */
void IniLoadFile::RemoveGroup(const char *name)
{
	size_t len = strlen(name);
	IniGroup *prev = nullptr;
	IniGroup *group;

	/* does it exist already? */
	for (group = this->group; group != nullptr; prev = group, group = group->next) {
		if (strncmp(group->name, name, len) == 0) {
			break;
		}
	}

	if (group == nullptr) return;

	if (prev != nullptr) {
		prev->next = prev->next->next;
		if (this->last_group == &group->next) this->last_group = &prev->next;
	} else {
		this->group = this->group->next;
		if (this->last_group == &group->next) this->last_group = &this->group;
	}

	group->next = nullptr;
	delete group;
}

/**
 * Load the Ini file's data from the disk.
 * @param filename the file to load.
 * @param subdir the sub directory to load the file from.
 * @pre nothing has been loaded yet.
 */
void IniLoadFile::LoadFromDisk(const char *filename, Subdirectory subdir)
{
	assert(this->last_group == &this->group);

	char buffer[1024];
	IniGroup *group = nullptr;

	char *comment = nullptr;
	uint comment_size = 0;
	uint comment_alloc = 0;

	size_t end;
	FILE *in = this->OpenFile(filename, subdir, &end);
	if (in == nullptr) return;

	end += ftell(in);

	/* for each line in the file */
	while ((size_t)ftell(in) < end && fgets(buffer, sizeof(buffer), in)) {
		char c, *s;
		/* trim whitespace from the left side */
		for (s = buffer; *s == ' ' || *s == '\t'; s++) {}

		/* trim whitespace from right side. */
		char *e = s + strlen(s);
		while (e > s && ((c = e[-1]) == '\n' || c == '\r' || c == ' ' || c == '\t')) e--;
		*e = '\0';

		/* Skip comments and empty lines outside IGT_SEQUENCE groups. */
		if ((group == nullptr || group->type != IGT_SEQUENCE) && (*s == '#' || *s == ';' || *s == '\0')) {
			uint ns = comment_size + (e - s + 1);
			uint a = comment_alloc;
			/* add to comment */
			if (ns > a) {
				a = max(a, 128U);
				do a *= 2; while (a < ns);
				comment = ReallocT(comment, comment_alloc = a);
			}
			uint pos = comment_size;
			comment_size += (e - s + 1);
			comment[pos + e - s] = '\n'; // comment newline
			memcpy(comment + pos, s, e - s); // copy comment contents
			continue;
		}

		/* it's a group? */
		if (s[0] == '[') {
			if (e[-1] != ']') {
				this->ReportFileError("ini: invalid group name '", buffer, "'");
			} else {
				e--;
			}
			s++; // skip [
			group = new IniGroup(this, s, e - 1);
			if (comment_size != 0) {
				group->comment = stredup(comment, comment + comment_size - 1);
				comment_size = 0;
			}
		} else if (group != nullptr) {
			if (group->type == IGT_SEQUENCE) {
				/* A sequence group, use the line as item name without further interpretation. */
				IniItem *item = new IniItem(group, buffer, e - 1);
				if (comment_size) {
					item->comment = stredup(comment, comment + comment_size - 1);
					comment_size = 0;
				}
				continue;
			}
			char *t;
			/* find end of keyname */
			if (*s == '\"') {
				s++;
				for (t = s; *t != '\0' && *t != '\"'; t++) {}
				if (*t == '\"') *t = ' ';
			} else {
				for (t = s; *t != '\0' && *t != '=' && *t != '\t' && *t != ' '; t++) {}
			}

			/* it's an item in an existing group */
			IniItem *item = new IniItem(group, s, t - 1);
			if (comment_size != 0) {
				item->comment = stredup(comment, comment + comment_size - 1);
				comment_size = 0;
			}

			/* find start of parameter */
			while (*t == '=' || *t == ' ' || *t == '\t') t++;

			bool quoted = (*t == '\"');
			/* remove starting quotation marks */
			if (*t == '\"') t++;
			/* remove ending quotation marks */
			e = t + strlen(t);
			if (e > t && e[-1] == '\"') e--;
			*e = '\0';

			/* If the value was not quoted and empty, it must be nullptr */
			item->value = (!quoted && e == t) ? nullptr : stredup(t);
			if (item->value != nullptr) str_validate(item->value, item->value + strlen(item->value));
		} else {
			/* it's an orphan item */
			this->ReportFileError("ini: '", buffer, "' outside of group");
		}
	}

	if (comment_size > 0) {
		this->comment = stredup(comment, comment + comment_size - 1);
		comment_size = 0;
	}

	free(comment);
	fclose(in);
}

