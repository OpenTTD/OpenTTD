/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file base_media_func.h Generic function implementations for base data (graphics, sounds).
 * @note You should _never_ include this file due to the SET_TYPE define.
 */

#include "base_media_base.h"
#include "debug.h"
#include "ini_type.h"
#include "string_func.h"
#include "error_func.h"

extern void CheckExternalFiles();

/**
 * Try to read a single piece of metadata and return false if it doesn't exist.
 * @param name the name of the item to fetch.
 */
#define fetch_metadata(name) \
	item = metadata->GetItem(name); \
	if (item == nullptr || !item->value.has_value() || item->value->empty()) { \
		Debug(grf, 0, "Base " SET_TYPE "set detail loading: {} field missing.", name); \
		Debug(grf, 0, "  Is {} readable for the user running OpenTTD?", full_filename); \
		return false; \
	}

/**
 * Read the set information from a loaded ini.
 * @param ini      the ini to read from
 * @param path     the path to this ini file (for filenames)
 * @param full_filename the full filename of the loaded file (for error reporting purposes)
 * @param allow_empty_filename empty filenames are valid
 * @return true if loading was successful.
 */
template <class T, size_t Tnum_files, bool Tsearch_in_tars>
bool BaseSet<T, Tnum_files, Tsearch_in_tars>::FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename, bool allow_empty_filename)
{
	const IniGroup *metadata = ini.GetGroup("metadata");
	if (metadata == nullptr) {
		Debug(grf, 0, "Base " SET_TYPE "set detail loading: metadata missing.");
		Debug(grf, 0, "  Is {} readable for the user running OpenTTD?", full_filename);
		return false;
	}
	const IniItem *item;

	fetch_metadata("name");
	this->name = *item->value;

	fetch_metadata("description");
	this->description[std::string{}] = *item->value;

	item = metadata->GetItem("url");
	if (item != nullptr) this->url = *item->value;

	/* Add the translations of the descriptions too. */
	for (const IniItem &titem : metadata->items) {
		if (titem.name.compare(0, 12, "description.") != 0) continue;

		this->description[titem.name.substr(12)] = titem.value.value_or("");
	}

	fetch_metadata("shortname");
	for (uint i = 0; (*item->value)[i] != '\0' && i < 4; i++) {
		this->shortname |= ((uint8_t)(*item->value)[i]) << (i * 8);
	}

	fetch_metadata("version");
	this->version = atoi(item->value->c_str());

	item = metadata->GetItem("fallback");
	this->fallback = (item != nullptr && item->value && *item->value != "0" && *item->value != "false");

	/* For each of the file types we want to find the file, MD5 checksums and warning messages. */
	const IniGroup *files  = ini.GetGroup("files");
	const IniGroup *md5s   = ini.GetGroup("md5s");
	const IniGroup *origin = ini.GetGroup("origin");
	for (uint i = 0; i < Tnum_files; i++) {
		MD5File *file = &this->files[i];
		/* Find the filename first. */
		item = files != nullptr ? files->GetItem(BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names[i]) : nullptr;
		if (item == nullptr || (!item->value.has_value() && !allow_empty_filename)) {
			Debug(grf, 0, "No " SET_TYPE " file for: {} (in {})", BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names[i], full_filename);
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
			Debug(grf, 0, "No MD5 checksum specified for: {} (in {})", filename, full_filename);
			return false;
		}
		const char *c = item->value->c_str();
		for (size_t i = 0; i < file->hash.size() * 2; i++, c++) {
			uint j;
			if ('0' <= *c && *c <= '9') {
				j = *c - '0';
			} else if ('a' <= *c && *c <= 'f') {
				j = *c - 'a' + 10;
			} else if ('A' <= *c && *c <= 'F') {
				j = *c - 'A' + 10;
			} else {
				Debug(grf, 0, "Malformed MD5 checksum specified for: {} (in {})", filename, full_filename);
				return false;
			}
			if (i % 2 == 0) {
				file->hash[i / 2] = j << 4;
			} else {
				file->hash[i / 2] |= j;
			}
		}

		/* Then find the warning message when the file's missing */
		item = origin != nullptr ? origin->GetItem(filename) : nullptr;
		if (item == nullptr) item = origin != nullptr ? origin->GetItem("default") : nullptr;
		if (item == nullptr || !item->value.has_value()) {
			Debug(grf, 1, "No origin warning message specified for: {}", filename);
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
				Debug(grf, 1, "MD5 checksum mismatch for: {} (in {})", filename, full_filename);
				this->found_files++;
				break;

			case MD5File::CR_NO_FILE:
				Debug(grf, 1, "The file {} specified in {} is missing", filename, full_filename);
				break;
		}
	}

	return true;
}

