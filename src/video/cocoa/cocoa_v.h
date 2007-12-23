/* $Id$ */

#ifndef VIDEO_COCOA_H
#define VIDEO_COCOA_H

#include <AvailabilityMacros.h>

#include "../video_driver.hpp"

class VideoDriver_Cocoa: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ void ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Cocoa: public VideoDriverFactory<FVideoDriver_Cocoa> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "cocoa"; }
	/* virtual */ const char *GetDescription() { return "Cocoa Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Cocoa(); }
};



class CocoaSubdriver {
public:
	virtual ~CocoaSubdriver() {}

	virtual void Draw() = 0;
	virtual void MakeDirty(int left, int top, int width, int height) = 0;
	virtual void UpdatePalette(uint first_color, uint num_colors) = 0;

	virtual uint ListModes(OTTD_Point* modes, uint max_modes) = 0;

	virtual bool ChangeResolution(int w, int h) = 0;

	virtual bool IsFullscreen() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual void *GetPixelBuffer() = 0;

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint* p) = 0;

	virtual NSPoint GetMouseLocation(NSEvent *event) = 0;
	virtual bool MouseIsInsideView(NSPoint *pt) = 0;

	virtual bool IsActive() = 0;
};

extern CocoaSubdriver* _cocoa_subdriver;

CocoaSubdriver *QZ_CreateFullscreenSubdriver(int width, int height, int bpp);

#ifdef ENABLE_COCOA_QUICKDRAW
CocoaSubdriver *QZ_CreateWindowQuickdrawSubdriver(int width, int height, int bpp);
#endif

#ifdef ENABLE_COCOA_QUARTZ
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
CocoaSubdriver *QZ_CreateWindowQuartzSubdriver(int width, int height, int bpp);
#endif
#endif

void QZ_GameSizeChanged();

void QZ_GameLoop();

void QZ_ShowMouse();
void QZ_HideMouse();

#endif /* VIDEO_COCOA_H */
