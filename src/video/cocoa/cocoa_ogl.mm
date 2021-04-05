/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_ogl.mm Code related to the cocoa OpengL video driver. */

#ifdef WITH_COCOA

#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "../../core/geometry_func.hpp"
#include "../../core/math_func.hpp"
#include "../../core/mem_func.hpp"
#include "cocoa_ogl.h"
#include "cocoa_wnd.h"
#include "../../blitter/factory.hpp"
#include "../../gfx_func.h"
#include "../../framerate_type.h"
#include "../opengl.h"

#import <dlfcn.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>


/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

/** Platform-specific callback to get an OpenGL funtion pointer. */
static OGLProc GetOGLProcAddressCallback(const char *proc)
{
	static void *dl = nullptr;

	if (dl == nullptr) {
		dl = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
	}

	return reinterpret_cast<OGLProc>(dlsym(dl, proc));
}

@interface OTTD_CGLLayer : CAOpenGLLayer {
@private
	CGLContextObj _context;
}

@property (class) bool allowSoftware;
+ (CGLPixelFormatObj)defaultPixelFormat;

- (instancetype)initWithContext:(CGLContextObj)context;
@end

@implementation OTTD_CGLLayer

static bool _allowSoftware;
+ (bool)allowSoftware
{
	return _allowSoftware;
}
+ (void)setAllowSoftware:(bool)newVal
{
	_allowSoftware = newVal;
}

- (instancetype)initWithContext:(CGLContextObj)context
{
	if (self = [ super init ]) {
		self->_context = context;

		self.opaque = YES;
		self.magnificationFilter = kCAFilterNearest;
	}
	return self;
}

+ (CGLPixelFormatObj)defaultPixelFormat
{
	CGLPixelFormatAttribute attribs[] = {
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)0,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)0,
		kCGLPFADoubleBuffer,
		kCGLPFAAllowOfflineRenderers,
		kCGLPFASupportsAutomaticGraphicsSwitching,
		kCGLPFANoRecovery,
		_allowSoftware ? (CGLPixelFormatAttribute)0 : kCGLPFAAccelerated,
		(CGLPixelFormatAttribute)0
	};

	CGLPixelFormatObj pxfmt = nullptr;
	GLint numPixelFormats;
	CGLChoosePixelFormat(attribs, &pxfmt, &numPixelFormats);

	return pxfmt;
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return [ OTTD_CGLLayer defaultPixelFormat ];
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf
{
	CGLContextObj ctx;
	CGLCreateContext(pf, self->_context, &ctx);

	/* Set context state that is not shared. */
	CGLSetCurrentContext(ctx);
	OpenGLBackend::Get()->PrepareContext();

	return ctx;
}

- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(nullable const CVTimeStamp *)ts
{
	CGLSetCurrentContext(ctx);

	OpenGLBackend::Get()->Paint();
	OpenGLBackend::Get()->DrawMouseCursor();

	[ super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts ];
}
@end

@interface OTTD_CGLLayerView : NSView
- (instancetype)initWithFrame:(NSRect)frameRect context:(CGLContextObj)context;
@end

@implementation OTTD_CGLLayerView

- (instancetype)initWithFrame:(NSRect)frameRect context:(CGLContextObj)context
{
	if (self = [ super initWithFrame:frameRect ]) {
		/* We manage our content updates ourselves. */
		self.wantsBestResolutionOpenGLSurface = _allow_hidpi_window ? YES : NO;
		self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

		/* Create backing layer. */
		CALayer *l = [ [ OTTD_CGLLayer alloc ] initWithContext:context ];
		self.layer = l;
		self.wantsLayer = YES;
		[ l release ];
	}
	return self;
}

- (BOOL)acceptsFirstResponder
{
	return NO;
}

- (BOOL)isOpaque
{
	return YES;
}

- (void)viewDidChangeBackingProperties
{
	self.layer.contentsScale = _allow_hidpi_window && [ self.window respondsToSelector:@selector(backingScaleFactor) ] ? [ self.window backingScaleFactor ] : 1.0f;
}
@end


static FVideoDriver_CocoaOpenGL iFVideoDriver_CocoaOpenGL;


