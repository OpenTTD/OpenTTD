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
const GRFConfig *GetGRFConfig(uint32 grfid);
void ClearGRFConfig(GRFConfig *config);
void ClearGRFConfigList(GRFConfig *config);
void ResetGRFConfig(bool defaults);
bool IsGoodGRFConfigList(void);
bool FillGRFDetails(GRFConfig *config, bool is_static);
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last);

/* In newgrf_gui.c */
void ShowNewGRFSettings(bool editable, bool show_params, GRFConfig **config);

#endif /* NEWGRF_CONFIG_H */
