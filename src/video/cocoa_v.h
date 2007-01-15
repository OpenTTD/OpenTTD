/* $Id$ */

#ifndef VIDEO_COCOA_H
#define VIDEO_COCOA_H

#include "../hal.h"

#ifndef __cplusplus
/* Really ugly workaround
 * It should be solved right as soon as possible */
typedef uint32 SpriteID;
#endif //__cplusplus

#include "../gfx.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

extern const HalVideoDriver _cocoa_video_driver;

#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

#endif