template <class Tbase_set>
bool BaseMedia<Tbase_set>::AddFile(const std::string &filename, size_t basepath_length, const std::string &)
{
	bool ret = false;
	Debug(grf, 1, "Checking {} for base " SET_TYPE " set", filename);

	Tbase_set *set = new Tbase_set();
	IniFile ini{};
	std::string path{ filename, basepath_length };
	ini.LoadFromDisk(path, BASESET_DIR);

	auto psep = path.rfind(PATHSEPCHAR);
	if (psep != std::string::npos) {
		path.erase(psep + 1);
	} else {
		path.clear();
	}

	if (set->FillSetDetails(ini, path, filename)) {
		Tbase_set *duplicate = nullptr;
		for (Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != nullptr; c = c->next) {
			if (c->name == set->name || c->shortname == set->shortname) {
				duplicate = c;
				break;
			}
		}
		if (duplicate != nullptr) {
			/* The more complete set takes precedence over the version number. */
			if ((duplicate->valid_files == set->valid_files && duplicate->version >= set->version) ||
					duplicate->valid_files > set->valid_files) {
				Debug(grf, 1, "Not adding {} ({}) as base " SET_TYPE " set (duplicate, {})", set->name, set->version,
						duplicate->valid_files > set->valid_files ? "less valid files" : "lower version");
				set->next = BaseMedia<Tbase_set>::duplicate_sets;
				BaseMedia<Tbase_set>::duplicate_sets = set;
			} else {
				Tbase_set **prev = &BaseMedia<Tbase_set>::available_sets;
				while (*prev != duplicate) prev = &(*prev)->next;

				*prev = set;
				set->next = duplicate->next;

				/* Keep baseset configuration, if compatible */
				set->CopyCompatibleConfig(*duplicate);

				/* If the duplicate set is currently used (due to rescanning this can happen)
				 * update the currently used set to the new one. This will 'lie' about the
				 * version number until a new game is started which isn't a big problem */
				if (BaseMedia<Tbase_set>::used_set == duplicate) BaseMedia<Tbase_set>::used_set = set;

				Debug(grf, 1, "Removing {} ({}) as base " SET_TYPE " set (duplicate, {})", duplicate->name, duplicate->version,
						duplicate->valid_files < set->valid_files ? "less valid files" : "lower version");
				duplicate->next = BaseMedia<Tbase_set>::duplicate_sets;
				BaseMedia<Tbase_set>::duplicate_sets = duplicate;
				ret = true;
			}
		} else {
			Tbase_set **last = &BaseMedia<Tbase_set>::available_sets;
			while (*last != nullptr) last = &(*last)->next;

			*last = set;
			ret = true;
		}
		if (ret) {
			Debug(grf, 1, "Adding {} ({}) as base " SET_TYPE " set", set->name, set->version);
		}
	} else {
		delete set;
	}

	return ret;
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

	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
		if (name == s->name) {
			return SetSet(s);
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

	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
		if (shortname == s->shortname) {
			return SetSet(s);
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
	fmt::format_to(output_iterator, "List of " SET_TYPE " sets:\n");
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
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

template <class Tbase_set> const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const Tbase_set *s)
{
	for (; s != nullptr; s = s->next) {
		if (s->GetNumMissing() != 0) continue;

		if (s->shortname != ci->unique_id) continue;
		if (!md5sum) return s->files[0].filename.c_str();

		MD5Hash md5;
		for (uint i = 0; i < Tbase_set::NUM_FILES; i++) {
			md5 ^= s->files[i].hash;
		}
		if (md5 == ci->md5sum) return s->files[0].filename.c_str();
	}
	return nullptr;
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo *ci, bool md5sum)
{
	return (TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::available_sets) != nullptr) ||
			(TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::duplicate_sets) != nullptr);
}

/**
 * Count the number of available graphics sets.
 * @return the number of sets
 */
template <class Tbase_set>
/* static */ int BaseMedia<Tbase_set>::GetNumSets()
{
	int n = 0;
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
		if (s != BaseMedia<Tbase_set>::used_set && s->GetNumMissing() != 0) continue;
		n++;
	}
	return n;
}

/**
 * Get the index of the currently active graphics set
 * @return the current set's index
 */
template <class Tbase_set>
/* static */ int BaseMedia<Tbase_set>::GetIndexOfUsedSet()
{
	int n = 0;
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
		if (s == BaseMedia<Tbase_set>::used_set) return n;
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
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != nullptr; s = s->next) {
		if (s != BaseMedia<Tbase_set>::used_set && s->GetNumMissing() != 0) continue;
		if (index == 0) return s;
		index--;
	}
	FatalError("Base" SET_TYPE "::GetSet(): index {} out of range", index);
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

/**
 * Return the available sets.
 * @return The available sets.
 */
template <class Tbase_set>
/* static */ Tbase_set *BaseMedia<Tbase_set>::GetAvailableSets()
{
	return BaseMedia<Tbase_set>::available_sets;
}

/**
 * Force instantiation of methods so we don't get linker errors.
 * @param repl_type the type of the BaseMedia to instantiate
 * @param set_type  the type of the BaseSet to instantiate
 */
#define INSTANTIATE_BASE_MEDIA_METHODS(repl_type, set_type) \
	template const char *repl_type::GetExtension(); \
	template bool repl_type::AddFile(const std::string &filename, size_t pathlength, const std::string &tar_filename); \
	template bool repl_type::HasSet(const struct ContentInfo *ci, bool md5sum); \
	template bool repl_type::SetSet(const set_type *set); \
	template bool repl_type::SetSetByName(const std::string &name); \
	template bool repl_type::SetSetByShortname(uint32_t shortname); \
	template void repl_type::GetSetsList(std::back_insert_iterator<std::string> &output_iterator); \
	template int repl_type::GetNumSets(); \
	template int repl_type::GetIndexOfUsedSet(); \
	template const set_type *repl_type::GetSet(int index); \
	template const set_type *repl_type::GetUsedSet(); \
	template bool repl_type::DetermineBestSet(); \
	template set_type *repl_type::GetAvailableSets(); \
	template const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const set_type *s);

