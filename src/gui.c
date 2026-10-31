#include "gui.h"

#if PLATFORM == PLATFORM_WINDOWS

extern int  WinGuiCreateApp(const char *, int, int);
extern void WinGuiRun(void);
extern void WinGuiBeginVertical(void); extern void WinGuiEndVertical(void);
extern void WinGuiBeginHorizontal(void); extern void WinGuiEndHorizontal(void);
extern void WinGuiBeginGrid(int); extern void WinGuiEndGrid(void);
extern void WinGuiBeginGroup(const char *); extern void WinGuiEndGroup(void);
extern void WinGuiCreateLabel(const char *, const char *);
extern void WinGuiCreateButton(const char *, const char *, GuiCallback);
extern void WinGuiCreateInput(const char *, const char *, int, GuiCallback);
extern void WinGuiCreateSlider(const char *, const char *, int, int, int, GuiCallback);
extern void WinGuiCreateTextArea(const char *, int, int);
extern void WinGuiCreateImagePlaceholder(const char *, const char *);
extern void WinGuiCreateImage(const char *i, int w, int h, GuiCallback c);
extern void WinGuiStartPaint(const char *);
extern void WinGuiSetColors(const char *, const char *, const char *);
extern void WinGuiMoveTo(const char *, int, int);
extern void WinGuiLineTo(const char *, int, int);
extern void WinGuiCopyImage(const char *, const Frame8 *);
extern void WinGuiBringToFront(const char *);
extern void WinGuiRepaintImage(const char *);
extern void WinGuiSetText(const char *, const char *);
extern const char *WinGuiGetText(const char *);
extern void WinGuiSetValue(const char *, int);
extern int WinGuiGetValue(const char *);
extern void WinGuiSetEnabled(const char *, int);
extern void WinGuiSetVisible(const char *, int);
extern void WinGuiLayout(void);

#define IMPL WinGui

#elif PLATFORM == PLATFORM_MACOS

extern int  MacGuiCreateApp(const char *, int, int);
extern void MacGuiRun(void);
extern void MacGuiBeginVertical(void); extern void MacGuiEndVertical(void);
extern void MacGuiBeginHorizontal(void); extern void MacGuiEndHorizontal(void);
extern void MacGuiBeginGrid(int); extern void MacGuiEndGrid(void);
extern void MacGuiBeginGroup(const char *); extern void MacGuiEndGroup(void);
extern void MacGuiCreateLabel(const char *, const char *);
extern void MacGuiCreateButton(const char *, const char *, GuiCallback);
extern void MacGuiCreateInput(const char *, const char *, int, GuiCallback);
extern void MacGuiCreateSlider(const char *, const char *, int, int, int, GuiCallback);
extern void MacGuiCreateTextArea(const char *, int, int);
extern void MacGuiCreateImagePlaceholder(const char *, const char *);
extern void MacGuiCreateImage(const char *i, int w, int h, GuiCallback c);
extern void MacGuiStartPaint(const char *);
extern void MacGuiSetColors(const char *, const char *, const char *);
extern void MacGuiMoveTo(const char *, int, int);
extern void MacGuiLineTo(const char *, int, int);
extern void MacGuiCopyImage(const char *, const Frame8 *);
extern void MacGuiBringToFront(const char *);
extern void MacGuiRepaintImage(const char *);
extern void MacGuiSetText(const char *, const char *);
extern const char *MacGuiGetText(const char *);
extern void MacGuiSetValue(const char *, int);
extern int MacGuiGetValue(const char *);
extern void MacGuiSetEnabled(const char *, int);
extern void MacGuiSetVisible(const char *, int);
extern void MacGuiLayout(void);

#define IMPL MacGui

#elif PLATFORM == PLATFORM_LINUX

