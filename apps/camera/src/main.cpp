#include <stdio.h>
#include <string.h>
#include <math.h>
#include <multicam.h>
#include "gui.h"
#include "camApi.h"
#include <mutex>

// slider for exposure "exp 435"    (40 1000)
// slide fo digital gain "gain:digital:mult 34"   (1-256)
// capacitor mode

// global data for the GUI
static int charts[5][1024];
static int **recorded_lines = nullptr;
static int n_recorded_lines = 0;
static int m_recorded_lines = 0;
static int record_state = 0;
static char *recordedDataStr = nullptr;

static camera::Cam *g_cam = NULL;
static int g_frameCounter = 0;
static Frame g_lastFrame = {0};
static std::mutex g_frameMutex;
static std::mutex g_chartMutex;

static void onClick(const char *id) {
    if (strcmp(id, "send") == 0) {
        char resp[5000];
        const char *command = GuiGetText("command");
        g_cam->doSerialCommand(command, resp);
        GuiSetText("response", resp);
    }
}

void serializeRecordedLines() {
    if (n_recorded_lines <= 0) {
        return;
    }
    if (recordedDataStr) {
        free(recordedDataStr);
    }
    int mStr = n_recorded_lines * 100;
    recordedDataStr = (char *)malloc(mStr);
    recordedDataStr[0] = '\0';
    char msg[200];
    for (int il = 0; il < n_recorded_lines; il++) {
        int *line = recorded_lines[il]; 
        for (int i = 50; i < 1024; i += 100) {
            sprintf(msg, "%d, ", line[i]);
            strcat(recordedDataStr, msg);
        }
        strcat(recordedDataStr, "  -   \r\n");
    }
    GuiSetText("log", recordedDataStr);
}

static void resetRecordedLines() {
    if (m_recorded_lines > 0) {
        for (int il = 0; il < n_recorded_lines; il++) {
            free(recorded_lines[il]);
        }
        n_recorded_lines = 0;
        free(recorded_lines);
        recorded_lines = nullptr;
    }
    GuiSetText("log", "");
}

static void addRecordedLine(int *line) {
    if (recorded_lines == nullptr) {
        m_recorded_lines = 10;
        recorded_lines = (int **)malloc(m_recorded_lines * sizeof(int *));
        for (int il = 0; il < m_recorded_lines; il++) {
            recorded_lines[il] = nullptr;
        }
    } else if (n_recorded_lines >= m_recorded_lines) {
        int new_m_recorded_lines = m_recorded_lines * 2;
        int **new_recorded_lines = (int **)malloc(new_m_recorded_lines * sizeof(int *));
        for (int il = 0; il < m_recorded_lines; il++) {
            new_recorded_lines[il] = recorded_lines[il];
            new_recorded_lines[il + m_recorded_lines] = nullptr;
        }
        free(recorded_lines);
        recorded_lines = new_recorded_lines;
        m_recorded_lines = new_m_recorded_lines;
    }
    recorded_lines[n_recorded_lines] = (int *)malloc(sizeof(int) * 1024);
    int * new_line = recorded_lines[n_recorded_lines++];
    for (int i = 0; i < 1024; i++) {
        new_line[i] = line[i];
    }
}
static void onClickCam(const char *id) {
    // button START
    if (strcmp(id, "startCam") == 0) {
        if (g_cam) {
            g_frameCounter = 0;
            g_cam->start();
        }
        GuiSetText("status", "Start Camera");
    // button STOP
    } else if (strcmp(id, "stopCam") == 0) {
        if (g_cam) {
            g_cam->stop();
        }
        GuiSetText("status", "Stop Camera");
    // button RESET
    } else if (strcmp(id, "resetCam") == 0) {
        int *chartMin = charts[1];
        int *chartMax = charts[2];
        for (int i = 0; i < 1024; ++i) {
            chartMin[i] = 99999;
            chartMax[i] = 0;
        }
        resetRecordedLines();
        GuiSetText("status", "Reset Camera");
    // button RECORD
    } else if (strcmp(id, "recordLine") == 0) {
        fprintf(stderr, "[Dem] Record Line\n"); fflush(stderr);
        record_state = 1;
        GuiSetText("status", "Record Line");
    }
}

