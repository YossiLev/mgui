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

    bool init(std::uint32_t width, std::uint32_t height, FrameCallback callback = {});
    bool start();
    void stop();

private:
    void run();

    FrameCallback callback_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace camera
