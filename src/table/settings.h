/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file table/settings.h Settings to save in the savegame and config file. */

/* Begin - Callback Functions for the various settings */
static bool v_PositionMainToolbar(int32 p1);
static bool v_PositionStatusbar(int32 p1);
static bool PopulationInLabelActive(int32 p1);
static bool RedrawScreen(int32 p1);
static bool RedrawSmallmap(int32 p1);
static bool InvalidateDetailsWindow(int32 p1);
static bool InvalidateStationBuildWindow(int32 p1);
static bool InvalidateBuildIndustryWindow(int32 p1);
static bool CloseSignalGUI(int32 p1);
static bool InvalidateTownViewWindow(int32 p1);
static bool DeleteSelectStationWindow(int32 p1);
static bool UpdateConsists(int32 p1);
static bool CheckInterval(int32 p1);
static bool TrainAccelerationModelChanged(int32 p1);
static bool RoadVehAccelerationModelChanged(int32 p1);
static bool TrainSlopeSteepnessChanged(int32 p1);
static bool RoadVehSlopeSteepnessChanged(int32 p1);
static bool DragSignalsDensityChanged(int32);
static bool TownFoundingChanged(int32 p1);
static bool DifficultyReset(int32 level);
static bool DifficultyChange(int32);
static bool DifficultyNoiseChange(int32 i);
static bool MaxNoAIsChange(int32 i);
static bool CheckRoadSide(int p1);
static int32 ConvertLandscape(const char *value);
static bool CheckFreeformEdges(int32 p1);
static bool ChangeDynamicEngines(int32 p1);
static bool StationCatchmentChanged(int32 p1);
static bool InvalidateVehTimetableWindow(int32 p1);
static bool InvalidateCompanyLiveryWindow(int32 p1);
static bool InvalidateNewGRFChangeWindows(int32 p1);
static bool InvalidateIndustryViewWindow(int32 p1);

#ifdef ENABLE_NETWORK
static bool UpdateClientName(int32 p1);
static bool UpdateServerPassword(int32 p1);
static bool UpdateRconPassword(int32 p1);
static bool UpdateClientConfigValues(int32 p1);
#endif /* ENABLE_NETWORK */
/* End - Callback Functions for the various settings */


/****************************
 * OTTD specific INI stuff
 ****************************/

/**
 * Settings-macro usage:
 * The list might look daunting at first, but is in general easy to understand.
 * We have two types of list:
 * 1. SDTG_something
 * 2. SDT_something
 * The 'G' stands for global, so this is the one you will use for a
 * SettingDescGlobVarList section meaning global variables. The other uses a
 * Base/Offset and runtime variable selection mechanism, known from the saveload
 * convention (it also has global so it should not be hard).
 * Of each type there are again two versions, the normal one and one prefixed
 * with 'COND'.
 * COND means that the setting is only valid in certain savegame versions
 * (since settings are saved to the savegame, this bookkeeping is necessary.
 * Now there are a lot of types. Easy ones are:
 * - VAR:  any number type, 'type' field specifies what number. eg int8 or uint32
 * - BOOL: a boolean number type
 * - STR:  a string or character. 'type' field specifies what string. Normal, string, or quoted
 * A bit more difficult to use are MMANY (meaning ManyOfMany) and OMANY (OneOfMany)
 * These are actually normal numbers, only bitmasked. In MMANY several bits can
 * be set, in the other only one.
 * The most complex type is INTLIST. This is basically an array of numbers. If
 * the intlist is only valid in certain savegame versions because for example
 * it has grown in size its length cannot be automatically be calculated so
 * use SDT(G)_CONDLISTO() meaning Old.
 * If nothing fits you, you can use the GENERAL macros, but it exposes the
 * internal structure somewhat so it needs a little looking. There are _NULL()
 * macros as well, these fill up space so you can add more settings there (in
 * place) and you DON'T have to increase the savegame version.
 *
 * While reading values from openttd.cfg, some values may not be converted
 * properly, for any kind of reasons.  In order to allow a process of self-cleaning
 * mechanism, a callback procedure is made available.  You will have to supply the function, which
 * will work on a string, one function per setting. And of course, enable the callback param
 * on the appropriate macro.
 */

#define NSD_GENERAL(name, def, cmd, guiflags, min, max, interval, many, str, proc, load)\
	{name, (const void*)(size_t)(def), {(byte)cmd}, {(uint16)guiflags}, min, max, interval, many, str, proc, load}

/* Macros for various objects to go in the configuration file.
 * This section is for global variables */
#define SDTG_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, var, length, def, min, max, interval, full, str, proc, from, to)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, proc, NULL), SLEG_GENERAL(sle_cmd, var, type | flags, length, from, to)}

#define SDTG_CONDVAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_NUMX, SL_VAR, type, flags, guiflags, var, 0, def, min, max, interval, NULL, str, proc, from, to)
#define SDTG_VAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc)\
	SDTG_CONDVAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDBOOL(name, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, var, 0, def, 0, 1, 0, NULL, str, proc, from, to)
