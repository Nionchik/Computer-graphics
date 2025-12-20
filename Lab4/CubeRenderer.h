//CubeRenderer.h
#pragma once
#include "Framework.h"
#include "Timer.h"

struct Vertex
{
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT4 color;
};

struct MatrixBuffer
{
  XMMATRIX world;
  XMMATRIX view;
  XMMATRIX projection;
};

struct LightBuffer
{
  XMFLOAT3 lightPos;
  float padding1;
  XMFLOAT3 cameraPos;
  float padding2;
  XMFLOAT4 lightColor;
};

class CubeRenderer
{
public:
  CubeRenderer();
  ~CubeRenderer();

  bool Initialize(HWND hwnd, int width, int height);
  void Render();
  void Cleanup();
  void Resize(int width, int height);
  bool IsInitialized() const { return m_Initialized; }

private:
  bool InitializeDirect3D(HWND hwnd);
  bool LoadShaders();
  bool CreatePipelineState();
  bool CreateBuffers();
  void SetupMatrices();
  void SetupLight();
  void PopulateCommandList();
  void WaitForPreviousFrame();

  static const int FrameCount = 2;

  ComPtr<ID3D12Device> m_Device;
  ComPtr<IDXGISwapChain3> m_SwapChain;
  ComPtr<ID3D12CommandQueue> m_CommandQueue;
  ComPtr<ID3D12GraphicsCommandList> m_CommandList;
  ComPtr<ID3D12CommandAllocator> m_CommandAllocators[FrameCount];
  ComPtr<ID3D12RootSignature> m_RootSignature;
  ComPtr<ID3D12PipelineState> m_PipelineState;
  ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
  ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
  ComPtr<ID3D12Resource> m_RenderTargets[FrameCount];
  ComPtr<ID3D12Resource> m_DepthStencil;
  ComPtr<ID3D12Resource> m_VertexBuffer;
  ComPtr<ID3D12Resource> m_IndexBuffer;
  ComPtr<ID3D12Resource> m_ConstantBuffer;
  ComPtr<ID3D12Resource> m_LightBuffer;

  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};
  D3D12_VIEWPORT m_Viewport = {};
  D3D12_RECT m_ScissorRect = {};

  UINT m_RtvDescriptorSize = 0;
  UINT m_FrameIndex = 0;
  UINT m_IndexCount = 0;
  HANDLE m_FenceEvent;
  ComPtr<ID3D12Fence> m_Fence;
  UINT64 m_FenceValues[FrameCount] = {};

  XMMATRIX m_WorldMatrix;
  XMMATRIX m_ViewMatrix;
  XMMATRIX m_ProjectionMatrix;
  float m_RotationAngle = 0.0f;
  int m_WindowWidth;
  int m_WindowHeight;
  bool m_Initialized = false;
  Timer m_Timer;
};