const char *VideoDriver_CocoaOpenGL::Start(const StringList &param)
{
	const char *err = this->Initialize();
	if (err != nullptr) return err;

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp != 8 && bpp != 32) {
		this->Stop();
		return "The cocoa OpenGL subdriver only supports 8 and 32 bpp.";
	}

	/* Try to allocate GL context. */
	err = this->AllocateContext(GetDriverParamBool(param, "software"));
	if (err != nullptr) {
		this->Stop();
		return err;
	}

	bool fullscreen = _fullscreen;
	if (!this->MakeWindow(_cur_resolution.width, _cur_resolution.height)) {
		this->Stop();
		return "Could not create window";
	}

	this->AllocateBackingStore(true);

	if (fullscreen) this->ToggleFullscreen(fullscreen);

	this->GameSizeChanged();
	this->UpdateVideoModes();
	MarkWholeScreenDirty();

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return nullptr;

}

void VideoDriver_CocoaOpenGL::Stop()
{
	this->VideoDriver_Cocoa::Stop();

	CGLSetCurrentContext(this->gl_context);
	OpenGLBackend::Destroy();
	CGLReleaseContext(this->gl_context);
}

void VideoDriver_CocoaOpenGL::PopulateSystemSprites()
{
	OpenGLBackend::Get()->PopulateCursorCache();
}

void VideoDriver_CocoaOpenGL::ClearSystemSprites()
{
	CGLSetCurrentContext(this->gl_context);
	OpenGLBackend::Get()->ClearCursorCache();
}

const char *VideoDriver_CocoaOpenGL::AllocateContext(bool allow_software)
{
	[ OTTD_CGLLayer setAllowSoftware:allow_software ];

	CGLPixelFormatObj pxfmt = [ OTTD_CGLLayer defaultPixelFormat ];
	if (pxfmt == nullptr) return "No suitable pixel format found";

	CGLCreateContext(pxfmt, nullptr, &this->gl_context);
	CGLDestroyPixelFormat(pxfmt);

	if (this->gl_context == nullptr) return "Can't create a rendering context";

	CGLSetCurrentContext(this->gl_context);

	return OpenGLBackend::Create(&GetOGLProcAddressCallback);
}

NSView *VideoDriver_CocoaOpenGL::AllocateDrawView()
{
	return [ [ OTTD_CGLLayerView alloc ] initWithFrame:this->cocoaview.bounds context:this->gl_context ];
}

/** Resize the window. */
void VideoDriver_CocoaOpenGL::AllocateBackingStore(bool force)
{
	if (this->window == nil || this->setup) return;

	if (_screen.dst_ptr != nullptr) this->ReleaseVideoPointer();

	CGLSetCurrentContext(this->gl_context);
	NSRect frame = [ this->cocoaview getRealRect:[ this->cocoaview frame ] ];
	OpenGLBackend::Get()->Resize(frame.size.width, frame.size.height, force);
	if (this->buffer_locked) _screen.dst_ptr = this->GetVideoPointer();
	this->dirty_rect = {};

	/* Redraw screen */
	this->GameSizeChanged();
}

void *VideoDriver_CocoaOpenGL::GetVideoPointer()
{
	CGLSetCurrentContext(this->gl_context);
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		this->anim_buffer = OpenGLBackend::Get()->GetAnimBuffer();
	}
	return OpenGLBackend::Get()->GetVideoBuffer();
}

void VideoDriver_CocoaOpenGL::ReleaseVideoPointer()
{
	CGLSetCurrentContext(this->gl_context);

	if (this->anim_buffer != nullptr) OpenGLBackend::Get()->ReleaseAnimBuffer(this->dirty_rect);
	OpenGLBackend::Get()->ReleaseVideoBuffer(this->dirty_rect);
	this->dirty_rect = {};
	_screen.dst_ptr = nullptr;
	this->anim_buffer = nullptr;
}

void VideoDriver_CocoaOpenGL::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		/* Always push a changed palette to OpenGL. */
		CGLSetCurrentContext(this->gl_context);
		OpenGLBackend::Get()->UpdatePalette(_cur_palette.palette, _cur_palette.first_dirty, _cur_palette.count_dirty);
		if (blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_BLITTER) {
			blitter->PaletteAnimate(_cur_palette);
		}

		_cur_palette.count_dirty = 0;
	}

	[ CATransaction begin ];
	[ this->cocoaview.subviews[0].layer setNeedsDisplay ];
	[ CATransaction commit ];
}

#endif /* WITH_COCOA */
