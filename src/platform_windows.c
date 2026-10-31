#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#define WM_GUI_REPAINT_IMAGE (WM_APP + 1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"

/*
 * Native Win32 backend.
 *
 * The application builds a small layout tree while calling Begin/End
 * functions.  Native HWNDs are created for widgets, and WinGuiLayout()
 * recursively lays that tree out whenever the window is resized.
 */

typedef enum {
    W_ROOT,
    W_VERTICAL,
    W_HORIZONTAL,
    W_GRID,
    W_GROUP,
    W_LABEL,
    W_BUTTON,
    W_INPUT,
    W_SLIDER,
    W_TEXTAREA,
    W_IMAGE
} WType;

typedef struct WWidget WWidget;

struct WWidget {
    char id[64];
    char text[512];
    WType type;
    HWND hwnd;
    HWND label;
    GuiCallback callback;
    int value;
    int min, max;
    int visible;
    int columns;
    int pw, ph;
    PAINTSTRUCT ps;
    HDC hdc;
    unsigned char *imagePixels;
    int imageWidth;
    int imageHeight;
    HPEN hOldPen;
    HBRUSH hOldBrush;

    WWidget *parent;
    WWidget *first;
    WWidget *last;
    WWidget *next;
};

static HINSTANCE g_instance;
static HWND g_main;
static WWidget *g_root;
static WWidget *g_current;
static int g_next_id = 1000;

static WWidget *find_in_tree(WWidget *node, const char *id)
{
    if (!node) return NULL;
    if (node->id[0] && strcmp(node->id, id) == 0) return node;

    for (WWidget *c = node->first; c; c = c->next) {
        WWidget *found = find_in_tree(c, id);
        if (found) return found;
    }
    return NULL;
}

static WWidget *find_in_tree_by_hwnd(WWidget *node, HWND hWnd)
{
    if (!node) return NULL;
    if (node->hwnd == hWnd) return node;

    for (WWidget *c = node->first; c; c = c->next) {
        WWidget *found = find_in_tree_by_hwnd(c, hWnd);
        if (found) return found;
    }
    return NULL;
}

static WWidget *find(const char *id)
{
    return find_in_tree(g_root, id);
}

static WWidget *new_node(WType type, const char *id, const char *text,
                         GuiCallback callback)
{
    WWidget *w = (WWidget *)calloc(1, sizeof(*w));
    if (!w) return NULL;

    w->type = type;
    w->callback = callback;
    w->visible = 1;

    if (id) strncpy(w->id, id, sizeof(w->id) - 1);
    if (text) strncpy(w->text, text, sizeof(w->text) - 1);

    return w;
}

static void append_child(WWidget *parent, WWidget *child)
{
    child->parent = parent;
    child->next = NULL;

    if (!parent->first)
        parent->first = child;
    else
        parent->last->next = child;

    parent->last = child;
}

static WWidget *add_node(WType type, const char *id, const char *text,
                         GuiCallback callback)
{
    WWidget *w = new_node(type, id, text, callback);
    if (!w) return NULL;
    append_child(g_current, w);
    return w;
}

static int is_container(WType type)
{
    return type == W_ROOT || type == W_VERTICAL || type == W_HORIZONTAL ||
           type == W_GRID || type == W_GROUP;
}

static int child_count(WWidget *w)
{
    int n = 0;
    for (WWidget *c = w->first; c; c = c->next)
        if (c->visible) ++n;
    return n;
}

static int preferred_height(WWidget *w)
{
    if (!w) return 0;

    switch (w->type) {
        case W_LABEL:    return 30;
        case W_BUTTON:   return 34;
        case W_INPUT:    return 56;
        case W_SLIDER:   return 60;
        case W_TEXTAREA: return 120;
        case W_IMAGE:    return w->ph;
        case W_GROUP: {
            int h = 0;
            for (WWidget *c = w->first; c; c = c->next) {
                int ch = preferred_height(c);
                if (ch > h) h = ch;
            }
            return h + 34;
        }
        case W_HORIZONTAL: {
            int h = 34;
            for (WWidget *c = w->first; c; c = c->next) {
                int ch = preferred_height(c);
                if (ch > h) h = ch;
            }
            return h;
        }

        case W_GRID: {
            int cols = w->columns > 0 ? w->columns : 1;
            int count = child_count(w);
            int rows = count ? (count + cols - 1) / cols : 1;
            int h = 0;
            for (WWidget *c = w->first; c; c = c->next) {
                int ch = preferred_height(c);
                if (ch > h) h = ch;
            }
            return rows * h + (rows - 1) * 8;
        }

        case W_VERTICAL: {
            int h = 0;
            int n = 0;
            for (WWidget *c = w->first; c; c = c->next) {
                if (!c->visible) continue;
                h += preferred_height(c);
                ++n;
            }
            return h + (n > 1 ? (n - 1) * 8 : 0);
        }

        case W_ROOT:
        default:
            return 0;
    }
}

