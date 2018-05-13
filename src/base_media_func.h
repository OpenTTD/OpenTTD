/* $Id$ */

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

/**
 * Try to read a single piece of metadata and return false if it doesn't exist.
 * @param name the name of the item to fetch.
 */
#define fetch_metadata(name) \
	item = metadata->GetItem(name, false); \
	if (item == NULL || StrEmpty(item->value)) { \
		DEBUG(grf, 0, "Base " SET_TYPE "set detail loading: %s field missing.", name); \
		DEBUG(grf, 0, "  Is %s readable for the user running OpenTTD?", full_filename); \
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
bool BaseSet<T, Tnum_files, Tsearch_in_tars>::FillSetDetails(IniFile *ini, const char *path, const char *full_filename, bool allow_empty_filename)
{
	IniGroup *metadata = ini->GetGroup("metadata");
	IniItem *item;

	fetch_metadata("name");
	this->name = stredup(item->value);

	fetch_metadata("description");
	this->description[stredup("")] = stredup(item->value);

	/* Add the translations of the descriptions too. */
	for (const IniItem *item = metadata->item; item != NULL; item = item->next) {
		if (strncmp("description.", item->name, 12) != 0) continue;

		this->description[stredup(item->name + 12)] = stredup(item->value);
	}

	fetch_metadata("shortname");
	for (uint i = 0; item->value[i] != '\0' && i < 4; i++) {
		this->shortname |= ((uint8)item->value[i]) << (i * 8);
	}

	fetch_metadata("version");
	this->version = atoi(item->value);

	item = metadata->GetItem("fallback", false);
	this->fallback = (item != NULL && strcmp(item->value, "0") != 0 && strcmp(item->value, "false") != 0);

	/* For each of the file types we want to find the file, MD5 checksums and warning messages. */
	IniGroup *files  = ini->GetGroup("files");
	IniGroup *md5s   = ini->GetGroup("md5s");
	IniGroup *origin = ini->GetGroup("origin");
	for (uint i = 0; i < Tnum_files; i++) {
		MD5File *file = &this->files[i];
		/* Find the filename first. */
		item = files->GetItem(BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names[i], false);
		if (item == NULL || (item->value == NULL && !allow_empty_filename)) {
			DEBUG(grf, 0, "No " SET_TYPE " file for: %s (in %s)", BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names[i], full_filename);
			return false;
		}

		const char *filename = item->value;
		if (filename == NULL) {
			file->filename = NULL;
			/* If we list no file, that file must be valid */
			this->valid_files++;
			this->found_files++;
			continue;
		}

		file->filename = str_fmt("%s%s", path, filename);

		/* Then find the MD5 checksum */
		item = md5s->GetItem(filename, false);
		if (item == NULL || item->value == NULL) {
			DEBUG(grf, 0, "No MD5 checksum specified for: %s (in %s)", filename, full_filename);
			return false;
		}
		char *c = item->value;
		for (uint i = 0; i < sizeof(file->hash) * 2; i++, c++) {
			uint j;
			if ('0' <= *c && *c <= '9') {
				j = *c - '0';
			} else if ('a' <= *c && *c <= 'f') {
				j = *c - 'a' + 10;
			} else if ('A' <= *c && *c <= 'F') {
				j = *c - 'A' + 10;
			} else {
				DEBUG(grf, 0, "Malformed MD5 checksum specified for: %s (in %s)", filename, full_filename);
				return false;
			}
			if (i % 2 == 0) {
				file->hash[i / 2] = j << 4;
			} else {
				file->hash[i / 2] |= j;
			}
		}

		/* Then find the warning message when the file's missing */
		item = origin->GetItem(filename, false);
		if (item == NULL) item = origin->GetItem("default", false);
		if (item == NULL) {
			DEBUG(grf, 1, "No origin warning message specified for: %s", filename);
			file->missing_warning = stredup("");
		} else {
			file->missing_warning = stredup(item->value);
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
				DEBUG(grf, 1, "MD5 checksum mismatch for: %s (in %s)", filename, full_filename);
				this->found_files++;
				break;

			case MD5File::CR_NO_FILE:
				DEBUG(grf, 1, "The file %s specified in %s is missing", filename, full_filename);
				break;
		}
	}

	return true;
}

template <class Tbase_set>
bool BaseMedia<Tbase_set>::AddFile(const char *filename, size_t basepath_length, const char *tar_filename)
{
	bool ret = false;
	DEBUG(grf, 1, "Checking %s for base " SET_TYPE " set", filename);

	Tbase_set *set = new Tbase_set();
	IniFile *ini = new IniFile();
	ini->LoadFromDisk(filename, BASESET_DIR);

	char *path = stredup(filename + basepath_length);
	char *psep = strrchr(path, PATHSEPCHAR);
	if (psep != NULL) {
		psep[1] = '\0';
	} else {
		*path = '\0';
	}

	if (set->FillSetDetails(ini, path, filename)) {
		Tbase_set *duplicate = NULL;
		for (Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != NULL; c = c->next) {
			if (strcmp(c->name, set->name) == 0 || c->shortname == set->shortname) {
				duplicate = c;
				break;
			}
		}
		if (duplicate != NULL) {
			/* The more complete set takes precedence over the version number. */
			if ((duplicate->valid_files == set->valid_files && duplicate->version >= set->version) ||
					duplicate->valid_files > set->valid_files) {
				DEBUG(grf, 1, "Not adding %s (%i) as base " SET_TYPE " set (duplicate, %s)", set->name, set->version,
						duplicate->valid_files > set->valid_files ? "less valid files" : "lower version");
				set->next = BaseMedia<Tbase_set>::duplicate_sets;
				BaseMedia<Tbase_set>::duplicate_sets = set;
			} else {
				Tbase_set **prev = &BaseMedia<Tbase_set>::available_sets;
				while (*prev != duplicate) prev = &(*prev)->next;

				*prev = set;
				set->next = duplicate->next;

				/* If the duplicate set is currently used (due to rescanning this can happen)
				 * update the currently used set to the new one. This will 'lie' about the
				 * version number until a new game is started which isn't a big problem */
				if (BaseMedia<Tbase_set>::used_set == duplicate) BaseMedia<Tbase_set>::used_set = set;

				DEBUG(grf, 1, "Removing %s (%i) as base " SET_TYPE " set (duplicate, %s)", duplicate->name, duplicate->version,
						duplicate->valid_files < set->valid_files ? "less valid files" : "lower version");
				duplicate->next = BaseMedia<Tbase_set>::duplicate_sets;
				BaseMedia<Tbase_set>::duplicate_sets = duplicate;
				ret = true;
			}
		} else {
			Tbase_set **last = &BaseMedia<Tbase_set>::available_sets;
			while (*last != NULL) last = &(*last)->next;

			*last = set;
			ret = true;
		}
		if (ret) {
			DEBUG(grf, 1, "Adding %s (%i) as base " SET_TYPE " set", set->name, set->version);
		}
	} else {
		delete set;
	}
	free(path);

	delete ini;
	return ret;
}

/**
 * Set the set to be used.
 * @param name of the set to use
 * @return true if it could be loaded
 */
template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::SetSet(const char *name)
{
	extern void CheckExternalFiles();

	if (StrEmpty(name)) {
		if (!BaseMedia<Tbase_set>::DetermineBestSet()) return false;
		CheckExternalFiles();
		return true;
	}

	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
		if (strcmp(name, s->name) == 0) {
			BaseMedia<Tbase_set>::used_set = s;
			CheckExternalFiles();
			return true;
		}
	}
	return false;
}

