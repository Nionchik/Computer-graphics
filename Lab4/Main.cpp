#include "Window.h"
#include "CubeRenderer.h"
#include "InputDevice.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // Создаем окно
  Window window(hInstance, 800, 600, L"DirectX 12 Cube with Phong Lighting");
  CubeRenderer renderer;
  InputDevice inputDevice;

  if (!window.Initialize())
  {
    MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  // Настройка колбэков
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

  // Инициализация рендерера
  if (!renderer.Initialize(window.GetHandle(), window.GetWidth(), window.GetHeight()))
  {
    MessageBoxW(nullptr, L"Failed to initialize DirectX 12 renderer", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  // Показ окна
  window.Show(nCmdShow);

  // Главный цикл
  while (window.ProcessMessages())
  {
    // Обновление состояния ввода
    inputDevice.Update();

    // Рендеринг
    renderer.Render();

    // Выход по Escape
    if (inputDevice.IsKeyPressed(VK_ESCAPE))
    {
      PostQuitMessage(0);
    }
  }

  // Очистка
  renderer.Cleanup();

  return 0;
}