#define SDTG_BOOL(name, flags, guiflags, var, def, str, proc)\
	SDTG_CONDBOOL(name, flags, guiflags, var, def, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDLIST(name, type, length, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_INTLIST, SL_ARR, type, flags, guiflags, var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTG_LIST(name, type, flags, guiflags, var, def, str, proc)\
	SDTG_GENERAL(name, SDT_INTLIST, SL_ARR, type, flags, guiflags, var, lengthof(var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDSTR(name, type, length, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_STRING, SL_STR, type, flags, guiflags, var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTG_STR(name, type, flags, guiflags, var, def, str, proc)\
	SDTG_GENERAL(name, SDT_STRING, SL_STR, type, flags, guiflags, var, lengthof(var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDOMANY(name, type, flags, guiflags, var, def, max, full, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, max, 0, full, str, proc, from, to)
#define SDTG_OMANY(name, type, flags, guiflags, var, def, max, full, str, proc)\
	SDTG_CONDOMANY(name, type, flags, guiflags, var, def, max, full, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDMMANY(name, type, flags, guiflags, var, def, full, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, 0, 0, full, str, proc, from, to)
#define SDTG_MMANY(name, type, flags, guiflags, var, def, full, str, proc)\
	SDTG_CONDMMANY(name, type, flags, guiflags, var, def, full, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDNULL(length, from, to)\
	{{"", NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLEG_CONDNULL(length, from, to)}

#define SDTG_END() {{NULL, NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLEG_END()}

/* Macros for various objects to go in the configuration file.
 * This section is for structures where their various members are saved */
#define SDT_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, base, var, length, def, min, max, interval, full, str, proc, load, from, to)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, proc, load), SLE_GENERAL(sle_cmd, base, var, type | flags, length, from, to)}

#define SDT_CONDVAR(base, var, type, from, to, flags, guiflags, def, min, max, interval, str, proc)\
	SDT_GENERAL(#var, SDT_NUMX, SL_VAR, type, flags, guiflags, base, var, 1, def, min, max, interval, NULL, str, proc, NULL, from, to)
#define SDT_VAR(base, var, type, flags, guiflags, def, min, max, interval, str, proc)\
	SDT_CONDVAR(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, min, max, interval, str, proc)

#define SDT_CONDBOOL(base, var, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, base, var, 1, def, 0, 1, 0, NULL, str, proc, NULL, from, to)
#define SDT_BOOL(base, var, flags, guiflags, def, str, proc)\
	SDT_CONDBOOL(base, var, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDT_CONDLIST(base, var, type, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, base, var, lengthof(((base*)8)->var), def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_LIST(base, var, type, flags, guiflags, def, str, proc)\
	SDT_CONDLIST(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDT_CONDSTR(base, var, type, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, base, var, lengthof(((base*)8)->var), def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_STR(base, var, type, flags, guiflags, def, str, proc)\
	SDT_CONDSTR(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)
#define SDT_CONDSTRO(base, var, length, type, from, to, flags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_STR, type, flags, 0, base, var, length, def, 0, 0, NULL, str, proc, from, to)

#define SDT_CONDCHR(base, var, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_VAR, SLE_CHAR, flags, guiflags, base, var, 1, def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_CHR(base, var, flags, guiflags, def, str, proc)\
	SDT_CONDCHR(base, var, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDT_CONDOMANY(base, var, type, from, to, flags, guiflags, def, max, full, str, proc, load)\
	SDT_GENERAL(#var, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, max, 0, full, str, proc, load, from, to)
#define SDT_OMANY(base, var, type, flags, guiflags, def, max, full, str, proc, load)\
	SDT_CONDOMANY(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, max, full, str, proc, load)

#define SDT_CONDMMANY(base, var, type, from, to, flags, guiflags, def, full, str, proc)\
	SDT_GENERAL(#var, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, 0, 0, full, str, proc, NULL, from, to)
#define SDT_MMANY(base, var, type, flags, guiflags, def, full, str, proc)\
	SDT_CONDMMANY(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, full, str, proc)

#define SDT_CONDNULL(length, from, to)\
	{{"", NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLE_CONDNULL(length, from, to)}


#define SDTC_CONDVAR(var, type, from, to, flags, guiflags, def, min, max, interval, str, proc)\
	SDTG_GENERAL(#var, SDT_NUMX, SL_VAR, type, flags, guiflags, _settings_client.var, 1, def, min, max, interval, NULL, str, proc, from, to)
#define SDTC_VAR(var, type, flags, guiflags, def, min, max, interval, str, proc)\
	SDTC_CONDVAR(var, type, 0, SL_MAX_VERSION, flags, guiflags, def, min, max, interval, str, proc)

#define SDTC_CONDBOOL(var, from, to, flags, guiflags, def, str, proc)\
	SDTG_GENERAL(#var, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, _settings_client.var, 1, def, 0, 1, 0, NULL, str, proc, from, to)
#define SDTC_BOOL(var, flags, guiflags, def, str, proc)\
	SDTC_CONDBOOL(var, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDTC_CONDLIST(var, type, length, flags, guiflags, def, str, proc, from, to)\
	SDTG_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, _settings_client.var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTC_LIST(var, type, flags, guiflags, def, str, proc)\
	SDTG_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, _settings_client.var, lengthof(_settings_client.var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTC_CONDSTR(var, type, length, flags, guiflags, def, str, proc, from, to)\
	SDTG_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, _settings_client.var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTC_STR(var, type, flags, guiflags, def, str, proc)\
	SDTG_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, _settings_client.var, lengthof(_settings_client.var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTC_CONDOMANY(var, type, from, to, flags, guiflags, def, max, full, str, proc)\
	SDTG_GENERAL(#var, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, _settings_client.var, 1, def, 0, max, 0, full, str, proc, from, to)
#define SDTC_OMANY(var, type, flags, guiflags, def, max, full, str, proc)\
	SDTC_CONDOMANY(var, type, 0, SL_MAX_VERSION, flags, guiflags, def, max, full, str, proc)

#define SDT_END() {{NULL, NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLE_END()}

/* Shortcuts for macros below. Logically if we don't save the value
 * we also don't sync it in a network game */
#define S SLF_SAVE_NO | SLF_NETWORK_NO
#define C SLF_CONFIG_NO
#define N SLF_NETWORK_NO

#define D0 SGF_0ISDISABLED
#define NC SGF_NOCOMMA
#define MS SGF_MULTISTRING
#define NO SGF_NETWORK_ONLY
#define CR SGF_CURRENCY
#define NN SGF_NO_NETWORK
#define NG SGF_NEWGAME_ONLY
#define NS SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO
#define PC SGF_PER_COMPANY

static const SettingDesc _music_settings[] = {
	 SDT_VAR(MusicFileSettings, playlist,   SLE_UINT8, S, 0,   0, 0,   5, 1,  STR_NULL, NULL),
	 SDT_VAR(MusicFileSettings, music_vol,  SLE_UINT8, S, 0, 127, 0, 127, 1,  STR_NULL, NULL),
	 SDT_VAR(MusicFileSettings, effect_vol, SLE_UINT8, S, 0, 127, 0, 127, 1,  STR_NULL, NULL),
	SDT_LIST(MusicFileSettings, custom_1,   SLE_UINT8, S, 0, NULL,            STR_NULL, NULL),
	SDT_LIST(MusicFileSettings, custom_2,   SLE_UINT8, S, 0, NULL,            STR_NULL, NULL),
	SDT_BOOL(MusicFileSettings, playing,               S, 0, true,            STR_NULL, NULL),
	SDT_BOOL(MusicFileSettings, shuffle,               S, 0, false,           STR_NULL, NULL),
	 SDT_END()
};

/* win32_v.cpp only settings */
#if defined(WIN32) && !defined(DEDICATED)
extern bool _force_full_redraw, _window_maximize;
extern uint _display_hz, _fullscreen_bpp;

static const SettingDescGlobVarList _win32_settings[] = {
	 SDTG_VAR("display_hz",     SLE_UINT, S, 0, _display_hz,       0, 0, 120, 0, STR_NULL, NULL),
	SDTG_BOOL("force_full_redraw",        S, 0, _force_full_redraw,false,        STR_NULL, NULL),
	 SDTG_VAR("fullscreen_bpp", SLE_UINT, S, 0, _fullscreen_bpp,   8, 8,  32, 0, STR_NULL, NULL),
	SDTG_BOOL("window_maximize",          S, 0, _window_maximize,  false,        STR_NULL, NULL),
	 SDTG_END()
};
#endif /* WIN32 */

extern char _config_language_file[MAX_PATH];

static const SettingDescGlobVarList _misc_settings[] = {
	SDTG_MMANY("display_opt",     SLE_UINT8, S, 0, _display_opt,       (1 << DO_SHOW_TOWN_NAMES | 1 << DO_SHOW_STATION_NAMES | 1 << DO_SHOW_SIGNS | 1 << DO_FULL_ANIMATION | 1 << DO_FULL_DETAIL | 1 << DO_SHOW_WAYPOINT_NAMES), "SHOW_TOWN_NAMES|SHOW_STATION_NAMES|SHOW_SIGNS|FULL_ANIMATION||FULL_DETAIL|WAYPOINTS", STR_NULL, NULL),
	 SDTG_BOOL("news_ticker_sound",          S, 0, _news_ticker_sound,     true,    STR_NULL, NULL),
	 SDTG_BOOL("fullscreen",                 S, 0, _fullscreen,           false,    STR_NULL, NULL),
	  SDTG_STR("graphicsset",      SLE_STRQ, S, 0, BaseGraphics::ini_set,  NULL,    STR_NULL, NULL),
	  SDTG_STR("soundsset",        SLE_STRQ, S, 0, BaseSounds::ini_set,    NULL,    STR_NULL, NULL),
	  SDTG_STR("musicset",         SLE_STRQ, S, 0, BaseMusic::ini_set,     NULL,    STR_NULL, NULL),
	  SDTG_STR("videodriver",      SLE_STRQ, S, 0, _ini_videodriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("musicdriver",      SLE_STRQ, S, 0, _ini_musicdriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("sounddriver",      SLE_STRQ, S, 0, _ini_sounddriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("blitter",          SLE_STRQ, S, 0, _ini_blitter,           NULL,    STR_NULL, NULL),
	  SDTG_STR("language",         SLE_STRB, S, 0, _config_language_file,  NULL,    STR_NULL, NULL),
	SDTG_CONDLIST("resolution",  SLE_INT, 2, S, 0, _cur_resolution,   "640,480",    STR_NULL, NULL, 0, SL_MAX_VERSION), // workaround for implicit lengthof() in SDTG_LIST
	  SDTG_STR("screenshot_format",SLE_STRB, S, 0, _screenshot_format_name,NULL,    STR_NULL, NULL),
	  SDTG_STR("savegame_format",  SLE_STRB, S, 0, _savegame_format,       NULL,    STR_NULL, NULL),
	 SDTG_BOOL("rightclick_emulate",         S, 0, _rightclick_emulate,   false,    STR_NULL, NULL),
#ifdef WITH_FREETYPE
	  SDTG_STR("small_font",       SLE_STRB, S, 0, _freetype.small_font,   NULL,    STR_NULL, NULL),
	  SDTG_STR("medium_font",      SLE_STRB, S, 0, _freetype.medium_font,  NULL,    STR_NULL, NULL),
	  SDTG_STR("large_font",       SLE_STRB, S, 0, _freetype.large_font,   NULL,    STR_NULL, NULL),
	  SDTG_VAR("small_size",       SLE_UINT, S, 0, _freetype.small_size,   8, 0, 72, 0, STR_NULL, NULL),
	  SDTG_VAR("medium_size",      SLE_UINT, S, 0, _freetype.medium_size, 10, 0, 72, 0, STR_NULL, NULL),
	  SDTG_VAR("large_size",       SLE_UINT, S, 0, _freetype.large_size,  16, 0, 72, 0, STR_NULL, NULL),
	 SDTG_BOOL("small_aa",                   S, 0, _freetype.small_aa,    false,    STR_NULL, NULL),
	 SDTG_BOOL("medium_aa",                  S, 0, _freetype.medium_aa,   false,    STR_NULL, NULL),
	 SDTG_BOOL("large_aa",                   S, 0, _freetype.large_aa,    false,    STR_NULL, NULL),
#endif
	  SDTG_VAR("sprite_cache_size",SLE_UINT, S, 0, _sprite_cache_size,     4, 1, 64, 0, STR_NULL, NULL),
	  SDTG_VAR("player_face",    SLE_UINT32, S, 0, _company_manager_face,0,0,0xFFFFFFFF,0, STR_NULL, NULL),
	  SDTG_VAR("transparency_options", SLE_UINT, S, 0, _transparency_opt,  0,0,0x1FF,0, STR_NULL, NULL),
	  SDTG_VAR("transparency_locks", SLE_UINT, S, 0, _transparency_lock,   0,0,0x1FF,0, STR_NULL, NULL),
	  SDTG_VAR("invisibility_options", SLE_UINT, S, 0, _invisibility_opt,  0,0, 0xFF,0, STR_NULL, NULL),
	  SDTG_STR("keyboard",         SLE_STRB, S, 0, _keyboard_opt[0],       NULL,    STR_NULL, NULL),
	  SDTG_STR("keyboard_caps",    SLE_STRB, S, 0, _keyboard_opt[1],       NULL,    STR_NULL, NULL),
	  SDTG_END()
};

static const uint GAME_DIFFICULTY_NUM = 18;
static uint16 _old_diff_custom[GAME_DIFFICULTY_NUM];

/* Most of these strings are used both for gameopt-backward compatability
 * and the settings tables. The rest is here for consistency. */
static const char *_locale_currencies = "GBP|USD|EUR|YEN|ATS|BEF|CHF|CZK|DEM|DKK|ESP|FIM|FRF|GRD|HUF|ISK|ITL|NLG|NOK|PLN|RON|RUR|SIT|SEK|YTL|SKK|BRL|EEK|custom";
static const char *_locale_units = "imperial|metric|si";
static const char *_town_names = "english|french|german|american|latin|silly|swedish|dutch|finnish|polish|slovak|norwegian|hungarian|austrian|romanian|czech|swiss|danish|turkish|italian|catalan";
static const char *_climates = "temperate|arctic|tropic|toyland";
static const char *_autosave_interval = "off|monthly|quarterly|half year|yearly";
static const char *_roadsides = "left|right";
static const char *_savegame_date = "long|short|iso";
#ifdef ENABLE_NETWORK
static const char *_server_langs = "ANY|ENGLISH|GERMAN|FRENCH|BRAZILIAN|BULGARIAN|CHINESE|CZECH|DANISH|DUTCH|ESPERANTO|FINNISH|HUNGARIAN|ICELANDIC|ITALIAN|JAPANESE|KOREAN|LITHUANIAN|NORWEGIAN|POLISH|PORTUGUESE|ROMANIAN|RUSSIAN|SLOVAK|SLOVENIAN|SPANISH|SWEDISH|TURKISH|UKRAINIAN|AFRIKAANS|CROATIAN|CATALAN|ESTONIAN|GALICIAN|GREEK|LATVIAN";
#endif /* ENABLE_NETWORK */

static const SettingDesc _gameopt_settings[] = {
	/* In version 4 a new difficulty setting has been added to the difficulty settings,
	 * town attitude towards demolishing. Needs special handling because some dimwit thought
	 * it funny to have the GameDifficulty struct be an array while it is a struct of
	 * same-sized members
	 * XXX - To save file-space and since values are never bigger than about 10? only
	 * save the first 16 bits in the savegame. Question is why the values are still int32
	 * and why not byte for example?
	 * 'SLE_FILE_I16 | SLE_VAR_U16' in "diff_custom" is needed to get around SlArray() hack
	 * for savegames version 0 - though it is an array, it has to go through the byteswap process */
	 SDTG_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_FILE_I16 | SLE_VAR_U16,    C, 0, _old_diff_custom, 17, 0, 0, 0, 0, NULL, STR_NULL, NULL, 0,  3),
	 SDTG_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_UINT16,                    C, 0, _old_diff_custom, 18, 0, 0, 0, 0, NULL, STR_NULL, NULL, 4, SL_MAX_VERSION),

	      SDT_VAR(GameSettings, difficulty.diff_level,    SLE_UINT8,                     0, 0, 3, 0,  3, 0, STR_NULL, NULL),
	    SDT_OMANY(GameSettings, locale.currency,          SLE_UINT8,                     N, 0, 0, CUSTOM_CURRENCY_ID, _locale_currencies, STR_NULL, NULL, NULL),
	    SDT_OMANY(GameSettings, locale.units,             SLE_UINT8,                     N, 0, 1, 2, _locale_units, STR_NULL, NULL, NULL),
	/* There are only 21 predefined town_name values (0-20), but you can have more with newgrf action F so allow these bigger values (21-255). Invalid values will fallback to english on use and (undefined string) in GUI. */
	    SDT_OMANY(GameSettings, game_creation.town_name,  SLE_UINT8,                     0, 0, 0, 255, _town_names, STR_NULL, NULL, NULL),
	    SDT_OMANY(GameSettings, game_creation.landscape,  SLE_UINT8,                     0, 0, 0, 3, _climates, STR_NULL, NULL, ConvertLandscape),
	      SDT_VAR(GameSettings, game_creation.snow_line,  SLE_UINT8,                     0, 0, DEF_SNOWLINE_HEIGHT * TILE_HEIGHT, MIN_SNOWLINE_HEIGHT * TILE_HEIGHT, MAX_SNOWLINE_HEIGHT * TILE_HEIGHT, 0, STR_NULL, NULL),
	 SDT_CONDNULL(                                                1,  0, 22),
 SDTC_CONDOMANY(              gui.autosave,             SLE_UINT8, 23, SL_MAX_VERSION, S, 0, 1, 4, _autosave_interval, STR_NULL, NULL),
	    SDT_OMANY(GameSettings, vehicle.road_side,        SLE_UINT8,                     0, 0, 1, 1, _roadsides, STR_NULL, NULL, NULL),
	    SDT_END()
};

/* Some settings do not need to be synchronised when playing in multiplayer.
 * These include for example the GUI settings and will not be saved with the
 * savegame.
 * It is also a bit tricky since you would think that service_interval
 * for example doesn't need to be synched. Every client assigns the
 * service_interval value to the v->service_interval, meaning that every client
 * assigns his value. If the setting was company-based, that would mean that
 * vehicles could decide on different moments that they are heading back to a
 * service depot, causing desyncs on a massive scale. */
const SettingDesc _settings[] = {
	/***************************************************************************/
	/* Saved settings variables. */
	/* Do not ADD or REMOVE something in this "difficulty.XXX" table or before it. It breaks savegame compatability. */
	 SDT_CONDVAR(GameSettings, difficulty.max_no_competitors,        SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     0,0,MAX_COMPANIES-1,1,STR_NULL,                                  MaxNoAIsChange),
	SDT_CONDNULL(                                                            1, 97, 109),
	 SDT_CONDVAR(GameSettings, difficulty.number_towns,              SLE_UINT8, 97, SL_MAX_VERSION, 0,NG,     2,     0,      4,  1, STR_NUM_VERY_LOW,                          DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.number_industries,         SLE_UINT8, 97, SL_MAX_VERSION, 0,NG,     4,     0,      4,  1, STR_NONE,                                  DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.max_loan,                 SLE_UINT32, 97, SL_MAX_VERSION, 0,NS|CR,300000,100000,500000,50000,STR_NULL,                               DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.initial_interest,          SLE_UINT8, 97, SL_MAX_VERSION, 0,NS,     2,     2,      4,  1, STR_NULL,                                  DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.vehicle_costs,             SLE_UINT8, 97, SL_MAX_VERSION, 0,NS,     0,     0,      2,  1, STR_SEA_LEVEL_LOW,                         DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.competitor_speed,          SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     2,     0,      4,  1, STR_AI_SPEED_VERY_SLOW,                    DifficultyChange),
	SDT_CONDNULL(                                                            1, 97, 109),
	 SDT_CONDVAR(GameSettings, difficulty.vehicle_breakdowns,        SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     1,     0,      2,  1, STR_DISASTER_NONE,                         DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.subsidy_multiplier,        SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     2,     0,      3,  1, STR_SUBSIDY_X1_5,                          DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.construction_cost,         SLE_UINT8, 97, SL_MAX_VERSION, 0,NS,     0,     0,      2,  1, STR_SEA_LEVEL_LOW,                         DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.terrain_type,              SLE_UINT8, 97, SL_MAX_VERSION, 0,NG,     1,     0,      3,  1, STR_TERRAIN_TYPE_VERY_FLAT,                DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.quantity_sea_lakes,        SLE_UINT8, 97, SL_MAX_VERSION, 0,NG,     0,     0,      4,  1, STR_SEA_LEVEL_VERY_LOW,                    DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.economy,                   SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     0,     0,      1,  1, STR_ECONOMY_STEADY,                        DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.line_reverse_mode,         SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     0,     0,      1,  1, STR_REVERSE_AT_END_OF_LINE_AND_AT_STATIONS,DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.disasters,                 SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     0,     0,      1,  1, STR_DISASTERS_OFF,                         DifficultyChange),
	 SDT_CONDVAR(GameSettings, difficulty.town_council_tolerance,    SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     0,     0,      2,  1, STR_CITY_APPROVAL_PERMISSIVE,                            DifficultyNoiseChange),
	 SDT_CONDVAR(GameSettings, difficulty.diff_level,                SLE_UINT8, 97, SL_MAX_VERSION, 0, 0,     3,     0,      3,  0, STR_NULL,                                  DifficultyReset),

	/* There are only 21 predefined town_name values (0-20), but you can have more with newgrf action F so allow these bigger values (21-255). Invalid values will fallback to english on use and (undefined string) in GUI. */
 SDT_CONDOMANY(GameSettings, game_creation.town_name,              SLE_UINT8, 97, SL_MAX_VERSION, 0,NN, 0, 255, _town_names,      STR_NULL,                                  NULL, NULL),
 SDT_CONDOMANY(GameSettings, game_creation.landscape,              SLE_UINT8, 97, SL_MAX_VERSION, 0,NN, 0,   3, _climates,        STR_NULL,                                  NULL, ConvertLandscape),
	 SDT_CONDVAR(GameSettings, game_creation.snow_line,              SLE_UINT8, 97, SL_MAX_VERSION, 0,NN, DEF_SNOWLINE_HEIGHT * TILE_HEIGHT, MIN_SNOWLINE_HEIGHT * TILE_HEIGHT, MAX_SNOWLINE_HEIGHT * TILE_HEIGHT, 0, STR_NULL,     NULL),
 SDT_CONDOMANY(GameSettings, vehicle.road_side,                    SLE_UINT8, 97, SL_MAX_VERSION, 0,NN, 1,   1, _roadsides,       STR_NULL,                                  CheckRoadSide, NULL),

	    SDT_BOOL(GameSettings, construction.build_on_slopes,                                        0,NN,  true,                    STR_CONFIG_SETTING_BUILDONSLOPES,          NULL),
	 SDT_CONDVAR(GameSettings, construction.command_pause_level,     SLE_UINT8,154, SL_MAX_VERSION, 0,MS|NN,  1,     0,       3, 1, STR_CONFIG_SETTING_COMMAND_PAUSE_LEVEL, NULL),
	SDT_CONDBOOL(GameSettings, construction.autoslope,                          75, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_AUTOSLOPE,              NULL),
	    SDT_BOOL(GameSettings, construction.extra_dynamite,                                         0, 0,  true,                    STR_CONFIG_SETTING_EXTRADYNAMITE,          NULL),
	    SDT_BOOL(GameSettings, construction.longbridges,                                            0,NN,  true,                    STR_CONFIG_SETTING_LONGBRIDGES,            NULL),
	    SDT_BOOL(GameSettings, construction.signal_side,                                            N,NN,  true,                    STR_CONFIG_SETTING_SIGNALSIDE,             RedrawScreen),
	    SDT_BOOL(GameSettings, station.never_expire_airports,                                       0,NN, false,                    STR_CONFIG_SETTING_NEVER_EXPIRE_AIRPORTS,  NULL),
	 SDT_CONDVAR(GameSettings, economy.town_layout,                  SLE_UINT8, 59, SL_MAX_VERSION, 0,MS,TL_ORIGINAL,TL_BEGIN,NUM_TLS - 1, 1, STR_CONFIG_SETTING_TOWN_LAYOUT,  TownFoundingChanged),
	SDT_CONDBOOL(GameSettings, economy.allow_town_roads,                       113, SL_MAX_VERSION, 0,NN,  true,                    STR_CONFIG_SETTING_ALLOW_TOWN_ROADS,       NULL),
	 SDT_CONDVAR(GameSettings, economy.found_town,                   SLE_UINT8,128, SL_MAX_VERSION, 0,MS,TF_FORBIDDEN,TF_BEGIN,TF_END - 1, 1, STR_CONFIG_SETTING_TOWN_FOUNDING, TownFoundingChanged),
	SDT_CONDBOOL(GameSettings, economy.allow_town_level_crossings,             143, SL_MAX_VERSION, 0,NN,  true,                    STR_CONFIG_SETTING_ALLOW_TOWN_LEVEL_CROSSINGS, NULL),

	     SDT_VAR(GameSettings, vehicle.train_acceleration_model,     SLE_UINT8,                     0,MS,     0,     0,       1, 1, STR_CONFIG_SETTING_TRAIN_ACCELERATION_MODEL, TrainAccelerationModelChanged),
	 SDT_CONDVAR(GameSettings, vehicle.roadveh_acceleration_model,   SLE_UINT8,139, SL_MAX_VERSION, 0,MS,     0,     0,       1, 1, STR_CONFIG_SETTING_ROAD_VEHICLE_ACCELERATION_MODEL, RoadVehAccelerationModelChanged),
	 SDT_CONDVAR(GameSettings, vehicle.train_slope_steepness,        SLE_UINT8,133, SL_MAX_VERSION, 0, 0,     3,     0,      10, 1, STR_CONFIG_SETTING_TRAIN_SLOPE_STEEPNESS,  TrainSlopeSteepnessChanged),
	 SDT_CONDVAR(GameSettings, vehicle.roadveh_slope_steepness,      SLE_UINT8,139, SL_MAX_VERSION, 0, 0,     7,     0,      10, 1, STR_CONFIG_SETTING_ROAD_VEHICLE_SLOPE_STEEPNESS,  RoadVehSlopeSteepnessChanged),
	    SDT_BOOL(GameSettings, pf.forbid_90_deg,                                                    0, 0, false,                    STR_CONFIG_SETTING_FORBID_90_DEG,          NULL),
	    SDT_BOOL(GameSettings, vehicle.mammoth_trains,                                              0,NN,  true,                    STR_CONFIG_SETTING_MAMMOTHTRAINS,          NULL),
	 SDT_CONDVAR(GameSettings, vehicle.smoke_amount,                 SLE_UINT8,145, SL_MAX_VERSION, 0,MS,     1,     0,       2, 0, STR_CONFIG_SETTING_SMOKE_AMOUNT,           NULL),
	    SDT_BOOL(GameSettings, order.gotodepot,                                                     0, 0,  true,                    STR_CONFIG_SETTING_GOTODEPOT,              NULL),
	    SDT_BOOL(GameSettings, pf.roadveh_queue,                                                    0, 0,  true,                    STR_CONFIG_SETTING_ROAD_VEHICLE_QUEUEING,  NULL),

	SDT_CONDBOOL(GameSettings, pf.new_pathfinding_all,                           0,             86, 0, 0, false,                    STR_NULL,                                  NULL),
	SDT_CONDBOOL(GameSettings, pf.yapf.ship_use_yapf,                           28,             86, 0, 0, false,                    STR_NULL,                                  NULL),
	SDT_CONDBOOL(GameSettings, pf.yapf.road_use_yapf,                           28,             86, 0, 0,  true,                    STR_NULL,                                  NULL),
	SDT_CONDBOOL(GameSettings, pf.yapf.rail_use_yapf,                           28,             86, 0, 0,  true,                    STR_NULL,                                  NULL),

	 SDT_CONDVAR(GameSettings, pf.pathfinder_for_trains,             SLE_UINT8, 87, SL_MAX_VERSION, 0, MS,    2,     1,       2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_TRAINS,  NULL),
	 SDT_CONDVAR(GameSettings, pf.pathfinder_for_roadvehs,           SLE_UINT8, 87, SL_MAX_VERSION, 0, MS,    2,     1,       2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_ROAD_VEHICLES, NULL),
	 SDT_CONDVAR(GameSettings, pf.pathfinder_for_ships,              SLE_UINT8, 87, SL_MAX_VERSION, 0, MS,    0,     0,       2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_SHIPS,   NULL),

	    SDT_BOOL(GameSettings, vehicle.never_expire_vehicles,                                       0,NN, false,                    STR_CONFIG_SETTING_NEVER_EXPIRE_VEHICLES,  NULL),
	     SDT_VAR(GameSettings, vehicle.max_trains,                  SLE_UINT16,                     0, 0,   500,     0,    5000, 0, STR_CONFIG_SETTING_MAX_TRAINS,             RedrawScreen),
	     SDT_VAR(GameSettings, vehicle.max_roadveh,                 SLE_UINT16,                     0, 0,   500,     0,    5000, 0, STR_CONFIG_SETTING_MAX_ROAD_VEHICLES,      RedrawScreen),
	     SDT_VAR(GameSettings, vehicle.max_aircraft,                SLE_UINT16,                     0, 0,   200,     0,    5000, 0, STR_CONFIG_SETTING_MAX_AIRCRAFT,           RedrawScreen),
	     SDT_VAR(GameSettings, vehicle.max_ships,                   SLE_UINT16,                     0, 0,   300,     0,    5000, 0, STR_CONFIG_SETTING_MAX_SHIPS,              RedrawScreen),
	SDTG_CONDBOOL(NULL,             0, NN, _old_vds.servint_ispercent, false,            STR_NULL, NULL, 0, 119),
	SDTG_CONDVAR(NULL,  SLE_UINT16, 0, D0, _old_vds.servint_trains,      150, 5, 800, 0, STR_NULL, NULL, 0, 119),
	SDTG_CONDVAR(NULL,  SLE_UINT16, 0, D0, _old_vds.servint_roadveh,     150, 5, 800, 0, STR_NULL, NULL, 0, 119),
	SDTG_CONDVAR(NULL,  SLE_UINT16, 0, D0, _old_vds.servint_ships,       360, 5, 800, 0, STR_NULL, NULL, 0, 119),
	SDTG_CONDVAR(NULL,  SLE_UINT16, 0, D0, _old_vds.servint_aircraft,    150, 5, 800, 0, STR_NULL, NULL, 0, 119),
	    SDT_BOOL(GameSettings, order.no_servicing_if_no_breakdowns,                                 0, 0,  true,                    STR_CONFIG_SETTING_NOSERVICE,              NULL),
	    SDT_BOOL(GameSettings, vehicle.wagon_speed_limits,                                          0,NN,  true,                    STR_CONFIG_SETTING_WAGONSPEEDLIMITS,       UpdateConsists),
	SDT_CONDBOOL(GameSettings, vehicle.disable_elrails,                         38, SL_MAX_VERSION, 0,NN, false,                    STR_CONFIG_SETTING_DISABLE_ELRAILS,        SettingsDisableElrail),
	 SDT_CONDVAR(GameSettings, vehicle.freight_trains,               SLE_UINT8, 39, SL_MAX_VERSION, 0,NN,     1,     1,     255, 1, STR_CONFIG_SETTING_FREIGHT_TRAINS,         NULL),
	SDT_CONDBOOL(GameSettings, order.timetabling,                               67, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_TIMETABLE_ALLOW,        NULL),
	 SDT_CONDVAR(GameSettings, vehicle.plane_speed,                  SLE_UINT8, 90, SL_MAX_VERSION, 0,NN,     4,     1,       4, 0, STR_CONFIG_SETTING_PLANE_SPEED,            NULL),
	SDT_CONDBOOL(GameSettings, vehicle.dynamic_engines,                         95, SL_MAX_VERSION, 0,NN,  true,                    STR_CONFIG_SETTING_DYNAMIC_ENGINES,        ChangeDynamicEngines),
	 SDT_CONDVAR(GameSettings, vehicle.plane_crashes,                SLE_UINT8,138, SL_MAX_VERSION, 0,MS,     2,     0,       2, 1, STR_CONFIG_SETTING_PLANE_CRASHES,          NULL),

	    SDT_BOOL(GameSettings, station.join_stations,                                               0, 0,  true,                    STR_CONFIG_SETTING_JOINSTATIONS,           NULL),
	SDTC_CONDBOOL(             gui.sg_full_load_any,                            22,             92, 0, 0,  true,                    STR_NULL,                                  NULL),
	    SDT_BOOL(GameSettings, order.improved_load,                                                 0,NN,  true,                    STR_CONFIG_SETTING_IMPROVEDLOAD,           NULL),
	    SDT_BOOL(GameSettings, order.selectgoods,                                                   0, 0,  true,                    STR_CONFIG_SETTING_SELECTGOODS,            NULL),
	SDTC_CONDBOOL(             gui.sg_new_nonstop,                              22,             92, 0, 0, false,                    STR_NULL,                                  NULL),
	    SDT_BOOL(GameSettings, station.nonuniform_stations,                                         0,NN,  true,                    STR_CONFIG_SETTING_NONUNIFORM_STATIONS,    NULL),
	     SDT_VAR(GameSettings, station.station_spread,               SLE_UINT8,                     0, 0,    12,     4,      64, 0, STR_CONFIG_SETTING_STATION_SPREAD,         InvalidateStationBuildWindow),
	    SDT_BOOL(GameSettings, order.serviceathelipad,                                              0, 0,  true,                    STR_CONFIG_SETTING_SERVICEATHELIPAD,       NULL),
	    SDT_BOOL(GameSettings, station.modified_catchment,                                          0, 0,  true,                    STR_CONFIG_SETTING_CATCHMENT,              StationCatchmentChanged),
	SDT_CONDBOOL(GameSettings, order.gradual_loading,                           40, SL_MAX_VERSION, 0,NN,  true,                    STR_CONFIG_SETTING_GRADUAL_LOADING,        NULL),
	SDT_CONDBOOL(GameSettings, construction.road_stop_on_town_road,             47, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_STOP_ON_TOWN_ROAD,      NULL),
	SDT_CONDBOOL(GameSettings, construction.road_stop_on_competitor_road,      114, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_STOP_ON_COMPETITOR_ROAD,NULL),
	SDT_CONDBOOL(GameSettings, station.adjacent_stations,                       62, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_ADJACENT_STATIONS,      NULL),
	SDT_CONDBOOL(GameSettings, economy.station_noise_level,                     96, SL_MAX_VERSION, 0,NN, false,                    STR_CONFIG_SETTING_NOISE_LEVEL,            InvalidateTownViewWindow),
	SDT_CONDBOOL(GameSettings, station.distant_join_stations,                  106, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_DISTANT_JOIN_STATIONS,  DeleteSelectStationWindow),

	    SDT_BOOL(GameSettings, economy.inflation,                                                   0, 0,  true,                    STR_CONFIG_SETTING_INFLATION,              NULL),
	     SDT_VAR(GameSettings, construction.raw_industry_construction, SLE_UINT8,                   0,MS,     0,     0,       2, 0, STR_CONFIG_SETTING_RAW_INDUSTRY_CONSTRUCTION_METHOD, InvalidateBuildIndustryWindow),
	 SDT_CONDVAR(GameSettings, construction.industry_platform,       SLE_UINT8,148, SL_MAX_VERSION, 0, 0,     1,     0,       4, 0, STR_CONFIG_SETTING_INDUSTRY_PLATFORM,      NULL),
	    SDT_BOOL(GameSettings, economy.multiple_industry_per_town,                                  0, 0, false,                    STR_CONFIG_SETTING_MULTIPINDTOWN,          NULL),
	SDT_CONDNULL(                                                            1,  0, 140),
	    SDT_BOOL(GameSettings, economy.bribe,                                                       0, 0,  true,                    STR_CONFIG_SETTING_BRIBE,                  NULL),
	SDT_CONDBOOL(GameSettings, economy.exclusive_rights,                        79, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_ALLOW_EXCLUSIVE,        NULL),
	SDT_CONDBOOL(GameSettings, economy.give_money,                              79, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_ALLOW_GIVE_MONEY,       NULL),
	     SDT_VAR(GameSettings, game_creation.snow_line_height,       SLE_UINT8,                     0, 0, DEF_SNOWLINE_HEIGHT, MIN_SNOWLINE_HEIGHT, MAX_SNOWLINE_HEIGHT, 0, STR_CONFIG_SETTING_SNOWLINE_HEIGHT, NULL),
	SDT_CONDNULL(                                                            4,  0, 143),
	     SDT_VAR(GameSettings, game_creation.starting_year,          SLE_INT32,                     0,NC,DEF_START_YEAR,MIN_YEAR,MAX_YEAR,1,STR_CONFIG_SETTING_STARTING_YEAR,  NULL),
	SDT_CONDNULL(                                                            4,  0, 104),
	    SDT_BOOL(GameSettings, economy.smooth_economy,                                              0, 0,  true,                    STR_CONFIG_SETTING_SMOOTH_ECONOMY,         InvalidateIndustryViewWindow),
	    SDT_BOOL(GameSettings, economy.allow_shares,                                                0, 0, false,                    STR_CONFIG_SETTING_ALLOW_SHARES,           NULL),
	 SDT_CONDVAR(GameSettings, economy.feeder_payment_share,         SLE_UINT8,134, SL_MAX_VERSION, 0, 0,    75,     0,     100, 0, STR_CONFIG_SETTING_FEEDER_PAYMENT_SHARE,   NULL),
	 SDT_CONDVAR(GameSettings, economy.town_growth_rate,             SLE_UINT8, 54, SL_MAX_VERSION, 0, MS,    2,     0,       4, 0, STR_CONFIG_SETTING_TOWN_GROWTH,            NULL),
	 SDT_CONDVAR(GameSettings, economy.larger_towns,                 SLE_UINT8, 54, SL_MAX_VERSION, 0, D0,    4,     0,     255, 1, STR_CONFIG_SETTING_LARGER_TOWNS,           NULL),
	 SDT_CONDVAR(GameSettings, economy.initial_city_size,            SLE_UINT8, 56, SL_MAX_VERSION, 0, 0,     2,     1,      10, 1, STR_CONFIG_SETTING_CITY_SIZE_MULTIPLIER,   NULL),
	SDT_CONDBOOL(GameSettings, economy.mod_road_rebuild,                        77, SL_MAX_VERSION, 0, 0,  true,                    STR_CONFIG_SETTING_MODIFIED_ROAD_REBUILD,  NULL),

	SDT_CONDNULL(1, 0, 106), // previously ai-new setting.
	    SDT_BOOL(GameSettings, ai.ai_in_multiplayer,                                                0, 0, true,                     STR_CONFIG_SETTING_AI_IN_MULTIPLAYER,      NULL),
	    SDT_BOOL(GameSettings, ai.ai_disable_veh_train,                                             0, 0, false,                    STR_CONFIG_SETTING_AI_BUILDS_TRAINS,       NULL),
	    SDT_BOOL(GameSettings, ai.ai_disable_veh_roadveh,                                           0, 0, false,                    STR_CONFIG_SETTING_AI_BUILDS_ROAD_VEHICLES,NULL),
	    SDT_BOOL(GameSettings, ai.ai_disable_veh_aircraft,                                          0, 0, false,                    STR_CONFIG_SETTING_AI_BUILDS_AIRCRAFT,     NULL),
	    SDT_BOOL(GameSettings, ai.ai_disable_veh_ship,                                              0, 0, false,                    STR_CONFIG_SETTING_AI_BUILDS_SHIPS,        NULL),
	 SDT_CONDVAR(GameSettings, ai.ai_max_opcode_till_suspend,       SLE_UINT32,107, SL_MAX_VERSION, 0, NG, 10000, 5000,250000,2500, STR_CONFIG_SETTING_AI_MAX_OPCODES,         NULL),

	     SDT_VAR(GameSettings, vehicle.extend_vehicle_life,          SLE_UINT8,                     0, 0,     0,     0,     100, 0, STR_NULL,                                  NULL),
	     SDT_VAR(GameSettings, economy.dist_local_authority,         SLE_UINT8,                     0, 0,    20,     5,      60, 0, STR_NULL,                                  NULL),
	     SDT_VAR(GameSettings, pf.wait_oneway_signal,                SLE_UINT8,                     0, 0,    15,     2,     255, 0, STR_NULL,                                  NULL),
	     SDT_VAR(GameSettings, pf.wait_twoway_signal,                SLE_UINT8,                     0, 0,    41,     2,     255, 0, STR_NULL,                                  NULL),
	 SDT_CONDVAR(GameSettings, economy.town_noise_population[0],    SLE_UINT16, 96, SL_MAX_VERSION, 0, 0,   800,   200,   65535, 0, STR_NULL,                                  NULL),
	 SDT_CONDVAR(GameSettings, economy.town_noise_population[1],    SLE_UINT16, 96, SL_MAX_VERSION, 0, 0,  2000,   400,   65535, 0, STR_NULL,                                  NULL),
	 SDT_CONDVAR(GameSettings, economy.town_noise_population[2],    SLE_UINT16, 96, SL_MAX_VERSION, 0, 0,  4000,   800,   65535, 0, STR_NULL,                                  NULL),

	 SDT_CONDVAR(GameSettings, pf.wait_for_pbs_path,                 SLE_UINT8,100, SL_MAX_VERSION, 0, 0,    30,     2,     255, 0, STR_NULL,                                  NULL),
	SDT_CONDBOOL(GameSettings, pf.reserve_paths,                               100, SL_MAX_VERSION, 0, 0, false,                    STR_NULL,                                  NULL),
	 SDT_CONDVAR(GameSettings, pf.path_backoff_interval,             SLE_UINT8,100, SL_MAX_VERSION, 0, 0,    20,     1,     255, 0, STR_NULL,                                  NULL),

	     SDT_VAR(GameSettings, pf.opf.pf_maxlength,                          SLE_UINT16,                     0, 0,  4096,                    64,   65535, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.opf.pf_maxdepth,                            SLE_UINT8,                     0, 0,    48,                     4,     255, 0, STR_NULL,         NULL),

	     SDT_VAR(GameSettings, pf.npf.npf_max_search_nodes,                    SLE_UINT,                     0, 0, 10000,                   500,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_firstred_penalty,               SLE_UINT,                     0, 0, ( 10 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_firstred_exit_penalty,          SLE_UINT,                     0, 0, (100 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_lastred_penalty,                SLE_UINT,                     0, 0, ( 10 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_station_penalty,                SLE_UINT,                     0, 0, (  1 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_slope_penalty,                  SLE_UINT,                     0, 0, (  1 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_curve_penalty,                  SLE_UINT,                     0, 0, (  1 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_rail_depot_reverse_penalty,          SLE_UINT,                     0, 0, ( 50 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.npf_rail_pbs_cross_penalty,              SLE_UINT,100, SL_MAX_VERSION, 0, 0, (  3 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.npf_rail_pbs_signal_back_penalty,        SLE_UINT,100, SL_MAX_VERSION, 0, 0, ( 15 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_buoy_penalty,                        SLE_UINT,                     0, 0, (  2 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_water_curve_penalty,                 SLE_UINT,                     0, 0, (  1 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_road_curve_penalty,                  SLE_UINT,                     0, 0, (  1 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	     SDT_VAR(GameSettings, pf.npf.npf_crossing_penalty,                    SLE_UINT,                     0, 0, (  3 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.npf_road_drive_through_penalty,          SLE_UINT, 47, SL_MAX_VERSION, 0, 0, (  8 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.npf_road_dt_occupied_penalty,            SLE_UINT,130, SL_MAX_VERSION, 0, 0, (  8 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.npf_road_bay_occupied_penalty,           SLE_UINT,130, SL_MAX_VERSION, 0, 0, ( 15 * NPF_TILE_LENGTH),   0,  100000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.npf.maximum_go_to_depot_penalty,             SLE_UINT,131, SL_MAX_VERSION, 0, 0, ( 20 * NPF_TILE_LENGTH),   0, 1000000, 0, STR_NULL,         NULL),

	SDT_CONDBOOL(GameSettings, pf.yapf.disable_node_optimization,                        28, SL_MAX_VERSION, 0, 0, false,                                    STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.max_search_nodes,                       SLE_UINT, 28, SL_MAX_VERSION, 0, 0, 10000,                   500, 1000000, 0, STR_NULL,         NULL),
	SDT_CONDBOOL(GameSettings, pf.yapf.rail_firstred_twoway_eol,                         28, SL_MAX_VERSION, 0, 0, false,                                    STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_firstred_penalty,                  SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_firstred_exit_penalty,             SLE_UINT, 28, SL_MAX_VERSION, 0, 0,   100 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_lastred_penalty,                   SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_lastred_exit_penalty,              SLE_UINT, 28, SL_MAX_VERSION, 0, 0,   100 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_station_penalty,                   SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_slope_penalty,                     SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     2 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_curve45_penalty,                   SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     1 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_curve90_penalty,                   SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     6 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_depot_reverse_penalty,             SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    50 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_crossing_penalty,                  SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     3 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_look_ahead_max_signals,            SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10,                     1,     100, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_look_ahead_signal_p0,               SLE_INT, 28, SL_MAX_VERSION, 0, 0,   500,              -1000000, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_look_ahead_signal_p1,               SLE_INT, 28, SL_MAX_VERSION, 0, 0,  -100,              -1000000, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_look_ahead_signal_p2,               SLE_INT, 28, SL_MAX_VERSION, 0, 0,     5,              -1000000, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_pbs_cross_penalty,                 SLE_UINT,100, SL_MAX_VERSION, 0, 0,     3 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_pbs_station_penalty,               SLE_UINT,100, SL_MAX_VERSION, 0, 0,     8 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_pbs_signal_back_penalty,           SLE_UINT,100, SL_MAX_VERSION, 0, 0,    15 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_doubleslip_penalty,                SLE_UINT,100, SL_MAX_VERSION, 0, 0,     1 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_longer_platform_penalty,           SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     8 * YAPF_TILE_LENGTH,  0,   20000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_longer_platform_per_tile_penalty,  SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     0 * YAPF_TILE_LENGTH,  0,   20000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_shorter_platform_penalty,          SLE_UINT, 33, SL_MAX_VERSION, 0, 0,    40 * YAPF_TILE_LENGTH,  0,   20000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.rail_shorter_platform_per_tile_penalty, SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     0 * YAPF_TILE_LENGTH,  0,   20000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_slope_penalty,                     SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     2 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_curve_penalty,                     SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     1 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_crossing_penalty,                  SLE_UINT, 33, SL_MAX_VERSION, 0, 0,     3 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_stop_penalty,                      SLE_UINT, 47, SL_MAX_VERSION, 0, 0,     8 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_stop_occupied_penalty,             SLE_UINT,130, SL_MAX_VERSION, 0, 0,     8 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.road_stop_bay_occupied_penalty,         SLE_UINT,130, SL_MAX_VERSION, 0, 0,    15 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),
	 SDT_CONDVAR(GameSettings, pf.yapf.maximum_go_to_depot_penalty,            SLE_UINT,131, SL_MAX_VERSION, 0, 0,    20 * YAPF_TILE_LENGTH,  0, 1000000, 0, STR_NULL,         NULL),

	 SDT_CONDVAR(GameSettings, game_creation.land_generator,                  SLE_UINT8, 30, SL_MAX_VERSION, 0,MS,     1,                     0,       1, 0, STR_CONFIG_SETTING_LAND_GENERATOR,        NULL),
	 SDT_CONDVAR(GameSettings, game_creation.oil_refinery_limit,              SLE_UINT8, 30, SL_MAX_VERSION, 0, 0,    32,                    12,      48, 0, STR_CONFIG_SETTING_OIL_REF_EDGE_DISTANCE, NULL),
	 SDT_CONDVAR(GameSettings, game_creation.tgen_smoothness,                 SLE_UINT8, 30, SL_MAX_VERSION, 0,MS,     1,                     0,       3, 0, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN,  NULL),
	     SDT_VAR(GameSettings, game_creation.variety,                         SLE_UINT8,                     S, 0,     0,                     0,       5, 0, STR_NULL,                                 NULL),
	 SDT_CONDVAR(GameSettings, game_creation.generation_seed,                SLE_UINT32, 30, SL_MAX_VERSION, 0, 0,      GENERATE_NEW_SEED, 0, UINT32_MAX, 0, STR_NULL,                                 NULL),
	 SDT_CONDVAR(GameSettings, game_creation.tree_placer,                     SLE_UINT8, 30, SL_MAX_VERSION, 0,MS,     2,                     0,       2, 0, STR_CONFIG_SETTING_TREE_PLACER,           NULL),
	     SDT_VAR(GameSettings, game_creation.heightmap_rotation,              SLE_UINT8,                     S,MS,     0,                     0,       1, 0, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION,    NULL),
	     SDT_VAR(GameSettings, game_creation.se_flat_world_height,            SLE_UINT8,                     S, 0,     1,                     0,      15, 0, STR_CONFIG_SETTING_SE_FLAT_WORLD_HEIGHT,  NULL),

	     SDT_VAR(GameSettings, game_creation.map_x,                           SLE_UINT8,                     S, 0,     8, MIN_MAP_SIZE_BITS, MAX_MAP_SIZE_BITS, 0, STR_NULL,                           NULL),
	     SDT_VAR(GameSettings, game_creation.map_y,                           SLE_UINT8,                     S, 0,     8, MIN_MAP_SIZE_BITS, MAX_MAP_SIZE_BITS, 0, STR_NULL,                           NULL),
	SDT_CONDBOOL(GameSettings, construction.freeform_edges,                             111, SL_MAX_VERSION, 0, 0,  true,                                    STR_CONFIG_SETTING_ENABLE_FREEFORM_EDGES, CheckFreeformEdges),
	 SDT_CONDVAR(GameSettings, game_creation.water_borders,                   SLE_UINT8,111, SL_MAX_VERSION, 0, 0,    15,                     0,      16, 0, STR_NULL,                                 NULL),
	 SDT_CONDVAR(GameSettings, game_creation.custom_town_number,             SLE_UINT16,115, SL_MAX_VERSION, 0, 0,     1,                     1,    5000, 0, STR_NULL,                                 NULL),
	 SDT_CONDVAR(GameSettings, construction.extra_tree_placement,             SLE_UINT8,132, SL_MAX_VERSION, 0,MS,     2,                     0,       2, 0, STR_CONFIG_SETTING_EXTRA_TREE_PLACEMENT,  NULL),
	 SDT_CONDVAR(GameSettings, game_creation.custom_sea_level,                SLE_UINT8,149, SL_MAX_VERSION, 0, 0,     1,                     2,      90, 0, STR_NULL,                                 NULL),

 SDT_CONDOMANY(GameSettings, locale.currency,                               SLE_UINT8, 97, SL_MAX_VERSION, N, 0, 0, CUSTOM_CURRENCY_ID, _locale_currencies, STR_NULL, RedrawScreen, NULL),
 SDT_CONDOMANY(GameSettings, locale.units,                                  SLE_UINT8, 97, SL_MAX_VERSION, N, 0, 1, 2, _locale_units,                       STR_NULL, RedrawScreen, NULL),
   SDT_CONDSTR(GameSettings, locale.digit_group_separator,                   SLE_STRQ,118, SL_MAX_VERSION, N, 0, NULL,                                      STR_NULL, RedrawScreen),
   SDT_CONDSTR(GameSettings, locale.digit_group_separator_currency,          SLE_STRQ,118, SL_MAX_VERSION, N, 0, NULL,                                      STR_NULL, RedrawScreen),
   SDT_CONDSTR(GameSettings, locale.digit_decimal_separator,                 SLE_STRQ,126, SL_MAX_VERSION, N, 0, NULL,                                      STR_NULL, RedrawScreen),

	/***************************************************************************/
	/* Unsaved setting variables. */
	SDTC_OMANY(gui.autosave,                  SLE_UINT8, S,  0, 1, 4, _autosave_interval,     STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.threaded_saves,                       S,  0,  true,                        STR_NULL,                                       NULL),
	SDTC_OMANY(gui.date_format_in_default_names,SLE_UINT8,S,MS, 0, 2, _savegame_date,         STR_CONFIG_SETTING_DATE_FORMAT_IN_SAVE_NAMES,   NULL),
	 SDTC_BOOL(gui.vehicle_speed,                        S,  0,  true,                        STR_CONFIG_SETTING_VEHICLESPEED,                NULL),
	 SDTC_BOOL(gui.status_long_date,                     S,  0,  true,                        STR_CONFIG_SETTING_LONGDATE,                    NULL),
	 SDTC_BOOL(gui.show_finances,                        S,  0,  true,                        STR_CONFIG_SETTING_SHOWFINANCES,                NULL),
	 SDTC_BOOL(gui.autoscroll,                           S,  0, false,                        STR_CONFIG_SETTING_AUTOSCROLL,                  NULL),
	 SDTC_BOOL(gui.reverse_scroll,                       S,  0, false,                        STR_CONFIG_SETTING_REVERSE_SCROLLING,           NULL),
	 SDTC_BOOL(gui.smooth_scroll,                        S,  0, false,                        STR_CONFIG_SETTING_SMOOTH_SCROLLING,            NULL),
	 SDTC_BOOL(gui.left_mouse_btn_scrolling,             S,  0, false,                        STR_CONFIG_SETTING_LEFT_MOUSE_BTN_SCROLLING,    NULL),
	 SDTC_BOOL(gui.measure_tooltip,                      S,  0,  true,                        STR_CONFIG_SETTING_MEASURE_TOOLTIP,             NULL),
	  SDTC_VAR(gui.errmsg_duration,           SLE_UINT8, S,  0,     5,        0,       20, 0, STR_CONFIG_SETTING_ERRMSG_DURATION,             NULL),
	  SDTC_VAR(gui.hover_delay,               SLE_UINT8, S, D0,     2,        1,        5, 0, STR_CONFIG_SETTING_HOVER_DELAY,                 NULL),
	  SDTC_VAR(gui.toolbar_pos,               SLE_UINT8, S, MS,     1,        0,        2, 0, STR_CONFIG_SETTING_TOOLBAR_POS,                 v_PositionMainToolbar),
	  SDTC_VAR(gui.statusbar_pos,             SLE_UINT8, S, MS,     1,        0,        2, 0, STR_CONFIG_SETTING_STATUSBAR_POS,               v_PositionStatusbar),
	  SDTC_VAR(gui.window_snap_radius,        SLE_UINT8, S, D0,    10,        1,       32, 0, STR_CONFIG_SETTING_SNAP_RADIUS,                 NULL),
	  SDTC_VAR(gui.window_soft_limit,         SLE_UINT8, S, D0,    20,        5,      255, 1, STR_CONFIG_SETTING_SOFT_LIMIT,                  NULL),
	 SDTC_BOOL(gui.population_in_label,                  S,  0,  true,                        STR_CONFIG_SETTING_POPULATION_IN_LABEL,         PopulationInLabelActive),
	 SDTC_BOOL(gui.link_terraform_toolbar,               S,  0, false,                        STR_CONFIG_SETTING_LINK_TERRAFORM_TOOLBAR,      NULL),
	  SDTC_VAR(gui.smallmap_land_colour,      SLE_UINT8, S, MS,     0,        0,        2, 0, STR_CONFIG_SETTING_SMALLMAP_LAND_COLOUR,        RedrawSmallmap),
	  SDTC_VAR(gui.liveries,                  SLE_UINT8, S, MS,     2,        0,        2, 0, STR_CONFIG_SETTING_LIVERIES,                    InvalidateCompanyLiveryWindow),
	 SDTC_BOOL(gui.prefer_teamchat,                      S,  0, false,                        STR_CONFIG_SETTING_PREFER_TEAMCHAT,             NULL),
	  SDTC_VAR(gui.scrollwheel_scrolling,     SLE_UINT8, S, MS,     0,        0,        2, 0, STR_CONFIG_SETTING_SCROLLWHEEL_SCROLLING,       NULL),
	  SDTC_VAR(gui.scrollwheel_multiplier,    SLE_UINT8, S,  0,     5,        1,       15, 1, STR_CONFIG_SETTING_SCROLLWHEEL_MULTIPLIER,      NULL),
	 SDTC_BOOL(gui.pause_on_newgame,                     S,  0, false,                        STR_CONFIG_SETTING_PAUSE_ON_NEW_GAME,           NULL),
	  SDTC_VAR(gui.advanced_vehicle_list,     SLE_UINT8, S, MS,     1,        0,        2, 0, STR_CONFIG_SETTING_ADVANCED_VEHICLE_LISTS,      NULL),
	 SDTC_BOOL(gui.timetable_in_ticks,                   S,  0, false,                        STR_CONFIG_SETTING_TIMETABLE_IN_TICKS,          InvalidateVehTimetableWindow),
	 SDTC_BOOL(gui.timetable_arrival_departure,          S,  0,  true,                        STR_CONFIG_SETTING_TIMETABLE_SHOW_ARRIVAL_DEPARTURE, InvalidateVehTimetableWindow),
	 SDTC_BOOL(gui.quick_goto,                           S,  0, false,                        STR_CONFIG_SETTING_QUICKGOTO,                   NULL),
	  SDTC_VAR(gui.loading_indicators,        SLE_UINT8, S, MS,     1,        0,        2, 0, STR_CONFIG_SETTING_LOADING_INDICATORS,          RedrawScreen),
	  SDTC_VAR(gui.default_rail_type,         SLE_UINT8, S, MS,     0,        0,        2, 0, STR_CONFIG_SETTING_DEFAULT_RAIL_TYPE,           NULL),
	 SDTC_BOOL(gui.enable_signal_gui,                    S,  0,  true,                        STR_CONFIG_SETTING_ENABLE_SIGNAL_GUI,           CloseSignalGUI),
	  SDTC_VAR(gui.coloured_news_year,        SLE_INT32, S, NC,  2000, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_COLOURED_NEWS_YEAR,          NULL),
	  SDTC_VAR(gui.drag_signals_density,      SLE_UINT8, S,  0,     4,        1,       20, 0, STR_CONFIG_SETTING_DRAG_SIGNALS_DENSITY,        DragSignalsDensityChanged),
	  SDTC_VAR(gui.semaphore_build_before,    SLE_INT32, S, NC,  1950, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_SEMAPHORE_BUILD_BEFORE_DATE, ResetSignalVariant),
	 SDTC_BOOL(gui.vehicle_income_warn,                  S,  0,  true,                        STR_CONFIG_SETTING_WARN_INCOME_LESS,            NULL),
	  SDTC_VAR(gui.order_review_system,       SLE_UINT8, S, MS,     2,        0,        2, 0, STR_CONFIG_SETTING_ORDER_REVIEW,                NULL),
	 SDTC_BOOL(gui.lost_vehicle_warn,                    S,  0,  true,                        STR_CONFIG_SETTING_WARN_LOST_VEHICLE,           NULL),
	 SDTC_BOOL(gui.always_build_infrastructure,          S,  0, false,                        STR_CONFIG_SETTING_ALWAYS_BUILD_INFRASTRUCTURE, RedrawScreen),
	 SDTC_BOOL(gui.new_nonstop,                          S,  0, false,                        STR_CONFIG_SETTING_NONSTOP_BY_DEFAULT,          NULL),
	  SDTC_VAR(gui.stop_location,             SLE_UINT8, S, MS,     2,        0,        2, 1, STR_CONFIG_SETTING_STOP_LOCATION,               NULL),
	 SDTC_BOOL(gui.keep_all_autosave,                    S,  0, false,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.autosave_on_exit,                     S,  0, false,                        STR_NULL,                                       NULL),
	  SDTC_VAR(gui.max_num_autosaves,         SLE_UINT8, S,  0,    16,        0,      255, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.bridge_pillars,                       S,  0,  true,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.auto_euro,                            S,  0,  true,                        STR_NULL,                                       NULL),
	  SDTC_VAR(gui.news_message_timeout,      SLE_UINT8, S,  0,     2,        1,      255, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.show_track_reservation,               S,  0, false,                        STR_CONFIG_SETTING_SHOW_TRACK_RESERVATION,      RedrawScreen),
	  SDTC_VAR(gui.default_signal_type,       SLE_UINT8, S, MS,     1,        0,        2, 1, STR_CONFIG_SETTING_DEFAULT_SIGNAL_TYPE,         NULL),
	  SDTC_VAR(gui.cycle_signal_types,        SLE_UINT8, S, MS,     2,        0,        2, 1, STR_CONFIG_SETTING_CYCLE_SIGNAL_TYPES,          NULL),
	  SDTC_VAR(gui.station_numtracks,         SLE_UINT8, S,  0,     1,        1,        7, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(gui.station_platlength,        SLE_UINT8, S,  0,     5,        1,        7, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.station_dragdrop,                     S,  0,  true,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.station_show_coverage,                S,  0, false,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.persistent_buildingtools,             S,  0,  true,                        STR_CONFIG_SETTING_PERSISTENT_BUILDINGTOOLS,    NULL),
	 SDTC_BOOL(gui.expenses_layout,                      S,  0, false,                        STR_CONFIG_SETTING_EXPENSES_LAYOUT,             RedrawScreen),

/* For the dedicated build we'll enable dates in logs by default. */
#ifdef DEDICATED
	 SDTC_BOOL(gui.show_date_in_logs,                    S,  0,  true,                        STR_NULL,                                       NULL),
#else
	 SDTC_BOOL(gui.show_date_in_logs,                    S,  0, false,                        STR_NULL,                                       NULL),
#endif
	  SDTC_VAR(gui.developer,                 SLE_UINT8, S,  0,     1,        0,        2, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.newgrf_developer_tools,               S,  0, false,                        STR_NULL,                                       InvalidateNewGRFChangeWindows),
	 SDTC_BOOL(gui.ai_developer_tools,                   S,  0, false,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(gui.scenario_developer,                   S,  0, false,                        STR_NULL,                                       InvalidateNewGRFChangeWindows),
	 SDTC_BOOL(gui.newgrf_show_old_versions,             S,  0, false,                        STR_NULL,                                       NULL),
	  SDTC_VAR(gui.console_backlog_timeout,  SLE_UINT16, S,  0,   100,       10,    65500, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(gui.console_backlog_length,   SLE_UINT16, S,  0,   100,       10,    65500, 0, STR_NULL,                                       NULL),
#ifdef ENABLE_NETWORK
	  SDTC_VAR(gui.network_chat_box_width,   SLE_UINT16, S,  0,   620,      200,    65535, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(gui.network_chat_box_height,   SLE_UINT8, S,  0,    25,        5,      255, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(gui.network_chat_timeout,     SLE_UINT16, S,  0,    20,        1,    65535, 0, STR_NULL,                                       NULL),

	  SDTC_VAR(network.sync_freq,            SLE_UINT16,C|S,NO,   100,        0,      100, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.frame_freq,            SLE_UINT8,C|S,NO,     0,        0,      100, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.commands_per_frame,   SLE_UINT16, S, NO,     2,        1,    65535, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.max_commands_in_queue,SLE_UINT16, S, NO,    16,        1,    65535, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.bytes_per_frame,      SLE_UINT16, S, NO,     8,        1,    65535, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.bytes_per_frame_burst,SLE_UINT16, S, NO,   256,        1,    65535, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.max_join_time,        SLE_UINT16, S, NO,   500,        0,    32000, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(network.pause_on_join,                    S, NO,  true,                        STR_NULL,                                       NULL),
	  SDTC_VAR(network.server_port,          SLE_UINT16, S, NO,NETWORK_DEFAULT_PORT,0,65535,0,STR_NULL,                                       NULL),
	  SDTC_VAR(network.server_admin_port,    SLE_UINT16, S, NO,  NETWORK_ADMIN_PORT,0,65535,0,STR_NULL,                                       NULL),
	 SDTC_BOOL(network.server_admin_chat,                S, NO,  true,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(network.server_advertise,                 S, NO, false,                        STR_NULL,                                       NULL),
	  SDTC_VAR(network.lan_internet,          SLE_UINT8, S, NO,     0,        0,        1, 0, STR_NULL,                                       NULL),
	  SDTC_STR(network.client_name,            SLE_STRB, S,  0,  NULL,                        STR_NULL,                                       UpdateClientName),
	  SDTC_STR(network.server_password,        SLE_STRB, S, NO,  NULL,                        STR_NULL,                                       UpdateServerPassword),
	  SDTC_STR(network.rcon_password,          SLE_STRB, S, NO,  NULL,                        STR_NULL,                                       UpdateRconPassword),
	  SDTC_STR(network.admin_password,         SLE_STRB, S, NO,  NULL,                        STR_NULL,                                       NULL),
	  SDTC_STR(network.default_company_pass,   SLE_STRB, S,  0,  NULL,                        STR_NULL,                                       NULL),
	  SDTC_STR(network.server_name,            SLE_STRB, S, NO,  NULL,                        STR_NULL,                                       NULL),
	  SDTC_STR(network.connect_to_ip,          SLE_STRB, S,  0,  NULL,                        STR_NULL,                                       NULL),
	  SDTC_STR(network.network_id,             SLE_STRB, S, NO,  NULL,                        STR_NULL,                                       NULL),
	 SDTC_BOOL(network.autoclean_companies,              S, NO, false,                        STR_NULL,                                       NULL),
	  SDTC_VAR(network.autoclean_unprotected, SLE_UINT8, S,D0|NO,  12,     0,         240, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.autoclean_protected,   SLE_UINT8, S,D0|NO,  36,     0,         240, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.autoclean_novehicles,  SLE_UINT8, S,D0|NO,   0,     0,         240, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.max_companies,         SLE_UINT8, S, NO,    15,     1,MAX_COMPANIES,0, STR_NULL,                                       UpdateClientConfigValues),
	  SDTC_VAR(network.max_clients,           SLE_UINT8, S, NO,    25,     2, MAX_CLIENTS, 0, STR_NULL,                                       NULL),
	  SDTC_VAR(network.max_spectators,        SLE_UINT8, S, NO,    15,     0, MAX_CLIENTS, 0, STR_NULL,                                       UpdateClientConfigValues),
	  SDTC_VAR(network.restart_game_year,     SLE_INT32, S,D0|NO|NC,0, MIN_YEAR, MAX_YEAR, 1, STR_NULL,                                       NULL),
	  SDTC_VAR(network.min_active_clients,    SLE_UINT8, S, NO,     0,     0, MAX_CLIENTS, 0, STR_NULL,                                       NULL),
	SDTC_OMANY(network.server_lang,           SLE_UINT8, S, NO,     0,    35, _server_langs,  STR_NULL,                                       NULL),
	 SDTC_BOOL(network.reload_cfg,                       S, NO, false,                        STR_NULL,                                       NULL),
	  SDTC_STR(network.last_host,              SLE_STRB, S,  0,    "",                        STR_NULL,                                       NULL),
	  SDTC_VAR(network.last_port,            SLE_UINT16, S,  0,     0,     0,  UINT16_MAX, 0, STR_NULL,                                       NULL),
	 SDTC_BOOL(network.no_http_content_downloads,        S,  0, false,                        STR_NULL,                                       NULL),
#endif /* ENABLE_NETWORK */

	/*
	 * Since the network code (CmdChangeSetting and friends) use the index in this array to decide
	 * which setting the server is talking about all conditional compilation of this array must be at the
	 * end. This isn't really the best solution, the settings the server can tell the client about should
	 * either use a seperate array or some other form of identifier.
	 */

#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	 SDTC_VAR(gui.right_mouse_btn_emulation, SLE_UINT8, S, MS, 0, 0, 2, 0, STR_CONFIG_SETTING_RIGHT_MOUSE_BTN_EMU, NULL),
#endif

	SDT_END()
};

static const SettingDesc _company_settings[] = {
	SDT_BOOL(CompanySettings, engine_renew,                          0, PC,     false,                  STR_CONFIG_SETTING_AUTORENEW_VEHICLE,     NULL),
	 SDT_VAR(CompanySettings, engine_renew_months,        SLE_INT16, 0, PC,         6, -12,      12, 0, STR_CONFIG_SETTING_AUTORENEW_MONTHS,      NULL),
	 SDT_VAR(CompanySettings, engine_renew_money,          SLE_UINT, 0, PC|CR, 100000,   0, 2000000, 0, STR_CONFIG_SETTING_AUTORENEW_MONEY,       NULL),
	SDT_BOOL(CompanySettings, renew_keep_length,                     0, PC,     false,                  STR_NULL,                                 NULL),
	SDT_BOOL(CompanySettings, vehicle.servint_ispercent,             0, PC,     false,                  STR_CONFIG_SETTING_SERVINT_ISPERCENT,     CheckInterval),
	 SDT_VAR(CompanySettings, vehicle.servint_trains,    SLE_UINT16, 0, PC|D0,    150,   5,     800, 0, STR_CONFIG_SETTING_SERVINT_TRAINS,        InvalidateDetailsWindow),
	 SDT_VAR(CompanySettings, vehicle.servint_roadveh,   SLE_UINT16, 0, PC|D0,    150,   5,     800, 0, STR_CONFIG_SETTING_SERVINT_ROAD_VEHICLES, InvalidateDetailsWindow),
	 SDT_VAR(CompanySettings, vehicle.servint_ships,     SLE_UINT16, 0, PC|D0,    360,   5,     800, 0, STR_CONFIG_SETTING_SERVINT_SHIPS,         InvalidateDetailsWindow),
	 SDT_VAR(CompanySettings, vehicle.servint_aircraft,  SLE_UINT16, 0, PC|D0,    100,   5,     800, 0, STR_CONFIG_SETTING_SERVINT_AIRCRAFT,      InvalidateDetailsWindow),
	SDT_END()
};

static const SettingDesc _currency_settings[] = {
	SDT_VAR(CurrencySpec, rate,    SLE_UINT16, S, 0, 1,      0, UINT16_MAX, 0, STR_NULL, NULL),
	SDT_CHR(CurrencySpec, separator,           S, 0, ".",                      STR_NULL, NULL),
	SDT_VAR(CurrencySpec, to_euro,  SLE_INT32, S, 0, 0, MIN_YEAR, MAX_YEAR, 0, STR_NULL, NULL),
	SDT_STR(CurrencySpec, prefix,   SLE_STRBQ, S, 0, NULL,                     STR_NULL, NULL),
	SDT_STR(CurrencySpec, suffix,   SLE_STRBQ, S, 0, " credits",               STR_NULL, NULL),
	SDT_END()
};

/* Undefine for the shortcut macros above */
#undef S
#undef C
#undef N

#undef D0
#undef NC
#undef MS
#undef NO
#undef CR
