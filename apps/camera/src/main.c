#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gui.h"

// global data for the GUI
int charts[5][1024];

static void onClick(const char *id) {
    if (strcmp(id, "send") == 0) {
        const char *command = GuiGetText("command");
        char msg[256];
        char *response = "ok"; // Simulated response for demonstration
        snprintf(msg, sizeof(msg), "%s -> %s", command, response);
        GuiSetText("response", msg);
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
    
    if (strcmp(id, "exposure") == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Exposure %s", GuiGetText(id));
        GuiSetText("exposureValue", msg);
    }
}
 static void draw_chart(const char *id, int chart[1024]) {

    GuiMoveTo(id, 0, chart[0] / 50);    
    for (int i = 1; i < 1024; i++) {
        GuiLineTo(id, i, chart[i] / 50);
    }
}

static void cbRepaintImage(const char *id) {
    GuiStartPaint(id);
    if (strcmp(id, "image1") == 0) {
        GuiSetColors("image1", "#00ff00", "#dd36b3");
        draw_chart("image1", charts[0]);
    } else if (strcmp(id, "image2") == 0) {
        GuiSetColors("image2", "#ff0000", "#0f00ff");
        GuiMoveTo("image2", 210, 10);
        GuiLineTo("image2", 310, 50);
    }
    GuiBringToFront(id);
}
        
void createGui(void)
{
    GuiCreateApp("Camera App", 1000, 850);

    GuiBeginVertical();
        GuiBeginHorizontal();

            GuiCreateButton("startCam", "START", onClickCam);
            GuiCreateButton("stopCam", "STOP", onClickCam);
            GuiCreateButton("resetCam", "RESET", onClickCam);
            GuiCreateButton("recordLine", "RECORD", onClickCam);

        GuiEndHorizontal();



        GuiBeginGroup("Parameters");
            GuiBeginVertical();
                GuiCreateLabel("exposureValue", "Exposure");
                GuiCreateSlider("exposure", "", 0, 1000, 50, onChange);
            GuiEndVertical();

        GuiEndGroup();
        GuiBeginGroup("Serial Command");
            GuiBeginHorizontal();
                GuiBeginVertical();
                    GuiCreateInput("command", "", 20, onChange);
                    //GuiCreateSlider("gain", "Gain", 0, 100, 50, onChange);
                    GuiCreateButton("send", "SEND", onClick);
                GuiEndVertical();

                GuiCreateTextArea("response", 10, 80);

            GuiEndHorizontal();
        GuiEndGroup();




        GuiCreateLabel("status", "Ready");
      
        GuiCreateImage("image1", 320, 200, cbRepaintImage);
        GuiCreateImage("image2", 320, 200, cbRepaintImage);

        GuiCreateTextArea("log", 8, 80);

    GuiEndVertical();
}
        
void initializeData(void)
{
    for (int iChart = 1; iChart < 5; iChart++) {
        for (int j = 0; j < 1024; j++) {
            charts[iChart][j] = 0;
        }
    }
    for (int j = 0; j < 1024; j++) {
        charts[0][j] = sin(j * 5 * 6.28 / 1024) * 2000 + 2000;
    }

    GuiSetText("command", "Hello");
    GuiSetText("response", "Response will appear here");
    GuiSetText("status", "Ready");
}

void initializeCamera(void)
{
    // Simulate camera initialization
    GuiSetText("status", "Camera initialized");
}
int main(void)
{


    initializeData();

    initializeCamera();

    createGui();
    GuiRun();
    
    return 0;
}
