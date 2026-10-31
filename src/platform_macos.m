#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define MACGUI_DEBUG(...) do { fprintf(stderr, "[MacGUI] "); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); fflush(stderr); } while (0)
#include "gui.h"

/*
 * Minimal Cocoa backend.
 * It is intentionally small; the Windows backend is the primary demo.
 */

typedef enum {
    M_ROOT, M_VERTICAL, M_HORIZONTAL, M_GRID, M_GROUP,
    M_LABEL, M_BUTTON, M_INPUT, M_SLIDER, M_TEXTAREA,
    M_IMAGE, M_IMAGE_PLACEHOLDER
} MType;

typedef struct MSegment {
    int x1, y1, x2, y2;
} MSegment;

typedef struct MWidget MWidget;
struct MWidget {
    char id[64];
    char text[512];
    MType type;
    NSView *view;
    NSControl *control;
    NSTextView *textView;
    NSView *labelView;
    NSView *innerView;
    GuiCallback cb;
    int value;
    int min;
    int max;
    int visible;
    int columns;
    int width;
    int height;
    char penColor[8];
    char bgColor[8];
    int currentX;
    int currentY;
    unsigned char *imagePixels;
    int imageWidth;
    int imageHeight;
    MSegment *segments;
    size_t segmentCount;
    size_t segmentCapacity;
    MWidget *parent;
    MWidget *first;
    MWidget *last;
    MWidget *next;
    MWidget *registryNext;
};

static NSWindow *window;
static MWidget *widgets;
static MWidget *g_root;
static MWidget *g_current;
static NSView *mainContent;
static id<NSWindowDelegate> macWindowDelegate;

static int child_count(MWidget *w);
static const char *mac_type_name(MType type);
static void debug_widget_tree(MWidget *w, int depth);
extern void MacGuiLayout(void);

@interface MacWindowDelegate : NSObject <NSWindowDelegate>
@end

@interface MacFlippedView : NSView
@end
@implementation MacFlippedView
- (BOOL)isFlipped { return YES; }
@end

@interface MacCallbackButton : NSButton
@property(nonatomic, assign) MWidget *widget;
@end
@implementation MacCallbackButton
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;
}

- (NSView *)hitTest:(NSPoint)point {
    NSView *result = [super hitTest:point];
    // if (result == self) {
    //     MACGUI_DEBUG("MacCallbackButton hitTest id='%s' point=(%.0f,%.0f)", self.widget ? self.widget->id : "-", point.x, point.y);
    // }
    return result;
}

- (void)mouseDown:(NSEvent *)event {
    // MACGUI_DEBUG("MacCallbackButton mouseDown id='%s' callback=%s",
    //              self.widget ? self.widget->id : "-",
    //              self.widget && self.widget->cb ? "yes" : "no");
    if (self.widget && self.widget->cb) {
        self.widget->cb(self.widget->id);
    }
    [super mouseDown:event];
}
@end

@interface CallbackTarget : NSObject
@property(nonatomic, assign) MWidget *widget;
- (void)fire:(id)sender;
@end
@implementation CallbackTarget
- (void)fire:(id)sender {
    if (self.widget && self.widget->cb) {
        if ([sender isKindOfClass:[NSSlider class]]) {
            self.widget->value = (int)[(NSSlider *)sender doubleValue];
            // MACGUI_DEBUG("Slider callback id='%s' value=%d", self.widget->id, self.widget->value);
        } else {
            // MACGUI_DEBUG("Button callback target id='%s'", self.widget->id);
        }
        self.widget->cb(self.widget->id);
    }
}
@end

@interface MacTextFieldDelegate : NSObject <NSTextFieldDelegate>
@property(nonatomic, assign) MWidget *widget;
@end
@implementation MacTextFieldDelegate
- (void)controlTextDidChange:(NSNotification *)notification {
    (void)notification;
    if (self.widget && self.widget->cb) self.widget->cb(self.widget->id);
}
@end

