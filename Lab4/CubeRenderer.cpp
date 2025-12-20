#include "CubeRenderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <DirectXColors.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

CubeRenderer::CubeRenderer()
  : m_WindowWidth(1280)
  , m_WindowHeight(720)
  , m_FenceEvent(nullptr)
  , m_WorldMatrix(XMMatrixIdentity())
  , m_ViewMatrix(XMMatrixIdentity())
  , m_ProjectionMatrix(XMMatrixIdentity())
  , m_MinBounds(0.0f, 0.0f, 0.0f)
  , m_MaxBounds(0.0f, 0.0f, 0.0f)
  , m_Center(0.0f, 0.0f, 0.0f)
  , m_Radius(1.0f)
  , m_CameraPosition(0.0f, 0.0f, 0.0f)
  , m_CameraTarget(0.0f, 0.0f, 0.0f)
  , m_CameraDistance(0.0f)
  , m_CameraRotationX(0.0f)
  , m_CameraRotationY(0.0f)
  , m_IndexCount(0)
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

  if (!CreatePipelineState())
  {
    MessageBoxW(hwnd, L"Failed to create pipeline state", L"Error", MB_OK);
    return false;
  }

  // Загрузка модели sponza.obj
  if (!LoadModel("sponza.obj"))
  {
    MessageBoxW(hwnd, L"Failed to load sponza.obj", L"Error", MB_OK);
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

  // Создание устройства
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
  // Вершинный шейдер с освещением по Фонгу
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
            
            // Преобразование нормали в мировое пространство
            output.normal = mul(float4(input.normal, 0.0f), world).xyz;
            output.color = input.color;
            
            return output;
        }
    )";

  // Пиксельный шейдер с освещением по Фонгу
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
            
            // Диффузное освещение
            float diffuse = max(dot(normal, lightDir), 0.0f);
            
            // Отраженное направление
            float3 reflectDir = reflect(-lightDir, normal);
            
            // Зеркальное освещение
            float specular = pow(max(dot(viewDir, reflectDir), 0.0f), 32.0f);
            
            // Комбинирование освещения
            float3 ambient = float3(0.2f, 0.2f, 0.2f);
            float3 lighting = ambient + 
                             diffuse * lightColor.rgb * 0.8f + 
                             specular * 0.5f;
            
            // Применяем освещение к цвету вершины
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

  // Растеризатор
  D3D12_RASTERIZER_DESC rasterizerDesc = {};
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
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
  // Эта функция просто проверяет, создан ли PSO
  return m_PipelineState != nullptr;
}

