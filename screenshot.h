#ifndef SCREENSHOT_H
#define SCREENSHOT_H

void InitializeScreenshotFormats(void);

const char *GetScreenshotFormatDesc(int i);
void SetScreenshotFormat(int i);

bool MakeScreenshot(void);
bool MakeWorldScreenshot(int left, int top, int width, int height, int zoom);

#endif