@interface MacPaintView : NSView
@property(nonatomic, assign) MWidget *widget;
@end
@implementation MacPaintView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    NSColor *background = [NSColor whiteColor];
    if (self.widget && self.widget->bgColor[0]) {
        unsigned int r = 255, g = 255, b = 255;
        const char *s = self.widget->bgColor;
        if (*s == '#') ++s;
        if (strlen(s) == 6) sscanf(s, "%2x%2x%2x", &r, &g, &b);
        background = [NSColor colorWithCalibratedRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0];
    }
    [background setFill]; NSRectFill(self.bounds);

    if (!self.widget) return;

    if (self.widget->imagePixels && self.widget->imageWidth > 0 && self.widget->imageHeight > 0) {
        CGColorSpaceRef graySpace = CGColorSpaceCreateDeviceGray();
        CGContextRef context = CGBitmapContextCreate(
            self.widget->imagePixels,
            self.widget->imageWidth,
            self.widget->imageHeight,
            8,
            self.widget->imageWidth,
            graySpace,
            (CGBitmapInfo)kCGImageAlphaNone);
        if (context) {
            CGImageRef image = CGBitmapContextCreateImage(context);
            if (image) {
                CGContextRef drawContext = [[NSGraphicsContext currentContext] CGContext];
                CGContextSaveGState(drawContext);
                CGContextSetBlendMode(drawContext, kCGBlendModeCopy);
                CGRect nativeRect = CGRectMake(
                    0,
                    0,
                    self.widget->imageWidth,
                    self.widget->imageHeight);
                CGContextDrawImage(drawContext, nativeRect, image);
                CGContextRestoreGState(drawContext);
                CGImageRelease(image);
            }
            CGContextRelease(context);
        }
        CGColorSpaceRelease(graySpace);
    }

    if (self.widget->cb) self.widget->cb(self.widget->id);

    unsigned int pr = 0, pg = 0, pb = 0;
    const char *s = self.widget->penColor;
    if (*s == '#') ++s;
    if (!*s || strlen(s) != 6 || sscanf(s, "%2x%2x%2x", &pr, &pg, &pb) != 3) { pr = 0; pg = 0; pb = 0; }
    [[NSColor colorWithCalibratedRed:pr/255.0 green:pg/255.0 blue:pb/255.0 alpha:1.0] setStroke];

    NSBezierPath *path = [NSBezierPath bezierPath]; [path setLineWidth:3.0];
    for (size_t i = 0; i < self.widget->segmentCount; ++i) {
        MSegment seg = self.widget->segments[i];
        [path moveToPoint:NSMakePoint(seg.x1, seg.y1)];
        [path lineToPoint:NSMakePoint(seg.x2, seg.y2)];
    }
    [path stroke];
}
@end

static void append_child(MWidget *parent, MWidget *child) {
    child->parent = parent;
    if (!parent->first) parent->first = child;
    else parent->last->next = child;
    parent->last = child;
}

static MWidget *new_widget(MType type, const char *id, const char *text, GuiCallback cb) {
    MWidget *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->type = type; w->cb = cb; w->visible = 1;
    if (id) strncpy(w->id, id, sizeof(w->id) - 1);
    if (text) strncpy(w->text, text, sizeof(w->text) - 1);
    return w;
}

static MWidget *add_widget(MType type, const char *id, const char *text, GuiCallback cb) {
    MWidget *w = new_widget(type, id, text, cb);
    if (!w) return NULL;
    append_child(g_current, w);
    w->registryNext = widgets; widgets = w;
    // NSLog(@"[MacGUI] create %-18s id='%s' parent=%s",
    //       mac_type_name(type), id ? id : "-",
    //       g_current ? mac_type_name(g_current->type) : "none");
    return w;
}

static MWidget *find(const char *id) {
    for (MWidget *w = widgets; w; w = w->registryNext)
        if (strcmp(w->id, id) == 0) return w;
    return NULL;
}

static int is_container(MType type) {
    return type == M_ROOT || type == M_VERTICAL || type == M_HORIZONTAL || type == M_GRID || type == M_GROUP;
}

static int child_count(MWidget *w) {
    int count = 0;
    for (MWidget *c = w->first; c; c = c->next)
        if (c->visible) ++count;
    return count;
}

