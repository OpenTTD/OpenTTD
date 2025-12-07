/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file base_media_func.h Generic function implementations for base data (graphics, sounds).
 */

#include "base_media_base.h"
#include "debug.h"
#include "ini_type.h"
#include "string_func.h"
#include "error_func.h"
#include "core/string_consumer.hpp"
#include "3rdparty/fmt/ranges.h"

extern void CheckExternalFiles();

/**
 * Log error from reading basesets.
 * @param full_filename the full filename of the loaded file
 * @param detail detail log message
 * @param level debug level
 */
template <class T>
void BaseSet<T>::LogError(std::string_view full_filename, std::string_view detail, int level) const
{
	Debug(misc, level, "Loading base {}set details failed: {}", BaseSet<T>::SET_TYPE, full_filename);
	Debug(misc, level, "  {}", detail);
}

/**
 * Try to read a single piece of metadata and return nullptr if it doesn't exist.
 * Log error, if the data is missing.
 * @param full_filename the full filename of the loaded file (for error reporting purposes)
 * @param group ini group to read from
 * @param name the name of the item to fetch.
 */
template <class T>
const IniItem *BaseSet<T>::GetMandatoryItem(std::string_view full_filename, const IniGroup &group, std::string_view name) const
{
	auto *item = group.GetItem(name);
	if (item != nullptr && item->value.has_value() && !item->value->empty()) return item;
	this->LogError(full_filename, fmt::format("{}.{} field missing.", group.name, name));
	return nullptr;
}

/**
 * Read the set information from a loaded ini.
 * @param ini      the ini to read from
 * @param path     the path to this ini file (for filenames)
 * @param full_filename the full filename of the loaded file (for error reporting purposes)
 * @param allow_empty_filename empty filenames are valid
 * @return true if loading was successful.
 */
template <class T>
bool BaseSet<T>::FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename, bool allow_empty_filename)
{
	const IniGroup *metadata = ini.GetGroup("metadata");
	if (metadata == nullptr) {
		this->LogError(full_filename, "Is the file readable for the user running OpenTTD?");
		return false;
	}
	const IniItem *item;

	item = this->GetMandatoryItem(full_filename, *metadata, "name");
	if (item == nullptr) return false;
	this->name = *item->value;

	item = this->GetMandatoryItem(full_filename, *metadata, "description");
	if (item == nullptr) return false;
	this->description[std::string{}] = *item->value;

	item = metadata->GetItem("url");
	if (item != nullptr) this->url = *item->value;

	/* Add the translations of the descriptions too. */
	for (const IniItem &titem : metadata->items) {
		if (!titem.name.starts_with("description.")) continue;

		this->description[titem.name.substr(12)] = titem.value.value_or("");
	}

	item = this->GetMandatoryItem(full_filename, *metadata, "shortname");
	if (item == nullptr) return false;
	for (uint i = 0; (*item->value)[i] != '\0' && i < 4; i++) {
		this->shortname |= ((uint8_t)(*item->value)[i]) << (i * 8);
	}

	item = this->GetMandatoryItem(full_filename, *metadata, "version");
	if (item == nullptr) return false;
	for (StringConsumer consumer{*item->value};;) {
		auto value = consumer.TryReadIntegerBase<uint32_t>(10);
		bool valid = value.has_value();
		if (valid) this->version.push_back(*value);
		if (valid && !consumer.AnyBytesLeft()) break;
		if (!valid || !consumer.ReadIf(".")) {
			this->LogError(full_filename, fmt::format("metadata.version field is invalid: {}", *item->value));
			return false;
		}
	}

	item = metadata->GetItem("fallback");
	this->fallback = (item != nullptr && item->value && *item->value != "0" && *item->value != "false");

	/* For each of the file types we want to find the file, MD5 checksums and warning messages. */
	const IniGroup *files  = ini.GetGroup("files");
	const IniGroup *md5s   = ini.GetGroup("md5s");
	const IniGroup *origin = ini.GetGroup("origin");
	auto file_names = BaseSet<T>::GetFilenames();
	bool original_set =
		std::byteswap(this->shortname) == 'TTDD' || // TTD DOS graphics, TTD DOS music
		std::byteswap(this->shortname) == 'TTDW' || // TTD WIN graphics, TTD WIN music
		std::byteswap(this->shortname) == 'TTDO' || // TTD sound
		std::byteswap(this->shortname) == 'TTOD'; // TTO music

	for (uint i = 0; i < BaseSet<T>::NUM_FILES; i++) {
		MD5File *file = &this->files[i];
		/* Find the filename first. */
		item = files != nullptr ? files->GetItem(file_names[i]) : nullptr;
		if (item == nullptr || (!item->value.has_value() && !allow_empty_filename)) {
			this->LogError(full_filename, fmt::format("files.{} field missing", file_names[i]));
			return false;
		}

		if (!item->value.has_value()) {
			file->filename.clear();
			/* If we list no file, that file must be valid */
			this->valid_files++;
			this->found_files++;
			continue;
		}

		const std::string &filename = item->value.value();
		file->filename = path + filename;

		/* Then find the MD5 checksum */
		item = md5s != nullptr ? md5s->GetItem(filename) : nullptr;
		if (item == nullptr || !item->value.has_value()) {
			this->LogError(full_filename, fmt::format("md5s.{} field missing", filename));
			return false;
		}
		if (!ConvertHexToBytes(*item->value, file->hash)) {
			this->LogError(full_filename, fmt::format("md5s.{} is malformed: {}", filename, *item->value));
			return false;
		}

		/* Then find the warning message when the file's missing */
		item = origin != nullptr ? origin->GetItem(filename) : nullptr;
		if (item == nullptr) item = origin != nullptr ? origin->GetItem("default") : nullptr;
		if (item == nullptr || !item->value.has_value()) {
			this->LogError(full_filename, fmt::format("origin.{} field missing", filename), 1);
			file->missing_warning.clear();
		} else {
			file->missing_warning = item->value.value();
		}

		file->check_result = T::CheckMD5(file, BASESET_DIR);
		switch (file->check_result) {
			case MD5File::CR_UNKNOWN:
				break;

			case MD5File::CR_MATCH:
				this->valid_files++;
				this->found_files++;
				break;

			case MD5File::CR_MISMATCH:
				/* This is normal for original sample.cat, which either matches with orig_dos or orig_win. */
				this->LogError(full_filename, fmt::format("MD5 checksum mismatch for: {}", filename), original_set ? 1 : 0);
				this->found_files++;
				break;

			case MD5File::CR_NO_FILE:
				/* Missing files is normal for the original basesets. Use lower debug level */
				this->LogError(full_filename, fmt::format("File is missing: {}", filename), original_set ? 1 : 0);
				break;
		}
	}

	return true;
}

