#include <iostream>
#include <thread>
#include <chrono>
#include <dev/devs.hpp>
#include <csignal>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

// Global flag for clean shutdown
volatile bool running = true;

// Signal handler for clean shutdown
void signalHandler(int signum) {
    std::cout << "\nShutdown signal received..." << std::endl;
    running = false;
}

// Helper function to execute system commands and get output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Status callback for camera updates
void onDeviceStatus(void* param, const void* data) {
    auto* status = static_cast<const Device::CameraStatus*>(data);
    std::cout << "\rStatus: "
              << "Battery: " << (int)status->tail_air.battery.capacity << "% | "
              << "Zoom: " << status->tail_air.digi_zoom_ratio/100.0 << "x | "
              << "AI Mode: " << (int)status->tail_air.ai_type 
              << std::flush;
}

// Callback for device connection/disconnection events
void onDeviceChange(std::string dev_sn, bool connected, void* param) {
    std::cout << "\nDevice " << dev_sn << (connected ? " connected" : " disconnected") << std::endl;
    
    if (connected) {
        // Print detailed device information
        std::cout << "\nUSB Device Information:" << std::endl;
        std::cout << exec("lsusb | grep -i obsbot") << std::endl;
        
        std::cout << "\nVideo Device Information:" << std::endl;
        std::cout << exec("v4l2-ctl --list-devices") << std::endl;
        
        std::cout << "\nDevice Permissions:" << std::endl;
        std::cout << exec("ls -l /dev/video*") << std::endl;
    }
}

// Print the interactive menu
void printMenu() {
    std::cout << "\n=== OBSBOT Control Menu ===" << std::endl;
    std::cout << "1: Enable AI Tracking (Human)" << std::endl;
    std::cout << "2: Disable AI Tracking" << std::endl;
    std::cout << "3: Zoom In" << std::endl;
    std::cout << "4: Zoom Out" << std::endl;
    std::cout << "5: Pan Left" << std::endl;
    std::cout << "6: Pan Right" << std::endl;
    std::cout << "7: Stop Movement" << std::endl;
    std::cout << "8: Reset To Home Position" << std::endl;
    std::cout << "9: Print Diagnostic Info" << std::endl;
    std::cout << "q: Quit" << std::endl;
    std::cout << "Enter command: " << std::flush;
}

// Print diagnostic information
void printDiagnostics(std::shared_ptr<Device>& camera) {
    std::cout << "\n=== Diagnostic Information ===" << std::endl;
    std::cout << "Device Name: " << camera->devName() << std::endl;
    std::cout << "Serial Number: " << camera->devSn() << std::endl;
    std::cout << "Firmware Version: " << camera->devVersion() << std::endl;
    std::cout << "Device Mode: " << camera->devMode() << std::endl;
    std::cout << "Video Device Path: " << camera->videoDevPath() << std::endl;
    std::cout << "Audio Device Path: " << camera->audioDevPath() << std::endl;
    
    std::cout << "\nSystem Information:" << std::endl;
    std::cout << "User and Groups:" << std::endl;
    std::cout << exec("id") << std::endl;
    
    std::cout << "USB Devices:" << std::endl;
    std::cout << exec("lsusb") << std::endl;
}

int main() {
    // Set up signal handling for clean shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "OBSBOT Camera Control Program" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Initialize device manager
    auto& devices = Devices::get();
    devices.setDevChangedCallback(onDeviceChange, nullptr);
    
    // Print initial system info
    std::cout << "\nInitial System State:" << std::endl;
    std::cout << exec("id") << std::endl;
    
    std::cout << "Looking for camera..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Get connected devices
    auto devList = devices.getDevList();
    if (devList.empty()) {
        std::cout << "No camera found!" << std::endl;
        return 1;
    }
    
    // Get first camera and set up status callback
    auto camera = devList.front();
    camera->setDevStatusCallbackFunc(onDeviceStatus, nullptr);
    camera->enableDevStatusCallback(true);
    
    std::cout << "\nConnected to camera:" << std::endl;
    std::cout << "Name: " << camera->devName() << std::endl;
    std::cout << "Serial: " << camera->devSn() << std::endl;
    
    // Try to wake up the camera
    camera->cameraSetDevRunStatusR(Device::DevStatusRun);
    
    // Main control loop
    char cmd;
    while (running) {
        printMenu();
        std::cin >> cmd;
        
        switch (cmd) {
            case '1':
                if (camera->aiSetAiTrackModeEnabledR(Device::AiTrackHumanNormal, true) == RM_RET_OK) {
                    std::cout << "AI Tracking enabled" << std::endl;
                } else {
                    std::cout << "Failed to enable AI Tracking" << std::endl;
                }
                break;
                
            case '2':
                if (camera->aiSetAiTrackModeEnabledR(Device::AiTrackNormal, false) == RM_RET_OK) {
                    std::cout << "AI Tracking disabled" << std::endl;
                } else {
                    std::cout << "Failed to disable AI Tracking" << std::endl;
                }
                break;
                
            case '3': {
                float current = camera->cameraStatus().tail_air.digi_zoom_ratio / 100.0f;
                if (camera->cameraSetZoomAbsoluteR(std::min(4.0f, current + 0.5f)) == RM_RET_OK) {
                    std::cout << "Zooming in..." << std::endl;
                } else {
                    std::cout << "Zoom failed" << std::endl;
                }
                break;
            }
                
            case '4': {
                float current = camera->cameraStatus().tail_air.digi_zoom_ratio / 100.0f;
                if (camera->cameraSetZoomAbsoluteR(std::max(1.0f, current - 0.5f)) == RM_RET_OK) {
                    std::cout << "Zooming out..." << std::endl;
                } else {
                    std::cout << "Zoom failed" << std::endl;
                }
                break;
            }
                
            case '5':
                if (camera->aiSetGimbalSpeedCtrlR(0, -20) == RM_RET_OK) {
                    std::cout << "Panning left..." << std::endl;
                } else {
                    std::cout << "Pan failed" << std::endl;
                }
                break;
                
            case '6':
                if (camera->aiSetGimbalSpeedCtrlR(0, 20) == RM_RET_OK) {
                    std::cout << "Panning right..." << std::endl;
                } else {
                    std::cout << "Pan failed" << std::endl;
                }
                break;
                
            case '7':
                if (camera->aiSetGimbalSpeedCtrlR(0, 0) == RM_RET_OK) {
                    std::cout << "Movement stopped" << std::endl;
                } else {
                    std::cout << "Stop failed" << std::endl;
                }
                break;
                
            case '8':
                if (camera->gimbalRstPosR() == RM_RET_OK) {
                    std::cout << "Returning to home position..." << std::endl;
                } else {
                    std::cout << "Home position reset failed" << std::endl;
                }
                break;
                
            case '9':
                printDiagnostics(camera);
                break;
                
            case 'q':
                running = false;
                break;
                
            default:
                std::cout << "Unknown command" << std::endl;
        }
    }
    
    // Clean shutdown
    std::cout << "\nShutting down..." << std::endl;
    if (camera) {
        camera->enableDevStatusCallback(false);
    }
    
    return 0;
}