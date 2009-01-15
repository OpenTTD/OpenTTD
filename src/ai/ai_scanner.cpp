/* $Id$ */

/** @file ai_scanner.cpp allows scanning AI scripts */

#include "../stdafx.h"
#include "../debug.h"
#include "../openttd.h"
#include "../string_func.h"
#include "../fileio_func.h"
#include "../fios.h"
#include "../network/network.h"
#include "../core/random_func.hpp"
#include <sys/types.h>
#include <sys/stat.h>

#include <squirrel.h>
#include "../script/squirrel.hpp"
#include "../script/squirrel_helper.hpp"
#include "../script/squirrel_class.hpp"
#include "ai.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"
#include "api/ai_controller.hpp"

void AIScanner::ScanDir(const char *dirname, bool library_scan, bool library_recursive)
{
	extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);
	extern bool FiosIsHiddenFile(const struct dirent *ent);

	char d_name[MAX_PATH];
	char temp_script[1024];
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;

	dir = ttd_opendir(dirname);
	/* Dir not found, so do nothing */
	if (dir == NULL) return;

	/* Walk all dirs trying to find a dir in which 'main.nut' exists */
	while ((dirent = readdir(dir)) != NULL) {
		ttd_strlcpy(d_name, FS2OTTD(dirent->d_name), sizeof(d_name));

		/* Valid file, not '.' or '..', not hidden */
		if (!FiosIsValidFile(dirname, dirent, &sb)) continue;
		if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) continue;
		if (FiosIsHiddenFile(dirent) && strncasecmp(d_name, PERSONAL_DIR, strlen(d_name)) != 0) continue;

		/* Create the full length dirname */
		ttd_strlcpy(temp_script, dirname, sizeof(temp_script));
		ttd_strlcat(temp_script, d_name,  sizeof(temp_script));

		if (S_ISDIR(sb.st_mode)) {
			/* Libraries are always in a subdirectory of their category, so scan those */
			if (library_scan && !library_recursive) {
				ttd_strlcat(temp_script, PATHSEP, sizeof(temp_script));
				ScanDir(temp_script, library_scan, true);
				continue;
			}
		} else if (S_ISREG(sb.st_mode)) {
			/* Only .tar files are allowed */
			char *ext = strrchr(d_name, '.');
			if (ext == NULL || strcasecmp(ext, ".tar") != 0) continue;
			/* .tar files are only allowed in the root of the library dir */
			if (library_recursive) continue;

			/* We always expect a directory in the TAR */
			const char *first_dir = FioTarFirstDir(temp_script);
			if (first_dir == NULL) continue;

			ttd_strlcat(temp_script, PATHSEP, sizeof(temp_script));
			ttd_strlcat(temp_script, first_dir, sizeof(temp_script));
			FioTarAddLink(temp_script, first_dir);
		} else {
			/* Skip any other type of file */
			continue;
		}

		/* Add an additional / where needed */
		if (temp_script[strlen(temp_script) - 1] != PATHSEPCHAR) ttd_strlcat(temp_script, PATHSEP, sizeof(temp_script));

		if (!library_scan) {
			char info_script[MAX_PATH];
			ttd_strlcpy(info_script, temp_script, sizeof(info_script));
			ttd_strlcpy(main_script, temp_script, sizeof(main_script));

			/* Every AI should contain an 'info.nut' and 'main.nut' file; else it is not a valid AI */
			ttd_strlcat(info_script, "info.nut", sizeof(info_script));
			ttd_strlcat(main_script, "main.nut", sizeof(main_script));
			if (!FioCheckFileExists(info_script, AI_DIR) || !FioCheckFileExists(main_script, AI_DIR)) continue;

			DEBUG(ai, 6, "Loading AI at location '%s'", main_script);
			this->engine->LoadScript(info_script);
		} else {
			char library_script[MAX_PATH];
			ttd_strlcpy(library_script, temp_script, sizeof(library_script));
			ttd_strlcpy(main_script, temp_script, sizeof(main_script));

			/* Every library should contain an 'library.nut' and 'main.nut' file; else it is not a valid library */
			ttd_strlcat(library_script, "library.nut", sizeof(library_script));
			ttd_strlcat(main_script, "main.nut", sizeof(main_script));
			if (!FioCheckFileExists(library_script, AI_LIBRARY_DIR) || !FioCheckFileExists(main_script, AI_LIBRARY_DIR)) continue;

			DEBUG(ai, 6, "Loading AI Library '%s' at location '%s'", main_script);
			this->engine->LoadScript(library_script);
		}
	}
	closedir(dir);
}