template <class Tbase_set>
bool BaseMedia<Tbase_set>::AddFile(const std::string &filename, size_t basepath_length, const std::string &)
{
	Debug(misc, 1, "Checking {} for base {} set", filename, BaseSet<Tbase_set>::SET_TYPE);

	auto set = std::make_unique<Tbase_set>();
	IniFile ini{};
	std::string path{ filename, basepath_length };
	ini.LoadFromDisk(path, BASESET_DIR);

	auto psep = path.rfind(PATHSEPCHAR);
	if (psep != std::string::npos) {
		path.erase(psep + 1);
	} else {
		path.clear();
	}

	if (!set->FillSetDetails(ini, path, filename)) return false;

	auto existing = std::ranges::find_if(BaseMedia<Tbase_set>::available_sets, [&set](const auto &c) { return c->name == set->name || c->shortname == set->shortname; });
	if (existing != std::end(BaseMedia<Tbase_set>::available_sets)) {
		/* The more complete set takes precedence over the version number. */
		if (((*existing)->valid_files == set->valid_files && (*existing)->version >= set->version) ||
				(*existing)->valid_files > set->valid_files) {

			Debug(misc, 1, "Not adding {} ({}) as base {} set (duplicate, {})", set->name, fmt::join(set->version, "."),
					BaseSet<Tbase_set>::SET_TYPE,
					(*existing)->valid_files > set->valid_files ? "fewer valid files" : "lower version");

			duplicate_sets.push_back(std::move(set));
			return false;
		}

		/* If the duplicate set is currently used (due to rescanning this can happen)
		 * update the currently used set to the new one. This will 'lie' about the
		 * version number until a new game is started which isn't a big problem */
		if (BaseMedia<Tbase_set>::used_set == existing->get()) BaseMedia<Tbase_set>::used_set = set.get();

		/* Keep baseset configuration, if compatible */
		set->CopyCompatibleConfig(**existing);

		Debug(misc, 1, "Removing {} ({}) as base {} set (duplicate, {})", (*existing)->name, fmt::join((*existing)->version, "."), BaseSet<Tbase_set>::SET_TYPE,
				(*existing)->valid_files < set->valid_files ? "fewer valid files" : "lower version");

		/* Existing set is worse, move it to duplicates and replace with the current set. */
		duplicate_sets.push_back(std::move(*existing));

		Debug(misc, 1, "Adding {} ({}) as base {} set", set->name, fmt::join(set->version, "."), BaseSet<Tbase_set>::SET_TYPE);
		*existing = std::move(set);
	} else {
		Debug(grf, 1, "Adding {} ({}) as base {} set", set->name, set->version, BaseSet<Tbase_set>::SET_TYPE);
		available_sets.push_back(std::move(set));
	}

	return true;
}

