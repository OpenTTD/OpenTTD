/* $Id$ */

/** @file ai_info.hpp AIInfo keeps track of all information of an AI, like Author, Description, ... */

#ifndef AI_INFO
#define AI_INFO

#include <list>
#include "api/ai_object.hpp"

enum AIConfigFlags {
	AICONFIG_NONE    = 0x0,
	AICONFIG_RANDOM  = 0x1, //!< When randomizing the AI, pick any value between min_value and max_value when on custom difficulty setting.
	AICONFIG_BOOLEAN = 0x2, //!< This value is a boolean (either 0 (false) or 1 (true) ).
};

struct AIConfigItem {
	const char *name;        //!< The name of the configuration setting.
	const char *description; //!< The description of the configuration setting.
	int min_value;           //!< The minimal value this configuration setting can have.
	int max_value;           //!< The maximal value this configuration setting can have.
	int custom_value;        //!< The default value on custom difficulty setting.
	int easy_value;          //!< The default value on easy difficulty setting.
	int medium_value;        //!< The default value on medium difficulty setting.
	int hard_value;          //!< The default value on hard difficulty setting.
	int random_deviation;    //!< The maximum random deviation from the default value.
	int step_size;           //!< The step size in the gui.
	AIConfigFlags flags;     //!< Flags for the configuration setting.
};

typedef std::list<AIConfigItem> AIConfigItemList;

class AIFileInfo : public AIObject {
public:
	friend class AIInfo;
	friend class AILibrary;

	AIFileInfo() : author(NULL), name(NULL), short_name(NULL), description(NULL), date(NULL), instance_name(NULL) {};
	~AIFileInfo();

	/**
	 * Get the Author of the AI.
	 */
	const char *GetAuthor();

	/**
	 * Get the Name of the AI.
	 */
	const char *GetName();

	/**
	 * Get the 4 character long short name of the AI.
	 */
	const char *GetShortName();

	/**
	 * Get the description of the AI.
	 */
	const char *GetDescription();

	/**
	 * Get the version of the AI.
	 */
	int GetVersion();

	/**
	 * Get the settings of the AI.
	 */
	void GetSettings();

	/**
	 * Get the date of the AI.
	 */
	const char *GetDate();

	/**
	 * Get the name of the instance of the AI to create.
	 */
	const char *GetInstanceName();

	/**
	 * Check if we can start this AI.
	 */
	bool CanLoadFromVersion(int version);

	/**
	 * Get the name of the dir this AI is in.
	 */
	const char *GetDirName();

	/**
	 * Get the complete script name of this AI.
	 */
	const char *GetScriptName();

	/**
	 * Check if a given method exists.
	 */
	void CheckMethods(SQInteger *res, const char *name);

	/**
	 * Process the creation of a FileInfo object.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm, AIFileInfo *info, bool library);

private:
	class Squirrel *engine;
	HSQOBJECT *SQ_instance;
	char *script_name;
	char *dir_name;
	class AIScanner *base;
	const char *author;
	const char *name;
	const char *short_name;
	const char *description;
	const char *date;
	const char *instance_name;
};

class AIInfo : public AIFileInfo {
public:
	static const char *GetClassName() { return "AIInfo"; }

	~AIInfo();

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);
	static SQInteger DummyConstructor(HSQUIRRELVM vm);

	/**
	 * Get the config list for this AI.
	 */
	const AIConfigItemList *GetConfigList();

	/**
	 * Get the description of a certain ai config option.
	 */
	const AIConfigItem *GetConfigItem(const char *name);

	/**
	 * Set a setting.
	 */
	SQInteger AddSetting(HSQUIRRELVM vm);

	/**
	 * Get the default value for a setting.
	 */
	int GetSettingDefaultValue(const char *name);

private:
	AIConfigItemList config_list;
};

class AILibrary : public AIFileInfo {
public:
	AILibrary() : AIFileInfo(), category(NULL) {};

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	static SQInteger Import(HSQUIRRELVM vm);

	/**
	 * Get the category this library is in.
	 */
	const char *GetCategory();

private:
	const char *category;
};

#endif /* AI_INFO */