void AIScanner::ScanAIDir()
{
	char buf[MAX_PATH];
	Searchpath sp;

	extern void ScanForTarFiles();
	ScanForTarFiles();

	FOR_ALL_SEARCHPATHS(sp) {
		FioAppendDirectory(buf, MAX_PATH, sp, AI_DIR);
		if (FileExists(buf)) this->ScanDir(buf, false);
		FioAppendDirectory(buf, MAX_PATH, sp, AI_LIBRARY_DIR);
		if (FileExists(buf)) this->ScanDir(buf, true);
	}
}

void AIScanner::RescanAIDir()
{
	this->ScanAIDir();
}

AIScanner::AIScanner() :
	info_dummy(NULL)
{
	this->engine = new Squirrel();
	this->main_script[0] = '\0';

	/* Create the AIInfo class, and add the RegisterAI function */
	DefSQClass <AIInfo> SQAIInfo("AIInfo");
	SQAIInfo.PreRegister(engine);
	SQAIInfo.AddConstructor<void (AIInfo::*)(), 1>(engine, "x");
	SQAIInfo.DefSQAdvancedMethod(this->engine, &AIInfo::AddSetting, "AddSetting");
	SQAIInfo.DefSQConst(engine, AICONFIG_RANDOM, "AICONFIG_RANDOM");
	SQAIInfo.DefSQConst(engine, AICONFIG_BOOLEAN, "AICONFIG_BOOLEAN");
	SQAIInfo.PostRegister(engine);
	this->engine->AddMethod("RegisterAI", &AIInfo::Constructor, 2, "tx");
	this->engine->AddMethod("RegisterDummyAI", &AIInfo::DummyConstructor, 2, "tx");

	/* Create the AILibrary class, and add the RegisterLibrary function */
	this->engine->AddClassBegin("AILibrary");
	this->engine->AddClassEnd();
	this->engine->AddMethod("RegisterLibrary", &AILibrary::Constructor, 2, "tx");

	/* Mark this class as global pointer */
	this->engine->SetGlobalPointer(this);

	/* Scan the AI dir for scripts */
	this->ScanAIDir();

	/* Create the dummy AI */
	extern void AI_CreateAIInfoDummy(HSQUIRRELVM vm);
	AI_CreateAIInfoDummy(this->engine->GetVM());
}

AIScanner::~AIScanner()
{
	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		delete (*it).second;
	}

	delete this->engine;
}

