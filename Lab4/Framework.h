//Framework.h
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <memory>

// Включаем вспомогательные заголовки DirectX
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Вспомогательные структуры DirectX 12
#include "d3dx12.h" 
