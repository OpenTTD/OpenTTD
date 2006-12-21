/* $Id$ */

#ifndef NEWGRF_CONFIG_H
#define NEWGRF_CONFIG_H

/* GRF config bit flags */
enum {
	GCF_DISABLED,
	GCF_NOT_FOUND,
	GCF_ACTIVATED,
	GCF_SYSTEM,
	GCF_UNSAFE,
	GCF_STATIC,
	GCF_COPY,      ///< The data is copied from a grf in _all_grfs
};

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
const GRFConfig *FindGRFConfig(uint32 grfid, uint8 *md5sum);
GRFConfig *GetGRFConfig(uint32 grfid);
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src);
void ClearGRFConfig(GRFConfig **config);
void ClearGRFConfigList(GRFConfig **config);
void ResetGRFConfig(bool defaults);
bool IsGoodGRFConfigList(void);
bool FillGRFDetails(GRFConfig *config, bool is_static);
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last);

/* In newgrf_gui.c */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config);

/* For communication about GRFs over the network */
#define UNKNOWN_GRF_NAME_PLACEHOLDER "<Unknown>"
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create);

#endif /* NEWGRF_CONFIG_H */
