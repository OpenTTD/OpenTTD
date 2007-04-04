/* $Id$ */

/** @file screenshot.h */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

void InitializeScreenshotFormats();

const char *GetScreenshotFormatDesc(int i);
void SetScreenshotFormat(int i);

enum ScreenshotType {
	SC_NONE,
	SC_VIEWPORT,
	SC_WORLD
};

bool MakeScreenshot();
void SetScreenshotType(ScreenshotType t);
bool IsScreenshotRequested();

extern char _screenshot_format_name[8];
extern uint _num_screenshot_formats;
extern uint _cur_screenshot_format;

#endif /* SCREENSHOT_H */