/**
 * Set the set to be used.
 * @param set the set to use
 * @return true if it could be loaded
 */
template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::SetSet(const Tbase_set *set)
{
	if (set == nullptr) {
		if (!BaseMedia<Tbase_set>::DetermineBestSet()) return false;
	} else {
		BaseMedia<Tbase_set>::used_set = set;
	}
	CheckExternalFiles();
	return true;
}

/**
 * Set the set to be used.
 * @param name of the set to use
 * @return true if it could be loaded
 */
template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::SetSetByName(const std::string &name)
{
	if (name.empty()) {
		return SetSet(nullptr);
	}

	for (const auto &s : BaseMedia<Tbase_set>::available_sets) {
		if (name == s->name) {
			return SetSet(s.get());
		}
	}
	return false;
}

/**
 * Set the set to be used.
 * @param shortname of the set to use
 * @return true if it could be loaded
 */
template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::SetSetByShortname(uint32_t shortname)
{
	if (shortname == 0) {
		return SetSet(nullptr);
	}

	for (const auto &s : BaseMedia<Tbase_set>::available_sets) {
		if (shortname == s->shortname) {
			return SetSet(s.get());
		}
	}
	return false;
}

/**
 * Returns a list with the sets.
 * @param output_iterator The iterator to write the string to.
 */
template <class Tbase_set>
/* static */ void BaseMedia<Tbase_set>::GetSetsList(std::back_insert_iterator<std::string> &output_iterator)
{
	fmt::format_to(output_iterator, "List of {} sets:\n", BaseSet<Tbase_set>::SET_TYPE);
	for (const auto &s : BaseMedia<Tbase_set>::available_sets) {
		fmt::format_to(output_iterator, "{:>18}: {}", s->name, s->GetDescription({}));
		int invalid = s->GetNumInvalid();
		if (invalid != 0) {
			int missing = s->GetNumMissing();
			if (missing == 0) {
				fmt::format_to(output_iterator, " ({} corrupt file{})\n", invalid, invalid == 1 ? "" : "s");
			} else {
				fmt::format_to(output_iterator, " (unusable: {} missing file{})\n", missing, missing == 1 ? "" : "s");
			}
		} else {
			fmt::format_to(output_iterator, "\n");
		}
	}
	fmt::format_to(output_iterator, "\n");
}

#include "network/core/tcp_content_type.h"

template <class Tbase_set> std::optional<std::string_view> TryGetBaseSetFile(const ContentInfo &ci, bool md5sum, std::span<const std::unique_ptr<Tbase_set>> sets)
{
	for (const auto &s : sets) {
		if (s->GetNumMissing() != 0) continue;

		if (s->shortname != ci.unique_id) continue;
		if (!md5sum) return s->files[0].filename;

		MD5Hash md5;
		for (const auto &file : s->files) {
			md5 ^= file.hash;
		}
		if (md5 == ci.md5sum) return s->files[0].filename;
	}
	return std::nullopt;
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo &ci, bool md5sum)
{
	return TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::GetAvailableSets()).has_value() ||
			TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::GetDuplicateSets()).has_value();
}

/**
 * Count the number of available graphics sets.
 * @return the number of sets
 */
template <class Tbase_set>
/* static */ int BaseMedia<Tbase_set>::GetNumSets()
{
	return std::ranges::count_if(BaseMedia<Tbase_set>::GetAvailableSets(), [](const auto &set) {
		return set.get() == BaseMedia<Tbase_set>::used_set || set->GetNumMissing() == 0;
	});
}

/**
 * Get the index of the currently active graphics set
 * @return the current set's index
 */
template <class Tbase_set>
/* static */ int BaseMedia<Tbase_set>::GetIndexOfUsedSet()
{
	int n = 0;
	for (const auto &s : BaseMedia<Tbase_set>::available_sets) {
		if (s.get() == BaseMedia<Tbase_set>::used_set) return n;
		if (s->GetNumMissing() != 0) continue;
		n++;
	}
	return -1;
}

/**
 * Get the name of the graphics set at the specified index
 * @return the name of the set
 */
template <class Tbase_set>
/* static */ const Tbase_set *BaseMedia<Tbase_set>::GetSet(int index)
{
	for (const auto &s : BaseMedia<Tbase_set>::available_sets) {
		if (s.get() != BaseMedia<Tbase_set>::used_set && s->GetNumMissing() != 0) continue;
		if (index == 0) return s.get();
		index--;
	}
	FatalError("Base{}::GetSet(): index {} out of range", BaseSet<Tbase_set>::SET_TYPE, index);
}

/**
 * Return the used set.
 * @return the used set.
 */
template <class Tbase_set>
/* static */ const Tbase_set *BaseMedia<Tbase_set>::GetUsedSet()
{
	return BaseMedia<Tbase_set>::used_set;
}
