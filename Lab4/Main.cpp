#include "Window.h"
#include "CubeRenderer.h"
#include "InputDevice.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

<<<<<<< HEAD
  Window window(hInstance, 1280, 720, L"DirectX 12 - Sponza Model");
=======
  // Создаем окно
  Window window(hInstance, 800, 600, L"DirectX 12 Cube with Phong Lighting");
>>>>>>> 47c5a376cd7ddd16a2eceabf6f29928622f59460
  CubeRenderer renderer;
  InputDevice inputDevice;

  if (!window.Initialize())
  {
    MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

<<<<<<< HEAD
=======
  // Настройка колбэков
>>>>>>> 47c5a376cd7ddd16a2eceabf6f29928622f59460
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

<<<<<<< HEAD
=======
  // Инициализация рендерера
>>>>>>> 47c5a376cd7ddd16a2eceabf6f29928622f59460
  if (!renderer.Initialize(window.GetHandle(), window.GetWidth(), window.GetHeight()))
  {
    MessageBoxW(nullptr, L"Failed to initialize DirectX 12 renderer", L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

<<<<<<< HEAD
  window.Show(nCmdShow);

  while (window.ProcessMessages())
  {
    inputDevice.Update();

    renderer.Render();

=======
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
>>>>>>> 47c5a376cd7ddd16a2eceabf6f29928622f59460
    if (inputDevice.IsKeyPressed(VK_ESCAPE))
    {
      PostQuitMessage(0);
    }
  }

<<<<<<< HEAD
=======
  // Очистка
>>>>>>> 47c5a376cd7ddd16a2eceabf6f29928622f59460
  renderer.Cleanup();

  return 0;
}
