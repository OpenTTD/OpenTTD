/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog.h Functions to be called to log a crash */

#ifndef CRASHLOG_H
#define CRASHLOG_H

/**
 * Helper class for creating crash logs.
 */
class CrashLog {
private:
	/** Pointer to the error message. */
	static const char *message;

	/** Temporary 'local' location of the buffer. */
	static char *gamelog_buffer;

	/** Temporary 'local' location of the end of the buffer. */
	static const char *gamelog_last;

	/**
	 * Helper function for printing the gamelog.
	 * @param s the string to print.
	 */
	static void GamelogFillCrashLog(const char *s);
protected:
	/**
	 * Writes OS' version to the buffer.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	virtual char *LogOSVersion(char *buffer, const char *last) const = 0;

	/**
	 * Writes actually encountered error to the buffer.
	 * @param buffer  The begin where to write at.
	 * @param last    The last position in the buffer to write to.
	 * @param messege Message passed to use for possible errors. Can be NULL.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	virtual char *LogError(char *buffer, const char *last, const char *message) const = 0;

	/**
	 * Writes the stack trace to the buffer, if there is information about it
	 * available.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	virtual char *LogStacktrace(char *buffer, const char *last) const = 0;

	/**
	 * Writes information about the data in the registers, if there is
	 * information about it available.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	virtual char *LogRegisters(char *buffer, const char *last) const;

	/**
	 * Writes the dynamically linked libaries/modules to the buffer, if there
	 * is information about it available.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	virtual char *LogModules(char *buffer, const char *last) const;


	/**
	 * Writes OpenTTD's version to the buffer.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	char *LogOpenTTDVersion(char *buffer, const char *last) const;

	/**
	 * Writes the (important) configuration settings to the buffer.
	 * E.g. graphics set, sound set, blitter and AIs.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	char *LogConfiguration(char *buffer, const char *last) const;

	/**
	 * Writes information (versions) of the used libraries.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	char *LogLibraries(char *buffer, const char *last) const;

	/**
	 * Writes the gamelog data to the buffer.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	char *LogGamelog(char *buffer, const char *last) const;

public:
	/** Stub destructor to silence some compilers. */
	virtual ~CrashLog() {}

	/**
	 * Fill the crash log buffer with all data of a crash log.
	 * @param buffer The begin where to write at.
	 * @param last   The last position in the buffer to write to.
	 * @return the position of the \c '\0' character after the buffer.
	 */
	char *FillCrashLog(char *buffer, const char *last) const;

	/**
	 * Write the crash log to a file.
	 * @note On success the filename will be filled with the full path of the
	 *       crash log file. Make sure filename is at least \c MAX_PATH big.
	 * @param buffer The begin of the buffer to write to the disk.
	 * @param filename      Output for the filename of the written file.
	 * @param filename_last The last position in the filename buffer.
	 * @return true when the crash log was successfully written.
	 */
	bool WriteCrashLog(const char *buffer, char *filename, const char *filename_last) const;

	/**
	 * Write the (crash) dump to a file.
	 * @note On success the filename will be filled with the full path of the
	 *       crash dump file. Make sure filename is at least \c MAX_PATH big.
	 * @param filename      Output for the filename of the written file.
	 * @param filename_last The last position in the filename buffer.
	 * @return if less than 0, error. If 0 no dump is made, otherwise the dump
	 *         was successfull (not all OSes support dumping files).
	 */
	virtual int WriteCrashDump(char *filename, const char *filename_last) const;

	/**
	 * Write the (crash) savegame to a file.
	 * @note On success the filename will be filled with the full path of the
	 *       crash save file. Make sure filename is at least \c MAX_PATH big.
	 * @param filename      Output for the filename of the written file.
	 * @param filename_last The last position in the filename buffer.
	 * @return true when the crash save was successfully made.
	 */
	bool WriteSavegame(char *filename, const char *filename_last) const;

	/**
	 * Makes the crash log, writes it to a file and then subsequently tries
	 * to make a crash dump and crash savegame. It uses DEBUG to write
	 * information like paths to the console.
	 * @return true when everything is made successfully.
	 */
	bool MakeCrashLog() const;

	/**
	 * Initialiser for crash logs; do the appropriate things so crashes are
	 * handled by our crash handler instead of returning straight to the OS.
	 * @note must be implemented by all implementers of CrashLog.
	 */
	static void InitialiseCrashLog();

	/**
	 * Sets a message for the error message handler.
	 * @param message The error message of the error.
	 */
	static void SetErrorMessage(const char *message);

	/**
	 * Try to close the sound/video stuff so it doesn't keep lingering around
	 * incorrect video states or so, e.g. keeping dpmi disabled.
	 */
	static void AfterCrashLogCleanup();
};

#endif /* CRASHLOG_H */
