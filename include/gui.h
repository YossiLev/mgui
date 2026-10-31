#ifndef GUI_H
#define GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_WINDOWS 1
#define PLATFORM_MACOS   2
#define PLATFORM_LINUX   3

#ifndef PLATFORM
#error "PLATFORM must be defined"
#endif


typedef struct _Frame {
    int width;
    int height;
    char *pixels;
} Frame;

typedef void (*GuiCallback)(const char *id);

int  GuiCreateApp(const char *title, int width, int height);
void GuiRun(void);

void GuiBeginVertical(void);
void GuiEndVertical(void);
void GuiBeginHorizontal(void);
void GuiEndHorizontal(void);
void GuiBeginGrid(int columns);
void GuiEndGrid(void);
void GuiBeginGroup(const char *title);
void GuiEndGroup(void);

void GuiCreateLabel(const char *id, const char *text);
void GuiCreateButton(const char *id, const char *title, GuiCallback onClick);
void GuiCreateInput(const char *id, const char *title, int length,
                    GuiCallback onChange);
void GuiCreateSlider(const char *id, const char *title,
                     int min, int max, int value, GuiCallback onChange);
void GuiCreateTextArea(const char *id, int rows, int columns);
void GuiCreateImagePlaceholder(const char *id, const char *title);
void GuiCreateImage(const char *i, int w, int h,GuiCallback c);
void GuiStartPaint(const char *i);
void GuiSetColors(const char *i, const char *p, const char *b);
void GuiMoveTo(const char *i, int x, int y);
void GuiLineTo(const char *i, int x, int y);
void GuiCopyImage(const char *i, const Frame *f);
void GuiBringToFront(const char *i);
void GuiRepaintImage(const char *i);

void GuiSetText(const char *id, const char *text);
const char *GuiGetText(const char *id);
void GuiSetValue(const char *id, int value);
int  GuiGetValue(const char *id);
void GuiSetEnabled(const char *id, int enabled);
void GuiSetVisible(const char *id, int visible);
void GuiLayout(void);

#ifdef __cplusplus
}
#endif
#endif
