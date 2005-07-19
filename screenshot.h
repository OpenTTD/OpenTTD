#ifndef SCREENSHOT_H
#define SCREENSHOT_H

void InitializeScreenshotFormats(void);

const char *GetScreenshotFormatDesc(int i);
void SetScreenshotFormat(int i);

bool MakeScreenshot(void);
bool MakeWorldScreenshot(int left, int top, int width, int height, int zoom);

extern char _screenshot_format_name[8];
extern uint _num_screenshot_formats;
extern uint _cur_screenshot_format;

#endif
