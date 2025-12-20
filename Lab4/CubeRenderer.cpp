#include "CubeRenderer.h"
#include <iostream>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

CubeRenderer::CubeRenderer()
  : m_WindowWidth(800)
  , m_WindowHeight(600)
  , m_FenceEvent(nullptr)
  , m_WorldMatrix(XMMatrixIdentity())
  , m_ViewMatrix(XMMatrixIdentity())
  , m_ProjectionMatrix(XMMatrixIdentity())
  , m_RotationAngle(0.0f)
{
}

CubeRenderer::~CubeRenderer()
{
  Cleanup();
}

bool CubeRenderer::Initialize(HWND hwnd, int width, int height)
{
  m_WindowWidth = width;
  m_WindowHeight = height;

  if (!InitializeDirect3D(hwnd))
  {
    MessageBoxW(hwnd, L"Failed to initialize Direct3D 12", L"Error", MB_OK);
    return false;
  }

  if (!LoadShaders())
  {
    MessageBoxW(hwnd, L"Failed to load shaders", L"Error", MB_OK);
    return false;
  }

  if (!CreateBuffers())
  {
    MessageBoxW(hwnd, L"Failed to create buffers", L"Error", MB_OK);
    return false;
  }

  SetupMatrices();
  SetupLight();

  m_Initialized = true;
  return true;
}

bool CubeRenderer::InitializeDirect3D(HWND hwnd)
{
  HRESULT hr;

#ifdef _DEBUG
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
  {
    debugController->EnableDebugLayer();
  }
#endif

  // Создание фабрики DXGI
  ComPtr<IDXGIFactory4> factory;
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) return false;

  // Создание устройства (пытаемся сначала аппаратное, потом WARP)
  hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
  if (FAILED(hr))
  {
    ComPtr<IDXGIAdapter> warpAdapter;
    factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
    hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
    if (FAILED(hr)) return false;
  }

  // Создание командной очереди
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));
  if (FAILED(hr)) return false;

  // Создание свопчейна
  DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
  swapChainDesc.BufferCount = FrameCount;
  swapChainDesc.BufferDesc.Width = m_WindowWidth;
  swapChainDesc.BufferDesc.Height = m_WindowHeight;
  swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.OutputWindow = hwnd;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.Windowed = TRUE;

  ComPtr<IDXGISwapChain> swapChain;
  hr = factory->CreateSwapChain(m_CommandQueue.Get(), &swapChainDesc, &swapChain);
  if (FAILED(hr)) return false;

  hr = swapChain.As(&m_SwapChain);
  if (FAILED(hr)) return false;

  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  // Создание дескрипторной кучи для RTV
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = FrameCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));
  if (FAILED(hr)) return false;

  m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // Создание RTV для каждого буфера
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
    if (FAILED(hr)) return false;

    m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

  // Создание дескрипторной кучи для DSV
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DsvHeap));
  if (FAILED(hr)) return false;

  // Создание ресурса глубины/трафарета
  D3D12_RESOURCE_DESC depthStencilDesc;
  depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthStencilDesc.Alignment = 0;
  depthStencilDesc.Width = m_WindowWidth;
  depthStencilDesc.Height = m_WindowHeight;
  depthStencilDesc.DepthOrArraySize = 1;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthStencilDesc.SampleDesc.Count = 1;
  depthStencilDesc.SampleDesc.Quality = 0;
  depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthOptimizedClearValue;
  depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
  depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
  depthOptimizedClearValue.DepthStencil.Stencil = 0;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &depthStencilDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &depthOptimizedClearValue,
    IID_PPV_ARGS(&m_DepthStencil));
  if (FAILED(hr)) return false;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Texture2D.MipSlice = 0;
  m_Device->CreateDepthStencilView(m_DepthStencil.Get(), &dsvDesc, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());

  // Создание командных аллокаторов
  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocators[i]));
    if (FAILED(hr)) return false;
  }

  // Создание списка команд
  hr = m_Device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    m_CommandAllocators[0].Get(),
    nullptr,
    IID_PPV_ARGS(&m_CommandList));
  if (FAILED(hr)) return false;

  m_CommandList->Close();

  // Настройка окна просмотра и прямоугольника отсечения
  m_Viewport.TopLeftX = 0;
  m_Viewport.TopLeftY = 0;
  m_Viewport.Width = static_cast<float>(m_WindowWidth);
  m_Viewport.Height = static_cast<float>(m_WindowHeight);
  m_Viewport.MinDepth = 0.0f;
  m_Viewport.MaxDepth = 1.0f;

  m_ScissorRect.left = 0;
  m_ScissorRect.top = 0;
  m_ScissorRect.right = static_cast<LONG>(m_WindowWidth);
  m_ScissorRect.bottom = static_cast<LONG>(m_WindowHeight);

  // Создание забора (fence)
  hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
  if (FAILED(hr)) return false;

  m_FenceValues[0] = 0;
  m_FenceValues[1] = 0;

  m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (m_FenceEvent == nullptr) return false;

  return true;
}