static int preferred_width(WWidget *w)
{
    if (!w) return 0;

    switch (w->type) {
        case W_BUTTON: return 120;
        case W_LABEL: return 150;
        case W_INPUT: return 300;
        case W_SLIDER: return 300;
        case W_IMAGE: return w->pw;
        case W_TEXTAREA: return 500;
        case W_GROUP: return 250;
        default: return 100;
    }
}

static void set_rect(HWND hwnd, int x, int y, int width, int height)
{
    if (!hwnd) return;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    MoveWindow(hwnd, x, y, width, height, TRUE);
}

static void layout_node(WWidget *node, RECT rc);

static void layout_vertical_children(WWidget *node, RECT rc)
{
    const int gap = 8;
    int n = child_count(node);
    if (n == 0) return;

    int total_pref = 0;
    for (WWidget *c = node->first; c; c = c->next)
        if (c->visible) total_pref += preferred_height(c);

    int total_gap = (n - 1) * gap;
    int available = rc.bottom - rc.top - total_gap;
    int extra = available - total_pref;
    int y = rc.top;

    for (WWidget *c = node->first; c; c = c->next) {
        if (!c->visible) continue;

        int h = preferred_height(c);
        if (extra > 0) h += extra / n;

        RECT child_rc = { rc.left, y, rc.right, y + h };
        layout_node(c, child_rc);
        y += h + gap;
    }
}

static void layout_node(WWidget *node, RECT rc)
{
    if (!node || !node->visible) return;

    const int gap = 8;
    const int pad = 10;

    if (node->type == W_GROUP) {
        set_rect(node->hwnd, rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top);

        RECT inner = rc;
        inner.left += pad;
        inner.top += 24;
        inner.right -= pad;
        inner.bottom -= pad;

        /* Group children are normal main-window children, so callbacks
           still arrive at the main window procedure. */
        layout_vertical_children(node, inner);
        return;
    }

    if (!is_container(node->type)) {
        if (node->type == W_INPUT || node->type == W_SLIDER) {
            int label_h = 22;
            if (node->label)
                set_rect(node->label, rc.left, rc.top,
                         rc.right - rc.left, label_h);
            set_rect(node->hwnd, rc.left, rc.top + label_h + 2,
                     rc.right - rc.left, rc.bottom - rc.top - label_h - 2);
        } else {
            set_rect(node->hwnd, rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top);
        }
        return;
    }

    if (node->type == W_VERTICAL || node->type == W_ROOT) {
        layout_vertical_children(node, rc);
        return;
    }

    if (node->type == W_HORIZONTAL) {
        int n = child_count(node);
        if (n == 0) return;

        int available = rc.right - rc.left - (n - 1) * gap;
        int cell_w = available / n;
        int x = rc.left;

        for (WWidget *c = node->first; c; c = c->next) {
            if (!c->visible) continue;
            RECT child_rc = { x, rc.top, x + cell_w, rc.bottom };
            layout_node(c, child_rc);
            x += cell_w + gap;
        }
        return;
    }

    if (node->type == W_GRID) {
        int cols = node->columns > 0 ? node->columns : 1;
        int n = child_count(node);
        if (n == 0) return;

        int rows = (n + cols - 1) / cols;
        int total_w_gap = (cols - 1) * gap;
        int total_h_gap = (rows - 1) * gap;
        int cell_w = (rc.right - rc.left - total_w_gap) / cols;
        int cell_h = (rc.bottom - rc.top - total_h_gap) / rows;

        int index = 0;
        for (WWidget *c = node->first; c; c = c->next) {
            if (!c->visible) continue;

            int row = index / cols;
            int col = index % cols;
            int x = rc.left + col * (cell_w + gap);
            int y = rc.top + row * (cell_h + gap);
            RECT child_rc = { x, y, x + cell_w, y + cell_h };
            layout_node(c, child_rc);
            ++index;
        }
    }
}

static void destroy_tree(WWidget *node)
{
    if (!node) return;
    for (WWidget *c = node->first; c;) {
        WWidget *next = c->next;
        destroy_tree(c);
        free(c);
        c = next;
    }
    node->first = node->last = NULL;
}

