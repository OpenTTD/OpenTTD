/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog.cpp Implementation of generic function to be called to log a crash */

#include "stdafx.h"
#include "crashlog.h"
#include "survey.h"
#include "gamelog.h"
#include "map_func.h"
#include "music/music_driver.hpp"
#include "sound/sound_driver.hpp"
#include "video/video_driver.hpp"
#include "saveload/saveload.h"
#include "screenshot.h"
#include "network/network_survey.h"
#include "news_gui.h"
#include "fileio_func.h"
#include "fileio_type.h"

#include "company_func.h"
#include "3rdparty/fmt/chrono.h"
#include "3rdparty/fmt/std.h"
#include "core/format.hpp"

#include "safeguards.h"

/* static */ std::string CrashLog::message{};

/** The version of the schema of the JSON information. */
constexpr uint8_t CRASHLOG_SURVEY_VERSION = 1;

/**
 * Writes the gamelog data to the buffer.
 * @param output_iterator Iterator to write the output to.
 */
static void SurveyGamelog(nlohmann::json &json)
{
	json = nlohmann::json::array();

	_gamelog.Print([&json](const std::string &s) {
		json.push_back(s);
	});
}

/**
 * Writes up to 32 recent news messages to the buffer, with the most recent first.
 * @param output_iterator Iterator to write the output to.
 */
static void SurveyRecentNews(nlohmann::json &json)
{
	json = nlohmann::json::array();

	int i = 0;
	for (NewsItem *news = _latest_news; i < 32 && news != nullptr; news = news->prev, i++) {
		TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(news->date);
		json.push_back(fmt::format("({}-{:02}-{:02}) StringID: {}, Type: {}, Ref1: {}, {}, Ref2: {}, {}",
		               ymd.year, ymd.month + 1, ymd.day, news->string_id, news->type,
		               news->reftype1, news->ref1, news->reftype2, news->ref2));
	}
}

/**
 * Create a timestamped filename.
 * @param ext           The extension for the filename.
 * @param with_dir      Whether to prepend the filename with the personal directory.
 * @return The filename
 */
std::string CrashLog::CreateFileName(const char *ext, bool with_dir) const
{
	static std::string crashname;

	if (crashname.empty()) {
		crashname = fmt::format("crash{:%Y%m%d%H%M%S}", fmt::gmtime(time(nullptr)));
	}
	return fmt::format("{}{}{}", with_dir ? _personal_dir : std::string{}, crashname, ext);
}

/**
 * Fill the crash log buffer with all data of a crash log.
 */
void CrashLog::FillCrashLog()
{
	/* Reminder: this JSON is read in an automated fashion.
	 * If any structural changes are applied, please bump the version. */
	this->survey["schema"] = CRASHLOG_SURVEY_VERSION;
	this->survey["date"] = fmt::format("{:%Y-%m-%d %H:%M:%S} (UTC)", fmt::gmtime(time(nullptr)));

	/* If no internal reason was logged, it must be a crash. */
	if (CrashLog::message.empty()) {
		this->SurveyCrash(this->survey["crash"]);
	} else {
		this->survey["crash"]["reason"] = CrashLog::message;
		CrashLog::message.clear();
	}

	if (!this->TryExecute("stacktrace", [this]() { this->SurveyStacktrace(this->survey["stacktrace"]); return true; })) {
		this->survey["stacktrace"] = "crashed while gathering information";
	}

	{
		auto &info = this->survey["info"];
		if (!this->TryExecute("os", [&info]() { SurveyOS(info["os"]); return true; })) {
			info["os"] = "crashed while gathering information";
		}
		if (!this->TryExecute("openttd", [&info]() { SurveyOpenTTD(info["openttd"]); return true; })) {
			info["openttd"] = "crashed while gathering information";
		}
		if (!this->TryExecute("configuration", [&info]() { SurveyConfiguration(info["configuration"]); return true; })) {
			info["configuration"] = "crashed while gathering information";
		}
		if (!this->TryExecute("font", [&info]() { SurveyFont(info["font"]); return true; })) {
			info["font"] = "crashed while gathering information";
		}
		if (!this->TryExecute("compiler", [&info]() { SurveyCompiler(info["compiler"]); return true; })) {
			info["compiler"] = "crashed while gathering information";
		}
		if (!this->TryExecute("libraries", [&info]() { SurveyLibraries(info["libraries"]); return true; })) {
			info["libraries"] = "crashed while gathering information";
		}
	}

	{
		auto &game = this->survey["game"];
		game["local_company"] = _local_company;
		game["current_company"] = _current_company;

		if (!this->TryExecute("timers", [&game]() { SurveyTimers(game["timers"]); return true; })) {
			game["libraries"] = "crashed while gathering information";
		}
		if (!this->TryExecute("companies", [&game]() { SurveyCompanies(game["companies"]); return true; })) {
			game["companies"] = "crashed while gathering information";
		}
		if (!this->TryExecute("settings", [&game]() { SurveySettings(game["settings_changed"], true); return true; })) {
			game["settings"] = "crashed while gathering information";
		}
		if (!this->TryExecute("grfs", [&game]() { SurveyGrfs(game["grfs"]); return true; })) {
			game["grfs"] = "crashed while gathering information";
		}
		if (!this->TryExecute("game_script", [&game]() { SurveyGameScript(game["game_script"]); return true; })) {
			game["game_script"] = "crashed while gathering information";
		}
		if (!this->TryExecute("gamelog", [&game]() { SurveyGamelog(game["gamelog"]); return true; })) {
			game["gamelog"] = "crashed while gathering information";
		}
		if (!this->TryExecute("news", [&game]() { SurveyRecentNews(game["news"]); return true; })) {
			game["news"] = "crashed while gathering information";
		}
	}
}

