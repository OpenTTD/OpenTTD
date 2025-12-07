/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ini_load.cpp Definition of the #IniLoadFile class, related to reading and storing '*.ini' files. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
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
	if (std::ranges::find(this->list_group_names, name) != this->list_group_names.end()) type = IGT_LIST;
	if (std::ranges::find(this->seq_group_names, name) != this->seq_group_names.end()) type = IGT_SEQUENCE;

	return this->groups.emplace_back(name, type);
}

/**
 * Remove the group with the given name.
 * @param name name of the group to remove.
 */
void IniLoadFile::RemoveGroup(std::string_view name)
{
	this->groups.remove_if([&name](const IniGroup &group) { return group.name.starts_with(name); });
}

/**
 * Load the Ini file's data from the disk.
 * @param filename the file to load.
 * @param subdir the sub directory to load the file from.
 * @pre nothing has been loaded yet.
 */
void IniLoadFile::LoadFromDisk(std::string_view filename, Subdirectory subdir)
{
	assert(this->groups.empty());

	char buffer[1024];
	IniGroup *group = nullptr;

	std::string comment;

	size_t end;
	auto in = this->OpenFile(filename, subdir, &end);
	if (!in.has_value()) return;

	end += ftell(*in);

	size_t line = 0;
	/* for each line in the file */
	while (static_cast<size_t>(ftell(*in)) < end && fgets(buffer, sizeof(buffer), *in)) {
		++line;
		StringConsumer consumer{StrTrimView(buffer, StringConsumer::WHITESPACE_OR_NEWLINE)};

		/* Skip comments and empty lines outside IGT_SEQUENCE groups. */
		if ((group == nullptr || group->type != IGT_SEQUENCE) && (!consumer.AnyBytesLeft() || consumer.PeekCharIfIn("#;"))) {
			comment += consumer.GetOrigData();
			comment += "\n";
			continue;
		}

		/* it's a group? */
		if (consumer.ReadCharIf('[')) {
			std::string_view group_name = consumer.ReadUntilChar(']', StringConsumer::KEEP_SEPARATOR);
			if (!consumer.ReadCharIf(']') || consumer.AnyBytesLeft()) {
				this->ReportFileError(fmt::format("ini [{}]: invalid group name '{}'", line, consumer.GetOrigData()));
			}
			group = &this->CreateGroup(group_name);
			group->comment = std::move(comment);
			comment.clear(); // std::move leaves comment in a "valid but unspecified state" according to the specification.
		} else if (group != nullptr) {
			if (group->type == IGT_SEQUENCE) {
				/* A sequence group, use the line as item name without further interpretation. */
				IniItem &item = group->CreateItem(consumer.GetOrigData());
				item.comment = std::move(comment);
				comment.clear(); // std::move leaves comment in a "valid but unspecified state" according to the specification.
				continue;
			}

			static const std::string_view key_parameter_separators = "=\t ";
			std::string_view key;
			/* find end of keyname */
			if (consumer.ReadCharIf('\"')) {
				key = consumer.ReadUntilChar('\"', StringConsumer::SKIP_ONE_SEPARATOR);
			} else {
				key = consumer.ReadUntilCharIn(key_parameter_separators);
			}

			/* it's an item in an existing group */
			IniItem &item = group->CreateItem(key);
			item.comment = std::move(comment);
			comment.clear(); // std::move leaves comment in a "valid but unspecified state" according to the specification.

			/* find start of parameter */
			consumer.SkipUntilCharNotIn(key_parameter_separators);

			if (consumer.ReadCharIf('\"')) {
				/* There is no escaping in our loader, so we just remove the first and last quote. */
				std::string_view value = consumer.GetLeftData();
				if (value.ends_with("\"")) value.remove_suffix(1);
				item.value = StrMakeValid(value);
			} else if (!consumer.AnyBytesLeft()) {
				/* If the value was not quoted and empty, it must be nullptr */
				item.value.reset();
			} else {
				item.value = StrMakeValid(consumer.GetLeftData());
			}
		} else {
			/* it's an orphan item */
			this->ReportFileError(fmt::format("ini [{}]: '{}' is outside of group", line, consumer.GetOrigData()));
		}
	}

	this->comment = std::move(comment);
}

