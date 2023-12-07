#include "App.h"
#include "Input.h"

using namespace DirectX;

App::App(const HWND &hwnd, uint32_t wWidth, uint32_t wHeight) : dx12(hwnd, wWidth, wHeight),
                                                                modelMatrix(XMMatrixIdentity()),
                                                                viewMatrix(XMMatrixIdentity()),
                                                                projectionMatrix(XMMatrixIdentity()) {
}

App::~App() {
}

void App::Init() {
    dx12.Initialize();
    dx12.LoadContent();
}

bool App::Update(double dt, double totalTime) {

    // INPUT:
    if (Input::Instance->KeyPressed(KEY_F)) {
        dx12.ToggleFullscreen();
    }

    if (Input::Instance->KeyDown(KEY_ESCAPE)) {
        return false;
    }

    // LOGIC:
    float          angle = (float)totalTime * 90.0;
    const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);

    modelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

    const XMVECTOR eyePos = XMVectorSet(0, 0, -10, 1);
    const XMVECTOR focusPos = XMVectorSet(0, 0, 0, 1);
    CONST XMVECTOR upDir = XMVectorSet(0, 1, 0, 0);
    viewMatrix = XMMatrixLookAtLH(eyePos, focusPos, upDir);
    float aspectRatio = dx12.windowWidth / (float)dx12.windowHeight;
    projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspectRatio, farPlane, nearPlane);
    dx12.Render(modelMatrix, viewMatrix, projectionMatrix);

    return true;
}

void App::CleanUp() {
    dx12.CleanUp();
}

void App::Resize(uint32_t w, uint32_t h) {
    dx12.Resize(w, h);
}
