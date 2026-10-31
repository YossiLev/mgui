#include "camApi.h"

#include <chrono>
#include <random>
#include <utility>

namespace camera {

Cam::Cam() {}
    

Cam::~Cam() {
    stop();
}

bool Cam::init(std::uint32_t width, std::uint32_t height, FrameCallback callback) {
    if (running_.load()) return false;
    if (width == 0 || height == 0) return false;

    width_ = width;
    height_ = height;
    if (callback) callback_ = std::move(callback);
    return true;
}

bool Cam::start() {
    if (running_.load()) return true;
    if (width_ == 0 || height_ == 0 || !callback_) return false;

    running_.store(true);
    worker_ = std::thread([this]() { run(); });
    return true;
}

void Cam::stop() {
    if (!running_.exchange(false)) {
        if (worker_.joinable()) worker_.join();
        return;
    }

    if (worker_.joinable()) worker_.join();
}

void Cam::run() {
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> pixelDistribution(0, 255);

    while (running_.load()) {
        Frame frame;
        frame.width = width_;
        frame.height = height_;
        frame.pixels = new char[static_cast<std::size_t>(width_) * height_];

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(width_) * height_; ++i) {
            frame.pixels[i] = static_cast<std::uint8_t>(pixelDistribution(generator));
        }

        try {
            callback_(frame);
        } catch (...) {
            // A frame callback must not terminate the camera thread.
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace camera
