
#include <iostream>
#include <array>
#include <multicam.h>
#include "camApi.h"
#include "CameraSerial.h"
#define IMAGE_WIDTH        1024
#define LINES_PER_SURFACE  -1
#define SURFACE_COUNT      8

#include <chrono>
//#include <random>
#include <utility>

std::thread g_acquisitionThread;
std::atomic<bool> g_running(false);


//std::mutex g_imageMutex;

std::vector<uint16_t> g_image;

int g_imageWidth  = 0;
int g_imageHeight = 0;

int surfaces_counter = 0;
int paints_counter = 0;
int g_recordState = 0;

camera::Cam *g_single_cam = nullptr;

MCHANDLE g_channel = 0;
MCHANDLE g_surface[SURFACE_COUNT];

// ------------------------------------------------------------
// Simple status checker
// ------------------------------------------------------------

bool check_status(MCSTATUS status, const char* name)
{
    if (status != MC_OK) {
        printf("MultiCam error at %s: status = %d\n",  name, status);
        return false;
    }

    return true;
}
// ------------------------------------------------------------
// Acquisition thread
// ------------------------------------------------------------

void AcquisitionThread()
{

    while (g_running)
    {
        MCHANDLE surface;
        MCSIGNALINFO signal_info = {};

        MCSTATUS status = McWaitSignal(
            g_channel, MC_SIG_SURFACE_PROCESSING, 1000, &signal_info);

        surface = (MCHANDLE)signal_info.SignalInfo;

        if (status != MC_OK) {
            if (!g_running)
                status = McSetParamInt(surface, MC_SurfaceState, MC_SurfaceState_FREE);
                break;
            continue;
        }        

        surfaces_counter++;

        // ----------------------------------------------------
        // SignalInfo is the surface handle.
        // ----------------------------------------------------

        if (!g_running) {
            status = McSetParamInt(surface, MC_SurfaceState, MC_SurfaceState_FREE);
            if (status != MC_OK) {
                printf("MC_SurfaceState_FREE failed: %d\n", status);
            }  
            break;
        }
        int width  = 0;
        int height = 0;
        int pitch = 0;

        if (!check_status(McGetParamInt(surface, MC_SurfaceSizeX, &width), "MC_SurfaceSizeX")) {
            continue;
        }

        if (!check_status(McGetParamInt(surface, MC_SurfaceSizeY, &height), "MC_SurfaceSizeY")) {
            continue;
        }

        if (!check_status(McGetParamInt(surface, MC_SurfacePitch, &pitch), "MC_SurfacePitch")) {
            continue;
        }
        
        // ----------------------------------------------------
        // Get image address.
        // ----------------------------------------------------
        void* address = nullptr;

        status = McGetParamPtr(surface, MC_SurfaceAddr, &address);

        if (status != MC_OK) {
            printf("MC_SurfaceAddr failed: %d\n", status);
            continue;
        }

        if (address != nullptr) {
            // ------------------------------------------------
            // Copy surface into our own CPU buffer.
            //
            // For now assume tightly packed 16-bit pixels.
            // We can add pitch handling later.
            // ------------------------------------------------

            g_imageWidth  = width;
            g_imageHeight = height;   

            g_image.resize((size_t)width * (size_t)height);

            memcpy(g_image.data(), address, (size_t)width * (size_t)height * 2);

            uint16_t *data = (uint16_t *)address;
            if (g_recordState) {
                uint16_t *line = data + (height / 2) * g_imageWidth;
                int delta = g_imageWidth / 12;
                for (int i = 0 ; i < 10; i++) {
                    printf("%d, ", line[(i + 1) * delta]);
                }
                printf("\n");

                g_recordState = 0;
            }
        }

        // ----------------------------------------------------
        // VERY IMPORTANT:
        //
        // We are finished using the MultiCam surface.
        // Return it to MultiCam.
        // ----------------------------------------------------
        status = McSetParamInt(surface, MC_SurfaceState, MC_SurfaceState_FREE);
        if (status != MC_OK) {
            printf("MC_SurfaceState_FREE failed: %d\n", status);
        }

        Frame frame;
        frame.width = width;
        frame.height = height;
        frame.pixels = g_image.data();

        try {
            g_single_cam->callback_(frame);
        } catch (...) {
            // A frame callback must not terminate the camera thread.
        }

    }

    printf("Acquisition thread stopped\n");
}


