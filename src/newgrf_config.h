/* $Id$ */

/** @file newgrf_config.h */

#ifndef NEWGRF_CONFIG_H
#define NEWGRF_CONFIG_H

#include "openttd.h"

/* GRF config bit flags */
enum GCF_Flags {
	GCF_SYSTEM,    ///< GRF file is an openttd-internal system grf
	GCF_UNSAFE,    ///< GRF file is unsafe for static usage
	GCF_STATIC,    ///< GRF file is used statically (can be used in any MP game)
	GCF_COMPATIBLE,///< GRF file does not exactly match the requested GRF (different MD5SUM), but grfid matches)
	GCF_COPY,      ///< The data is copied from a grf in _all_grfs
	GCF_INIT_ONLY, ///< GRF file is processed up to GLS_INIT
};

enum GRFStatus {
	GCS_UNKNOWN,      ///< The status of this grf file is unknown
	GCS_DISABLED,     ///< GRF file is disabled
	GCS_NOT_FOUND,    ///< GRF file was not found in the local cache
	GCS_INITIALISED,  ///< GRF file has been initialised
	GCS_ACTIVATED     ///< GRF file has been activated
};

enum GRFListCompatibility{
	GLC_ALL_GOOD,
	GLC_COMPATIBLE,
	GLC_NOT_FOUND
};

struct GRFIdentifier {
	uint32 grfid;
	uint8 md5sum[16];
};

struct GRFError {
	char *custom_message;
	char *data;
	StringID message;
	StringID severity;
	uint8 num_params;
	uint8 param_number[2];
};

struct GRFConfig : public GRFIdentifier {
	char *filename;
	char *name;
	char *info;
	GRFError *error;

	uint8 flags;
	GRFStatus status;
	uint32 param[0x80];
	uint8 num_params;

	struct GRFConfig *next;
};

/* First item in list of all scanned NewGRFs */
extern GRFConfig *_all_grfs;

/* First item in list of current GRF set up */
extern GRFConfig *_grfconfig;

/* First item in list of default GRF set up */
extern GRFConfig *_grfconfig_newgame;

/* First item in list of static GRF set up */
extern GRFConfig *_grfconfig_static;

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
