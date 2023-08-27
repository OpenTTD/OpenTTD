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
	/** Error message coming from #FatalError(format, ...). */
	static std::string message;
protected:
	/**
	 * Writes OS' version to the buffer.
	 * @param output_iterator Iterator to write the output to.
	 */
	virtual void LogOSVersion(std::back_insert_iterator<std::string> &output_iterator) const = 0;

	/**
	 * Writes compiler (and its version, if available) to the buffer.
	 * @param output_iterator Iterator to write the output to.
	 */
	virtual void LogCompiler(std::back_insert_iterator<std::string> &output_iterator) const;

	/**
	 * Writes actually encountered error to the buffer.
	 * @param output_iterator Iterator to write the output to.
	 * @param message Message passed to use for errors.
	 */
	virtual void LogError(std::back_insert_iterator<std::string> &output_iterator, const std::string_view message) const = 0;

	/**
	 * Writes the stack trace to the buffer, if there is information about it
	 * available.
	 * @param output_iterator Iterator to write the output to.
	 */
	virtual void LogStacktrace(std::back_insert_iterator<std::string> &output_iterator) const = 0;

	void LogOpenTTDVersion(std::back_insert_iterator<std::string> &output_iterator) const;
	void LogConfiguration(std::back_insert_iterator<std::string> &output_iterator) const;
	void LogLibraries(std::back_insert_iterator<std::string> &output_iterator) const;
	void LogGamelog(std::back_insert_iterator<std::string> &output_iterator) const;
	void LogRecentNews(std::back_insert_iterator<std::string> &output_iterator) const;

	std::string CreateFileName(const char *ext, bool with_dir = true) const;

	/**
	 * Execute the func() and return its value. If any exception / signal / crash happens,
	 * catch it and return false. This function should, in theory, never not return, even
	 * in the worst conditions.
	 *
	 * @param section_name The name of the section to be executed. Printed when a crash happens.
	 * @param func The function to call.
	 * @return true iff the function returned true.
	 */
	virtual bool TryExecute(std::string_view section_name, std::function<bool()> &&func) = 0;

public:
	/** Stub destructor to silence some compilers. */
	virtual ~CrashLog() = default;

	std::string crashlog;
	std::string crashlog_filename;
	std::string crashdump_filename;
	std::string savegame_filename;
	std::string screenshot_filename;

	void FillCrashLog(std::back_insert_iterator<std::string> &output_iterator) const;
	bool WriteCrashLog();

	virtual bool WriteCrashDump();
	bool WriteSavegame();
	bool WriteScreenshot();

	void SendSurvey() const;

	void MakeCrashLog();

	/**
	 * Initialiser for crash logs; do the appropriate things so crashes are
	 * handled by our crash handler instead of returning straight to the OS.
	 * @note must be implemented by all implementers of CrashLog.
	 */
	static void InitialiseCrashLog();

	/**
	 * Prepare crash log handler for a newly started thread.
	 * @note must be implemented by all implementers of CrashLog.
	 */
	static void InitThread();

	static void SetErrorMessage(const std::string &message);
	static void AfterCrashLogCleanup();
};

#endif /* CRASHLOG_H */
