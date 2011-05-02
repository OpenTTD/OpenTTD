/* $Id$ */

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

INSTANTIATE_BASE_MEDIA_METHODS(BaseMedia<MusicSet>, MusicSet)

/** Names corresponding to the music set's files */
static const char * const _music_file_names[] = {
	"theme",
	"old_0", "old_1", "old_2", "old_3", "old_4", "old_5", "old_6", "old_7", "old_8", "old_9",
	"new_0", "new_1", "new_2", "new_3", "new_4", "new_5", "new_6", "new_7", "new_8", "new_9",
	"ezy_0", "ezy_1", "ezy_2", "ezy_3", "ezy_4", "ezy_5", "ezy_6", "ezy_7", "ezy_8", "ezy_9",
};
/** Make sure we aren't messing things up. */
assert_compile(lengthof(_music_file_names) == NUM_SONGS_AVAILABLE);

template <class T, size_t Tnum_files, Subdirectory Tsubdir>
/* static */ const char * const *BaseSet<T, Tnum_files, Tsubdir>::file_names = _music_file_names;

template <class Tbase_set>
/* static */ const char *BaseMedia<Tbase_set>::GetExtension()
{
	return ".obm"; // OpenTTD Base Music
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::DetermineBestSet()
{
	if (BaseMedia<Tbase_set>::used_set != NULL) return true;

	const Tbase_set *best = NULL;
	for (const Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != NULL; c = c->next) {
		if (c->GetNumMissing() != 0) continue;

		if (best == NULL ||
				(best->fallback && !c->fallback) ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files &&
					(best->shortname == c->shortname && best->version < c->version))) {
			best = c;
		}
	}

	BaseMedia<Tbase_set>::used_set = best;
	return BaseMedia<Tbase_set>::used_set != NULL;
}

bool MusicSet::FillSetDetails(IniFile *ini, const char *path, const char *full_filename)
{
	bool ret = this->BaseSet<MusicSet, NUM_SONGS_AVAILABLE, GM_DIR>::FillSetDetails(ini, path, full_filename);
	if (ret) {
		this->num_available = 0;
		IniGroup *names = ini->GetGroup("names");
		for (uint i = 0, j = 1; i < lengthof(this->song_name); i++) {
			const char *filename = this->files[i].filename;
			if (names == NULL || StrEmpty(filename)) {
				this->song_name[i][0] = '\0';
				continue;
			}

			IniItem *item = NULL;
			/* As we possibly add a path to the filename and we compare
			 * on the filename with the path as in the .obm, we need to
			 * keep stripping path elements until we find a match. */
			for (const char *p = filename; p != NULL; p = strchr(p, PATHSEPCHAR)) {
				/* Remove possible double path separator characters from
				 * the beginning, so we don't start reading e.g. root. */
				while (*p == PATHSEPCHAR) p++;

				item = names->GetItem(p, false);
				if (item != NULL && !StrEmpty(item->value)) break;
			}

			if (item == NULL || StrEmpty(item->value)) {
				DEBUG(grf, 0, "Base music set song name missing: %s", filename);
				return false;
			}

			strecpy(this->song_name[i], item->value, lastof(this->song_name[i]));
			this->track_nr[i] = j++;
			this->num_available++;
		}
	}
	return ret;
}
