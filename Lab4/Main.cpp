#include "Window.h"
#include "CubeRenderer.h"
#include "InputDevice.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  Window window(hInstance, 1280, 720, L"DirectX 12 - Sponza Model");
  CubeRenderer renderer;
  InputDevice inputDevice;

  if (!window.Initialize())
  {
    MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  window.SetKeyCallback([&](WPARAM key, bool pressed) {
    if (pressed)
      inputDevice.OnKeyDown(key);
    else
      inputDevice.OnKeyUp(key);
    });

  window.SetResizeCallback([&](int width, int height) {
    if (renderer.IsInitialized())
      renderer.Resize(width, height);
    });

  if (!renderer.Initialize(window.GetHandle(), window.GetWidth(), window.GetHeight()))
  {
    MessageBoxW(nullptr, L"Failed to initialize DirectX 12 renderer", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  window.Show(nCmdShow);

  while (window.ProcessMessages())
  {
    inputDevice.Update();

    renderer.Render();

    if (inputDevice.IsKeyPressed(VK_ESCAPE))
    {
      PostQuitMessage(0);
    }
  }

  renderer.Cleanup();

  return 0;
}