#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "Input.h"


void ToggleFullscreen(HWND hwnd, WINDOWPLACEMENT &wpPrev) {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(hwnd, &wpPrev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE,
                          style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

int64_t HiResPerformanceFreq() {
    LARGE_INTEGER countsPerSec;
    QueryPerformanceFrequency(&countsPerSec);
    int64_t result = countsPerSec.QuadPart;
    return result;
}

int64_t HiResPerformanceQuery() {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    int64_t result = currentTime.QuadPart;
    return result;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main(int argc, char **argv) {

    // NOTE(pf): Window setup.
    const wchar_t *appName = L"DX12";
    int            screenW = 800; // 1280;
    int            screenH = 800; // 720;

    WNDCLASSEX wc = {0};
    WINDOWPLACEMENT windowPlacementPrev = {sizeof(WINDOWPLACEMENT)};
    HINSTANCE  hinstance = GetModuleHandle(NULL);

    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(NULL, IDC_ARROW);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = appName;
    wc.cbSize = sizeof(WNDCLASSEX);

    RegisterClassEx(&wc);

    RECT rect = {0};
    rect.top = 50;
    rect.left = 50;
    rect.bottom = 0 + screenH;
    rect.right = 0 + screenW;
    DWORD windowStyles = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
    AdjustWindowRect(&rect, windowStyles, true);
    HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW, appName, appName,
                               windowStyles,
                               rect.left, rect.top, rect.right + 8, rect.bottom + 32,
                               NULL, NULL, hinstance, NULL);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // NOTE(pf): State setup:
    double totalTime = 0.0f;
    double prevTotalTime = 0.0f;
    double deltaTime = 0.0f;
    bool   isRunning = true;

    Input input = {0};
    input.Instance = &input;

    while (isRunning) {
        int64_t startTime = HiResPerformanceQuery();
        MSG     msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            switch (msg.message) {
            case WM_QUIT: {
                isRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                unsigned int vkCode = (unsigned int)msg.wParam;
                bool WasDown = ((msg.lParam & (1 << 30)) != 0);
                bool isDown = ((msg.lParam & (1 << 31L)) == 0);
                if (WasDown != isDown) {
                    Input::Instance->UpdateKey(vkCode, isDown);
                }

                bool altKeyWasDown = (msg.lParam & (1 << 29));
                if ((vkCode == VK_F4) && altKeyWasDown) {
                    isRunning = false;
                }
            } break;

            default: {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            } break;
            }
        }

        if (Input::Instance->KeyDown(KEY_ESCAPE)) {
            isRunning = false;
        }
        
        if (Input::Instance->KeyPressed(KEY_F) || 
            (Input::Instance->KeyDown(KEY_ALT) && Input::Instance->KeyPressed(KEY_ENTER))) {
            ToggleFullscreen(hwnd, windowPlacementPrev);
        }

        Input::Instance->Update();
        
        int64_t endTime = HiResPerformanceQuery();
        deltaTime = (float)Max(((endTime - startTime) / (double)HiResPerformanceFreq()), (int64_t)0);
        totalTime += deltaTime;
        deltaTime = (float)deltaTime;

        if ((totalTime - prevTotalTime) >= 0.5f) {
            printf("FPS: %ld, Delta(ms): %lf Elapsed Time: %ld\n", (long)(1.0f / deltaTime), deltaTime * 1000.0f, (long)totalTime);
            prevTotalTime = totalTime;
        }
    }

    return 0;
}