bool CubeRenderer::LoadShaders()
{
  // ВЕРШИННЫЙ ШЕЙДЕР с освещением по Фонгу
  const char* vsCode = R"(
        cbuffer MatrixBuffer : register(b0)
        {
            matrix world;
            matrix view;
            matrix projection;
        };

        cbuffer LightBuffer : register(b1)
        {
            float3 lightPos;
            float padding1;
            float3 cameraPos;
            float padding2;
            float4 lightColor;
        };

        struct VS_IN
        {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float4 color : COLOR;
        };

        struct VS_OUT
        {
            float4 position : SV_POSITION;
            float3 worldPos : POSITION;
            float3 normal : NORMAL;
            float4 color : COLOR;
        };

        VS_OUT main(VS_IN input)
        {
            VS_OUT output;
            
            float4 pos = float4(input.position, 1.0f);
            pos = mul(pos, world);
            output.worldPos = pos.xyz;
            pos = mul(pos, view);
            pos = mul(pos, projection);
            output.position = pos;
            
            output.normal = mul(float4(input.normal, 0.0f), world).xyz;
            output.color = input.color;
            
            return output;
        }
    )";

  // ПИКСЕЛЬНЫЙ ШЕЙДЕР с освещением по Фонгу
  const char* psCode = R"(
        struct PS_IN
        {
            float4 position : SV_POSITION;
            float3 worldPos : POSITION;
            float3 normal : NORMAL;
            float4 color : COLOR;
        };

        cbuffer LightBuffer : register(b1)
        {
            float3 lightPos;
            float padding1;
            float3 cameraPos;
            float padding2;
            float4 lightColor;
        };

        float4 main(PS_IN input) : SV_TARGET
        {
            float3 normal = normalize(input.normal);
            float3 lightDir = normalize(lightPos - input.worldPos);
            float3 viewDir = normalize(cameraPos - input.worldPos);
            
            float diffuse = max(dot(normal, lightDir), 0.0f);
            float3 reflectDir = reflect(-lightDir, normal);
            float specular = pow(max(dot(viewDir, reflectDir), 0.0f), 32.0f);
            
            float3 ambient = float3(0.2f, 0.2f, 0.2f);
            float3 lighting = ambient + 
                             diffuse * lightColor.rgb * 0.8f + 
                             specular * 0.5f;
            
            float4 finalColor = input.color;
            finalColor.rgb *= lighting;
            
            return saturate(finalColor);
        }
    )";

  // Компиляция вершинного шейдера
  ComPtr<ID3DBlob> vertexShaderBlob;
  ComPtr<ID3DBlob> errorBlob;
  HRESULT hr = D3DCompile(
    vsCode,
    strlen(vsCode),
    "VS",
    nullptr,
    nullptr,
    "main",
    "vs_5_0",
    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
    0,
    &vertexShaderBlob,
    &errorBlob
  );

  if (FAILED(hr))
  {
    if (errorBlob)
    {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    return false;
  }

  // Компиляция пиксельного шейдера
  ComPtr<ID3DBlob> pixelShaderBlob;
  hr = D3DCompile(
    psCode,
    strlen(psCode),
    "PS",
    nullptr,
    nullptr,
    "main",
    "ps_5_0",
    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
    0,
    &pixelShaderBlob,
    &errorBlob
  );

  if (FAILED(hr))
  {
    if (errorBlob)
    {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    return false;
  }

  // Определение корневой сигнатуры с двумя CBV
  D3D12_ROOT_PARAMETER rootParameters[2];
  ZeroMemory(rootParameters, sizeof(rootParameters));

  // CBV 0: Матрицы (b0) - для вершинного шейдера
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[0].Descriptor.ShaderRegister = 0;
  rootParameters[0].Descriptor.RegisterSpace = 0;
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

  // CBV 1: Освещение (b1) - для пиксельного шейдера
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[1].Descriptor.ShaderRegister = 1;
  rootParameters[1].Descriptor.RegisterSpace = 0;
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
  rootSignatureDesc.NumParameters = 2;
  rootSignatureDesc.pParameters = rootParameters;
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signatureBlob;
  ComPtr<ID3DBlob> signatureErrorBlob;
  hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &signatureErrorBlob);

  if (FAILED(hr))
  {
    if (signatureErrorBlob)
    {
      OutputDebugStringA((char*)signatureErrorBlob->GetBufferPointer());
    }
    return false;
  }

  hr = m_Device->CreateRootSignature(
    0,
    signatureBlob->GetBufferPointer(),
    signatureBlob->GetBufferSize(),
    IID_PPV_ARGS(&m_RootSignature));

  if (FAILED(hr)) return false;

  // Определение входного макета
  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
  {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };

  // Создание PSO
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
  psoDesc.pRootSignature = m_RootSignature.Get();
  psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
  psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

  // Растеризатор (CULL_NONE как в DirectX 11)
  D3D12_RASTERIZER_DESC rasterizerDesc = {};
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // ВАЖНО: как в DX11 коде
  rasterizerDesc.FrontCounterClockwise = FALSE;
  rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  rasterizerDesc.DepthClipEnable = TRUE;
  rasterizerDesc.MultisampleEnable = FALSE;
  rasterizerDesc.AntialiasedLineEnable = FALSE;
  rasterizerDesc.ForcedSampleCount = 0;
  rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
  psoDesc.RasterizerState = rasterizerDesc;

  // Блендинг
  D3D12_BLEND_DESC blendDesc = {};
  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;

  D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
      FALSE, FALSE,
      D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
      D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
      D3D12_LOGIC_OP_NOOP,
      D3D12_COLOR_WRITE_ENABLE_ALL
  };

  for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

  psoDesc.BlendState = blendDesc;

  // Глубина/трафарет
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
  depthStencilDesc.DepthEnable = TRUE;
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  depthStencilDesc.StencilEnable = FALSE;
  depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
  depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
  depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
  psoDesc.DepthStencilState = depthStencilDesc;

  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;

  hr = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
  if (FAILED(hr)) return false;

  return true;
}

