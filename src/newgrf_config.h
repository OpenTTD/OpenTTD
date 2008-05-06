/* $Id$ */

/** @file newgrf_config.h Functions to find and configure NewGRFs. */

#ifndef NEWGRF_CONFIG_H
#define NEWGRF_CONFIG_H

#include "strings_type.h"

/** GRF config bit flags */
enum GCF_Flags {
	GCF_SYSTEM,     ///< GRF file is an openttd-internal system grf
	GCF_UNSAFE,     ///< GRF file is unsafe for static usage
	GCF_STATIC,     ///< GRF file is used statically (can be used in any MP game)
	GCF_COMPATIBLE, ///< GRF file does not exactly match the requested GRF (different MD5SUM), but grfid matches)
	GCF_COPY,       ///< The data is copied from a grf in _all_grfs
	GCF_INIT_ONLY,  ///< GRF file is processed up to GLS_INIT
	GCF_RESERVED,   ///< GRF file passed GLS_RESERVE stage

};

/** Status of GRF */
enum GRFStatus {
	GCS_UNKNOWN,      ///< The status of this grf file is unknown
	GCS_DISABLED,     ///< GRF file is disabled
	GCS_NOT_FOUND,    ///< GRF file was not found in the local cache
	GCS_INITIALISED,  ///< GRF file has been initialised
	GCS_ACTIVATED     ///< GRF file has been activated
};

/** Status of post-gameload GRF compatibility check */
enum GRFListCompatibility{
	GLC_ALL_GOOD,   ///< All GRF needed by game are present
	GLC_COMPATIBLE, ///< Compatible (eg. the same ID, but different chacksum) GRF found in at least one case
	GLC_NOT_FOUND   ///< At least one GRF couldn't be found (higher priority than GLC_COMPATIBLE)
};

/** Basic data to distinguish a GRF. Used in the server list window */
struct GRFIdentifier {
	uint32 grfid;     ///< GRF ID (defined by Action 0x08)
	uint8 md5sum[16]; ///< MD5 checksum of file to distinguish files with the same GRF ID (eg. newer version of GRF)
};

/** Information about why GRF had problems during initialisation */
struct GRFError {
	char *custom_message;  ///< Custom message (if present)
	char *data;            ///< Additional data for message and custom_message
	StringID message;      ///< Default message
	StringID severity;     ///< Info / Warning / Error / Fatal
	uint8 num_params;      ///< Number of additinal parameters for custom_message (0, 1 or 2)
	uint8 param_number[2]; ///< GRF parameters to show for custom_message
};

/** Information about GRF, used in the game and (part of it) in savegames */
struct GRFConfig : public GRFIdentifier {
	char *filename;     ///< Filename - either with or without full path
	char *name;         ///< NOSAVE: GRF name (Action 0x08)
	char *info;         ///< NOSAVE: GRF info (author, copyright, ...) (Action 0x08)
	GRFError *error;    ///< NOSAVE: Error/Warning during GRF loading (Action 0x0B)

	uint8 flags;        ///< NOSAVE: GCF_Flags, bitset
	GRFStatus status;   ///< NOSAVE: GRFStatus, enum
	uint32 param[0x80]; ///< GRF parameters
	uint8 num_params;   ///< Number of used parameters

	struct GRFConfig *next; ///< NOSAVE: Next item in the linked list

	bool IsOpenTTDBaseGRF() const;
};

extern GRFConfig *_all_grfs;          ///< First item in list of all scanned NewGRFs
extern GRFConfig *_grfconfig;         ///< First item in list of current GRF set up
extern GRFConfig *_grfconfig_newgame; ///< First item in list of default GRF set up
extern GRFConfig *_grfconfig_static;  ///< First item in list of static GRF set up

void ScanNewGRFFiles();
const GRFConfig *FindGRFConfig(uint32 grfid, const uint8 *md5sum = NULL);
GRFConfig *GetGRFConfig(uint32 grfid);
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src, bool init_only);
void AppendStaticGRFConfigs(GRFConfig **dst);
void AppendToGRFConfigList(GRFConfig **dst, GRFConfig *el);
void ClearGRFConfig(GRFConfig **config);
void ClearGRFConfigList(GRFConfig **config);
void ResetGRFConfig(bool defaults);
GRFListCompatibility IsGoodGRFConfigList();
bool FillGRFDetails(GRFConfig *config, bool is_static);
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last);

/* In newgrf_gui.cpp */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config);

#ifdef ENABLE_NETWORK
/* For communication about GRFs over the network */
#define UNKNOWN_GRF_NAME_PLACEHOLDER "<Unknown>"
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create);
#endif /* ENABLE_NETWORK */

#endif /* NEWGRF_CONFIG_H */