bool CubeRenderer::LoadModel(const std::string& filename)
{
  std::ifstream file(filename);
  if (!file.is_open())
  {
    // Если файл не найден, создаем куб по умолчанию
    CreateDefaultCube();
    return true; // Продолжаем с кубом
  }

  std::vector<XMFLOAT3> positions;
  std::vector<XMFLOAT3> normals;
  std::vector<XMFLOAT2> texcoords;
  std::string line;

  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    std::string type;
    iss >> type;

    if (type == "v")
    {
      XMFLOAT3 pos;
      iss >> pos.x >> pos.y >> pos.z;
      positions.push_back(pos);
    }
    else if (type == "vn")
    {
      XMFLOAT3 normal;
      iss >> normal.x >> normal.y >> normal.z;
      normals.push_back(normal);
    }
    else if (type == "vt")
    {
      XMFLOAT2 texcoord;
      iss >> texcoord.x >> texcoord.y;
      texcoords.push_back(texcoord);
    }
    else if (type == "f")
    {
      std::string v1, v2, v3;
      iss >> v1 >> v2 >> v3;

      // Функция для парсинга индексов OBJ формата
      auto parseFace = [](const std::string& token, int& posIdx, int& texIdx, int& normIdx) {
        std::string tokenCopy = token;
        std::replace(tokenCopy.begin(), tokenCopy.end(), '/', ' ');
        std::istringstream tokenStream(tokenCopy);

        std::string posStr, texStr, normStr;
        tokenStream >> posStr;
        if (!tokenStream.eof()) tokenStream >> texStr;
        if (!tokenStream.eof()) tokenStream >> normStr;

        posIdx = posStr.empty() ? -1 : std::stoi(posStr) - 1;
        texIdx = texStr.empty() ? -1 : std::stoi(texStr) - 1;
        normIdx = normStr.empty() ? -1 : std::stoi(normStr) - 1;
        };

      int posIdx1, texIdx1, normIdx1;
      int posIdx2, texIdx2, normIdx2;
      int posIdx3, texIdx3, normIdx3;

      parseFace(v1, posIdx1, texIdx1, normIdx1);
      parseFace(v2, posIdx2, texIdx2, normIdx2);
      parseFace(v3, posIdx3, texIdx3, normIdx3);

      if (posIdx1 >= 0 && posIdx1 < (int)positions.size() &&
        posIdx2 >= 0 && posIdx2 < (int)positions.size() &&
        posIdx3 >= 0 && posIdx3 < (int)positions.size())
      {
        Vertex v[3];

        // Позиции
        v[0].position = positions[posIdx1];
        v[1].position = positions[posIdx2];
        v[2].position = positions[posIdx3];

        // Нормали (если есть в файле)
        if (normIdx1 >= 0 && normIdx1 < (int)normals.size())
          v[0].normal = normals[normIdx1];
        else
          v[0].normal = XMFLOAT3(0, 1, 0);

        if (normIdx2 >= 0 && normIdx2 < (int)normals.size())
          v[1].normal = normals[normIdx2];
        else
          v[1].normal = XMFLOAT3(0, 1, 0);

        if (normIdx3 >= 0 && normIdx3 < (int)normals.size())
          v[2].normal = normals[normIdx3];
        else
          v[2].normal = XMFLOAT3(0, 1, 0);

        // Пока что белый цвет, потом заменим на градиентный
        v[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        v[1].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        v[2].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        m_Vertices.push_back(v[0]);
        m_Vertices.push_back(v[1]);
        m_Vertices.push_back(v[2]);
      }
    }
  }

  file.close();

  if (m_Vertices.empty())
  {
    // Если модель не загрузилась, создаем простой куб
    CreateDefaultCube();
  }
  else
  {
    // Если нормалей нет в файле, вычисляем их
    if (normals.empty())
    {
      for (size_t i = 0; i < m_Vertices.size(); i += 3)
      {
        XMFLOAT3 normal = CalculateNormal(
          m_Vertices[i].position,
          m_Vertices[i + 1].position,
          m_Vertices[i + 2].position
        );

        m_Vertices[i].normal = normal;
        m_Vertices[i + 1].normal = normal;
        m_Vertices[i + 2].normal = normal;
      }
    }
  }

  // Вычисляем bounding box
  ComputeBoundingBox();

  // Теперь применяем градиентную раскраску
  for (auto& vertex : m_Vertices)
  {
    vertex.color = GenerateVertexColor(vertex.position, vertex.normal);
  }

  // Создаем индексы
  m_Indices.resize(m_Vertices.size());
  for (size_t i = 0; i < m_Vertices.size(); i++)
  {
    m_Indices[i] = static_cast<UINT>(i);
  }

  m_IndexCount = static_cast<UINT>(m_Indices.size());

  std::cout << "Model loaded: " << m_Vertices.size() << " vertices, "
    << m_IndexCount << " indices" << std::endl;

  // Создаем вершинный буфер
  const UINT vertexBufferSize = static_cast<UINT>(m_Vertices.size() * sizeof(Vertex));

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

  memcpy(pVertexDataBegin, m_Vertices.data(), vertexBufferSize);
  m_VertexBuffer->Unmap(0, nullptr);

  // Инициализация вершинного буферного представления
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Vertex);
  m_VertexBufferView.SizeInBytes = vertexBufferSize;

  // Создание индексного буфера
  const UINT indexBufferSize = static_cast<UINT>(m_Indices.size() * sizeof(UINT));
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

  memcpy(pIndexDataBegin, m_Indices.data(), indexBufferSize);
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

void CubeRenderer::CreateDefaultCube()
{
  // Создаем простой куб если модель не загрузилась
  Vertex cubeVertices[] = {
    // Передняя грань
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    // Задняя грань
    { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3(-1.0f,  1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3(1.0f,  1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f) },
  };

  UINT cubeIndices[] = {
      0, 1, 2, 0, 2, 3,  // Передняя
      4, 6, 5, 4, 7, 6,  // Задняя
      4, 5, 1, 4, 1, 0,  // Левая
      3, 2, 6, 3, 6, 7,  // Правая
      1, 5, 6, 1, 6, 2,  // Верхняя
      4, 0, 3, 4, 3, 7   // Нижняя
  };

  m_Vertices.assign(cubeVertices, cubeVertices + 8);
  m_Indices.assign(cubeIndices, cubeIndices + 36);
}

XMFLOAT3 CubeRenderer::CalculateNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
  XMFLOAT3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
  XMFLOAT3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);

  XMFLOAT3 normal;
  normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
  normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
  normal.z = edge1.x * edge2.y - edge1.y * edge2.x;

  // Нормализуем
  float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
  if (length > 0)
  {
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
  }

  return normal;
}