bool CubeRenderer::CreatePipelineState()
{
  return m_PipelineState != nullptr;
}

bool CubeRenderer::CreateBuffers()
{
  // Вершины куба с ГРАДИЕНТНЫМИ цветами (как в DirectX 11)
  Vertex vertices[24] = {
    // Нижняя грань (y = -1)
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // Красный
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },   // Зеленый
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },   // Синий
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },  // Желтый

    // Верхняя грань (y = 1)
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.5f, 0.0f, 0.5f, 1.0f) },   // Фиолетовый
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f) },    // Оранжевый
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },    // Бирюзовый
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.5f, 1.0f) },   // Розовый

    // Передняя грань (z = 1)
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },   // Синий
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },    // Зеленый
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },   // Красный
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },  // Желтый

    // Задняя грань (z = -1)
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }, // Желтый
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },  // Красный
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },   // Зеленый
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },   // Синий

    // Левая грань (x = -1)
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },  // Зеленый
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },   // Синий
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },   // Красный
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },   // Желтый

    // Правая грань (x = 1)
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },    // Красный
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },    // Желтый
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },    // Зеленый
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }     // Синий
  };

  // Индексы (как в DirectX 11)
  UINT indices[36] = {
      0, 1, 2,  0, 2, 3,     // Нижняя
      4, 5, 6,  4, 6, 7,     // Верхняя
      8, 9, 10, 8, 10, 11,   // Передняя
      12, 13, 14, 12, 14, 15,// Задняя
      16, 17, 18, 16, 18, 19,// Левая
      20, 21, 22, 20, 22, 23 // Правая
  };

  m_IndexCount = 36;

  // Создание вершинного буфера
  const UINT vertexBufferSize = sizeof(vertices);

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resourceDesc.Alignment = 0;
  resourceDesc.Width = vertexBufferSize;
  resourceDesc.Height = 1;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.SampleDesc.Quality = 0;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  HRESULT hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&m_VertexBuffer)
  );

  if (FAILED(hr)) return false;

  // Копирование данных вершин в буфер
  void* pVertexDataBegin = nullptr;
  D3D12_RANGE readRange = { 0, 0 };
  hr = m_VertexBuffer->Map(0, &readRange, &pVertexDataBegin);
  if (FAILED(hr)) return false;

  memcpy(pVertexDataBegin, vertices, sizeof(vertices));
  m_VertexBuffer->Unmap(0, nullptr);

  // Инициализация вершинного буферного представления
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Vertex);
  m_VertexBufferView.SizeInBytes = vertexBufferSize;

  // Создание индексного буфера
  const UINT indexBufferSize = sizeof(indices);
  resourceDesc.Width = indexBufferSize;

  hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&m_IndexBuffer)
  );

  if (FAILED(hr)) return false;

  // Копирование данных индексов в буфер
  void* pIndexDataBegin = nullptr;
  hr = m_IndexBuffer->Map(0, &readRange, &pIndexDataBegin);
  if (FAILED(hr)) return false;

  memcpy(pIndexDataBegin, indices, sizeof(indices));
  m_IndexBuffer->Unmap(0, nullptr);

  // Инициализация индексного буферного представления
  m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
  m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
  m_IndexBufferView.SizeInBytes = indexBufferSize;

  // Создание константного буфера для матриц
  const UINT constantBufferSize = (sizeof(MatrixBuffer) + 255) & ~255;
  resourceDesc.Width = constantBufferSize;

  hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&m_ConstantBuffer)
  );

  if (FAILED(hr)) return false;

  // Создание константного буфера для освещения
  const UINT lightBufferSize = (sizeof(LightBuffer) + 255) & ~255;
  resourceDesc.Width = lightBufferSize;

  hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&m_LightBuffer)
  );

  if (FAILED(hr)) return false;

  return true;
}

