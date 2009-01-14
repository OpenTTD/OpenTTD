/* $Id$ */

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

	/**
	 * Construct a new in-memory item of an Ini file.
	 * @param parent the group we belong to
	 * @param name   the name of the item
	 * @param len    the length of the name of the item
	 */
	IniItem(struct IniGroup *parent, const char *name, size_t len = 0);

	/** Free everything we loaded. */
	~IniItem();

	/**
	 * Replace the current value with another value.
	 * @param value the value to replace with.
	 */
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

	/**
	 * Construct a new in-memory group of an Ini file.
	 * @param parent the file we belong to
	 * @param name   the name of the group
	 * @param len    the length of the name of the group
	 */
	IniGroup(struct IniFile *parent, const char *name, size_t len = 0);

	/** Free everything we loaded. */
	~IniGroup();

	/**
	 * Get the item with the given name, and if it doesn't exist
	 * and create is true it creates a new item.
	 * @param name   name of the item to find.
	 * @param create whether to create an item when not found or not.
	 * @return the requested item or NULL if not found.
	 */
	IniItem *GetItem(const char *name, bool create);

	/**
	 * Clear all items in the group
	 */
	void Clear();
};

/** The complete ini file. */
struct IniFile {
	IniGroup *group;               ///< the first group in the ini
	IniGroup **last_group;         ///< the last group in the ini
	char *comment;                 ///< last comment in file
	const char **list_group_names; ///< NULL terminated list with group names that are lists

	/**
	 * Construct a new in-memory Ini file representation.
	 * @param list_group_names A NULL terminated list with groups that should be
	 *                         loaded as lists instead of variables.
	 */
	IniFile(const char **list_group_names = NULL);

	/** Free everything we loaded. */
	~IniFile();

	/**
	 * Get the group with the given name, and if it doesn't exist
	 * create a new group.
	 * @param name name of the group to find.
	 * @param len  the maximum length of said name.
	 * @return the requested group.
	 */
	IniGroup *GetGroup(const char *name, size_t len = 0);

	/**
	 * Remove the group with the given name.
	 * @param name name of the group to remove.
	 */
	void RemoveGroup(const char *name);

	/**
	 * Load the Ini file's data from the disk.
	 * @param filename the file to load.
	 * @pre nothing has been loaded yet.
	 */
	void LoadFromDisk(const char *filename);

	/**
	 * Save the Ini file's data to the disk.
	 * @param filename the file to save to.
	 * @return true if saving succeeded.
	 */
	bool SaveToDisk(const char *filename);
};

#endif /* INI_TYPE_H */