static void layout_widget(MWidget *w, NSRect rect);

static int preferred_height(MWidget *w) {
    if (!w) return 0;
    switch (w->type) {
        case M_LABEL: return 24;
        case M_BUTTON: return 34;
        case M_INPUT: return 56;
        case M_SLIDER: return w->labelView ? 56 : 34;
        case M_TEXTAREA: return 120;
        case M_IMAGE: return w->height > 0 ? w->height : 120;
        case M_IMAGE_PLACEHOLDER: return 120;
        case M_GROUP:  {
            int h = 0;
            for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
                int ch = preferred_height(c); if (ch > h) h = ch;
            }
            return h + 34;
        }
        case M_HORIZONTAL: {
            int h = 34;
            for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
                int ch = preferred_height(c); if (ch > h) h = ch;
            }
            return h;
        }
        case M_GRID: {
            int cols = w->columns > 0 ? w->columns : 1;
            int count = child_count(w);
            int rows = count ? (count + cols - 1) / cols : 1;
            int rowHeight = 34;
            for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
                int ch = preferred_height(c); if (ch > rowHeight) rowHeight = ch;
            }
            return rows * rowHeight + (rows - 1) * 8;
        }
        case M_VERTICAL: {
            int h = 0, n = 0;
            for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
                h += preferred_height(c); ++n;
            }
            return h + (n > 1 ? (n - 1) * 8 : 0);
        }
        default: return 34;
    }
}

static void layout_vertical(MWidget *w, NSRect rect) {
    int n = child_count(w);
    if (!n) return;
    int preferred = 0;
    for (MWidget *c = w->first; c; c = c->next) if (c->visible)
        preferred += preferred_height(c);
    int available = (int)(NSHeight(rect) - (n - 1) * 8);
    int extra = available - preferred;
    CGFloat y = NSMinY(rect);
    for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
        CGFloat h = preferred_height(c);
        if (extra > 0) h += extra / (CGFloat)n;
        layout_widget(c, NSMakeRect(NSMinX(rect), y, NSWidth(rect), h));
        y += h + 8;
    }
}

static void layout_widget(MWidget *w, NSRect rect) {
    if (!w || !w->visible) {
        if (w) NSLog(@"[MacGUI] layout SKIP id='%s' type=%s visible=%d", w->id, mac_type_name(w->type), w->visible);
        return;
    }

    // NSLog(@"[MacGUI] layout type=%-18s id='%s' rect=%@",
    //       mac_type_name(w->type), w->id[0] ? w->id : "-", NSStringFromRect(rect));
    if (w->type == M_GROUP) {
        if (w->innerView) {
            NSRect inner = NSInsetRect(rect, 10, 10);
            inner.origin.y += 20;
            inner.size.height -= 20;
            if (inner.size.height < 1) inner.size.height = 1;
            [w->innerView setFrame:rect];
            layout_vertical(w, inner);
        }
        return;
    }

    if (!is_container(w->type)) {
        if (w->labelView) {
            [w->labelView setFrame:NSMakeRect(NSMinX(rect), NSMinY(rect), NSWidth(rect), 22)];
            [w->view setFrame:NSMakeRect(NSMinX(rect), NSMinY(rect) + 24, NSWidth(rect), MAX(1, NSHeight(rect) - 24))];
        } else if (w->view) {
            [w->view setFrame:rect];
        }
        return;
    }

    if (w->type == M_VERTICAL || w->type == M_ROOT) {
        layout_vertical(w, rect); return;
    }

    if (w->type == M_HORIZONTAL) {
        int n = child_count(w); if (!n) return;
        CGFloat cell = (NSWidth(rect) - (n - 1) * 8) / n;
        CGFloat x = NSMinX(rect);
        for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
            layout_widget(c, NSMakeRect(x, NSMinY(rect), cell, NSHeight(rect)));
            x += cell + 8;
        }
        return;
    }

    if (w->type == M_GRID) {
        int cols = w->columns > 0 ? w->columns : 1;
        int n = child_count(w); if (!n) return;
        int rows = (n + cols - 1) / cols;
        CGFloat cellW = (NSWidth(rect) - (cols - 1) * 8) / cols;
        CGFloat cellH = (NSHeight(rect) - (rows - 1) * 8) / rows;
        int index = 0;
        for (MWidget *c = w->first; c; c = c->next) if (c->visible) {
            int row = index / cols; int col = index % cols;
            CGFloat x = NSMinX(rect) + col * (cellW + 8);
            CGFloat y = NSMinY(rect) + row * (cellH + 8);
            layout_widget(c, NSMakeRect(x, y, cellW, cellH)); ++index;
        }
    }
}

