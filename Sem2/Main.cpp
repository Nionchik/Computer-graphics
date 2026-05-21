#include "Window.h"
#include "Renderer.h"
#include "InputDevice.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  Window window(hInstance, 1280, 720, L"DirectX 12 - Textured Model");
  Renderer renderer;
  InputDevice input;

  if (!window.Initialize())
  {
    MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  window.SetKeyCallback([&](WPARAM key, bool pressed)
    {
      if (pressed)
        input.OnKeyDown(key);
      else
        input.OnKeyUp(key);
    });

  window.SetResizeCallback([&](int width, int height)
    {
      if (renderer.IsInitialized())
        renderer.Resize(width, height);
    });

  if (!renderer.Initialize(window.GetHandle(), window.GetWidth(), window.GetHeight()))
  {
    MessageBoxW(nullptr, L"Failed to initialize DirectX 12 renderer", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  renderer.SetTexTiling(5.0f, 5.0f);
  renderer.SetTexScroll(0.2f, 0.0f);

  window.Show(nCmdShow);

  while (window.ProcessMessages())
  {
    input.Update();

    renderer.Render();

    if (input.IsKeyPressed(VK_ESCAPE))
    {
      PostQuitMessage(0);
    }
  }

  renderer.Cleanup();

  return 0;
}