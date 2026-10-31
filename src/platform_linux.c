#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "gui.h"

/*
 * Minimal GTK4 backend.
 * It demonstrates the same API. The layout engine is intentionally simple.
 */

typedef struct LWidget {
    char id[64];
    GtkWidget *widget;
    GuiCallback cb;
    int value;
    struct LWidget *next;
} LWidget;

static GtkWidget *window, *root;
static LWidget *widgets;

static LWidget *add(const char *id,GtkWidget *w,GuiCallback cb)
{
    LWidget *x=calloc(1,sizeof(*x));
    strncpy(x->id,id,sizeof(x->id)-1);
    x->widget=w; x->cb=cb; x->next=widgets; widgets=x;
    return x;
}
static LWidget *find(const char *id)
{
    for(LWidget *w=widgets;w;w=w->next)
        if(strcmp(w->id,id)==0)return w;
    return NULL;
}
static void clicked(GtkButton *b,gpointer data)
{
    LWidget *w=data; if(w->cb)w->cb(w->id);
}
static void changed(GtkEditable *e,gpointer data)
{
    LWidget *w=data; if(w->cb)w->cb(w->id);
}
static void scaled(GtkRange *r,gpointer data)
{
    LWidget *w=data; w->value=(int)gtk_range_get_value(r);
    if(w->cb)w->cb(w->id);
}

int LinuxGuiCreateApp(const char *title,int width,int height)
{
    gtk_init();
    window=gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window),title);
    gtk_window_set_default_size(GTK_WINDOW(window),width,height);
    root=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_widget_set_margin_start(root,15); gtk_widget_set_margin_end(root,15);
    gtk_widget_set_margin_top(root,15); gtk_widget_set_margin_bottom(root,15);
    gtk_window_set_child(GTK_WINDOW(window),root);
    gtk_window_present(GTK_WINDOW(window));
    return 1;
}
void LinuxGuiBeginVertical(void) {}
void LinuxGuiEndVertical(void) {}
void LinuxGuiBeginHorizontal(void) {}
void LinuxGuiEndHorizontal(void) {}
void LinuxGuiBeginGrid(int n) {(void)n;}
void LinuxGuiEndGrid(void) {}
void LinuxGuiBeginGroup(const char *title)
{
    GtkWidget *f=gtk_frame_new(title);
    gtk_box_append(GTK_BOX(root),f);
}
void LinuxGuiEndGroup(void) {}

void LinuxGuiCreateLabel(const char *id,const char *text)
{
    GtkWidget *w=gtk_label_new(text); add(id,w,NULL);
    gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiCreateButton(const char *id,const char *title,GuiCallback cb)
{
    GtkWidget *w=gtk_button_new_with_label(title);
    LWidget *x=add(id,w,cb); g_signal_connect(w,"clicked",G_CALLBACK(clicked),x);
    gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiCreateInput(const char *id,const char *title,int length,GuiCallback cb)
{
    (void)title;(void)length;
    GtkWidget *w=gtk_entry_new();
    LWidget *x=add(id,w,cb); g_signal_connect(w,"changed",G_CALLBACK(changed),x);
    gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiCreateSlider(const char *id,const char *title,int min,int max,int value,GuiCallback cb)
{
    (void)title;
    GtkWidget *w=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,min,max,1);
    gtk_range_set_value(GTK_RANGE(w),value);
    LWidget *x=add(id,w,cb); x->value=value;
    g_signal_connect(w,"value-changed",G_CALLBACK(scaled),x);
    gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiCreateTextArea(const char *id,int rows,int columns)
{
    (void)rows;(void)columns;
    GtkWidget *w=gtk_text_view_new(); add(id,w,NULL);
    gtk_widget_set_vexpand(w,TRUE); gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiCreateImagePlaceholder(const char *id,const char *title)
{
    GtkWidget *w=gtk_label_new(title); add(id,w,NULL);
    gtk_widget_set_vexpand(w,TRUE); gtk_box_append(GTK_BOX(root),w);
}
void LinuxGuiSetText(const char *id,const char *text)
{
    LWidget *x=find(id); if(!x)return;
    if(GTK_IS_ENTRY(x->widget)) gtk_editable_set_text(GTK_EDITABLE(x->widget),text?text:"");
    else if(GTK_IS_LABEL(x->widget)) gtk_label_set_text(GTK_LABEL(x->widget),text?text:"");
}
const char *LinuxGuiGetText(const char *id)
{
    static char buf[512]; LWidget *x=find(id); if(!x)return "";
    if(GTK_IS_ENTRY(x->widget)) {
        strncpy(buf,gtk_editable_get_text(GTK_EDITABLE(x->widget)),sizeof(buf)-1);
        buf[sizeof(buf)-1]=0; return buf;
    }
    return "";
}
void LinuxGuiSetValue(const char *id,int value)
{
    LWidget *x=find(id); if(x && GTK_IS_RANGE(x->widget))
        gtk_range_set_value(GTK_RANGE(x->widget),value);
}
int LinuxGuiGetValue(const char *id)
{
    LWidget *x=find(id); if(!x)return 0;
    return GTK_IS_RANGE(x->widget)?(int)gtk_range_get_value(GTK_RANGE(x->widget)):x->value;
}
void LinuxGuiSetEnabled(const char *id,int enabled)
{
    LWidget *x=find(id); if(x)gtk_widget_set_sensitive(x->widget,enabled);
}
void LinuxGuiSetVisible(const char *id,int visible)
{
    LWidget *x=find(id); if(x)gtk_widget_set_visible(x->widget,visible);
}
void LinuxGuiLayout(void) {}
void LinuxGuiRepaintImage(const char *id)
{
    LWidget *x = find(id);
    if (x && x->widget) gtk_widget_queue_draw(x->widget);
}
void LinuxGuiRun(void) { while(g_main_context_iteration(NULL,TRUE)); }
