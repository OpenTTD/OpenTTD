/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file backup_type.hpp Class for backupping variables and making sure they are restored later. */

#ifndef BACKUP_TYPE_HPP
#define BACKUP_TYPE_HPP

#include "../debug.h"

/**
 * Class to backup a specific variable and restore it later.
 * The variable is not restored automatically, but assertions make sure it is restored.
 * You have to call either Trash() or Restore() exactly once.
 */
template <typename T>
struct Backup {
	/**
	 * Backup variable.
	 * @param original Variable to backup.
	 * @param location Source location for debug output.
	 */
	Backup(T &original, const std::source_location location = std::source_location::current()) : original(original), valid(true), original_value(original), location(location) {}

	/**
	 * Backup variable and switch to new value.
	 * @param original Variable to backup.
	 * @param new_value New value for variable.
	 * @param location Source location for debug output.
	 */
	template <typename U>
	Backup(T &original, const U &new_value, const std::source_location location = std::source_location::current()) : original(original), valid(true), original_value(original), location(location)
	{
		/* Note: We use a separate typename U, so type conversions are handled by assignment operator. */
		original = new_value;
	}

	/**
	 * Check whether the variable was restored on object destruction.
	 */
	~Backup()
	{
		/* Check whether restoration was done */
		if (this->valid)
		{
			/* We cannot assert here, as missing restoration is 'normal' when exceptions are thrown.
			 * Exceptions are especially used to abort world generation. */
			Debug(misc, 0, "{}:{}: Backed-up value was not restored!", this->location.file_name(), this->location.line());
			this->Restore();
		}
	}

	/**
	 * Checks whether the variable was already restored.
	 * @return true if variable has already been restored.
	 */
	bool IsValid() const
	{
		return this->valid;
	}

	/**
	 * Returns the backupped value.
	 * @return value from the backup.
	 */
	const T &GetOriginalValue() const
	{
		assert(this->valid);
		return original_value;
	}

	/**
	 * Change the value of the variable.
	 * While this does not touch the backup at all, it ensures that the variable is only modified while backupped.
	 * @param new_value New value for variable.
	 */
	template <typename U>
	void Change(const U &new_value)
	{
		/* Note: We use a separate typename U, so type conversions are handled by assignment operator. */
		assert(this->valid);
		original = new_value;
	}

	/**
	 * Revert the variable to its original value, but do not mark it as restored.
	 */
	void Revert()
	{
		assert(this->valid);
		this->original = this->original_value;
	}

	/**
	 * Trash the backup. The variable shall not be restored anymore.
	 */
	void Trash()
	{
		assert(this->valid);
		this->valid = false;
	}

	/**
	 * Restore the variable.
	 */
	void Restore()
	{
		this->Revert();
		this->Trash();
	}

	/**
	 * Update the backup.
	 * That is trash the old value and make the current value of the variable the value to be restored later.
	 */
	void Update()
	{
		assert(this->valid);
		this->original_value = this->original;
	}

	/**
	 * Check whether the variable is currently equals the backup.
	 * @return true if equal
	 */
	bool Verify() const
	{
		assert(this->valid);
		return this->original_value == this->original;
	}

private:
	T &original;
	bool valid;
	T original_value;

	const std::source_location location;
};

/**
 * Class to backup a specific variable and restore it upon destruction of this object to prevent
 * stack values going out of scope before resetting the global to its original value. Contrary to
 * #Backup this restores the variable automatically and there is no manual option to restore.
 */
template <typename T>
struct AutoRestoreBackup {
	/*
	 * There is explicitly no only original constructor version, as that would make it possible
	 * for the new value to go out of scope before this object goes out of scope, thus defeating
	 * the whole goal and reason for existing of this object.
	 */

	/**
	 * Backup variable and switch to new value.
	 * @param original Variable to backup.
	 * @param new_value New value for variable.
	 */
	AutoRestoreBackup(T &original, T new_value) : original(original), original_value(original)
	{
		original = new_value;
	}

	/**
	 * Restore the variable upon object destruction.
	 */
	~AutoRestoreBackup()
	{
		this->original = this->original_value;
	}

private:
	T &original;
	T original_value;

	/* Prevent copy, assignment and allocation on stack. */
	AutoRestoreBackup(const AutoRestoreBackup&) = delete;
	AutoRestoreBackup& operator=(AutoRestoreBackup&) = delete;
	static void *operator new(std::size_t) = delete;
	static void *operator new[](std::size_t) = delete;
};

#endif /* BACKUP_TYPE_HPP */
