/* $Id$ */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

void InitializeScreenshotFormats(void);

const char *GetScreenshotFormatDesc(int i);
void SetScreenshotFormat(int i);

typedef enum ScreenshotType {
	SC_NONE,
	SC_VIEWPORT,
	SC_WORLD
} ScreenshotType;

bool MakeScreenshot(void);
void SetScreenshotType(ScreenshotType t);
bool IsScreenshotRequested(void);

extern char _screenshot_format_name[8];
extern uint _num_screenshot_formats;
extern uint _cur_screenshot_format;

#endif /* SCREENSHOT_H */