extern int  LinuxGuiCreateApp(const char *, int, int);
extern void LinuxGuiRun(void);
extern void LinuxGuiBeginVertical(void); extern void LinuxGuiEndVertical(void);
extern void LinuxGuiBeginHorizontal(void); extern void LinuxGuiEndHorizontal(void);
extern void LinuxGuiBeginGrid(int); extern void LinuxGuiEndGrid(void);
extern void LinuxGuiBeginGroup(const char *); extern void LinuxGuiEndGroup(void);
extern void LinuxGuiCreateLabel(const char *, const char *);
extern void LinuxGuiCreateButton(const char *, const char *, GuiCallback);
extern void LinuxGuiCreateInput(const char *, const char *, int, GuiCallback);
extern void LinuxGuiCreateSlider(const char *, const char *, int, int, int, GuiCallback);
extern void LinuxGuiCreateTextArea(const char *, int, int);
extern void LinuxGuiCreateImagePlaceholder(const char *, const char *);
extern void LinuxGuiRepaintImage(const char *);
extern void LinuxGuiSetText(const char *, const char *);
extern const char *LinuxGuiGetText(const char *);
extern void LinuxGuiSetValue(const char *, int);
extern int LinuxGuiGetValue(const char *);
extern void LinuxGuiSetEnabled(const char *, int);
extern void LinuxGuiSetVisible(const char *, int);
extern void LinuxGuiLayout(void);

#define IMPL LinuxGui

#else
#error Unsupported PLATFORM
#endif

#if PLATFORM == PLATFORM_WINDOWS
#define F(name) WinGui##name
#elif PLATFORM == PLATFORM_MACOS
#define F(name) MacGui##name
#else
#define F(name) LinuxGui##name
#endif

int GuiCreateApp(const char *t,int w,int h){return F(CreateApp)(t,w,h);}
void GuiRun(void){F(Run)();}
void GuiBeginVertical(void){F(BeginVertical)();}
void GuiEndVertical(void){F(EndVertical)();}
void GuiBeginHorizontal(void){F(BeginHorizontal)();}
void GuiEndHorizontal(void){F(EndHorizontal)();}
void GuiBeginGrid(int n){F(BeginGrid)(n);}
void GuiEndGrid(void){F(EndGrid)();}
void GuiBeginGroup(const char *t){F(BeginGroup)(t);}
void GuiEndGroup(void){F(EndGroup)();}
void GuiCreateLabel(const char *i,const char *t){F(CreateLabel)(i,t);}
void GuiCreateButton(const char *i,const char *t,GuiCallback c){F(CreateButton)(i,t,c);}
void GuiCreateInput(const char *i,const char *t,int n,GuiCallback c){F(CreateInput)(i,t,n,c);}
void GuiCreateSlider(const char *i,const char *t,int a,int b,int v,GuiCallback c){F(CreateSlider)(i,t,a,b,v,c);}
void GuiCreateTextArea(const char *i,int r,int c){F(CreateTextArea)(i,r,c);}
void GuiCreateImagePlaceholder(const char *i,const char *t){F(CreateImagePlaceholder)(i,t);}
void GuiCreateImage(const char *i, int w, int h,GuiCallback c){F(CreateImage)(i,w,h,c);}
void GuiStartPaint(const char *i){F(StartPaint)(i);}
void GuiSetColors(const char *i, const char *p, const char *b){F(SetColors)(i,p,b);}
void GuiMoveTo(const char *i, int x, int y){F(MoveTo)(i,x,y);}
void GuiLineTo(const char *i, int x, int y){F(LineTo)(i,x,y);}
void GuiCopyImage(const char *i, const Frame8 *f){F(CopyImage)(i,f);}
void GuiBringToFront(const char *i){F(BringToFront)(i);}
void GuiRepaintImage(const char *i){F(RepaintImage)(i);}
void GuiSetText(const char *i,const char *t){F(SetText)(i,t);}
const char *GuiGetText(const char *i){return F(GetText)(i);}
void GuiSetValue(const char *i,int v){F(SetValue)(i,v);}
int GuiGetValue(const char *i){return F(GetValue)(i);}
void GuiSetEnabled(const char *i,int v){F(SetEnabled)(i,v);}
void GuiSetVisible(const char *i,int v){F(SetVisible)(i,v);}
void GuiLayout(void){F(Layout)();}
