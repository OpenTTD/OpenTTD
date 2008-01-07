/* $Id$ */

/** @file newgrf_config.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "variables.h"
#include "saveload.h"
#include "md5.h"
#include "network/network_data.h"
#include "newgrf.h"
#include "newgrf_config.h"
#include "core/alloc_func.hpp"
#include "string_func.h"

#include "fileio.h"
#include "fios.h"
#include <sys/stat.h>

#ifdef WIN32
# include <io.h>
#endif /* WIN32 */


GRFConfig *_all_grfs;
GRFConfig *_grfconfig;
GRFConfig *_grfconfig_newgame;
GRFConfig *_grfconfig_static;


/* Calculate the MD5 Sum for a GRF */
static bool CalcGRFMD5Sum(GRFConfig *config)
{
	FILE *f;
	Md5 checksum;
	uint8 buffer[1024];
	size_t len, size;

	/* open the file */
	f = FioFOpenFile(config->filename, "rb", DATA_DIR, &size);
	if (f == NULL) return false;

	/* calculate md5sum */
	while ((len = fread(buffer, 1, (size > sizeof(buffer)) ? sizeof(buffer) : size, f)) != 0 && size != 0) {
		size -= len;
		checksum.Append(buffer, len);
	}
	checksum.Finish(config->md5sum);

	FioFCloseFile(f);

	return true;
}


/* Find the GRFID and calculate the md5sum */
bool FillGRFDetails(GRFConfig *config, bool is_static)
{
	if (!FioCheckFileExists(config->filename)) {
		config->status = GCS_NOT_FOUND;
		return false;
	}

	/* Find and load the Action 8 information */
	LoadNewGRFFile(config, CONFIG_SLOT, GLS_FILESCAN);

	/* Skip if the grfid is 0 (not read) or 0xFFFFFFFF (ttdp system grf) */
	if (config->grfid == 0 || config->grfid == 0xFFFFFFFF || config->IsOpenTTDBaseGRF()) return false;

	if (is_static) {
		/* Perform a 'safety scan' for static GRFs */
		LoadNewGRFFile(config, 62, GLS_SAFETYSCAN);

		/* GCF_UNSAFE is set if GLS_SAFETYSCAN finds unsafe actions */
		if (HasBit(config->flags, GCF_UNSAFE)) return false;
	}

	return CalcGRFMD5Sum(config);
}


void ClearGRFConfig(GRFConfig **config)
{
	/* GCF_COPY as in NOT strdupped/alloced the filename, name and info */
	if (!HasBit((*config)->flags, GCF_COPY)) {
		free((*config)->filename);
		free((*config)->name);
		free((*config)->info);

		if ((*config)->error != NULL) {
			free((*config)->error->custom_message);
			free((*config)->error->data);
			free((*config)->error);
		}
	}
	free(*config);
	*config = NULL;
}


/* Clear a GRF Config list */
void ClearGRFConfigList(GRFConfig **config)
{
	GRFConfig *c, *next;
	for (c = *config; c != NULL; c = next) {
		next = c->next;
		ClearGRFConfig(&c);
	}
	*config = NULL;
}


/** Copy a GRF Config list
 * @param dst pointer to destination list
 * @param src pointer to source list values
 * @param init_only the copied GRF will be processed up to GLS_INIT
 * @return pointer to the last value added to the destination list */
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src, bool init_only)
{
	/* Clear destination as it will be overwritten */
	ClearGRFConfigList(dst);
	for (; src != NULL; src = src->next) {
		GRFConfig *c = CallocT<GRFConfig>(1);
		*c = *src;
		if (src->filename  != NULL) c->filename  = strdup(src->filename);
		if (src->name      != NULL) c->name      = strdup(src->name);
		if (src->info      != NULL) c->info      = strdup(src->info);
		if (src->error     != NULL) {
			c->error = CallocT<GRFError>(1);
			memcpy(c->error, src->error, sizeof(GRFError));
			if (src->error->data != NULL) c->error->data = strdup(src->error->data);
			if (src->error->custom_message != NULL) c->error->custom_message = strdup(src->error->custom_message);
		}

		ClrBit(c->flags, GCF_INIT_ONLY);
		if (init_only) SetBit(c->flags, GCF_INIT_ONLY);

		*dst = c;
		dst = &c->next;
	}

	return dst;
}

/**
 * Removes duplicates from lists of GRFConfigs. These duplicates
 * are introduced when the _grfconfig_static GRFs are appended
 * to the _grfconfig on a newgame or savegame. As the parameters
 * of the static GRFs could be different that the parameters of
 * the ones used non-statically. This can result in desyncs in
 * multiplayers, so the duplicate static GRFs have to be removed.
 *
 * This function _assumes_ that all static GRFs are placed after
 * the non-static GRFs.
 *
 * @param list the list to remove the duplicates from
 */
