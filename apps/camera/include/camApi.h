#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <gui.h>

namespace camera {

class Cam {
public:
    using FrameCallback = std::function<void(const Frame&)>;

    explicit Cam();
    ~Cam();

    Cam(const Cam&) = delete;
    Cam& operator=(const Cam&) = delete;

    int stop_camera_communication();
    int start_camera_communication();
    int doSerialCommand(char *command, char *resp);
    void resetCounts();
    void recordSample();
    //void AcquisitionThread();
    void StartAcquisition();
    void StopAcquisition();
    void initCamera();
    void closeCamera();

    bool init(std::uint32_t width, std::uint32_t height, FrameCallback callback = {});
    bool start();
    void stop();

    FrameCallback callback_;

private:
    void run();

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace camera