static void onChange(const char *id) {
    char resp[5000];
    char msg[256];

    if (!id) return;
    
    if (strcmp(id, "exposure") == 0) {
        int value = GuiGetValue(id);
        snprintf(msg, sizeof(msg), "Exp %d", value);
        g_cam->doSerialCommand(msg, resp);
        GuiSetText("response", resp);
        snprintf(msg, sizeof(msg), "Exposure %d", value);
        GuiSetText("exposureValue", msg);
    } else if (strcmp(id, "digitalGain") == 0) {
        int value = GuiGetValue(id);
        snprintf(msg, sizeof(msg), "gain:digital:mult %d", value);
        g_cam->doSerialCommand(msg, resp);
        GuiSetText("response", resp);
        snprintf(msg, sizeof(msg), "Digital Gain %d", value);
        GuiSetText("digitalGainValue", msg);
    }
}
 static void draw_chart(const char *id, int chart[1024]) {

    GuiMoveTo(id, 0, chart[0] >> 5);    
    for (int i = 1; i < 1024; i++) {
        GuiLineTo(id, i, chart[i] >> 5);
    }
}

static void cbRepaintImage(const char *id) {
    GuiStartPaint(id);
    if (strcmp(id, "image2") == 0) {
        std::lock_guard<std::mutex> lock(g_chartMutex);
        GuiSetColors("image2", "#00ff00", "#ffffff");
        GuiClear("image2");
        draw_chart("image2", charts[0]);
        GuiSetColors("image2", "#ff0000", "#ffffff");
        draw_chart("image2", charts[1]);
        GuiSetColors("image2", "#0000ff", "#ffffff");
        draw_chart("image2", charts[2]);
    } else if (strcmp(id, "image1") == 0) {
        std::lock_guard<std::mutex> lock(g_frameMutex);
        if (!g_lastFrame.pixels) {
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
                    GuiCreateLabel("digitalGainValue", "Digital Gain");
                    GuiCreateSlider("digitalGain", "", 1, 256, 50, onChange);
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

        GuiCreateTextArea("log", 20, 100);

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
        charts[0][j] = (sin(j * 5 * 6.28 / 1024) * 2048 + 2048); // Scale to fit the image height
    }

    GuiSetText("command", "Hello");
    GuiSetText("response", "Response will appear here");
    GuiSetText("status", "Ready");
}

void frameToCharts(const Frame& frame, int chartsData[5][1024]) {
    int *chart = chartsData[0];
    int *chartMin = chartsData[1];
    int *chartMax = chartsData[2];
    for (int i = 0; i < frame.width; ++i) {
        chart[i] = static_cast<int>(frame.pixels[i]);
    }
    if (record_state) {
        addRecordedLine(chart);
        record_state = 0;
    }
    for (int ih = 0; ih < frame.height; ih++) {
        u_int16_t *line = frame.pixels + ih * frame.width;
        for (int i = 0; i < frame.width; ++i) {
            if (chartMin[i] > line[i]) {
                chartMin[i] = line[i];
            }
            if (chartMax[i] < line[i]) {
                chartMax[i] = line[i];
            }
        }
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
    lastFrame->pixels = new u_int16_t[frame->width * frame->height];
    memcpy(lastFrame->pixels, frame->pixels, frame->width * frame->height * sizeof(u_int16_t));
}

Frame8 *makeFrame8(const Frame* frame) {
    Frame8 *frame8 = new Frame8();
    frame8->width = frame->width;
    frame8->height = frame->height;
    frame8->pixels = new u_int8_t[frame->width * frame->height];
    for (std::uint32_t i = 0; i < frame->width * frame->height; ++i) {
        frame8->pixels[i] = static_cast<u_int8_t>(frame->pixels[i] >> 5); // Scale down to 8 bits
    }
    return frame8;
}

void initializeCamera(void)
{
    g_cam = new camera::Cam();
    g_cam->init(1024, 100, [](const Frame& frame) {
        {
            std::lock_guard<std::mutex> chartLock(g_chartMutex);
            frameToCharts(frame, charts);
        }
        {
            std::lock_guard<std::mutex> frameLock(g_frameMutex);
            frameCopyTolastFrame(&frame, &g_lastFrame);
        }
        char msg[256];
        snprintf(msg, sizeof(msg), "Frame %d", ++g_frameCounter);
        GuiSetText("status", msg);
        GuiCopyImage("image1", makeFrame8(&frame));
        GuiRepaintImage("image2");
        serializeRecordedLines();
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