static void RemoveDuplicatesFromGRFConfigList(GRFConfig *list)
{
	GRFConfig *prev;
	GRFConfig *cur;

	if (list == NULL) return;

	for (prev = list, cur = list->next; cur != NULL; prev = cur, cur = cur->next) {
		if (cur->grfid != list->grfid) continue;

		prev->next = cur->next;
		ClearGRFConfig(&cur);
		cur = prev; // Just go back one so it continues as normal later on
	}

	RemoveDuplicatesFromGRFConfigList(list->next);
}

/**
 * Appends the static GRFs to a list of GRFs
 * @param dst the head of the list to add to
 */
void AppendStaticGRFConfigs(GRFConfig **dst)
{
	GRFConfig **tail = dst;
	while (*tail != NULL) tail = &(*tail)->next;

	CopyGRFConfigList(tail, _grfconfig_static, false);
	RemoveDuplicatesFromGRFConfigList(*dst);
}

/** Appends an element to a list of GRFs
 * @param dst the head of the list to add to
 * @param el the new tail to be */
void AppendToGRFConfigList(GRFConfig **dst, GRFConfig *el)
{
	GRFConfig **tail = dst;
	while (*tail != NULL) tail = &(*tail)->next;
	*tail = el;

	RemoveDuplicatesFromGRFConfigList(*dst);
}


/* Reset the current GRF Config to either blank or newgame settings */
void ResetGRFConfig(bool defaults)
{
	CopyGRFConfigList(&_grfconfig, _grfconfig_newgame, !defaults);
	AppendStaticGRFConfigs(&_grfconfig);
}


/** Check if all GRFs in the GRF config from a savegame can be loaded.
 * @return will return any of the following 3 values:<br>
 * <ul>
 * <li> GLC_ALL_GOOD: No problems occured, all GRF files were found and loaded
 * <li> GLC_COMPATIBLE: For one or more GRF's no exact match was found, but a
 *     compatible GRF with the same grfid was found and used instead
 * <li> GLC_NOT_FOUND: For one or more GRF's no match was found at all
 * </ul> */
GRFListCompatibility IsGoodGRFConfigList()
{
	GRFListCompatibility res = GLC_ALL_GOOD;

	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		const GRFConfig *f = FindGRFConfig(c->grfid, c->md5sum);
		if (f == NULL) {
			char buf[256];

			/* If we have not found the exactly matching GRF try to find one with the
			 * same grfid, as it most likely is compatible */
			f = FindGRFConfig(c->grfid);
			if (f != NULL) {
				md5sumToString(buf, lastof(buf), c->md5sum);
				DEBUG(grf, 1, "NewGRF %08X (%s) not found; checksum %s. Compatibility mode on", BSWAP32(c->grfid), c->filename, buf);
				SetBit(c->flags, GCF_COMPATIBLE);

				/* Non-found has precedence over compatibility load */
				if (res != GLC_NOT_FOUND) res = GLC_COMPATIBLE;
				goto compatible_grf;
			}

			/* No compatible grf was found, mark it as disabled */
			md5sumToString(buf, lastof(buf), c->md5sum);
			DEBUG(grf, 0, "NewGRF %08X (%s) not found; checksum %s", BSWAP32(c->grfid), c->filename, buf);

			c->status = GCS_NOT_FOUND;
			res = GLC_NOT_FOUND;
		} else {
compatible_grf:
			DEBUG(grf, 1, "Loading GRF %08X from %s", BSWAP32(f->grfid), f->filename);
			/* The filename could be the filename as in the savegame. As we need
			 * to load the GRF here, we need the correct filename, so overwrite that
			 * in any case and set the name and info when it is not set already.
			 * When the GCF_COPY flag is set, it is certain that the filename is
			 * already a local one, so there is no need to replace it. */
			if (!HasBit(c->flags, GCF_COPY)) {
				free(c->filename);
				c->filename = strdup(f->filename);
				memcpy(c->md5sum, f->md5sum, sizeof(c->md5sum));
				if (c->name == NULL && f->name != NULL) c->name = strdup(f->name);
				if (c->info == NULL && f->info != NULL) c->info = strdup(f->info);
				c->error = NULL;
			}
		}
	}

	return res;
}