/**
 * Returns a list with the sets.
 * @param p    where to print to
 * @param last the last character to print to
 * @return the last printed character
 */
template <class Tbase_set>
/* static */ char *BaseMedia<Tbase_set>::GetSetsList(char *p, const char *last)
{
	p += seprintf(p, last, "List of " SET_TYPE " sets:\n");
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
		p += seprintf(p, last, "%18s: %s", s->name, s->GetDescription());
		int invalid = s->GetNumInvalid();
		if (invalid != 0) {
			int missing = s->GetNumMissing();
			if (missing == 0) {
				p += seprintf(p, last, " (%i corrupt file%s)\n", invalid, invalid == 1 ? "" : "s");
			} else {
				p += seprintf(p, last, " (unusable: %i missing file%s)\n", missing, missing == 1 ? "" : "s");
			}
		} else {
			p += seprintf(p, last, "\n");
		}
	}
	p += seprintf(p, last, "\n");

	return p;
}

#if defined(ENABLE_NETWORK)
#include "network/network_content.h"

template <class Tbase_set> const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const Tbase_set *s)
{
	for (; s != NULL; s = s->next) {
		if (s->GetNumMissing() != 0) continue;

		if (s->shortname != ci->unique_id) continue;
		if (!md5sum) return  s->files[0].filename;

		byte md5[16];
		memset(md5, 0, sizeof(md5));
		for (uint i = 0; i < Tbase_set::NUM_FILES; i++) {
			for (uint j = 0; j < sizeof(md5); j++) {
				md5[j] ^= s->files[i].hash[j];
			}
		}
		if (memcmp(md5, ci->md5sum, sizeof(md5)) == 0) return s->files[0].filename;
	}
	return NULL;
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo *ci, bool md5sum)
{
	return (TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::available_sets) != NULL) ||
			(TryGetBaseSetFile(ci, md5sum, BaseMedia<Tbase_set>::duplicate_sets) != NULL);
}

#else

template <class Tbase_set>
const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const Tbase_set *s)
{
	return NULL;
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo *ci, bool md5sum)
{
	return false;
}

#endif /* ENABLE_NETWORK */

/**
 * Count the number of available graphics sets.
 * @return the number of sets
 */
template <class Tbase_set>
/* static */ int BaseMedia<Tbase_set>::GetNumSets()
{
	int n = 0;
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
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
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
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
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
		if (s != BaseMedia<Tbase_set>::used_set && s->GetNumMissing() != 0) continue;
		if (index == 0) return s;
		index--;
	}
	error("Base" SET_TYPE "::GetSet(): index %d out of range", index);
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
	template const char *repl_type::ini_set; \
	template const char *repl_type::GetExtension(); \
	template bool repl_type::AddFile(const char *filename, size_t pathlength, const char *tar_filename); \
	template bool repl_type::HasSet(const struct ContentInfo *ci, bool md5sum); \
	template bool repl_type::SetSet(const char *name); \
	template char *repl_type::GetSetsList(char *p, const char *last); \
	template int repl_type::GetNumSets(); \
	template int repl_type::GetIndexOfUsedSet(); \
	template const set_type *repl_type::GetSet(int index); \
	template const set_type *repl_type::GetUsedSet(); \
	template bool repl_type::DetermineBestSet(); \
	template set_type *repl_type::GetAvailableSets(); \
	template const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const set_type *s);