XMFLOAT4 CubeRenderer::GenerateVertexColor(const XMFLOAT3& position, const XMFLOAT3& normal)
{
  // Проверяем, чтобы не было деления на ноль
  if (m_MaxBounds.x - m_MinBounds.x < 0.0001f ||
    m_MaxBounds.y - m_MinBounds.y < 0.0001f ||
    m_MaxBounds.z - m_MinBounds.z < 0.0001f)
  {
    // Если bounding box слишком маленький, используем случайные цвета
    return XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
  }

  // Градиентный цвет на основе позиции
  float r = (position.x - m_MinBounds.x) / (m_MaxBounds.x - m_MinBounds.x);
  float g = (position.y - m_MinBounds.y) / (m_MaxBounds.y - m_MinBounds.y);
  float b = (position.z - m_MinBounds.z) / (m_MaxBounds.z - m_MinBounds.z);

  // Яркие цвета
  r = 0.3f + 0.7f * r;
  g = 0.3f + 0.7f * g;
  b = 0.3f + 0.7f * b;

  // Влияние нормали для объемности
  float lightFactor = 0.5f + 0.5f * normal.y;

  r *= lightFactor;
  g *= lightFactor;
  b *= lightFactor;

  // Ограничиваем
  r = max(0.0f, min(1.0f, r));
  g = max(0.0f, min(1.0f, g));
  b = max(0.0f, min(1.0f, b));

  return XMFLOAT4(r, g, b, 1.0f);
}

void CubeRenderer::ComputeBoundingBox()
{
  if (m_Vertices.empty())
    return;

  m_MinBounds = m_Vertices[0].position;
  m_MaxBounds = m_Vertices[0].position;

  for (const auto& vertex : m_Vertices)
  {
    m_MinBounds.x = min(m_MinBounds.x, vertex.position.x);
    m_MinBounds.y = min(m_MinBounds.y, vertex.position.y);
    m_MinBounds.z = min(m_MinBounds.z, vertex.position.z);

    m_MaxBounds.x = max(m_MaxBounds.x, vertex.position.x);
    m_MaxBounds.y = max(m_MaxBounds.y, vertex.position.y);
    m_MaxBounds.z = max(m_MaxBounds.z, vertex.position.z);
  }

  m_Center = XMFLOAT3(
    (m_MinBounds.x + m_MaxBounds.x) * 0.5f,
    (m_MinBounds.y + m_MaxBounds.y) * 0.5f,
    (m_MinBounds.z + m_MaxBounds.z) * 0.5f
  );

  // Вычисляем радиус
  m_Radius = 0.0f;
  for (const auto& vertex : m_Vertices)
  {
    float dx = vertex.position.x - m_Center.x;
    float dy = vertex.position.y - m_Center.y;
    float dz = vertex.position.z - m_Center.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    m_Radius = max(m_Radius, distance);
  }

  std::cout << "Bounding box: min(" << m_MinBounds.x << ", " << m_MinBounds.y << ", " << m_MinBounds.z << ")"
    << " max(" << m_MaxBounds.x << ", " << m_MaxBounds.y << ", " << m_MaxBounds.z << ")" << std::endl;
  std::cout << "Center: (" << m_Center.x << ", " << m_Center.y << ", " << m_Center.z << ")" << std::endl;
  std::cout << "Radius: " << m_Radius << std::endl;
}

void CubeRenderer::SetupMatrices()
{
  // Начинаем с единичной матрицы
  m_WorldMatrix = XMMatrixIdentity();

  // Настраиваем камеру для охвата всей модели
  m_CameraDistance = m_Radius * 3.0f;
  m_CameraRotationX = XM_PIDIV4;
  m_CameraRotationY = XM_PIDIV4;

  UpdateCamera();
}

void CubeRenderer::UpdateCamera()
{
  // Сферические координаты
  float cosX = cosf(m_CameraRotationX);
  float sinX = sinf(m_CameraRotationX);
  float cosY = cosf(m_CameraRotationY);
  float sinY = sinf(m_CameraRotationY);

  m_CameraPosition = XMFLOAT3(
    m_Center.x + m_CameraDistance * cosX * sinY,
    m_Center.y + m_CameraDistance * sinX,
    m_Center.z + m_CameraDistance * cosX * cosY
  );

  m_CameraTarget = m_Center;

  XMVECTOR eye = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
  XMVECTOR focus = XMVectorSet(m_CameraTarget.x, m_CameraTarget.y, m_CameraTarget.z, 0.0f);
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

  m_ViewMatrix = XMMatrixLookAtLH(eye, focus, up);

  m_ProjectionMatrix = XMMatrixPerspectiveFovLH(
    XM_PIDIV4,
    (float)m_WindowWidth / (float)m_WindowHeight,
    0.1f,
    m_Radius * 10.0f
  );
}