static WWidget *find_by_hwnd(WWidget *node, HWND hwnd)
{
    if (!node) return NULL;
    if (node->hwnd == hwnd || node->label == hwnd) return node;

    for (WWidget *c = node->first; c; c = c->next) {
        WWidget *found = find_by_hwnd(c, hwnd);
        if (found) return found;
    }
    return NULL;
}

static HWND create_control(const char *class_name, const char *text,
                           DWORD style, DWORD exstyle, HWND parent)
{
    return CreateWindowExA(
        exstyle, class_name, text ? text : "", style,
        0, 0, 10, 10, parent,
        (HMENU)(INT_PTR)g_next_id++, g_instance, NULL);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_COMMAND: {
            HWND source = (HWND)lp;
            WWidget *w = find_by_hwnd(g_root, source);
            if (!w || !w->callback) break;

            if (HIWORD(wp) == BN_CLICKED || HIWORD(wp) == EN_CHANGE)
                w->callback(w->id);
            break;
        }

        case WM_HSCROLL: {
            HWND source = (HWND)lp;
            WWidget *w = find_by_hwnd(g_root, source);
            if (w && w->callback) {
                w->value = (int)SendMessage(source, TBM_GETPOS, 0, 0);
                w->callback(w->id);
            }
            break;
        }

        case WM_SIZE:
            GuiLayout();
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void winDrawImagePixels(WWidget *w, HDC dc);

LRESULT CALLBACK imageControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_GUI_REPAINT_IMAGE:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            WWidget *w = find_in_tree_by_hwnd(g_root, hwnd);
            if (w) {
                winDrawImagePixels(w, dc);
                if (w->callback) {
                    w->hdc = dc;
                    w->ps = ps;
                    w->callback(w->id);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WinGuiCreateApp(const char *title, int width, int height)
{
    g_instance = GetModuleHandleA(NULL);

    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES };
    InitCommonControlsEx(&ic);

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MiniGuiFrameworkWindow";
    RegisterClassA(&wc);

    WNDCLASSA wci;
    ZeroMemory(&wci, sizeof(wci));
    wci.lpfnWndProc = imageControlWndProc;
    wci.hInstance = g_instance;
    wci.lpszClassName = "GuiImageControl";
    wci.hCursor = LoadCursor(NULL, IDC_ARROW);
    wci.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wci);

    g_main = CreateWindowA(
        wc.lpszClassName, title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL, g_instance, NULL);

    if (!g_main) return 0;

    g_root = new_node(W_ROOT, "__root__", NULL, NULL);
    g_current = g_root;

    ShowWindow(g_main, SW_SHOW);
    UpdateWindow(g_main);
    return 1;
}

void WinGuiBeginVertical(void)
{
    WWidget *w = add_node(W_VERTICAL, NULL, NULL, NULL);
    if (w) g_current = w;
}

void WinGuiEndVertical(void)
{
    if (g_current && g_current->parent)
        g_current = g_current->parent;
}

void WinGuiBeginHorizontal(void)
{
    WWidget *w = add_node(W_HORIZONTAL, NULL, NULL, NULL);
    if (w) g_current = w;
}

void WinGuiEndHorizontal(void)
{
    if (g_current && g_current->parent)
        g_current = g_current->parent;
}

void WinGuiBeginGrid(int columns)
{
    WWidget *w = add_node(W_GRID, NULL, NULL, NULL);
    if (w) {
        w->columns = columns > 0 ? columns : 1;
        g_current = w;
    }
}

void WinGuiEndGrid(void)
{
    if (g_current && g_current->parent)
        g_current = g_current->parent;
}

void WinGuiBeginGroup(const char *title)
{
    WWidget *w = add_node(W_GROUP, NULL, title, NULL);
    if (!w) return;

    w->hwnd = create_control(
        "BUTTON", title,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, g_main);

    /* Group children must have the group box as their native parent. */
    g_current = w;
}

void WinGuiEndGroup(void)
{
    if (g_current && g_current->parent)
        g_current = g_current->parent;
}

void WinGuiCreateLabel(const char *id, const char *text)
{
    WWidget *w = add_node(W_LABEL, id, text, NULL);
    if (!w) return;

    HWND parent = g_main;
    w->hwnd = create_control("STATIC", text,
                             WS_CHILD | WS_VISIBLE,
                             0, parent);
}

void WinGuiCreateButton(const char *id, const char *title, GuiCallback cb)
{
    WWidget *w = add_node(W_BUTTON, id, title, cb);
    if (!w) return;

    HWND parent = g_main;
    w->hwnd = create_control("BUTTON", title,
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             0, parent);
}

void WinGuiCreateInput(const char *id, const char *title, int length,
                       GuiCallback cb)
{
    WWidget *w = add_node(W_INPUT, id, "", cb);
    if (!w) return;

    HWND parent = g_main;

    w->label = create_control("STATIC", title,
                              WS_CHILD | WS_VISIBLE,
                              0, parent);
    w->hwnd = create_control("EDIT", "",
                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                             WS_EX_CLIENTEDGE, parent);
    (void)length;
}

void WinGuiCreateSlider(const char *id, const char *title,
                        int min, int max, int value, GuiCallback cb)
{
    WWidget *w = add_node(W_SLIDER, id, title, cb);
    if (!w) return;

    w->min = min;
    w->max = max;
    w->value = value;

    HWND parent = g_main;

    w->label = create_control("STATIC", title,
                              WS_CHILD | WS_VISIBLE,
                              0, parent);
    w->hwnd = create_control("msctls_trackbar32", "",
                             WS_CHILD | WS_VISIBLE | TBS_HORZ,
                             0, parent);

    SendMessageA(w->hwnd, TBM_SETRANGE, TRUE, MAKELONG(min, max));
    SendMessageA(w->hwnd, TBM_SETPOS, TRUE, value);
}

void WinGuiCreateTextArea(const char *id, int rows, int columns)
{
    WWidget *w = add_node(W_TEXTAREA, id, "", NULL);
    if (!w) return;

    HWND parent = g_main;
    w->hwnd = create_control(
        "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
        WS_EX_CLIENTEDGE, parent);
    (void)rows;
    (void)columns;
}

void WinGuiCreateImagePlaceholder(const char *id, const char *title)
{
    WWidget *w = add_node(W_IMAGE, id, title, NULL);
    if (!w) return;

    HWND parent = g_main;
    w->hwnd = create_control("STATIC", title,
                             WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                             WS_EX_CLIENTEDGE, parent);
}

void WinGuiCreateImage(const char *id, int pw, int ph, GuiCallback cb)
{
    WWidget *w = add_node(W_IMAGE, id, "", cb);
    if (!w) return;
    w->pw = pw;
    w->ph = ph;

    HWND parent = g_main;

    w->hwnd = create_control("GuiImageControl", "",
                             WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                             WS_EX_CLIENTEDGE, parent);
}

void WinGuiSetText(const char *id, const char *text)
{
    WWidget *w = find(id);
    if (!w) return;

    strncpy(w->text, text ? text : "", sizeof(w->text) - 1);
    w->text[sizeof(w->text) - 1] = 0;

    if (w->hwnd)
        SetWindowTextA(w->hwnd, w->text);
}

const char *WinGuiGetText(const char *id)
{
    static char buf[512];
    WWidget *w = find(id);
    if (!w) return "";

    if (w->hwnd)
        GetWindowTextA(w->hwnd, buf, sizeof(buf));
    else
        strncpy(buf, w->text, sizeof(buf) - 1);

    buf[sizeof(buf) - 1] = 0;
    return buf;
}

void WinGuiSetValue(const char *id, int value)
{
    WWidget *w = find(id);
    if (!w) return;

    w->value = value;
    if (w->type == W_SLIDER && w->hwnd)
        SendMessageA(w->hwnd, TBM_SETPOS, TRUE, value);
}

int WinGuiGetValue(const char *id)
{
    WWidget *w = find(id);
    if (!w) return 0;

    if (w->type == W_SLIDER && w->hwnd)
        w->value = (int)SendMessageA(w->hwnd, TBM_GETPOS, 0, 0);

    return w->value;
}

void WinGuiSetEnabled(const char *id, int enabled)
{
    WWidget *w = find(id);
    if (!w) return;
    if (w->hwnd) EnableWindow(w->hwnd, enabled);
    if (w->label) EnableWindow(w->label, enabled);
}

void WinGuiSetVisible(const char *id, int visible)
{
    WWidget *w = find(id);
    if (!w) return;

    w->visible = visible != 0;
    if (w->hwnd) ShowWindow(w->hwnd, visible ? SW_SHOW : SW_HIDE);
    if (w->label) ShowWindow(w->label, visible ? SW_SHOW : SW_HIDE);
    GuiLayout();
}

void WinGuiStartPaint(const char *id) {
    WWidget *w = find(id);
    if (!w) return;
    (void)w;
}

static COLORREF parseColor(const char *s, COLORREF fallback) {
    unsigned int r, g, b;
    if (!s) return fallback;
    if (s[0] == '#') ++s;
    if (strlen(s) != 6 || sscanf_s(s, "%2x%2x%2x", &r, &g, &b) != 3) return fallback;
    return RGB(r, g, b);
}

void WinGuiSetColors(const char *id, const char *pc, const char *bc) {
    WWidget *w = find(id);
    if (!w) return;
    HPEN hPen = CreatePen(PS_SOLID, 3, parseColor(pc, RGB(0, 0, 0)));
    HPEN hOldPen = (HPEN)SelectObject(w->hdc, hPen);
    if (w->hOldPen == NULL) {
        w->hOldPen = hOldPen;
    } else if (w->hOldPen != hOldPen) {
        DeleteObject(hOldPen);
    }
    HBRUSH hBrush = CreateSolidBrush(parseColor(bc, RGB(255, 255, 255)));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(w->hdc, hBrush);
    if (w->hOldBrush == NULL) {
        w->hOldBrush = hOldBrush;
    } else if (w->hOldBrush != hOldBrush) {
        DeleteObject(hOldBrush);
    }
}
void WinGuiMoveTo(const char *id, int x, int y) {
    WWidget *w = find(id);
    if (!w) return;
    MoveToEx(w->hdc, x, y, NULL);    
}
void WinGuiLineTo(const char *id, int x, int y) {
    WWidget *w = find(id);
    if (!w) return;
    LineTo(w->hdc, x, y);     
}
void WinGuiCopyImage(const char *id, const Frame *frame) {
    WWidget *w = find(id);
    if (!w || !frame || !frame->pixels || frame->width <= 0 || frame->height <= 0)
        return;

    size_t pixelCount = (size_t)frame->width * (size_t)frame->height;
    unsigned char *pixels = (unsigned char *)malloc(pixelCount);
    if (!pixels) return;
    memcpy(pixels, frame->pixels, pixelCount);

    EnterCriticalSection(&winImageCriticalSection);
    if (w->imagePixels) free(w->imagePixels);
    w->imagePixels = pixels;
    w->imageWidth = frame->width;
    w->imageHeight = frame->height;
    LeaveCriticalSection(&winImageCriticalSection);

    if (w->hwnd)
        PostMessageA(w->hwnd, WM_GUI_REPAINT_IMAGE, 0, 0);
}

void WinGuiBringToFront(const char *id) {
    WWidget *w = find(id);
    if (!w) return;
    if (w->hOldPen != NULL) {
        HPEN otherPen = (HPEN)SelectObject(w->hdc, w->hOldPen);
        DeleteObject(otherPen);
    };
}

void WinGuiRepaintImage(const char *id) {
    WWidget *w = find(id);
    if (!w || !w->hwnd) return;
    PostMessageA(w->hwnd, WM_GUI_REPAINT_IMAGE, 0, 0);
}

void WinGuiLayout(void)
{
    if (!g_main || !g_root) return;

    RECT rc;
    GetClientRect(g_main, &rc);

    const int margin = 20;
    rc.left += margin;
    rc.top += margin;
    rc.right -= margin;
    rc.bottom -= margin;

    if (rc.right < rc.left) rc.right = rc.left;
    if (rc.bottom < rc.top) rc.bottom = rc.top;

    layout_node(g_root, rc);
}

void WinGuiRun(void)
{
    GuiLayout();

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    destroy_tree(g_root);
    free(g_root);
    g_root = NULL;
    g_current = NULL;
}

static void winDrawImagePixels(WWidget *w, HDC dc) {
    if (!winImageCriticalSectionInitialized || !w || !w->imagePixels ||
        w->imageWidth <= 0 || w->imageHeight <= 0)
        return;

    EnterCriticalSection(&winImageCriticalSection);

    size_t bmiSize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, bmiSize);
    if (!bmi) {
        LeaveCriticalSection(&winImageCriticalSection);
        return;
    }

    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = w->imageWidth;
    bmi->bmiHeader.biHeight = -w->imageHeight;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 8;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biSizeImage = (DWORD)((size_t)w->imageWidth * w->imageHeight);

    for (int i = 0; i < 256; ++i) {
        bmi->bmiColors[i].rgbRed = (BYTE)i;
        bmi->bmiColors[i].rgbGreen = (BYTE)i;
        bmi->bmiColors[i].rgbBlue = (BYTE)i;
    }

    SetDIBitsToDevice(dc, 0, 0, w->imageWidth, w->imageHeight,
                      0, 0, 0, w->imageHeight,
                      w->imagePixels, bmi, DIB_RGB_COLORS);

    LeaveCriticalSection(&winImageCriticalSection);
    free(bmi);
}

static CRITICAL_SECTION winImageCriticalSection;
static int winImageCriticalSectionInitialized;
