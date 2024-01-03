/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music.cpp The songs that OpenTTD knows. */

#include "stdafx.h"


/** The type of set we're replacing */
#define SET_TYPE "music"
#include "base_media_func.h"
#include "random_access_file_type.h"

#include "safeguards.h"


/**
 * Read the name of a music CAT file entry.
 * @param filename Name of CAT file to read from
 * @param entrynum Index of entry whose name to read
 * @return Pointer to string, caller is responsible for freeing memory,
 *         nullptr if entrynum does not exist.
 */
char *GetMusicCatEntryName(const std::string &filename, size_t entrynum)
{
	if (!FioCheckFileExists(filename, BASESET_DIR)) return nullptr;

	RandomAccessFile file(filename, BASESET_DIR);
	uint32_t ofs = file.ReadDword();
	size_t entry_count = ofs / 8;
	if (entrynum < entry_count) {
		file.SeekTo(entrynum * 8, SEEK_SET);
		file.SeekTo(file.ReadDword(), SEEK_SET);
		byte namelen = file.ReadByte();
		char *name = MallocT<char>(namelen + 1);
		file.ReadBlock(name, namelen);
		name[namelen] = '\0';
		return name;
	}
	return nullptr;
}

/**
 * Read the full data of a music CAT file entry.
 * @param filename Name of CAT file to read from.
 * @param entrynum Index of entry to read
 * @param[out] entrylen Receives length of data read
 * @return Pointer to buffer with data read, caller is responsible for freeind memory,
 *         nullptr if entrynum does not exist.
 */
byte *GetMusicCatEntryData(const std::string &filename, size_t entrynum, size_t &entrylen)
{
	entrylen = 0;
	if (!FioCheckFileExists(filename, BASESET_DIR)) return nullptr;

	RandomAccessFile file(filename, BASESET_DIR);
	uint32_t ofs = file.ReadDword();
	size_t entry_count = ofs / 8;
	if (entrynum < entry_count) {
		file.SeekTo(entrynum * 8, SEEK_SET);
		size_t entrypos = file.ReadDword();
		entrylen = file.ReadDword();
		file.SeekTo(entrypos, SEEK_SET);
		file.SkipBytes(file.ReadByte());
		byte *data = MallocT<byte>(entrylen);
		file.ReadBlock(data, entrylen);
		return data;
	}
	return nullptr;
}

INSTANTIATE_BASE_MEDIA_METHODS(BaseMedia<MusicSet>, MusicSet)

/** Names corresponding to the music set's files */
static const char * const _music_file_names[] = {
	"theme",
	"old_0", "old_1", "old_2", "old_3", "old_4", "old_5", "old_6", "old_7", "old_8", "old_9",
	"new_0", "new_1", "new_2", "new_3", "new_4", "new_5", "new_6", "new_7", "new_8", "new_9",
	"ezy_0", "ezy_1", "ezy_2", "ezy_3", "ezy_4", "ezy_5", "ezy_6", "ezy_7", "ezy_8", "ezy_9",
};
/** Make sure we aren't messing things up. */
static_assert(lengthof(_music_file_names) == NUM_SONGS_AVAILABLE);

template <class T, size_t Tnum_files, bool Tsearch_in_tars>
/* static */ const char * const *BaseSet<T, Tnum_files, Tsearch_in_tars>::file_names = _music_file_names;

template <class Tbase_set>
/* static */ const char *BaseMedia<Tbase_set>::GetExtension()
{
	return ".obm"; // OpenTTD Base Music
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::DetermineBestSet()
{
	if (BaseMedia<Tbase_set>::used_set != nullptr) return true;

	const Tbase_set *best = nullptr;
	for (const Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != nullptr; c = c->next) {
		if (c->GetNumMissing() != 0) continue;

		if (best == nullptr ||
				(best->fallback && !c->fallback) ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files &&
					(best->shortname == c->shortname && best->version < c->version))) {
			best = c;
		}
	}

	BaseMedia<Tbase_set>::used_set = best;
	return BaseMedia<Tbase_set>::used_set != nullptr;
}

bool MusicSet::FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename)
{
	bool ret = this->BaseSet<MusicSet, NUM_SONGS_AVAILABLE, false>::FillSetDetails(ini, path, full_filename);
	if (ret) {
		this->num_available = 0;
		const IniGroup *names = ini.GetGroup("names");
		const IniGroup *catindex = ini.GetGroup("catindex");
		const IniGroup *timingtrim = ini.GetGroup("timingtrim");
		uint tracknr = 1;
		for (uint i = 0; i < lengthof(this->songinfo); i++) {
			const std::string &filename = this->files[i].filename;
			if (filename.empty() || this->files[i].check_result == MD5File::CR_NO_FILE) {
				continue;
			}

			this->songinfo[i].filename = filename; // non-owned pointer

			const IniItem *item = catindex != nullptr ? catindex->GetItem(_music_file_names[i]) : nullptr;
			if (item != nullptr && item->value.has_value() && !item->value->empty()) {
				/* Song has a CAT file index, assume it's MPS MIDI format */
				this->songinfo[i].filetype = MTT_MPSMIDI;
				this->songinfo[i].cat_index = atoi(item->value->c_str());
				char *songname = GetMusicCatEntryName(filename, this->songinfo[i].cat_index);
				if (songname == nullptr) {
					Debug(grf, 0, "Base music set song missing from CAT file: {}/{}", filename, this->songinfo[i].cat_index);
					continue;
				}
				this->songinfo[i].songname = songname;
				free(songname);
			} else {
				this->songinfo[i].filetype = MTT_STANDARDMIDI;
			}

			const char *trimmed_filename = filename.c_str();
			/* As we possibly add a path to the filename and we compare
			 * on the filename with the path as in the .obm, we need to
			 * keep stripping path elements until we find a match. */
			for (; trimmed_filename != nullptr; trimmed_filename = strchr(trimmed_filename, PATHSEPCHAR)) {
				/* Remove possible double path separator characters from
				 * the beginning, so we don't start reading e.g. root. */
				while (*trimmed_filename == PATHSEPCHAR) trimmed_filename++;

				item = names != nullptr ? names->GetItem(trimmed_filename) : nullptr;
				if (item != nullptr && item->value.has_value() && !item->value->empty()) break;
			}

			if (this->songinfo[i].filetype == MTT_STANDARDMIDI) {
				if (item != nullptr && item->value.has_value() && !item->value->empty()) {
					this->songinfo[i].songname = item->value.value();
				} else {
					Debug(grf, 0, "Base music set song name missing: {}", filename);
					return false;
				}
			}
			this->num_available++;

			/* Number the theme song (if any) track 0, rest are normal */
			if (i == 0) {
				this->songinfo[i].tracknr = 0;
			} else {
				this->songinfo[i].tracknr = tracknr++;
			}

			item = trimmed_filename != nullptr && timingtrim != nullptr ? timingtrim->GetItem(trimmed_filename) : nullptr;
			if (item != nullptr && item->value.has_value() && !item->value->empty()) {
				auto endpos = item->value->find(':');
				if (endpos != std::string::npos) {
					this->songinfo[i].override_start = atoi(item->value->c_str());
					this->songinfo[i].override_end = atoi(item->value->c_str() + endpos + 1);
				}
			}
		}
	}
	return ret;
}