namespace camera {

Cam::Cam() {
    g_single_cam = this;
}

Cam::~Cam() {
    stop();
}

int Cam::stop_camera_communication() {
    CameraSerial::Close();
    return 1;
}

int Cam::start_camera_communication() {
    std::string error;

    // First enumerate ports to identify the correct Grablink connection.

    unsigned int numPorts = 0;
    if (!CameraSerial::GetNumSerialPorts(numPorts, error)) {
        std::cerr << "GetNumSerialPorts failed: " << error << '\n';
        return 1;
    }
    std::cout << "Number of serial ports: " << numPorts << '\n';

    for (unsigned int i = 0; i < numPorts; ++i) {
        std::string id;
        if (!CameraSerial::GetPortIdentifier(i, id, error)) {
            std::cerr << "GetPortIdentifier failed for index " << i << ": " << error << '\n';
            return 1;
        }
        std::cout << i << ": " << id << '\n';
    }

    // Replace 0 with the index of the camera's actual serial port.
    if (!CameraSerial::Open(0, 57600, error)) {
        std::cerr << "Open failed: " << error << '\n';
        return 1;
    }

    // printf("open was ok now flush\n");

    // if (!CameraSerial::FlushPort()) {
    //     std::cerr << "Flush Por failed: " << error << '\n';
    //     return 1;        
    // }
    // printf("Flushport ok\n");
    {
        if (!CameraSerial::WriteCommand("EXP?", error))
            std::cerr << "Write failed: " << error << '\n';

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::array<char, 1024> response{};
        std::size_t bytesRead = 0;

        if (CameraSerial::ReadAvailable(response.data(), response.size(), bytesRead, error)) {
            // std::cout.write(response.data(), static_cast<std::streamsize>(bytesRead));
            // std::cout << '\n';
            // Useful for seeing CR/LF/etc.
            std::cout << "Chars: ";
            for (std::size_t i = 0; i < bytesRead; ++i) {
                if (response[i] == 0x0d) {
                    printf("\\r");
                } else {
                    printf("%c", static_cast<unsigned char>(response[i]));
                }
            }
            std::cout << '\n';
        } else {
            std::cerr << "Read failed: " << error << '\n';
        }
        printf("_________________________ after read\n");
    }

    {
        if (!CameraSerial::WriteCommand("EXP?", error))
            std::cerr << "Write failed: " << error << '\n';

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::array<char, 1024> response{};
        std::size_t bytesRead = 0;

        if (CameraSerial::ReadAvailable(response.data(), response.size(), bytesRead, error)) {
            // std::cout.write(response.data(), static_cast<std::streamsize>(bytesRead));
            // std::cout << '\n';
            // Useful for seeing CR/LF/etc.
            std::cout << "Chars: ";
            for (std::size_t i = 0; i < bytesRead; ++i) {
                if (response[i] == 0x0d) {
                    printf("\\r");
                } else {
                    printf("%c", static_cast<unsigned char>(response[i]));
                }
            }
            std::cout << '\n';
        } else {
            std::cerr << "Read failed: " << error << '\n';
        }
        printf("_________________________ after read\n");    
    }


    return 0;
}

int Cam::doSerialCommand(char *command, char *resp) {
    std::string error;

    if (!CameraSerial::WriteCommand(command, error)) {
        std::cerr << "Write failed: " << error << '\n';
        return -10;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::array<char, 4096> response{};
    std::size_t bytesRead = 0;

    if (CameraSerial::ReadAvailable(response.data(), response.size(), bytesRead, error)) {
        std::cout << "Chars: ";
        int iOut = 0;
        for (std::size_t i = 0; i < bytesRead; ++i) {
            if (response[i] == 0x0d) {
                printf("\\r");
                resp[iOut++] = '\r';
                resp[iOut++] = '\n';

            } else {
                printf("%c", static_cast<unsigned char>(response[i]));
                resp[iOut++] = response[i];
            }
        }
        std::cout << '\n';
        resp[iOut] = '\0';

    } else {
        std::cerr << "Read failed: " << error << '\n';
        return -10;
    }
    return 0;
}

void Cam::resetCounts() {
    surfaces_counter = 0;
    paints_counter = 0;

    // for (int i = 0 ; i < sizeof(g_image_min) / sizeof(g_image_min[0]) ; i++) {
    //     g_image_min[i] = 999999;
    // }
    // for (int i = 0 ; i < sizeof(g_image_max) / sizeof(g_image_max[0]); i++) {
    //     g_image_max[i] = 0;
    // }
}

void Cam::recordSample() {
    g_recordState = 1;

}


// ------------------------------------------------------------
// START
// ------------------------------------------------------------

void Cam::StartAcquisition()
{
    if (g_running)
        return;

    resetCounts();

    // --------------------------------------------------------
    // Enable PROCESSING signal.
    // --------------------------------------------------------

    MCSTATUS status = McSetParamInt(g_channel, MC_SignalEnable + MC_SIG_SURFACE_PROCESSING, MC_SignalEnable_ON);

    if (!check_status(status, "Enable MC_SIG_SURFACE_PROCESSING")) {
        return;
    }

    status = McSetParamInt(g_channel, MC_ChannelState, MC_ChannelState_ACTIVE);

    if (!check_status(status, "MC_ChannelState_ACTIVE")) {
        return;
    }

    g_running = true;

    g_acquisitionThread = std::thread(AcquisitionThread);

    printf("Acquisition started\n");
}

// ------------------------------------------------------------
// STOP
// ------------------------------------------------------------

void Cam::StopAcquisition()
{
    printf("Stopping Acquisition\n");

    if (!g_running)
        return;

    g_running = false;
        
    printf("Stopping Acquisition running false\n");


    if (g_acquisitionThread.joinable()) {
        printf("try to join\n");

        g_acquisitionThread.join();
    }
    printf("joined\n");


    MCSTATUS status = McSetParamInt(g_channel, MC_ChannelState, MC_ChannelState_IDLE);

    if (status != MC_OK) {
        printf("MC_ChannelState_IDLE failed: %d\n", status);
    }
    printf("MC_ChannelState_IDLE\n");

    printf("Acquisition stopped\n");
}

void Cam::initCamera() {
    MCSTATUS status;
    MCHANDLE channel = 0;
    MCSIGNALINFO signal_info;
    int i;
    uint64_t total_lines = 0, total_surfaces = 0;
    //LARGE_INTEGER freq, t0, t1;
    MCHANDLE *surface = g_surface;

    check_status(McOpenDriver(NULL), "McOpenDriver");
    check_status(McCreate(MC_CHANNEL, &channel), "McCreate(MC_CHANNEL)");
    g_channel = channel;

    check_status(McSetParamInt(channel, MC_DriverIndex, 0), "MC_DriverIndex");
    check_status(McSetParamInt(channel, MC_Connector, MC_Connector_M), "MC_Connector");
    McSetParamStr(channel, MC_CamFile, "C:\\Users\\levinsa2\\Documents\\projects\\CamGrab\\SU1024LDH2_L1024RG.cam");

    char camera[256] = {0};
    check_status(McGetParamStr(channel, MC_Camera, camera, sizeof(camera)), "MC_CamFile");
    printf("Get Camera: value='%s'\n", camera);

    check_status(McSetParamInt(channel, MC_SeqLength_Ln, LINES_PER_SURFACE), "MC_SeqLength_Ln");

    // int slider_value = SendMessage(g_slider1, TBM_GETPOS, 0, 0);

    // McSetParamInt(g_channel, MC_Expose_us, slider_value);

    for (i = 0; i < SURFACE_COUNT; i++) {
        surface[i] = 0;

        status = McCreate(MC_DEFAULT_SURFACE_HANDLE, &surface[i]);

        check_status(status, "McCreate(surface)");
        //printf("s%d h = %d \n", i, surface[i]);

    }

    for (i = 0; i < SURFACE_COUNT; i++) {
        status = McSetParamInst(channel, MC_Cluster, surface[i]);
        check_status(status, "MC_Cluster");
    }

    start_camera_communication();

}

void Cam::closeCamera() {

    MCHANDLE *surface = g_surface;
    
    McSetParamInt(g_channel, MC_ChannelState, MC_ChannelState_IDLE);

    for (int i = 0; i < SURFACE_COUNT; i++) {
        if (surface[i] != 0) McDelete(surface[i]);
    }

    if (g_channel != 0)
        McDelete(g_channel);

    stop_camera_communication();

    McCloseDriver();
}


bool Cam::init(std::uint32_t width, std::uint32_t height, FrameCallback callback) {
    //if (running_.load()) return false;
    if (width == 0 || height == 0) return false;

    width_ = width;
    height_ = height;
    if (callback) callback_ = std::move(callback);
    initCamera();
    return true;
}

bool Cam::start() {
    // if (running_.load()) return true;
    // if (width_ == 0 || height_ == 0 || !callback_) return false;

    // running_.store(true);
    // worker_ = std::thread([this]() { run(); });
    StartAcquisition();
    return true;
}

void Cam::stop() {
    StopAcquisition();
    // if (!running_.exchange(false)) {
    //     if (worker_.joinable()) worker_.join();
    //     return;
    // }

    // if (worker_.joinable()) worker_.join();
}

void Cam::run() {
    // std::random_device randomDevice;
    // std::mt19937 generator(randomDevice());
    // std::uniform_int_distribution<int> pixelDistribution(0, 255);
    int center = width_ / 2;
    while (running_.load()) {
        Frame frame;
        frame.width = width_;
        frame.height = height_;
        frame.pixels = new u_int16_t[static_cast<std::size_t>(width_) * height_];

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(width_) * height_; ++i) {
            int ii = i % width_;
            int t = 4096 - abs((center - ii) * (center - ii) / 64);
            frame.pixels[i] = t;
        }
        center = (center + 10) % width_;

        try {
            callback_(frame);
        } catch (...) {
            // A frame callback must not terminate the camera thread.
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace camera
