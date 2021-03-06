/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_ogl.h The Cocoa OpenGL video driver. */

#ifndef VIDEO_COCOA_OGL_H
#define VIDEO_COCOA_OGL_H

#include "cocoa_v.h"

@class OTTD_OpenGLView;

class VideoDriver_CocoaOpenGL : public VideoDriver_Cocoa {
	CGLContextObj gl_context;

	uint8 *anim_buffer; ///< Animation buffer from OpenGL back-end.

	const char *AllocateContext(bool allow_software);

public:
	VideoDriver_CocoaOpenGL() : gl_context(nullptr), anim_buffer(nullptr) {}

	const char *Start(const StringList &param) override;
	void Stop() override;

	bool HasEfficient8Bpp() const override { return true; }

	bool UseSystemCursor() override { return true; }

	void ClearSystemSprites() override;

	bool HasAnimBuffer() override { return true; }
	uint8 *GetAnimBuffer() override { return this->anim_buffer; }

	/** Return driver name */
	const char *GetName() const override { return "cocoa-opengl"; }

	void AllocateBackingStore(bool force = false) override;

protected:
	void Paint() override;

	void *GetVideoPointer() override;
	void ReleaseVideoPointer() override;

	NSView* AllocateDrawView() override;
};

class FVideoDriver_CocoaOpenGL : public DriverFactoryBase {
public:
	FVideoDriver_CocoaOpenGL() : DriverFactoryBase(Driver::DT_VIDEO, 9, "cocoa-opengl", "Cocoa OpenGL Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_CocoaOpenGL(); }

protected:
	bool IsUsable() const override { return _video_hw_accel; }
};

#endif /* VIDEO_COCOA_OGL_H */