static bool ScanPathAddGrf(const char *filename)
{
	GRFConfig *c = CallocT<GRFConfig>(1);
	c->filename = strdup(filename);

	bool added = true;
	if (FillGRFDetails(c, false)) {
		if (_all_grfs == NULL) {
			_all_grfs = c;
		} else {
			/* Insert file into list at a position determined by its
				* name, so the list is sorted as we go along */
			GRFConfig **pd, *d;
			bool stop = false;
			for (pd = &_all_grfs; (d = *pd) != NULL; pd = &d->next) {
				if (c->grfid == d->grfid && memcmp(c->md5sum, d->md5sum, sizeof(c->md5sum)) == 0) added = false;
				/* Because there can be multiple grfs with the same name, make sure we checked all grfs with the same name,
				 *  before inserting the entry. So insert a new grf at the end of all grfs with the same name, instead of
				 *  just after the first with the same name. Avoids doubles in the list. */
				if (strcasecmp(c->name, d->name) <= 0) stop = true;
				else if (stop) break;
			}
			if (added) {
				c->next = d;
				*pd = c;
			}
		}
	} else {
		added = false;
	}

	if (!added) {
		/* File couldn't be opened, or is either not a NewGRF or is a
			* 'system' NewGRF or it's already known, so forget about it. */
		free(c->filename);
		free(c->name);
		free(c->info);
		free(c);
	}

	return added;
}

/* Scan a path for NewGRFs */
static uint ScanPath(const char *path, int basepath_length)
{
	extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);

	uint num = 0;
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;

	if (path == NULL || (dir = ttd_opendir(path)) == NULL) return 0;

	while ((dirent = readdir(dir)) != NULL) {
		const char *d_name = FS2OTTD(dirent->d_name);
		char filename[MAX_PATH];

		if (!FiosIsValidFile(path, dirent, &sb)) continue;

		snprintf(filename, lengthof(filename), "%s%s", path, d_name);

		if (sb.st_mode & S_IFDIR) {
			/* Directory */
			if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) continue;
			AppendPathSeparator(filename, lengthof(filename));
			num += ScanPath(filename, basepath_length);
		} else if (sb.st_mode & S_IFREG) {
			/* File */
			char *ext = strrchr(filename, '.');

			/* If no extension or extension isn't .grf, skip the file */
			if (ext == NULL) continue;
			if (strcasecmp(ext, ".grf") != 0) continue;

			if (ScanPathAddGrf(filename + basepath_length)) num++;
		}
	}

	closedir(dir);

	return num;
}

static uint ScanTar(TarFileList::iterator tar)
{
	uint num = 0;
	const char *filename = (*tar).first.c_str();
	const char *ext = strrchr(filename, '.');

	/* If no extension or extension isn't .grf, skip the file */
	if (ext == NULL) return false;
	if (strcasecmp(ext, ".grf") != 0) return false;

	if (ScanPathAddGrf(filename)) num++;

	return num;
}

/**
 * Simple sorter for GRFS
 * @param p1 the first GRFConfig *
 * @param p2 the second GRFConfig *
 * @return the same strcmp would return for the name of the NewGRF.
 */
static int CDECL GRFSorter(const void *p1, const void *p2)
{
	const GRFConfig *c1 = *(const GRFConfig **)p1;
	const GRFConfig *c2 = *(const GRFConfig **)p2;

	return strcmp(c1->name != NULL ? c1->name : c1->filename,
		c2->name != NULL ? c2->name : c2->filename);
}

/* Scan for all NewGRFs */
void ScanNewGRFFiles()
{
	Searchpath sp;
	char path[MAX_PATH];
	TarFileList::iterator tar;
	uint num = 0;

	ClearGRFConfigList(&_all_grfs);

	DEBUG(grf, 1, "Scanning for NewGRFs");
	FOR_ALL_SEARCHPATHS(sp) {
		FioAppendDirectory(path, MAX_PATH, sp, DATA_DIR);
		num += ScanPath(path, strlen(path));
	}
	FOR_ALL_TARS(tar) {
		num += ScanTar(tar);
	}

	DEBUG(grf, 1, "Scan complete, found %d files", num);
	if (num == 0 || _all_grfs == NULL) return;

	/* Sort the linked list using quicksort.
	 * For that we first have to make an array, the qsort and
	 * then remake the linked list. */
	GRFConfig **to_sort = MallocT<GRFConfig*>(num);
	if (to_sort == NULL) return; // No memory, then don't sort

	uint i = 0;
	for (GRFConfig *p = _all_grfs; p != NULL; p = p->next, i++) {
		to_sort[i] = p;
	}
	/* Number of files is not necessarily right */
	num = i;

	qsort(to_sort, num, sizeof(GRFConfig*), GRFSorter);

	for (i = 1; i < num; i++) {
		to_sort[i - 1]->next = to_sort[i];
	}
	to_sort[num - 1]->next = NULL;
	_all_grfs = to_sort[0];
}