void CubeRenderer::SetupMatrices()
{
  // Матрицы как в DirectX 11 версии
  m_WorldMatrix = XMMatrixIdentity();

  XMVECTOR eyePosition = XMVectorSet(3.0f, 3.0f, -3.0f, 0.0f);
  XMVECTOR focusPoint = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  XMVECTOR upDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  m_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

  m_ProjectionMatrix = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,
    (float)m_WindowWidth / (float)m_WindowHeight,
    0.1f,
    100.0f
  );
}

void CubeRenderer::SetupLight()
{
  // Данные освещения как в DirectX 11
  LightBuffer lightData;
  lightData.lightPos = XMFLOAT3(2.0f, 5.0f, -3.0f);
  lightData.cameraPos = XMFLOAT3(3.0f, 3.0f, -3.0f);
  lightData.lightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

  void* pLightDataBegin = nullptr;
  D3D12_RANGE readRange = { 0, 0 };
  m_LightBuffer->Map(0, &readRange, &pLightDataBegin);
  memcpy(pLightDataBegin, &lightData, sizeof(LightBuffer));
  m_LightBuffer->Unmap(0, nullptr);
}

void CubeRenderer::PopulateCommandList()
{
  // Сброс командного аллокатора
  HRESULT hr = m_CommandAllocators[m_FrameIndex]->Reset();
  if (FAILED(hr)) return;

  // Сброс списка команд
  hr = m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), m_PipelineState.Get());
  if (FAILED(hr)) return;

  // ВРАЩЕНИЕ КАК В DIRECTX 11
  m_Timer.Tick();
  m_RotationAngle += m_Timer.GetDeltaTime() * 0.3f;  // Такая же скорость как в DX11

  // Обновление матриц - ТАКОЕ ЖЕ ВРАЩЕНИЕ как в DirectX 11
  XMMATRIX rotationX = XMMatrixRotationX(m_RotationAngle * 0.5f);
  XMMATRIX rotationY = XMMatrixRotationY(m_RotationAngle);
  XMMATRIX rotationZ = XMMatrixRotationZ(m_RotationAngle * 0.3f);
  m_WorldMatrix = rotationX * rotationY * rotationZ;

  // Обновление константного буфера матриц
  MatrixBuffer matrices;
  matrices.world = XMMatrixTranspose(m_WorldMatrix);
  matrices.view = XMMatrixTranspose(m_ViewMatrix);
  matrices.projection = XMMatrixTranspose(m_ProjectionMatrix);

  void* pConstantDataBegin = nullptr;
  D3D12_RANGE readRange = { 0, 0 };
  m_ConstantBuffer->Map(0, &readRange, &pConstantDataBegin);
  memcpy(pConstantDataBegin, &matrices, sizeof(MatrixBuffer));
  m_ConstantBuffer->Unmap(0, nullptr);

  // Установка корневой сигнатуры
  m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

  // Установка CBV для матриц (b0)
  m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress());

  // Установка CBV для освещения (b1)
  m_CommandList->SetGraphicsRootConstantBufferView(1, m_LightBuffer->GetGPUVirtualAddress());

  // Установка целей рендеринга
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHandle.ptr += m_FrameIndex * m_RtvDescriptorSize;

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();

  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  // Очистка светло-зеленым цветом (как в DirectX 11)
  float clearColor[4] = { 0.56f, 0.93f, 0.56f, 1.0f };
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // Установка состояния конвейера
  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

  // Установка примитивной топологии
  m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Установка буферов вершин и индексов
  m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  m_CommandList->IASetIndexBuffer(&m_IndexBufferView);

  // Отрисовка
  m_CommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);

  // Переход состояния ресурса
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  m_CommandList->ResourceBarrier(1, &barrier);

  // Закрытие списка команд
  hr = m_CommandList->Close();
}

