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

void AIScanner::ScanDir(const char *dirname, bool library_dir, char *library_depth)
{
	extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);
	extern bool FiosIsHiddenFile(const struct dirent *ent);

	struct stat sb;
	struct dirent *dirent;
	DIR *dir;
	char d_name[MAX_PATH];
	char script_name[MAX_PATH];
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

		if (S_ISDIR(sb.st_mode)) {
			/* Create the full-length script-name */
			ttd_strlcpy(script_name, dirname, sizeof(script_name));
			ttd_strlcat(script_name, d_name, sizeof(script_name));
			ttd_strlcat(script_name, PATHSEP, sizeof(script_name));

			if (library_dir && library_depth == NULL) {
				ScanDir(script_name, library_dir, d_name);
				continue;
			}
		}
		if (S_ISREG(sb.st_mode)) {
			if (library_dir) continue;

			char *ext = strrchr(d_name, '.');
			if (ext == NULL || strcasecmp(ext, ".tar") != 0) continue;

			/* Create the full path to the tarfile */
			char tarname[MAX_PATH];
			ttd_strlcpy(tarname, dirname, sizeof(tarname));
			ttd_strlcat(tarname, d_name, sizeof(tarname));

			/* Now the script-name starts with the first dir in the tar */
			if (FioTarFirstDir(tarname) == NULL) continue;
			ttd_strlcpy(script_name, "%aitar%", sizeof(tarname));
			ttd_strlcat(script_name, FioTarFirstDir(tarname), sizeof(script_name));
			FioTarAddLink(script_name, FioTarFirstDir(tarname));

			/* The name of the AI is the name of the tar minus the .tar */
			*ext = '\0';
		}

		if (!library_dir) {
			/* We look for the file 'info.nut' inside the AI dir.. if it doesn't exists, it isn't an AI */
			ttd_strlcat(script_name, "info.nut", sizeof(script_name));
			if (FioCheckFileExists(script_name, AI_DIR)) {
				char load_script[MAX_PATH];
				ttd_strlcpy(load_script, script_name, sizeof(load_script));

				/* Remove the 'info.nut' part and replace it with 'main.nut' */
				script_name[strlen(script_name) - 8] = '\0';
				ttd_strlcat(script_name, "main.nut", sizeof(script_name));

				DEBUG(ai, 6, "[script] Loading script '%s' for AI handling", load_script);
				this->current_script = script_name;
				this->current_dir = d_name;
				this->engine->LoadScript(load_script);
			}
		} else {
			/* We look for the file 'library.nut' inside the library dir.. */
			ttd_strlcat(script_name, "library.nut", sizeof(script_name));
			if (FioCheckFileExists(script_name, AI_DIR)) {
				char load_script[MAX_PATH];
				char dir_name[MAX_PATH];
				char d_name_2[MAX_PATH];
				/* In case the directory has a dot in it, ignore it, as that is the
				 *  indicator for multiple versions of the same library */
				ttd_strlcpy(d_name_2, d_name, sizeof(d_name_2));
				char *e = strrchr(d_name_2, '.');
				if (e != NULL) *e = '\0';

				ttd_strlcpy(load_script, script_name, sizeof(load_script));
				ttd_strlcpy(dir_name, library_depth, sizeof(dir_name));
				ttd_strlcat(dir_name, ".", sizeof(dir_name));
				ttd_strlcat(dir_name, d_name_2, sizeof(dir_name));

				/* Remove the 'library.nut' part and replace it with 'main.nut' */
				script_name[strlen(script_name) - 11] = '\0';
				ttd_strlcat(script_name, "main.nut", sizeof(script_name));

				DEBUG(ai, 6, "[script] Loading script '%s' for Squirrel library", load_script);
				this->current_script = script_name;
				this->current_dir = dir_name;
				this->engine->LoadScript(load_script);
			}
		}
	}
	closedir(dir);
}

void AIScanner::ScanAIDir()
{
	char buf[MAX_PATH];
	Searchpath sp;

	FOR_ALL_SEARCHPATHS(sp) {
		FioAppendDirectory(buf, MAX_PATH, sp, AI_DIR);
		if (FileExists(buf)) this->ScanDir(buf, false);
		ttd_strlcat(buf, "library" PATHSEP, MAX_PATH);
		if (FileExists(buf)) this->ScanDir(buf, true);
	}
}

void AIScanner::RescanAIDir()
{
	extern void ScanForTarFiles();
	ScanForTarFiles();
	this->ScanAIDir();
}

AIScanner::AIScanner()
{
	this->engine = new Squirrel();

	/* Create the AIInfo class, and add the RegisterAI function */
	DefSQClass <AIInfo> SQAIInfo("AIInfo");
	SQAIInfo.PreRegister(engine);
	SQAIInfo.AddConstructor<void (AIInfo::*)(), 1>(engine, "x");
	SQAIInfo.DefSQAdvancedMethod(this->engine, &AIInfo::AddSetting, "AddSetting");
	SQAIInfo.PostRegister(engine);
	this->engine->AddMethod("RegisterAI", &AIInfo::Constructor, 2, "tx");
	/* Create the AILibrary class, and add the RegisterLibrary function */
	this->engine->AddClassBegin("AILibrary");
	this->engine->AddClassEnd();
	this->engine->AddMethod("RegisterLibrary", &AILibrary::Constructor, 2, "tx");
	this->engine->AddConst("AICONFIG_RANDOM", AICONFIG_RANDOM);
	this->engine->AddConst("AICONFIG_BOOLEAN", AICONFIG_BOOLEAN);

	/* Mark this class as global pointer */
	this->engine->SetGlobalPointer(this);

	/* Scan the AI dir for scripts */
	this->ScanAIDir();

	/* Create the dummy AI */
	this->engine->AddMethod("RegisterDummyAI", &AIInfo::DummyConstructor, 2, "tx");
	this->current_script = (char *)"%_dummy";
	this->current_dir = (char *)"%_dummy";
	extern void AI_CreateAIInfoDummy(HSQUIRRELVM vm);
	AI_CreateAIInfoDummy(this->engine->GetVM());
}