/* Find a NewGRF in the scanned list, if md5sum is NULL, we don't care about it*/
const GRFConfig *FindGRFConfig(uint32 grfid, const uint8 *md5sum)
{
	for (const GRFConfig *c = _all_grfs; c != NULL; c = c->next) {
		if (c->grfid == grfid) {
			if (md5sum == NULL) return c;

			if (memcmp(md5sum, c->md5sum, sizeof(c->md5sum)) == 0) return c;
		}
	}

	return NULL;
}

#ifdef ENABLE_NETWORK

/** Structure for UnknownGRFs; this is a lightweight variant of GRFConfig */
struct UnknownGRF : public GRFIdentifier {
	UnknownGRF *next;
	char   name[NETWORK_GRF_NAME_LENGTH];
};

/**
 * Finds the name of a NewGRF in the list of names for unknown GRFs. An
 * unknown GRF is a GRF where the .grf is not found during scanning.
 *
 * The names are resolved via UDP calls to servers that should know the name,
 * though the replies may not come. This leaves "<Unknown>" as name, though
 * that shouldn't matter _very_ much as they need GRF crawler or so to look
 * up the GRF anyway and that works better with the GRF ID.
 *
 * @param grfid  the GRF ID part of the 'unique' GRF identifier
 * @param md5sum the MD5 checksum part of the 'unique' GRF identifier
 * @param create whether to create a new GRFConfig if the GRFConfig did not
 *               exist in the fake list of GRFConfigs.
 * @return the GRFConfig with the given GRF ID and MD5 checksum or NULL when
 *         it does not exist and create is false. This value must NEVER be
 *         freed by the caller.
 */
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create)
{
	UnknownGRF *grf;
	static UnknownGRF *unknown_grfs = NULL;

	for (grf = unknown_grfs; grf != NULL; grf = grf->next) {
		if (grf->grfid == grfid) {
			if (memcmp(md5sum, grf->md5sum, sizeof(grf->md5sum)) == 0) return grf->name;
		}
	}

	if (!create) return NULL;

	grf = CallocT<UnknownGRF>(1);
	grf->grfid = grfid;
	grf->next  = unknown_grfs;
	ttd_strlcpy(grf->name, UNKNOWN_GRF_NAME_PLACEHOLDER, sizeof(grf->name));
	memcpy(grf->md5sum, md5sum, sizeof(grf->md5sum));

	unknown_grfs = grf;
	return grf->name;
}

#endif /* ENABLE_NETWORK */


/* Retrieve a NewGRF from the current config by its grfid */
GRFConfig *GetGRFConfig(uint32 grfid)
{
	GRFConfig *c;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (c->grfid == grfid) return c;
	}

	return NULL;
}


/* Build a space separated list of parameters, and terminate */
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last)
{
	uint i;

	/* Return an empty string if there are no parameters */
	if (c->num_params == 0) return strecpy(dst, "", last);

	for (i = 0; i < c->num_params; i++) {
		if (i > 0) dst = strecpy(dst, " ", last);
		dst += snprintf(dst, last - dst, "%d", c->param[i]);
	}
	return dst;
}

/** Base GRF ID for OpenTTD's base graphics GRFs. */
static const uint32 OPENTTD_GRAPHICS_BASE_GRF_ID = BSWAP32(0xFF4F5400);

/**
 * Checks whether this GRF is a OpenTTD base graphic GRF.
 * @return true if and only if it is a base GRF.
 */
bool GRFConfig::IsOpenTTDBaseGRF() const
{
	return (this->grfid & 0x00FFFFFF) == OPENTTD_GRAPHICS_BASE_GRF_ID;
}


static const SaveLoad _grfconfig_desc[] = {
	SLE_STR(GRFConfig, filename,   SLE_STR, 0x40),
	SLE_VAR(GRFConfig, grfid,      SLE_UINT32),
	SLE_ARR(GRFConfig, md5sum,     SLE_UINT8, 16),
	SLE_ARR(GRFConfig, param,      SLE_UINT32, 0x80),
	SLE_VAR(GRFConfig, num_params, SLE_UINT8),
	SLE_END()
};


static void Save_NGRF()
{
	int index = 0;

	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		if (HasBit(c->flags, GCF_STATIC)) continue;
		SlSetArrayIndex(index++);
		SlObject(c, _grfconfig_desc);
	}
}


static void Load_NGRF()
{
	ClearGRFConfigList(&_grfconfig);
	while (SlIterateArray() != -1) {
		GRFConfig *c = CallocT<GRFConfig>(1);
		SlObject(c, _grfconfig_desc);
		AppendToGRFConfigList(&_grfconfig, c);
	}

	/* Append static NewGRF configuration */
	AppendStaticGRFConfigs(&_grfconfig);
}

extern const ChunkHandler _newgrf_chunk_handlers[] = {
	{ 'NGRF', Save_NGRF, Load_NGRF, CH_ARRAY | CH_LAST }
};



