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
IniItem::IniItem(std::string_view name)
{
	this->name = StrMakeValid(name);
}

/**
 * Replace the current value with another value.
 * @param value the value to replace with.
 */
void IniItem::SetValue(std::string_view value)
{
	this->value.emplace(value);
}

/**
 * Construct a new in-memory group of an Ini file.
 * @param parent the file we belong to
 * @param name   the name of the group
 */
IniGroup::IniGroup(std::string_view name, IniGroupType type) : type(type), comment("\n")
{
	this->name = StrMakeValid(name);
}

/**
 * Get the item with the given name.
 * @param name   name of the item to find.
 * @return the requested item or nullptr if not found.
 */
const IniItem *IniGroup::GetItem(std::string_view name) const
{
	for (const IniItem &item : this->items) {
		if (item.name == name) return &item;
	}

	return nullptr;
}

/**
 * Get the item with the given name, and if it doesn't exist create a new item.
 * @param name   name of the item to find.
 * @return the requested item.
 */
IniItem &IniGroup::GetOrCreateItem(std::string_view name)
{
	for (IniItem &item : this->items) {
		if (item.name == name) return item;
	}

	/* Item doesn't exist, make a new one. */
	return this->CreateItem(name);
}

/**
 * Create an item with the given name. This does not reuse an existing item of the same name.
 * @param name name of the item to create.
 * @return the created item.
 */
IniItem &IniGroup::CreateItem(std::string_view name)
{
	return this->items.emplace_back(name);
}

/**
 * Remove the item with the given name.
 * @param name Name of the item to remove.
 */
void IniGroup::RemoveItem(std::string_view name)
{
	this->items.remove_if([&name](const IniItem &item) { return item.name == name; });
}

/**
 * Clear all items in the group
 */
void IniGroup::Clear()
{
	this->items.clear();
}

/**
 * Construct a new in-memory Ini file representation.
 * @param list_group_names A list with group names that should be loaded as lists instead of variables. @see IGT_LIST
 * @param seq_group_names  A list with group names that should be loaded as lists of names. @see IGT_SEQUENCE
 */
IniLoadFile::IniLoadFile(const IniGroupNameList &list_group_names, const IniGroupNameList &seq_group_names) :
		list_group_names(list_group_names),
		seq_group_names(seq_group_names)
{
}

/**
 * Get the group with the given name.
 * @param name name of the group to find.
 * @return The requested group or \c nullptr if not found.
 */
const IniGroup *IniLoadFile::GetGroup(std::string_view name) const
{
	for (const IniGroup &group : this->groups) {
		if (group.name == name) return &group;
	}

	return nullptr;
}

/**
 * Get the group with the given name.
 * @param name name of the group to find.
 * @return The requested group or \c nullptr if not found.
 */
IniGroup *IniLoadFile::GetGroup(std::string_view name)
{
	for (IniGroup &group : this->groups) {
		if (group.name == name) return &group;
	}

	return nullptr;
}

/**
 * Get the group with the given name, and if it doesn't exist create a new group.
 * @param name name of the group to find.
 * @return the requested group.
 */
IniGroup &IniLoadFile::GetOrCreateGroup(std::string_view name)
{
	for (IniGroup &group : this->groups) {
		if (group.name == name) return group;
	}

	/* Group doesn't exist, make a new one. */
	return this->CreateGroup(name);
}

/**
 * Create an group with the given name. This does not reuse an existing group of the same name.
 * @param name name of the group to create.
 * @return the created group.
 */
IniGroup &IniLoadFile::CreateGroup(std::string_view name)
{
	IniGroupType type = IGT_VARIABLES;
	if (std::find(this->list_group_names.begin(), this->list_group_names.end(), name) != this->list_group_names.end()) type = IGT_LIST;
	if (std::find(this->seq_group_names.begin(), this->seq_group_names.end(), name) != this->seq_group_names.end()) type = IGT_SEQUENCE;

	return this->groups.emplace_back(name, type);
}

/**
 * Remove the group with the given name.
 * @param name name of the group to remove.
 */
void IniLoadFile::RemoveGroup(std::string_view name)
{
	size_t len = name.length();
	this->groups.remove_if([&name, &len](const IniGroup &group) { return group.name.compare(0, len, name) == 0; });
}

/**
 * Load the Ini file's data from the disk.
 * @param filename the file to load.
 * @param subdir the sub directory to load the file from.
 * @pre nothing has been loaded yet.
 */
void IniLoadFile::LoadFromDisk(const std::string &filename, Subdirectory subdir)
{
	assert(this->groups.empty());

	char buffer[1024];
	IniGroup *group = nullptr;

	std::string comment;

	size_t end;
	auto in = this->OpenFile(filename, subdir, &end);
	if (!in.has_value()) return;

	end += ftell(*in);

	/* for each line in the file */
	while (static_cast<size_t>(ftell(*in)) < end && fgets(buffer, sizeof(buffer), *in)) {
		char c, *s;
		/* trim whitespace from the left side */
		for (s = buffer; *s == ' ' || *s == '\t'; s++) {}

		/* trim whitespace from right side. */
		char *e = s + strlen(s);
		while (e > s && ((c = e[-1]) == '\n' || c == '\r' || c == ' ' || c == '\t')) e--;
		*e = '\0';

		/* Skip comments and empty lines outside IGT_SEQUENCE groups. */
		if ((group == nullptr || group->type != IGT_SEQUENCE) && (*s == '#' || *s == ';' || *s == '\0')) {
			comment += std::string_view(s, e - s);
			comment += '\n'; // comment newline
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
			group = &this->CreateGroup(std::string_view(s, e - s));
			group->comment = std::move(comment);
		} else if (group != nullptr) {
			if (group->type == IGT_SEQUENCE) {
				/* A sequence group, use the line as item name without further interpretation. */
				IniItem &item = group->CreateItem(std::string_view(buffer, e - buffer));
				item.comment = std::move(comment);
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
			IniItem &item = group->CreateItem(std::string_view(s, t - s));
			item.comment = std::move(comment);

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
				item.value = StrMakeValid(std::string_view(t));
			}
		} else {
			/* it's an orphan item */
			this->ReportFileError("ini: '", buffer, "' outside of group");
		}
	}

	this->comment = std::move(comment);
}