void CubeRenderer::SetupLight()
{
  LightBuffer lightData;
  lightData.lightPos = XMFLOAT3(5.0f, 10.0f, -5.0f);
  lightData.cameraPos = m_CameraPosition;
  lightData.lightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

  void* pLightDataBegin = nullptr;
  D3D12_RANGE readRange = { 0, 0 };
  m_LightBuffer->Map(0, &readRange, &pLightDataBegin);
  memcpy(pLightDataBegin, &lightData, sizeof(LightBuffer));
  m_LightBuffer->Unmap(0, nullptr);
}

void CubeRenderer::PopulateCommandList()
{
  HRESULT hr = m_CommandAllocators[m_FrameIndex]->Reset();
  if (FAILED(hr)) return;

  hr = m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), m_PipelineState.Get());
  if (FAILED(hr)) return;

  // ВРАЩЕНИЕ МОДЕЛИ - как у куба в оригинальном коде
  m_Timer.Tick();
  m_RotationAngle += m_Timer.GetDeltaTime() * 0.3f;

  // Создаем матрицы вращения
  XMMATRIX rotationX = XMMatrixRotationX(m_RotationAngle * 0.5f);
  XMMATRIX rotationY = XMMatrixRotationY(m_RotationAngle);
  XMMATRIX rotationZ = XMMatrixRotationZ(m_RotationAngle * 0.3f);

  // Комбинируем вращения
  m_WorldMatrix = rotationX * rotationY * rotationZ;

  // Обновление буфера освещения
  LightBuffer lightData;
  lightData.lightPos = XMFLOAT3(5.0f, 10.0f, -5.0f);
  lightData.cameraPos = m_CameraPosition;
  lightData.lightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

  void* pLightDataBegin = nullptr;
  D3D12_RANGE readRange = { 0, 0 };
  m_LightBuffer->Map(0, &readRange, &pLightDataBegin);
  memcpy(pLightDataBegin, &lightData, sizeof(LightBuffer));
  m_LightBuffer->Unmap(0, nullptr);

  // Обновление матриц
  MatrixBuffer matrices;
  matrices.world = XMMatrixTranspose(m_WorldMatrix);
  matrices.view = XMMatrixTranspose(m_ViewMatrix);
  matrices.projection = XMMatrixTranspose(m_ProjectionMatrix);

  void* pConstantDataBegin = nullptr;
  m_ConstantBuffer->Map(0, &readRange, &pConstantDataBegin);
  memcpy(pConstantDataBegin, &matrices, sizeof(MatrixBuffer));
  m_ConstantBuffer->Unmap(0, nullptr);

  // Установка корневой сигнатуры
  m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

  // Установка CBV
  m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress());
  m_CommandList->SetGraphicsRootConstantBufferView(1, m_LightBuffer->GetGPUVirtualAddress());

  // Установка целей рендеринга
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHandle.ptr += m_FrameIndex * m_RtvDescriptorSize;

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();

  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  // Очистка
  float clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f };
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // Установка состояния
  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
  m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  m_CommandList->IASetIndexBuffer(&m_IndexBufferView);

  // Отрисовка
  m_CommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);

  // Переход состояния
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  m_CommandList->ResourceBarrier(1, &barrier);

  hr = m_CommandList->Close();
}

void CubeRenderer::Render()
{
  if (!m_Initialized) return;

  PopulateCommandList();

  ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
  m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

  m_SwapChain->Present(1, 0);

  WaitForPreviousFrame();
}

void CubeRenderer::WaitForPreviousFrame()
{
  const UINT64 fenceValue = m_FenceValues[m_FrameIndex];
  HRESULT hr = m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
  if (FAILED(hr)) return;

  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
  {
    hr = m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
    if (FAILED(hr)) return;

    WaitForSingleObject(m_FenceEvent, INFINITE);
  }

  m_FenceValues[m_FrameIndex] = fenceValue + 1;
}

void CubeRenderer::Resize(int width, int height)
{
  if (!m_Initialized) return;

  m_WindowWidth = width;
  m_WindowHeight = height;

  WaitForPreviousFrame();

  for (UINT i = 0; i < FrameCount; i++)
  {
    m_RenderTargets[i].Reset();
    m_FenceValues[i] = m_FenceValues[m_FrameIndex];
  }

  DXGI_SWAP_CHAIN_DESC desc;
  m_SwapChain->GetDesc(&desc);
  HRESULT hr = m_SwapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags);
  if (FAILED(hr)) return;

  m_FrameIndex = 0;

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
    if (FAILED(hr)) return;

    m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

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

  m_Viewport.Width = static_cast<float>(width);
  m_Viewport.Height = static_cast<float>(height);
  m_ScissorRect.right = static_cast<LONG>(width);
  m_ScissorRect.bottom = static_cast<LONG>(height);

  UpdateCamera();
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