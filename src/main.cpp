#include <iostream>
#include <thread>
#include <chrono>
#include <dev/devs.hpp>
#include <csignal>

// Global flag for program control
volatile bool running = true;

// Signal handler
void signalHandler(int signum) {
    running = false;
}

// Status callback
void onDeviceStatus(void* param, const void* data) {
    auto* status = static_cast<const Device::CameraStatus*>(data);
    std::cout << "\rBattery: " << (int)status->tail_air.battery.capacity 
              << "% | AI Mode: " << (int)status->tail_air.ai_type 
              << " | USB Status: " << (int)status->tail_air.usb_status 
              << std::flush;
}

// Device change callback
void onDeviceChange(std::string dev_sn, bool connected, void* param) {
    std::cout << "Device " << dev_sn << (connected ? " connected" : " disconnected") << std::endl;
    
    if (connected) {
        auto* device = static_cast<std::shared_ptr<Device>*>(param);
        if (device) {
            *device = Devices::get().getDevBySn(dev_sn);
            if (*device) {
                (*device)->setDevStatusCallbackFunc(onDeviceStatus, nullptr);
                (*device)->enableDevStatusCallback(true);
            }
        }
    }
}

int main() {
    std::cout << "OBSBOT Camera Test Program" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize device manager
    auto& devices = Devices::get();
    std::shared_ptr<Device> camera;
    
    // Register device callback with camera pointer
    devices.setDevChangedCallback(onDeviceChange, &camera);
    
    std::cout << "Waiting for camera connection..." << std::endl;
    
    // Wait for device connection
    int retries = 0;
    while (!camera && retries < 10 && running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        retries++;
    }
    
    if (!camera) {
        std::cout << "No camera found after " << retries << " seconds." << std::endl;
        return 1;
    }
    
    std::cout << "\nCamera Information:" << std::endl;
    std::cout << "Name: " << camera->devName() << std::endl;
    std::cout << "Serial: " << camera->devSn() << std::endl;
    std::cout << "Version: " << camera->devVersion() << std::endl;
    
    // Try to wake up the camera
    camera->cameraSetDevRunStatusR(Device::DevStatusRun);
    
    std::cout << "\nPress Ctrl+C to exit" << std::endl;
    
    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nShutting down..." << std::endl;
    
    // Clean shutdown
    if (camera) {
        camera->enableDevStatusCallback(false);
    }
    
    return 0;
}