bool AIScanner::ImportLibrary(const char *library, const char *class_name, int version, HSQUIRRELVM vm, AIController *controller)
{
	/* Internally we store libraries as 'library.version' */
	char library_name[1024];
	snprintf(library_name, sizeof(library_name), "%s.%d", library, version);
	strtolower(library_name);

	/* Check if the library + version exists */
	AILibraryList::iterator iter = this->library_list.find(library_name);
	if (iter == this->library_list.end()) {
		char error[1024];

		/* Now see if the version doesn't exist, or the library */
		iter = this->library_list.find(library);
		if (iter == this->library_list.end()) {
			snprintf(error, sizeof(error), "couldn't find library '%s'", library);
		} else {
			snprintf(error, sizeof(error), "couldn't find library '%s' version %d. The latest version available is %d", library, version, (*iter).second->GetVersion());
		}
		sq_throwerror(vm, OTTD2FS(error));
		return false;
	}

	/* Get the current table/class we belong to */
	HSQOBJECT parent;
	sq_getstackobj(vm, 1, &parent);

	char fake_class[1024];
	int next_number;

	if (!controller->LoadedLibrary(library_name, &next_number, &fake_class[0], sizeof(fake_class))) {
		/* Create a new fake internal name */
		snprintf(fake_class, sizeof(fake_class), "_internalNA%d", next_number);

		/* Load the library in a 'fake' namespace, so we can link it to the name the user requested */
		sq_pushroottable(vm);
		sq_pushstring(vm, OTTD2FS(fake_class), -1);
		sq_newclass(vm, SQFalse);
		/* Load the library */
		if (!Squirrel::LoadScript(vm, (*iter).second->GetMainScript(), false)) {
			char error[1024];
			snprintf(error, sizeof(error), "there was a compile error when importing '%s' version %d", library, version);
			sq_throwerror(vm, OTTD2FS(error));
			return false;
		}
		/* Create the fake class */
		sq_newslot(vm, -3, SQFalse);
		sq_pop(vm, 1);

		controller->AddLoadedLibrary(library_name, fake_class);
	}

	/* Find the real class inside the fake class (like 'sets.Vector') */
	sq_pushroottable(vm);
	sq_pushstring(vm, OTTD2FS(fake_class), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		sq_throwerror(vm, _SC("internal error assigning library class"));
		return false;
	}
	sq_pushstring(vm, OTTD2FS((*iter).second->GetInstanceName()), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		char error[1024];
		snprintf(error, sizeof(error), "unable to find class '%s' in the library '%s' version %d", (*iter).second->GetInstanceName(), library, version);
		sq_throwerror(vm, OTTD2FS(error));
		return false;
	}
	HSQOBJECT obj;
	sq_getstackobj(vm, -1, &obj);
	sq_pop(vm, 3);

	if (StrEmpty(class_name)) {
		sq_pushobject(vm, obj);
		return true;
	}

	/* Now link the name the user wanted to our 'fake' class */
	sq_pushobject(vm, parent);
	sq_pushstring(vm, OTTD2FS(class_name), -1);
	sq_pushobject(vm, obj);
	sq_newclass(vm, SQTrue);
	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);

	sq_pushobject(vm, obj);
	return true;
}

void AIScanner::RegisterLibrary(AILibrary *library)
{
	char library_name[1024];
	snprintf(library_name, sizeof(library_name), "%s.%s.%d", library->GetCategory(), library->GetInstanceName(), library->GetVersion());
	strtolower(library_name);

	if (this->library_list.find(library_name) != this->library_list.end()) {
		/* This AI was already registered */
		if (strcmp(this->library_list[library_name]->GetMainScript(), library->GetMainScript()) == 0) return;

		DEBUG(ai, 0, "Registering two libraries with the same name and version");
		DEBUG(ai, 0, "  1: %s", this->library_list[library_name]->GetMainScript());
		DEBUG(ai, 0, "  2: %s", library->GetMainScript());
		DEBUG(ai, 0, "The first is taking precedence.");

		delete library;
		return;
	}

	this->library_list[strdup(library_name)] = library;
}

void AIScanner::RegisterAI(AIInfo *info)
{
	char ai_name[1024];
	snprintf(ai_name, sizeof(ai_name), "%s.%d", info->GetInstanceName(), info->GetVersion());
	strtolower(ai_name);

	/* Check if GetShortName follows the rules */
	if (strlen(info->GetShortName()) != 4) {
		DEBUG(ai, 0, "The AI '%s' returned a string from GetShortName() which is not four characaters. Unable to load the AI.", info->GetInstanceName());
		delete info;
		return;
	}

	if (this->info_list.find(ai_name) != this->info_list.end()) {
		/* This AI was already registered */
		if (strcmp(this->info_list[ai_name]->GetMainScript(), info->GetMainScript()) == 0) return;

		DEBUG(ai, 0, "Registering two AIs with the same name and version");
		DEBUG(ai, 0, "  1: %s", this->info_list[ai_name]->GetMainScript());
		DEBUG(ai, 0, "  2: %s", info->GetMainScript());
		DEBUG(ai, 0, "The first is taking precedence.");

		delete info;
		return;
	}

	this->info_list[strdup(ai_name)] = info;
}