static void register_control(MWidget *w, NSView *view) {
    w->view = view;
    if ([view respondsToSelector:@selector(setAutoresizingMask:)])
        [(NSView *)view setAutoresizingMask:NSViewNotSizable];
    [mainContent addSubview:view positioned:NSWindowAbove relativeTo:nil];
}

int MacGuiCreateApp(const char *title,int width,int height)
{
    // MACGUI_DEBUG("CreateApp START title='%s' requested=%dx%d", title ? title : "", width, height);
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect r = NSMakeRect(0, 0, width, height);
    window = [[NSWindow alloc] initWithContentRect:r
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    [window setTitle:[NSString stringWithUTF8String:title ? title : ""]];

    mainContent = [[MacFlippedView alloc] initWithFrame:r];
    [window setContentView:mainContent];
    macWindowDelegate = [MacWindowDelegate new];
    [window setDelegate:macWindowDelegate];

    g_root = new_widget(M_ROOT, "__root__", NULL, NULL);
    g_current = g_root;

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    return 1;
}

void MacGuiBeginVertical(void) {
    MWidget *w = add_widget(M_VERTICAL, NULL, NULL, NULL);
    if (w) g_current = w;
}
void MacGuiEndVertical(void) { if (g_current && g_current->parent) g_current = g_current->parent; }
void MacGuiBeginHorizontal(void) {
    MWidget *w = add_widget(M_HORIZONTAL, NULL, NULL, NULL);
    if (w) g_current = w;
}
void MacGuiEndHorizontal(void) { if (g_current && g_current->parent) g_current = g_current->parent; }
void MacGuiBeginGrid(int n) {
    MWidget *w = add_widget(M_GRID, NULL, NULL, NULL);
    if (w) { w->columns = n > 0 ? n : 1; g_current = w; }
}
void MacGuiEndGrid(void) { if (g_current && g_current->parent) g_current = g_current->parent; }

void MacGuiBeginGroup(const char *title) {
    MWidget *w = add_widget(M_GROUP, NULL, title, NULL);
    if (!w) return;
    NSBox *box = [[NSBox alloc] init];
    [box setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    [box setTitlePosition:NSAtTop];
    w->view = box; w->innerView = box;
    [mainContent addSubview:box];
    g_current = w;
}
void MacGuiEndGroup(void) { if (g_current && g_current->parent) g_current = g_current->parent; }

void MacGuiCreateLabel(const char *id,const char *text) {
    MWidget *w = add_widget(M_LABEL, id, text, NULL);
    if (!w) return;
    NSTextField *view = [NSTextField labelWithString:[NSString stringWithUTF8String:text ? text : ""]];
    register_control(w, view); w->control = view;
}

void MacGuiCreateButton(const char *id,const char *title,GuiCallback cb) {
    MWidget *w = add_widget(M_BUTTON, id, title, cb);
    if (!w) return;

    MacCallbackButton *view = [[MacCallbackButton alloc] initWithFrame:NSZeroRect];
    [view setButtonType:NSButtonTypeMomentaryPushIn];
    [view setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    view.widget = w;
    [view setEnabled:YES];
    register_control(w, view);
    w->control = view;
    [view setTarget:nil];
    [view setAction:NULL];

    // MACGUI_DEBUG("CreateButton id='%s' title='%s' callback=%s",
    //              id ? id : "", title ? title : "", cb ? "yes" : "no");
}

void MacGuiCreateInput(const char *id,const char *title,int length,GuiCallback cb) {
    MWidget *w = add_widget(M_INPUT, id, "", cb);
    if (!w) return;
    NSTextField *label = [NSTextField labelWithString:[NSString stringWithUTF8String:title ? title : ""]];
    NSTextField *view = [[NSTextField alloc] init];
    [view setEditable:YES];
    if (length > 0) [view setStringValue:@""]; // length is a hint; Cocoa has no fixed length property
    register_control(w, view);
    w->control = view; w->labelView = label;
    [mainContent addSubview:label];
    MacTextFieldDelegate *delegate = [MacTextFieldDelegate new]; delegate.widget = w;
    [view setDelegate:delegate]; objc_setAssociatedObject(view, "delegate", delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void MacGuiCreateSlider(const char *id,const char *title,int min,int max,int value,GuiCallback cb) {
    MWidget *w = add_widget(M_SLIDER, id, title ? title : "", cb);
    if (!w) return;
    w->min = min; w->max = max; w->value = value;
    NSSlider *view = [[NSSlider alloc] init];
    [view setMinValue:min]; [view setMaxValue:max]; [view setDoubleValue:value];
    [view setContinuous:YES];
    register_control(w, view);
    w->control = view;

    if (title && title[0] != '\0') {
        NSTextField *label = [NSTextField labelWithString:[NSString stringWithUTF8String:title]];
        w->labelView = label;
        [mainContent addSubview:label];
    } else {
        w->labelView = nil;
    }

    CallbackTarget *target = [CallbackTarget new]; target.widget = w;
    [view setTarget:target]; [view setAction:@selector(fire:)];
    objc_setAssociatedObject(view, "target", target, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void MacGuiCreateTextArea(const char *id,int rows,int columns) {
    MWidget *w = add_widget(M_TEXTAREA, id, "", NULL);
    if (!w) return;
    NSScrollView *scroll = [[NSScrollView alloc] init];
    [scroll setHasVerticalScroller:YES]; [scroll setHasHorizontalScroller:YES];
    [scroll setAutohidesScrollers:NO];
    NSTextView *view = [[NSTextView alloc] initWithFrame:NSZeroRect];
    [view setEditable:YES]; [view setVerticallyResizable:YES]; [view setHorizontallyResizable:YES];
    [view setMinSize:NSMakeSize(0, 0)]; [view setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [view.textContainer setContainerSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [view.textContainer setWidthTracksTextView:NO];
    scroll.documentView = view;
    register_control(w, scroll); w->textView = view; w->control = (NSControl *)view;
    (void)rows; (void)columns;
}

void MacGuiCreateImagePlaceholder(const char *id,const char *title) {
    MWidget *w = add_widget(M_IMAGE_PLACEHOLDER, id, title, NULL);
    if (!w) return;
    NSTextField *view = [NSTextField labelWithString:[NSString stringWithUTF8String:title ? title : ""]];
    [view setAlignment:NSTextAlignmentCenter]; register_control(w, view); w->control = view;
}

void MacGuiCreateImage(const char *id, int width, int height, GuiCallback cb) {
    MWidget *w = add_widget(M_IMAGE, id, "", cb);
    if (!w) return;
    w->width = width; w->height = height;
    MacPaintView *view = [[MacPaintView alloc] initWithFrame:NSMakeRect(0, 0, MAX(1, width), MAX(1, height))];
    view.widget = w; register_control(w, view);
}

void MacGuiSetText(const char *id,const char *text) {
    MWidget *w = find(id);
    if (!w) {
        // MACGUI_DEBUG("SetText: widget not found id='%s'", id ? id : "");
        return;
    }

    NSString *value = [NSString stringWithUTF8String:text ? text : ""];
    strncpy(w->text, text ? text : "", sizeof(w->text) - 1);
    w->text[sizeof(w->text) - 1] = 0;

    if (w->textView) {
        [w->textView setString:value];
        [w->textView setNeedsDisplay:YES];
    } else if ([w->control isKindOfClass:[NSTextField class]]) {
        NSTextField *field = (NSTextField *)w->control;
        [field setStringValue:value];
        [field setNeedsDisplay:YES];
    } else if ([w->control respondsToSelector:@selector(setStringValue:)]) {
        [w->control setStringValue:value];
    }

    if (w->view) {
        [w->view setNeedsDisplay:YES];
    }

    // MACGUI_DEBUG("SetText id='%s' value='%s' type=%s view=%s",
    //              id ? id : "", text ? text : "", mac_type_name(w->type),
    //              w->view ? [[w->view className] UTF8String] : "nil");
}

const char *MacGuiGetText(const char *id) {
    static char buf[4096];
    MWidget *w = find(id);
    buf[0] = 0;
    if (!w) return buf;

    NSString *value = nil;
    if (w->textView) {
        value = w->textView.string;
    } else if ([w->control respondsToSelector:@selector(stringValue)]) {
        value = w->control.stringValue;
    }
    if (value) {
        const char *s = value.UTF8String;
        if (s) { strncpy(buf, s, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0; }
    }
    return buf;
}

void MacGuiSetValue(const char *id,int value) {
    MWidget *w = find(id);
    if (!w) return;
    w->value = value;
    if ([w->control isKindOfClass:[NSSlider class]]) {
        [(NSSlider *)w->control setDoubleValue:value];
    } else if ([w->control respondsToSelector:@selector(setIntValue:)]) {
        [w->control setIntValue:value];
    }
}

int MacGuiGetValue(const char *id) {
    MWidget *w = find(id);
    if (!w) return 0;
    if ([w->control isKindOfClass:[NSSlider class]]) {
        return (int)[(NSSlider *)w->control doubleValue];
    }
    if ([w->control respondsToSelector:@selector(intValue)]) {
        return [w->control intValue];
    }
    return w->value;
}

void MacGuiSetEnabled(const char *id,int enabled) {
    MWidget *w = find(id);
    if (w && w->control) [w->control setEnabled:enabled != 0];
}

void MacGuiSetVisible(const char *id,int visible) {
    MWidget *w = find(id);
    if (!w) return;
    w->visible = visible != 0;
    if (w->view) [w->view setHidden:!w->visible];
    if (w->labelView) [w->labelView setHidden:!w->visible];
    MacGuiLayout();
}

void MacGuiStartPaint(const char *id) {
    MWidget *w = find(id);
    if (w) w->segmentCount = 0;
}

void MacGuiSetColors(const char *id,const char *pen,const char *background) {
    MWidget *w = find(id);
    if (!w) return;
    strncpy(w->penColor, pen ? pen : "", sizeof(w->penColor) - 1);
    w->penColor[sizeof(w->penColor) - 1] = 0;
    strncpy(w->bgColor, background ? background : "", sizeof(w->bgColor) - 1);
    w->bgColor[sizeof(w->bgColor) - 1] = 0;
}

void MacGuiMoveTo(const char *id,int x,int y) {
    MWidget *w = find(id);
    if (w) { w->currentX = x; w->currentY = w->height - 1 - y; }
    // NSLog(@"[MacGUI] MoveTo x=%@ y=%@ id=%@",
    //       @(x), @(y), id ? [NSString stringWithUTF8String:id] : @"-");
}

void MacGuiLineTo(const char *id,int x,int y) {
    MWidget *w = find(id);
    if (!w) return;
    if (w->segmentCount == w->segmentCapacity) {
        w->segmentCapacity = w->segmentCapacity ? w->segmentCapacity * 2 : 16;
        w->segments = realloc(w->segments, w->segmentCapacity * sizeof(MSegment));
    }
    if (!w->segments) return;
    // NSLog(@"[MacGUI] LineTo x=%@ y=%@ xp=%@ yp=%@ id=%@",
    //       @(x), @(y), @(w->currentX), @(w->currentY), id ? [NSString stringWithUTF8String:id] : @"-");
    w->segments[w->segmentCount++] = (MSegment){w->currentX, w->currentY, x, w->height - 1 - y};
    w->currentX = x; w->currentY = w->height - 1 - y;
}

void MacGuiCopyImage(const char *id, const Frame8 *frame) {
    MWidget *w = find(id);
    if (!w || !w->view || !frame || !frame->pixels || frame->width <= 0 || frame->height <= 0) return;

    MWidget *widget = w;
    int width = frame->width;
    int height = frame->height;
    size_t pixelCount = (size_t)width * (size_t)height;
    unsigned char *pixels = malloc(pixelCount);
    if (!pixels) return;
    memcpy(pixels, frame->pixels, pixelCount);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (widget->imagePixels) free(widget->imagePixels);
        widget->imagePixels = pixels;
        widget->imageWidth = width;
        widget->imageHeight = height;
        [widget->view setNeedsDisplay:YES];
    });
}

void MacGuiBringToFront(const char *id) {
    MWidget *w = find(id);
    if (w && w->view) [w->view setNeedsDisplay:YES];
}

void MacGuiRepaintImage(const char *id) {
    MWidget *w = find(id);
    if (!w || !w->view) return;

    NSView *view = w->view;
    dispatch_async(dispatch_get_main_queue(), ^{
        [view setNeedsDisplay:YES];
    });
}

void MacGuiLayout(void) {
    if (!mainContent || !g_root) {
        NSLog(@"[MacGUI] layout skipped: mainContent=%@ g_root=%@", mainContent ? @"yes" : @"no", g_root ? @"yes" : @"no");
        return;
    }
    NSRect bounds = [mainContent bounds];
    NSRect rect = NSInsetRect(bounds, 20, 20);
    // NSLog(@"[MacGUI] layout START bounds=%@ layoutRect=%@ window=%@",
    //       NSStringFromRect(bounds), NSStringFromRect(rect), NSStringFromRect(window.frame));
    layout_widget(g_root, rect);
    debug_widget_tree(g_root, 0);
    // NSLog(@"[MacGUI] layout END");
}

void MacGuiRun(void) {
    // MACGUI_DEBUG("Run START");
    MacGuiLayout();
    // MACGUI_DEBUG("Entering NSApplication run loop");
    [NSApp run];
}

static const char *mac_type_name(MType type) {
    switch (type) {
        case M_ROOT: return "ROOT";
        case M_VERTICAL: return "VERTICAL";
        case M_HORIZONTAL: return "HORIZONTAL";
        case M_GRID: return "GRID";
        case M_GROUP: return "GROUP";
        case M_LABEL: return "LABEL";
        case M_BUTTON: return "BUTTON";
        case M_INPUT: return "INPUT";
        case M_SLIDER: return "SLIDER";
        case M_TEXTAREA: return "TEXTAREA";
        case M_IMAGE: return "IMAGE";
        case M_IMAGE_PLACEHOLDER: return "IMAGE_PLACEHOLDER";
        default: return "UNKNOWN";
    }
}

static void debug_widget_tree(MWidget *w, int depth) {
    if (!w) return;
    char indent[32];
    depth = depth > 30 ? 30 : depth;
    memset(indent, ' ', depth * 2); indent[depth * 2] = '\0';
    // NSLog(@"[MacGUI] %s%s id='%s' visible=%d children=%d frame=(%.0f,%.0f %.0fx%.0f)",
    //       indent,
    //       mac_type_name(w->type),
    //       w->id[0] ? w->id : "-",
    //       w->visible,
    //       child_count(w),
    //       w->view ? NSMinX([w->view frame]) : 0.0,
    //       w->view ? NSMinY([w->view frame]) : 0.0,
    //       w->view ? NSWidth([w->view frame]) : 0.0,
    //       w->view ? NSHeight([w->view frame]) : 0.0);
    for (MWidget *c = w->first; c; c = c->next)
        debug_widget_tree(c, depth + 1);
}

@implementation MacWindowDelegate
- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    // NSLog(@"[MacGUI] windowDidResize windowFrame=%@ contentFrame=%@",
    //       NSStringFromRect(window.frame), NSStringFromRect(mainContent.bounds));
    MacGuiLayout();
}
@end
