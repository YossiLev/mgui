#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gui.h"
#include "camApi.h"

// global data for the GUI
int charts[5][1024];
camera::Cam *g_cam = NULL;
int g_frameCounter = 0;
Frame g_lastFrame = {0};

static void onClick(const char *id) {
    if (strcmp(id, "send") == 0) {
        const char *command = GuiGetText("command");
        char msg[256];
        const char *response = "ok"; // Simulated response for demonstration
        snprintf(msg, sizeof(msg), "%s -> %s", command, response);
        GuiSetText("response", msg);
    }
}

static void onClickCam(const char *id) {
    if (strcmp(id, "startCam") == 0) {
        if (g_cam) {
            g_frameCounter = 0;
            g_cam->start();
        }
        GuiSetText("status", "Start Camera");
    } else if (strcmp(id, "stopCam") == 0) {
        if (g_cam) {
            g_cam->stop();
        }
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
    } else if (strcmp(id, "pageLength") == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Page Length %s", GuiGetText(id));
        GuiSetText("pageLengthValue", msg);
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
    if (strcmp(id, "image2") == 0) {
        GuiSetColors("image2", "#00ff00", "#dd36b3");
        draw_chart("image2", charts[0]);
    } else if (strcmp(id, "image1") == 0) {
        if (g_lastFrame.pixels) {
            Frame frameToDraw = g_lastFrame;
            GuiCopyImage("image1", &frameToDraw);
        } else {
            GuiSetColors("image1", "#ff0000", "#0f00ff");
            GuiMoveTo("image1", 210, 10);
            GuiLineTo("image1", 310, 50);
        }
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
            GuiBeginHorizontal();
                GuiBeginVertical();
                    GuiCreateLabel("exposureValue", "Exposure");
                    GuiCreateSlider("exposure", "", 0, 500, 50, onChange);
                GuiEndVertical();
                GuiBeginVertical();
                    GuiCreateLabel("pageLengthValue", "Page Length");
                    GuiCreateSlider("pageLength", "", 0, 500, 50, onChange);
                GuiEndVertical();
            GuiEndHorizontal();

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

void frameToChart(const Frame& frame, int chart[1024]) {
    for (std::uint32_t i = 0; i < std::min(frame.width, 1024); ++i) {
        chart[i] = static_cast<int>(frame.pixels[i]) * 10;
    }
}

void frameCopyTolastFrame(const Frame* frame, Frame* lastFrame) {
    if (!lastFrame) {
        lastFrame = new Frame();
    }
    if (lastFrame->pixels) {
        delete[] lastFrame->pixels;
    }
    lastFrame->width = frame->width;
    lastFrame->height = frame->height;
    lastFrame->pixels = new char[frame->width * frame->height];
    memcpy(lastFrame->pixels, frame->pixels, frame->width * frame->height);
}

void initializeCamera(void)
{
    g_cam = new camera::Cam();
    g_cam->init(400, 100, [](const Frame& frame) {
        // Handle the received frame (for demonstration, we just print its size)
        char msg[256];
        snprintf(msg, sizeof(msg), "frame %d", g_frameCounter++);
        GuiSetText("status", msg);
        frameToChart(frame, charts[0]);
        frameCopyTolastFrame(&frame, &g_lastFrame);
        GuiRepaintImage("image1");
        GuiRepaintImage("image2");
    });
    // Simulate camera initialization
    GuiSetText("status", "Camera initialized");
}
int main(void)
{
    initializeData();

    initializeCamera();

    createGui();
    GuiRun();
    
    if (g_cam) {
        delete g_cam;
    }

    return 0;
}