void CubeRenderer::Render()
{
  if (!m_Initialized) return;

  PopulateCommandList();

  // Выполнение списка команд
  ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
  m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

  // Презентация
  m_SwapChain->Present(1, 0);

  WaitForPreviousFrame();
}

void CubeRenderer::WaitForPreviousFrame()
{
  // Сигнал забора
  const UINT64 fenceValue = m_FenceValues[m_FrameIndex];
  HRESULT hr = m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
  if (FAILED(hr)) return;

  // Обновление индекса кадра
  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  // Если последний кадр еще не закончен, ждем его завершения
  if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
  {
    hr = m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
    if (FAILED(hr)) return;

    WaitForSingleObject(m_FenceEvent, INFINITE);
  }

  // Установка значения забора для следующего кадра
  m_FenceValues[m_FrameIndex] = fenceValue + 1;
}

void CubeRenderer::Resize(int width, int height)
{
  if (!m_Initialized) return;

  m_WindowWidth = width;
  m_WindowHeight = height;

  WaitForPreviousFrame();

  // Освобождаем ресурсы
  for (UINT i = 0; i < FrameCount; i++)
  {
    m_RenderTargets[i].Reset();
    m_FenceValues[i] = m_FenceValues[m_FrameIndex];
  }

  // Изменяем размер буферов свопчейна
  DXGI_SWAP_CHAIN_DESC desc;
  m_SwapChain->GetDesc(&desc);
  HRESULT hr = m_SwapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags);
  if (FAILED(hr)) return;

  m_FrameIndex = 0;

  // Создаем новые представления для буферов рендеринга
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
    if (FAILED(hr)) return;

    m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

  // Создаем новый ресурс глубины/трафарета
  m_DepthStencil.Reset();

  D3D12_RESOURCE_DESC depthStencilDesc = {};
  depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthStencilDesc.Alignment = 0;
  depthStencilDesc.Width = width;
  depthStencilDesc.Height = height;
  depthStencilDesc.DepthOrArraySize = 1;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthStencilDesc.SampleDesc.Count = 1;
  depthStencilDesc.SampleDesc.Quality = 0;
  depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
  depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
  depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
  depthOptimizedClearValue.DepthStencil.Stencil = 0;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_Device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &depthStencilDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &depthOptimizedClearValue,
    IID_PPV_ARGS(&m_DepthStencil));

  if (FAILED(hr)) return;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Texture2D.MipSlice = 0;
  m_Device->CreateDepthStencilView(m_DepthStencil.Get(), &dsvDesc, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());

  // Обновляем вьюпорт и прямоугольник отсечения
  m_Viewport.Width = static_cast<float>(width);
  m_Viewport.Height = static_cast<float>(height);
  m_ScissorRect.right = static_cast<LONG>(width);
  m_ScissorRect.bottom = static_cast<LONG>(height);

  // Обновляем матрицу проекции
  m_ProjectionMatrix = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,
    (float)width / (float)height,
    0.1f,
    100.0f
  );
}

void CubeRenderer::Cleanup()
{
  if (!m_Initialized) return;

  WaitForPreviousFrame();

  if (m_FenceEvent)
  {
    CloseHandle(m_FenceEvent);
    m_FenceEvent = nullptr;
  }

  m_Initialized = false;
}