void AIScanner::UnregisterAI(AIInfo *info)
{
	char ai_name[1024];
	snprintf(ai_name, sizeof(ai_name), "%s.%d", info->GetInstanceName(), info->GetVersion());

	this->info_list.erase(ai_name);
}

AIInfo *AIScanner::SelectRandomAI()
{
	if (this->info_list.size() == 0) {
		DEBUG(ai, 0, "No suitable AI found, loading 'dummy' AI.");
		return this->info_dummy;
	}

	/* Find a random AI */
	uint pos;
	if (_networking) pos = InteractiveRandomRange((uint16)this->info_list.size());
	else             pos =            RandomRange((uint16)this->info_list.size());

	/* Find the Nth item from the array */
	AIInfoList::iterator it = this->info_list.begin();
	for (; pos > 0; pos--) it++;
	AIInfoList::iterator first_it = it;
	return (*it).second;
}

AIInfo *AIScanner::FindInfo(const char *nameParam, int versionParam)
{
	if (this->info_list.size() == 0) return NULL;
	if (nameParam == NULL) return NULL;

	char ai_name[1024];
	char name[1024];
	ttd_strlcpy(name, nameParam, sizeof(name));

	AIInfo *info = NULL;
	int version = -1;

	if (versionParam == -1) {
		snprintf(ai_name, sizeof(ai_name), "%s", name);
		strtolower(ai_name);

		/* We want to load the latest version of this AI; so find it */
		AIInfoList::iterator it = this->info_list.begin();
		for (; it != this->info_list.end(); it++) {
			char ai_name_compare[1024];
			snprintf(ai_name_compare, sizeof(ai_name_compare), "%s", (*it).second->GetInstanceName());
			strtolower(ai_name_compare);

			if (strcasecmp(ai_name, ai_name_compare) == 0 && (*it).second->GetVersion() > version) {
				version = (*it).second->GetVersion();
				info = (*it).second;
			}
		}

		if (info != NULL) return info;

		/* If we didn't find a match AI, maybe the user included a version */
		char *e = strrchr(name, '.');
		if (e == NULL) return NULL;
		*e = '\0';
		e++;
		versionParam = atoi(e);
		/* Fall-through, like we were calling this function with a version */
	}

	/* Try to find a direct 'name.version' match */
	snprintf(ai_name, sizeof(ai_name), "%s.%d", name, versionParam);
	strtolower(ai_name);
	if (this->info_list.find(ai_name) != this->info_list.end()) return this->info_list[ai_name];

	/* See if there is a compatible AI which goes by that name, with the highest
	 *  version which allows loading the requested version */
	snprintf(ai_name, sizeof(ai_name), "%s", name);
	strtolower(ai_name);
	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		char ai_name_compare[1024];
		snprintf(ai_name_compare, sizeof(ai_name_compare), "%s", (*it).second->GetInstanceName());
		strtolower(ai_name_compare);

		if (strcasecmp(ai_name, ai_name_compare) == 0 && (*it).second->CanLoadFromVersion(versionParam) && (*it).second->GetVersion() > version) {
			version = (*it).second->GetVersion();
			info = (*it).second;
		}
	}

	return info;
}

char *AIScanner::GetAIConsoleList(char *p, const char *last)
{
	p += seprintf(p, last, "List of AIs:\n");
	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		AIInfo *i = (*it).second;
		p += seprintf(p, last, "%10s (v%d): %s\n", i->GetInstanceName(), i->GetVersion(), i->GetDescription());
	}
	p += seprintf(p, last, "\n");

	return p;
}
