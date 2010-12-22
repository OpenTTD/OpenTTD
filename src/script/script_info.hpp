/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info.hpp ScriptInfo keeps track of all information of a script, like Author, Description, ... */

#ifndef SCRIPT_INFO_HPP
#define SCRIPT_INFO_HPP

#include <squirrel.h>
#include "../misc/countedptr.hpp"

class ScriptFileInfo : public SimpleCountedObject {
public:
	ScriptFileInfo() :
		SQ_instance(NULL),
		main_script(NULL),
		author(NULL),
		name(NULL),
		short_name(NULL),
		description(NULL),
		date(NULL),
		instance_name(NULL),
		version(0),
		url(NULL)
	{}
	~ScriptFileInfo();

	/**
	 * Get the Author of the script.
	 */
	const char *GetAuthor() const { return this->author; }

	/**
	 * Get the Name of the script.
	 */
	const char *GetName() const { return this->name; }

	/**
	 * Get the 4 character long short name of the script.
	 */
	const char *GetShortName() const { return this->short_name; }

	/**
	 * Get the description of the script.
	 */
	const char *GetDescription() const { return this->description; }

	/**
	 * Get the version of the script.
	 */
	int GetVersion() const { return this->version; }

	/**
	 * Get the last-modified date of the script.
	 */
	const char *GetDate() const { return this->date; }

	/**
	 * Get the name of the instance of the script to create.
	 */
	const char *GetInstanceName() const { return this->instance_name; }

	/**
	 * Get the website for this script.
	 */
	const char *GetURL() const { return this->url; }

	/**
	 * Get the filename of the main.nut script.
	 */
	const char *GetMainScript() const { return this->main_script; }

	/**
	 * Check if a given method exists.
	 */
	bool CheckMethod(const char *name) const;

	/**
	 * Process the creation of a FileInfo object.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm, ScriptFileInfo *info);

protected:
	class Squirrel *engine;
	HSQOBJECT *SQ_instance;
private:
	char *main_script;
	const char *author;
	const char *name;
	const char *short_name;
	const char *description;
	const char *date;
	const char *instance_name;
	int version;
	const char *url;
};

#endif /* SCRIPT_INFO_HPP */
