#ifndef _APP_H_
#define _APP_H_

#include "DX12.h"

class App {
  public:
    App(const HWND &hwnd, uint32_t wWidth, uint32_t wHeight);
    ~App();

    void Init();
    bool Update(double dt, double totalTime);
    void CleanUp();

  private:
    DX12 dx12;

    DirectX::XMMATRIX modelMatrix;
    DirectX::XMMATRIX viewMatrix;
    DirectX::XMMATRIX projectionMatrix;

    float fov = {45.0f};
    float nearPlane = {1.0f};
    float farPlane = {1000.0f};
};

#endif //!_APP_H_
