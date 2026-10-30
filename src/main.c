#include "gui.h"
#include <stdio.h>
#include <string.h>

static void onClick(const char *id) {
    if (id && id[0] && id[0] == 'q') {
        /* Demonstrates callback dispatch. */
        GuiSetText("status", "Quit button pressed");
    } else if (id && id[0] == 's') {
        GuiSetText("status", "START pressed");
    } else if (id && id[0] == 'r') {
        GuiSetText("status", "RESET pressed");
        GuiSetText("name", "");
        GuiSetValue("gain", 50);
    }
}

static void onClickCam(const char *id) {
    if (strcmp(id, "startCam") == 0) {
        GuiSetText("status", "Start Camera");
    } else if (strcmp(id, "stopCam") == 0) {
        GuiSetText("status", "Stop Camera");
    } else if (strcmp(id, "resetCam") == 0) {
        GuiSetText("status", "Reset Camera");
    } else if (strcmp(id, "recordLine") == 0) {
        fprintf(stderr, "[Dem] Record Line\n"); fflush(stderr);
        GuiSetText("status", "Record Line");
    }
}

static void onChange(const char *id) {
    if (!id) return;

    if (id[0] == 'n') {
        char msg[256];
        snprintf(msg, sizeof(msg), "Input changed: %s", GuiGetText(id));
        GuiSetText("status", msg);
    } else if (id[0] == 'g') {
        char msg[256];
        snprintf(msg, sizeof(msg), "Gain = %d", GuiGetValue(id));
        GuiSetText("status", msg);
    }
}

static void cbRepaintImage(const char *id) {
    GuiStartPaint(id);
    if (strcmp(id, "image1") == 0) {
        GuiSetColors("image1", "#00ff00", "#dd36b3");
        GuiMoveTo("image1", 10, 10);
        GuiLineTo("image1", 100, 50);
    } else if (strcmp(id, "image2") == 0) {
        GuiSetColors("image2", "#ff0000", "#0f00ff");
        GuiMoveTo("image2", 210, 10);
        GuiLineTo("image2", 310, 50);
    }
    GuiBringToFront(id);
}
        
int main(void)
{
    GuiCreateApp("Mini GUI Framework Demo", 1000, 850);

    GuiBeginVertical();

        GuiCreateLabel("title", "Mini GUI Framework");

        GuiBeginGrid(2);

            GuiBeginGroup("Controls");

                GuiBeginVertical();
                    GuiCreateInput("name", "Name", 20, onChange);
                    GuiCreateSlider("gain", "Gain", 0, 100, 50, onChange);

                    GuiBeginHorizontal();
                        GuiCreateButton("start", "START", onClick);
                        GuiCreateButton("reset", "RESET", onClick);
                    GuiEndHorizontal();

                GuiEndVertical();

            GuiEndGroup();

            GuiBeginGroup("Actions");

                GuiCreateButton("startCam", "START", onClickCam);
                GuiCreateButton("stopCam", "STOP", onClickCam);
                GuiCreateButton("resetCam", "RESET", onClickCam);
                GuiCreateButton("recordLine", "RECORD", onClickCam);

            GuiEndGroup();

        GuiEndGrid();

        GuiCreateLabel("status", "Ready");

        GuiCreateImagePlaceholder("image", "Image / Plot area");
       
        GuiCreateImage("image1", 320, 200, cbRepaintImage);
        GuiCreateImage("image2", 320, 200, cbRepaintImage);

        GuiCreateTextArea("log", 8, 80);

    GuiEndVertical();

    GuiRun();
    return 0;
}
