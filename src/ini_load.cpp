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
 */
IniItem::IniItem(IniGroup *parent, const std::string &name) : next(nullptr)
{
	this->name = StrMakeValid(name);

	*parent->last_item = this;
	parent->last_item = &this->next;
}

/** Free everything we loaded. */
IniItem::~IniItem()
{
	delete this->next;
}

/**
 * Replace the current value with another value.
 * @param value the value to replace with.
 */
void IniItem::SetValue(const std::string_view value)
{
	this->value.emplace(value);
}

/**
 * Construct a new in-memory group of an Ini file.
 * @param parent the file we belong to
 * @param name   the name of the group
 */
IniGroup::IniGroup(IniLoadFile *parent, const std::string &name) : next(nullptr), type(IGT_VARIABLES), item(nullptr)
{
	this->name = StrMakeValid(name);

	this->last_item = &this->item;
	*parent->last_group = this;
	parent->last_group = &this->next;

	if (parent->list_group_names != nullptr) {
		for (uint i = 0; parent->list_group_names[i] != nullptr; i++) {
			if (this->name == parent->list_group_names[i]) {
				this->type = IGT_LIST;
				return;
			}
		}
	}
	if (parent->seq_group_names != nullptr) {
		for (uint i = 0; parent->seq_group_names[i] != nullptr; i++) {
			if (this->name == parent->seq_group_names[i]) {
				this->type = IGT_SEQUENCE;
				return;
			}
		}
	}
}

/** Free everything we loaded. */
IniGroup::~IniGroup()
{
	delete this->item;
	delete this->next;
}

/**
 * Get the item with the given name.
 * @param name   name of the item to find.
 * @return the requested item or nullptr if not found.
 */
IniItem *IniGroup::GetItem(const std::string &name) const
{
	for (IniItem *item = this->item; item != nullptr; item = item->next) {
		if (item->name == name) return item;
	}

	return nullptr;
}

/**
 * Get the item with the given name, and if it doesn't exist create a new item.
 * @param name   name of the item to find.
 * @return the requested item.
 */
IniItem &IniGroup::GetOrCreateItem(const std::string &name)
{
	for (IniItem *item = this->item; item != nullptr; item = item->next) {
		if (item->name == name) return *item;
	}

	/* Item doesn't exist, make a new one. */
	return this->CreateItem(name);
}

/**
 * Create an item with the given name. This does not reuse an existing item of the same name.
 * @param name name of the item to create.
 * @return the created item.
 */
IniItem &IniGroup::CreateItem(const std::string &name)
{
	return *(new IniItem(this, name));
}

/**
 * Remove the item with the given name.
 * @param name Name of the item to remove.
 */
void IniGroup::RemoveItem(const std::string &name)
{
	IniItem **prev = &this->item;

	for (IniItem *item = this->item; item != nullptr; prev = &item->next, item = item->next) {
		if (item->name != name) continue;

		*prev = item->next;
		/* "last_item" is a pointer to the "real-last-item"->next. */
		if (this->last_item == &item->next) {
			this->last_item = prev;
		}

		item->next = nullptr;
		delete item;

		return;
	}
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
		list_group_names(list_group_names),
		seq_group_names(seq_group_names)
{
	this->last_group = &this->group;
}

/** Free everything we loaded. */
IniLoadFile::~IniLoadFile()
{
	delete this->group;
}

/**
 * Get the group with the given name. If it doesn't exist
 * and \a create_new is \c true create a new group.
 * @param name name of the group to find.
 * @param create_new Allow creation of group if it does not exist.
 * @return The requested group if it exists or was created, else \c nullptr.
 */
IniGroup *IniLoadFile::GetGroup(const std::string &name, bool create_new)
{
	/* does it exist already? */
	for (IniGroup *group = this->group; group != nullptr; group = group->next) {
		if (group->name == name) return group;
	}

	if (!create_new) return nullptr;

	/* otherwise make a new one */
	return &this->CreateGroup(name);
}

/**
 * Get the group with the given name, and if it doesn't exist create a new group.
 * @param name name of the group to find.
 * @return the requested group.
 */
IniGroup &IniLoadFile::GetOrCreateGroup(const std::string &name)
{
	for (IniGroup *group = this->group; group != nullptr; group = group->next) {
		if (group->name == name) return *group;
	}

	/* Group doesn't exist, make a new one. */
	return this->CreateGroup(name);
}

/**
 * Create an group with the given name. This does not reuse an existing group of the same name.
 * @param name name of the group to create.
 * @return the created group.
 */
IniGroup &IniLoadFile::CreateGroup(const std::string &name)
{
	IniGroup *group = new IniGroup(this, name);
	group->comment = "\n";
	return *group;
}

/**
 * Remove the group with the given name.
 * @param name name of the group to remove.
 */
void IniLoadFile::RemoveGroup(const std::string &name)
{
	size_t len = name.length();
	IniGroup *prev = nullptr;
	IniGroup *group;

	/* does it exist already? */
	for (group = this->group; group != nullptr; prev = group, group = group->next) {
		if (group->name.compare(0, len, name) == 0) {
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
void IniLoadFile::LoadFromDisk(const std::string &filename, Subdirectory subdir)
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
				a = std::max(a, 128U);
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
			group = &this->CreateGroup(std::string(s, e - s));
			if (comment_size != 0) {
				group->comment.assign(comment, comment_size);
				comment_size = 0;
			}
		} else if (group != nullptr) {
			if (group->type == IGT_SEQUENCE) {
				/* A sequence group, use the line as item name without further interpretation. */
				IniItem &item = group->CreateItem(std::string(buffer, e - buffer));
				if (comment_size) {
					item.comment.assign(comment, comment_size);
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
			IniItem &item = group->CreateItem(std::string(s, t - s));
			if (comment_size != 0) {
				item.comment.assign(comment, comment_size);
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
			if (!quoted && e == t) {
				item.value.reset();
			} else {
				item.value = StrMakeValid(std::string(t));
			}
		} else {
			/* it's an orphan item */
			this->ReportFileError("ini: '", buffer, "' outside of group");
		}
	}

	if (comment_size > 0) {
		this->comment.assign(comment, comment_size);
		comment_size = 0;
	}

	free(comment);
	fclose(in);
}

