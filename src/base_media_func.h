/* $Id$ */

/** @file base_media_func.h Generic function implementations for base data (graphics, sounds). */

#include "base_media_base.h"
#include "string_func.h"
#include "ini_type.h"

template <class Tbase_set> /* static */ const char *BaseMedia<Tbase_set>::ini_set;
template <class Tbase_set> /* static */ const Tbase_set *BaseMedia<Tbase_set>::used_set;
template <class Tbase_set> /* static */ Tbase_set *BaseMedia<Tbase_set>::available_sets;

/**
 * Try to read a single piece of metadata and return false if it doesn't exist.
 * @param name the name of the item to fetch.
 */
#define fetch_metadata(name) \
	item = metadata->GetItem(name, false); \
	if (item == NULL || strlen(item->value) == 0) { \
		DEBUG(grf, 0, "Base " SET_TYPE "set detail loading: %s field missing", name); \
		return false; \
	}

template <class T, size_t Tnum_files>
bool BaseSet<T, Tnum_files>::FillSetDetails(IniFile *ini, const char *path)
{
	memset(this, 0, sizeof(*this));

	IniGroup *metadata = ini->GetGroup("metadata");
	IniItem *item;

	fetch_metadata("name");
	this->name = strdup(item->value);

	fetch_metadata("description");
	this->description = strdup(item->value);

	fetch_metadata("shortname");
	for (uint i = 0; item->value[i] != '\0' && i < 4; i++) {
		this->shortname |= ((uint8)item->value[i]) << (i * 8);
	}

	fetch_metadata("version");
	this->version = atoi(item->value);

	/* For each of the file types we want to find the file, MD5 checksums and warning messages. */
	IniGroup *files  = ini->GetGroup("files");
	IniGroup *md5s   = ini->GetGroup("md5s");
	IniGroup *origin = ini->GetGroup("origin");
	for (uint i = 0; i < Tnum_files; i++) {
		MD5File *file = &this->files[i];
		/* Find the filename first. */
		item = files->GetItem(BaseSet<T, Tnum_files>::file_names[i], false);
		if (item == NULL) {
			DEBUG(grf, 0, "No " SET_TYPE " file for: %s", BaseSet<T, Tnum_files>::file_names[i]);
			return false;
		}

		const char *filename = item->value;
		file->filename = str_fmt("%s%s", path, filename);

		/* Then find the MD5 checksum */
		item = md5s->GetItem(filename, false);
		if (item == NULL) {
			DEBUG(grf, 0, "No MD5 checksum specified for: %s", filename);
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
				DEBUG(grf, 0, "Malformed MD5 checksum specified for: %s", filename);
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
			file->missing_warning = strdup("");
		} else {
			file->missing_warning = strdup(item->value);
		}

		switch (file->CheckMD5()) {
			case MD5File::CR_MATCH:
				this->valid_files++;
				/* FALL THROUGH */
			case MD5File::CR_MISMATCH:
				this->found_files++;
				break;

			case MD5File::CR_NO_FILE:
				break;
		}
	}

	return true;
}

template <class Tbase_set>
bool BaseMedia<Tbase_set>::AddFile(const char *filename, size_t basepath_length)
{
	bool ret = false;
	DEBUG(grf, 1, "Checking %s for base " SET_TYPE " set", filename);

	Tbase_set *set = new Tbase_set();;
	IniFile *ini = new IniFile();
	ini->LoadFromDisk(filename);

	char *path = strdup(filename + basepath_length);
	char *psep = strrchr(path, PATHSEPCHAR);
	if (psep != NULL) {
		psep[1] = '\0';
	} else {
		*path = '\0';
	}

	if (set->FillSetDetails(ini, path)) {
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
				DEBUG(grf, 1, "Not adding %s (%i) as base " SET_TYPE " set (duplicate)", set->name, set->version);
				delete set;
			} else {
				Tbase_set **prev = &BaseMedia<Tbase_set>::available_sets;
				while (*prev != duplicate) prev = &(*prev)->next;

				*prev = set;
				set->next = duplicate->next;
				/* don't allow recursive delete of all remaining items */
				duplicate->next = NULL;

				DEBUG(grf, 1, "Removing %s (%i) as base " SET_TYPE " set (duplicate)", duplicate->name, duplicate->version);
				delete duplicate;
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

template <class Tbase_set>
/* static */ char *BaseMedia<Tbase_set>::GetSetsList(char *p, const char *last)
{
	p += seprintf(p, last, "List of " SET_TYPE " sets:\n");
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
		p += seprintf(p, last, "%18s: %s", s->name, s->description);
		int invalid = s->GetNumInvalid();
		if (invalid != 0) {
			int missing = s->GetNumMissing();
			if (missing == 0) {
				p += seprintf(p, last, " (%i corrupt file%s)\n", invalid, invalid == 1 ? "" : "s");
			} else {
				p += seprintf(p, last, " (unuseable: %i missing file%s)\n", missing, missing == 1 ? "" : "s");
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

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo *ci, bool md5sum)
{
	for (const Tbase_set *s = BaseMedia<Tbase_set>::available_sets; s != NULL; s = s->next) {
		if (s->GetNumMissing() != 0) continue;

		if (s->shortname != ci->unique_id) continue;
		if (!md5sum) return true;

		byte md5[16];
		memset(md5, 0, sizeof(md5));
		for (uint i = 0; i < Tbase_set::NUM_FILES; i++) {
			for (uint j = 0; j < sizeof(md5); j++) {
				md5[j] ^= s->files[i].hash[j];
			}
		}
		if (memcmp(md5, ci->md5sum, sizeof(md5)) == 0) return true;
	}

	return false;
}

#else

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::HasSet(const ContentInfo *ci, bool md5sum)
{
	return false;
}

#endif /* ENABLE_NETWORK */

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

template <class Tbase_set>
/* static */ const Tbase_set *BaseMedia<Tbase_set>::GetUsedSet()
{
	return BaseMedia<Tbase_set>::used_set;
}

/**
 * Force instantiation of methods so we don't get linker errors.
 * @param repl_type the type of the BaseMedia to instantiate
 * @param set_type  the type of the BaseSet to instantiate
 */
#define INSTANTIATE_BASE_MEDIA_METHODS(repl_type, set_type) \
	template const char *repl_type::ini_set; \
	template const char *repl_type::GetExtension(); \
	template bool repl_type::AddFile(const char *filename, size_t pathlength); \
	template bool repl_type::HasSet(const struct ContentInfo *ci, bool md5sum); \
	template bool repl_type::SetSet(const char *name); \
	template char *repl_type::GetSetsList(char *p, const char *last); \
	template int repl_type::GetNumSets(); \
	template int repl_type::GetIndexOfUsedSet(); \
	template const set_type *repl_type::GetSet(int index); \
	template const set_type *repl_type::GetUsedSet(); \
	template bool repl_type::DetermineBestSet();