void CrashLog::PrintCrashLog() const
{
	fmt::print("  OpenTTD version:\n");
	fmt::print("    Version: {}\n", this->survey["info"]["openttd"]["version"]["revision"].get<std::string>());
	fmt::print("    Hash: {}\n", this->survey["info"]["openttd"]["version"]["hash"].get<std::string>());
	fmt::print("    NewGRF ver: {}\n", this->survey["info"]["openttd"]["version"]["newgrf"].get<std::string>());
	fmt::print("    Content ver: {}\n", this->survey["info"]["openttd"]["version"]["content"].get<std::string>());
	fmt::print("\n");

	fmt::print("  Crash:\n");
	fmt::print("    Reason: {}\n", this->survey["crash"]["reason"].get<std::string>());
	fmt::print("\n");

	fmt::print("  Stacktrace:\n");
	for (const auto &line : this->survey["stacktrace"]) {
		fmt::print("    {}\n", line.get<std::string>());
	}
	fmt::print("\n");
}

/**
 * Write the crash log to a file.
 * @note The filename will be written to \c crashlog_filename.
 * @return true when the crash log was successfully written.
 */
bool CrashLog::WriteCrashLog()
{
	this->crashlog_filename = this->CreateFileName(".json.log");

	FILE *file = FioFOpenFile(this->crashlog_filename, "w", NO_DIRECTORY);
	if (file == nullptr) return false;

	std::string survey_json = this->survey.dump(4);

	size_t len = survey_json.size();
	size_t written = fwrite(survey_json.data(), 1, len, file);

	FioFCloseFile(file);
	return len == written;
}

/**
 * Write the (crash) dump to a file.
 *
 * @note Sets \c crashdump_filename when there is a successful return.
 * @return True iff the crashdump was successfully created.
 */
/* virtual */ bool CrashLog::WriteCrashDump()
{
	fmt::print("No method to create a crash.dmp available.\n");
	return false;
}

/**
 * Write the (crash) savegame to a file.
 * @note The filename will be written to \c savegame_filename.
 * @return true when the crash save was successfully made.
 */
bool CrashLog::WriteSavegame()
{
	/* If the map doesn't exist, saving will fail too. If the map got
	 * initialised, there is a big chance the rest is initialised too. */
	if (!Map::IsInitialized()) return false;

	try {
		_gamelog.Emergency();

		this->savegame_filename = this->CreateFileName(".sav");

		/* Don't do a threaded saveload. */
		return SaveOrLoad(this->savegame_filename, SLO_SAVE, DFT_GAME_FILE, NO_DIRECTORY, false) == SL_OK;
	} catch (...) {
		return false;
	}
}

