/* $Id$ */

#ifndef NEWGRF_CONFIG_H
#define NEWGRF_CONFIG_H

/* GRF config bit flags */
typedef enum {
	GCF_DISABLED,  ///< GRF file is disabled
	GCF_NOT_FOUND, ///< GRF file was not found in the local cache
	GCF_ACTIVATED, ///< GRF file is active
	GCF_SYSTEM,    ///< GRF file is an openttd-internal system grf
	GCF_UNSAFE,    ///< GRF file is unsafe for static usage
	GCF_STATIC,    ///< GRF file is used statically (can be used in any MP game)
	GCF_COMPATIBLE,///< GRF file does not exactly match the requested GRF (different MD5SUM), but grfid matches)
	GCF_COPY,      ///< The data is copied from a grf in _all_grfs
} GCF_Flags;

typedef struct GRFConfig {
	char *filename;
	char *name;
	char *info;
	uint32 grfid;

	uint8 flags;
	uint8 md5sum[16];
	uint32 param[0x80];
	uint8 num_params;

	struct GRFConfig *next;
} GRFConfig;

/* First item in list of all scanned NewGRFs */
extern GRFConfig *_all_grfs;

/* First item in list of current GRF set up */
extern GRFConfig *_grfconfig;

/* First item in list of default GRF set up */
extern GRFConfig *_grfconfig_newgame;

/* First item in list of static GRF set up */
extern GRFConfig *_grfconfig_static;

void ScanNewGRFFiles(void);
const GRFConfig *FindGRFConfig(uint32 grfid, const uint8 *md5sum = NULL);
GRFConfig *GetGRFConfig(uint32 grfid);
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src);
void AppendStaticGRFConfigs(GRFConfig **dst);
void ClearGRFConfig(GRFConfig **config);
void ClearGRFConfigList(GRFConfig **config);
void ResetGRFConfig(bool defaults);
GCF_Flags IsGoodGRFConfigList(void);
bool FillGRFDetails(GRFConfig *config, bool is_static);
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last);

/* In newgrf_gui.c */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config);

#ifdef ENABLE_NETWORK
/* For communication about GRFs over the network */
#define UNKNOWN_GRF_NAME_PLACEHOLDER "<Unknown>"
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create);
#endif /* ENABLE_NETWORK */

#endif /* NEWGRF_CONFIG_H */
