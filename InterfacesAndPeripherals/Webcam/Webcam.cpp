#ifdef UNICODE
#define tcout wcout
#else
#define tcout cout
#endif 

#include <tchar.h>
#include <stdio.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <locale.h>
#include <string.h> 
#include <iostream>
#include <wdmguid.h>
#include <devguid.h>
#include <iomanip>
#include <map>
#include <thread>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "SetupAPI.lib")

#define BUFFER_SIZE 256

using namespace std;
using namespace cv;

static string SAVE_PATH = "Frames/";

string GetFrameName()
{
    time_t now = time(0);
    tm* ltm = new tm;
    localtime_s(ltm, &now);
    stringstream sstream;
    sstream << ltm->tm_year + 1900 << '_'
        << ltm->tm_mon + 1 << '_'
        << ltm->tm_mday << '_'
        << ltm->tm_hour << '_'
        << ltm->tm_min << '_'
        << ltm->tm_sec;

    return sstream.str();
}

void TakePhoto() {
    VideoCapture camera(0);
    if (camera.isOpened()) {
        Mat frame;
        camera >> frame;
        imwrite((SAVE_PATH + "photo_" + GetFrameName() + ".jpg"), frame);
    }
}

void TakeVideo() {
    static bool tracking = false;
    static VideoCapture camera;
    static VideoWriter video;

    if (!tracking) {
        tracking = true;
        thread scanner([]() {
            camera.open(0);
            if (!camera.isOpened()) {
                return;
            }

            double frame_width = camera.get(CAP_PROP_FRAME_WIDTH);
            double frame_height = camera.get(CAP_PROP_FRAME_HEIGHT);

            video.open((SAVE_PATH + "video_" + GetFrameName() + ".avi"),
                VideoWriter::fourcc('M', 'P', '4', '2'), // mpeg2 avi
                20,
                Size(frame_width, frame_height), true);
            if (!video.isOpened()) {
                return;
            }

            while (true) {
                Mat frame;
                if (!camera.read(frame)) {
                    break;
                }

                if (frame.empty()) {
                    break;
                }
                    
                video << frame;
            }
        });

        scanner.detach();
    }
    else {
        camera.release();
        video.release();
        tracking = false;
    }
}

HHOOK hKeyboardHook;

__declspec(dllexport)
LRESULT
CALLBACK
KeyboardEvent(int nCode, WPARAM wParam, LPARAM lParam) {
    static bool state = 0;
    if ((nCode == HC_ACTION) &&
        ((wParam == WM_SYSKEYDOWN) ||
            (wParam == WM_KEYDOWN))) {
        KBDLLHOOKSTRUCT key = *reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        switch (key.vkCode)
        {
            case 0x30:
            case VK_NUMPAD0: {
                state ? ShowWindow(::GetConsoleWindow(), SW_HIDE) : ShowWindow(::GetConsoleWindow(), SW_SHOW);
                state = !state;
                break;
            }
            case 0x31:
            case VK_NUMPAD1: {
                TakePhoto();
                cout << "Photo taked" << endl;
                break;
            }
            case 0x32:
            case VK_NUMPAD2: {
                TakeVideo();
                cout << "Video record toggled" << endl;
                break;
            }
            case VK_ESCAPE: {
                UnhookWindowsHookEx(hKeyboardHook);
                PostQuitMessage(0);
                break;
            }
        }
    }

    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

void MessageLoop() {
    MSG message; 
    while (GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message); 
        DispatchMessage(&message); 
    }
}

int main() {
    ShowWindow(::GetConsoleWindow(), SW_HIDE);

    setlocale(LC_ALL, "Russian");

    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_CAMERA, nullptr, nullptr, DIGCF_PRESENT);
    if (hDeviceInfo == INVALID_HANDLE_VALUE)
        return 1;
    SP_DEVINFO_DATA spDeviceInfoData = { 0 };

    ZeroMemory(&spDeviceInfoData, sizeof(SP_DEVINFO_DATA));
    spDeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    SetupDiEnumDeviceInfo(hDeviceInfo, 0, &spDeviceInfoData);

    TCHAR buffer[BUFFER_SIZE];

    cout << "CAMERA HARDWARE ID: ";
    if (!SetupDiGetDeviceRegistryProperty(
        hDeviceInfo,
        &spDeviceInfoData,
        SPDRP_HARDWAREID, nullptr,
        (PBYTE)buffer, 
        sizeof(buffer), 
        nullptr)) {
        cout << "Failed to get camera ID. Error code: " << GetLastError() << endl;
    }

    tcout << buffer << endl;

    ZeroMemory(buffer, BUFFER_SIZE);

    cout << "CAMERA VENDOR: ";
    if (!SetupDiGetDeviceRegistryProperty(
        hDeviceInfo,
        &spDeviceInfoData,
        SPDRP_MFG, nullptr,
        (PBYTE)buffer,
        sizeof(buffer),
        nullptr)) {
        cout << "Failed to get camera vendor. Error code: " << GetLastError() << endl;
    }

    tcout << buffer << endl;

    ZeroMemory(buffer, BUFFER_SIZE);

    cout << "CAMERA NAME: ";
    if (!SetupDiGetDeviceRegistryProperty(
        hDeviceInfo,
        &spDeviceInfoData,
        SPDRP_FRIENDLYNAME, nullptr,
        (PBYTE)buffer,
        sizeof(buffer),
        nullptr)) {
        cout << "Failed to get camera name. Error code: " << GetLastError() << endl;
    }

    tcout << buffer << endl << endl;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    hKeyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        (HOOKPROC)KeyboardEvent,
        hInstance,
        NULL);

    MessageLoop();
    UnhookWindowsHookEx(hKeyboardHook);
    SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return 0;
}