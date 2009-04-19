/* $Id$ */

/** @file script_info.hpp ScriptInfo keeps track of all information of a script, like Author, Description, ... */

#ifndef SCRIPT_INFO
#define SCRIPT_INFO

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

#endif /* SCRIPT_INFO */