AIScanner::~AIScanner()
{
	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		AIInfo *i = (*it).second;
		delete i;
	}

	delete this->engine;
}

bool AIScanner::ImportLibrary(const char *library, const char *class_name, int version, HSQUIRRELVM vm, AIController *controller)
{
	/* Internally we store libraries as 'library.version' */
	char library_name[1024];
	snprintf(library_name, sizeof(library_name), "%s.%d", library, version);

	/* Check if the library + version exists */
	AILibraryList::iterator iter = this->library_list.find(library_name);
	if (iter == this->library_list.end()) {
		char error[1024];

		/* Now see if the version doesn't exist, or the library */
		iter = this->library_list.find(library);
		if (iter == this->library_list.end()) {
			snprintf(error, sizeof(error), "couldn't find library '%s'", library);
		} else {
			snprintf(error, sizeof(error), "this AI is expecting library '%s' to be version %d, but the latest available is version %d", library, version, (*iter).second->GetVersion());
		}
		sq_throwerror(vm, OTTD2FS(error));
		return false;
	}

	/* Get the current table/class we belong to */
	HSQOBJECT parent;
	sq_getstackobj(vm, 1, &parent);

	char fake_class[1024];
	int next_number;
	/* Check if we already have this library loaded.. if so, fill fake_class
	 *  with the class-name it is nested in */
	if (!controller->LoadedLibrary(library_name, &next_number, &fake_class[0], sizeof(fake_class))) {
		/* Create a new fake internal name */
		snprintf(fake_class, sizeof(fake_class), "_internalNA%d", next_number);

		/* Load the library in a 'fake' namespace, so we can link it to the name the user requested */
		sq_pushroottable(vm);
		sq_pushstring(vm, OTTD2FS(fake_class), -1);
		sq_newclass(vm, SQFalse);
		/* Load the library */
		if (!Squirrel::LoadScript(vm, (*iter).second->GetScriptName(), false)) {
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
	const char *ai_name_without_version = library->GetDirName();
	char ai_name[1024];
	snprintf(ai_name, sizeof(ai_name), "%s.%d", library->GetDirName(), library->GetVersion());

	/* Check if we register twice; than the first always wins */
	if (this->library_list.find(ai_name) != this->library_list.end()) {
		/* In case they are not the same dir, give a warning */
		if (strcasecmp(library->GetScriptName(), this->library_list[ai_name]->GetScriptName()) != 0) {
			DEBUG(ai, 0, "Registering two libraries with the same name");
			DEBUG(ai, 0, "  1: %s", this->library_list[ai_name]->GetScriptName());
			DEBUG(ai, 0, "  2: %s", library->GetScriptName());
			DEBUG(ai, 0, "The first is taking precedence");
		}
		/* Delete the new AILibrary, as we will be using the old one */
		delete library;
		return;
	}

	this->library_list[strdup(ai_name)] = library;
	/* Insert the global name too, so we if the library is known at all */
	if (this->library_list.find(ai_name_without_version) == this->library_list.end()) {
		this->library_list[strdup(ai_name_without_version)] = library;
	} else if (this->library_list[ai_name_without_version]->GetVersion() < library->GetVersion()) {
		this->library_list[ai_name_without_version] = library;
	}
}

void AIScanner::RegisterAI(AIInfo *info)
{
	const char *ai_name = info->GetDirName();

	/* Check if we register twice; than the first always wins */
	if (this->info_list.find(ai_name) != this->info_list.end()) {
		/* In case they are not the same dir, give a warning */
		if (strcasecmp(info->GetScriptName(), this->info_list[ai_name]->GetScriptName()) != 0) {
			DEBUG(ai, 0, "Registering two AIs with the same name");
			DEBUG(ai, 0, "  1: %s", this->info_list[ai_name]->GetScriptName());
			DEBUG(ai, 0, "  2: %s", info->GetScriptName());
			DEBUG(ai, 0, "The first is taking precedence");
		}
		/* Delete the new AIInfo, as we will be using the old one */
		delete info;
		return;
	}

	this->info_list[strdup(ai_name)] = info;
}

void AIScanner::UnregisterAI(AIInfo *info)
{
	this->info_list.erase(info->GetDirName());
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

AIInfo *AIScanner::FindInfo(const char *name, int version)
{
	if (this->info_list.size() == 0) return NULL;
	if (name == NULL) return NULL;

	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		AIInfo *i = (*it).second;

		if (strcasecmp(name, (*it).first) == 0 && i->CanLoadFromVersion(version)) {
			return i;
		}
	}

	return NULL;
}

char *AIScanner::GetAIConsoleList(char *p, const char *last)
{
	p += seprintf(p, last, "List of AIs:\n");
	AIInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		AIInfo *i = (*it).second;
		p += seprintf(p, last, "%10s (v%d): %s\n", (*it).first, i->GetVersion(), i->GetDescription());
	}
	p += seprintf(p, last, "\n");

	return p;
}