/**
 * Write the (crash) screenshot to a file.
 * @note The filename will be written to \c screenshot_filename.
 * @return std::nullopt when the crash screenshot could not be made, otherwise the filename.
 */
bool CrashLog::WriteScreenshot()
{
	/* Don't draw when we have invalid screen size */
	if (_screen.width < 1 || _screen.height < 1 || _screen.dst_ptr == nullptr) return false;

	std::string filename = this->CreateFileName("", false);
	bool res = MakeScreenshot(SC_CRASHLOG, filename);
	if (res) this->screenshot_filename = _full_screenshot_path;
	return res;
}

/**
 * Send the survey result, noting it was a crash.
 */
void CrashLog::SendSurvey() const
{
	if (_game_mode == GM_NORMAL) {
		_survey.Transmit(NetworkSurveyHandler::Reason::CRASH, true);
	}
}

/**
 * Makes the crash log, writes it to a file and then subsequently tries
 * to make a crash dump and crash savegame. It uses DEBUG to write
 * information like paths to the console.
 */
void CrashLog::MakeCrashLog()
{
	/* Don't keep looping logging crashes. */
	static bool crashlogged = false;
	if (crashlogged) return;
	crashlogged = true;

	fmt::print("Crash encountered, generating crash log...\n");
	this->FillCrashLog();
	fmt::print("Crash log generated.\n\n");

	fmt::print("Crash in summary:\n");
	this->TryExecute("crashlog", [this]() { this->PrintCrashLog(); return true; });

	fmt::print("Writing crash log to disk...\n");
	bool ret = this->TryExecute("crashlog", [this]() { return this->WriteCrashLog(); });
	if (ret) {
		fmt::print("Crash log written to {}. Please add this file to any bug reports.\n\n", this->crashlog_filename);
	} else {
		fmt::print("Writing crash log failed. Please attach the output above to any bug reports.\n\n");
		this->crashlog_filename = "(failed to write crash log)";
	}

	fmt::print("Writing crash dump to disk...\n");
	ret = this->TryExecute("crashdump", [this]() { return this->WriteCrashDump(); });
	if (ret) {
		fmt::print("Crash dump written to {}. Please add this file to any bug reports.\n\n", this->crashdump_filename);
	} else {
		fmt::print("Writing crash dump failed.\n\n");
		this->crashdump_filename = "(failed to write crash dump)";
	}

	fmt::print("Writing crash savegame...\n");
	ret = this->TryExecute("savegame", [this]() { return this->WriteSavegame(); });
	if (ret) {
		fmt::print("Crash savegame written to {}. Please add this file and the last (auto)save to any bug reports.\n\n", this->savegame_filename);
	} else {
		fmt::print("Writing crash savegame failed. Please attach the last (auto)save to any bug reports.\n\n");
		this->savegame_filename = "(failed to write crash savegame)";
	}

	fmt::print("Writing crash screenshot...\n");
	ret = this->TryExecute("screenshot", [this]() { return this->WriteScreenshot(); });
	if (ret) {
		fmt::print("Crash screenshot written to {}. Please add this file to any bug reports.\n\n", this->screenshot_filename);
	} else {
		fmt::print("Writing crash screenshot failed.\n\n");
		this->screenshot_filename = "(failed to write crash screenshot)";
	}

	this->TryExecute("survey", [this]() { this->SendSurvey(); return true; });
}

/**
 * Sets a message for the error message handler.
 * @param message The error message of the error.
 */
/* static */ void CrashLog::SetErrorMessage(const std::string &message)
{
	CrashLog::message = message;
}

/**
 * Try to close the sound/video stuff so it doesn't keep lingering around
 * incorrect video states or so, e.g. keeping dpmi disabled.
 */
/* static */ void CrashLog::AfterCrashLogCleanup()
{
	if (MusicDriver::GetInstance() != nullptr) MusicDriver::GetInstance()->Stop();
	if (SoundDriver::GetInstance() != nullptr) SoundDriver::GetInstance()->Stop();
	if (VideoDriver::GetInstance() != nullptr) VideoDriver::GetInstance()->Stop();
}
