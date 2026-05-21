//Renderer.cpp
#include "Renderer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>

using namespace DirectX;

static std::wstring Utf8ToWide(const std::string& utf8)
{
  if (utf8.empty()) return std::wstring();
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], size_needed);
  return wstr;
}

static XMFLOAT3 CalculateNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
  XMFLOAT3 e1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
  XMFLOAT3 e2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
  XMFLOAT3 n;
  n.x = e1.y * e2.z - e1.z * e2.y;
  n.y = e1.z * e2.x - e1.x * e2.z;
  n.z = e1.x * e2.y - e1.y * e2.x;
  float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
  if (len > 0) { n.x /= len; n.y /= len; n.z /= len; }
  return n;
}
Renderer::Renderer()
  : m_WorldMatrix(XMMatrixIdentity())
  , m_ViewMatrix(XMMatrixIdentity())
  , m_ProjectionMatrix(XMMatrixIdentity())
  , m_MinBounds(0, 0, 0)
  , m_MaxBounds(0, 0, 0)
  , m_Center(0, 0, 0)
  , m_Radius(1.0f)
  , m_CameraPosition(0, 0, 0)
  , m_CameraTarget(0, 0, 0)
  , m_CameraDistance(0)
  , m_CameraRotationX(0)
  , m_CameraRotationY(0)
  , m_RotationAngle(0)
{
}

Renderer::~Renderer()
{
  Cleanup();
}

bool Renderer::Initialize(HWND hwnd, int width, int height)
{
  m_Width = width;
  m_Height = height;

  try
  {
    InitializeDirect3D(hwnd);
    LoadShaders();

    if (!LoadModel("sponza.obj"))
    {
      OutputDebugStringA("Failed to load sponza.obj\n");
    }

    CreateWhiteDummyTexture();
    CreateConstantBuffer();

    SetupMatrices();

    m_Initialized = true;
    return true;
  }
  catch (const std::exception& e)
  {
    OutputDebugStringA(e.what());
    return false;
  }
}

bool Renderer::InitializeDirect3D(HWND hwnd)
{
  HRESULT hr;

#ifdef _DEBUG
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    debugController->EnableDebugLayer();
#endif

  ComPtr<IDXGIFactory4> factory;
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) return false;

  hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
  if (FAILED(hr))
  {
    ComPtr<IDXGIAdapter> warpAdapter;
    factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
    hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
    if (FAILED(hr)) return false;
  }

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));
  if (FAILED(hr)) return false;

  DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
  swapChainDesc.BufferCount = FrameCount;
  swapChainDesc.BufferDesc.Width = m_Width;
  swapChainDesc.BufferDesc.Height = m_Height;
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

  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = FrameCount;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));
  if (FAILED(hr)) return false;

  m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
    if (FAILED(hr)) return false;
    m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DsvHeap));
  if (FAILED(hr)) return false;

  D3D12_RESOURCE_DESC depthDesc = {};
  depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthDesc.Width = m_Width;
  depthDesc.Height = m_Height;
  depthDesc.DepthOrArraySize = 1;
  depthDesc.MipLevels = 1;
  depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthDesc.SampleDesc.Count = 1;
  depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthClear = {};
  depthClear.Format = DXGI_FORMAT_D32_FLOAT;
  depthClear.DepthStencil.Depth = 1.0f;

  D3D12_HEAP_PROPERTIES defaultHeap = {};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

  hr = m_Device->CreateCommittedResource(
    &defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
    IID_PPV_ARGS(&m_DepthStencil));
  if (FAILED(hr)) return false;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  m_Device->CreateDepthStencilView(m_DepthStencil.Get(), &dsvDesc,
    m_DsvHeap->GetCPUDescriptorHandleForHeapStart());

  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = MaxTextures;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = m_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SrvHeap));
  if (FAILED(hr)) return false;

  m_SrvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  for (UINT i = 0; i < FrameCount; i++)
  {
    hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocators[i]));
    if (FAILED(hr)) return false;
  }

  hr = m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
    m_CommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_CommandList));
  if (FAILED(hr)) return false;
  m_CommandList->Close();

  m_Viewport = { 0.0f, 0.0f, (float)m_Width, (float)m_Height, 0.0f, 1.0f };
  m_ScissorRect = { 0, 0, m_Width, m_Height };

  hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
  if (FAILED(hr)) return false;
  m_FenceValues[0] = m_FenceValues[1] = 0;
  m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!m_FenceEvent) return false;

  return true;
}

bool Renderer::LoadShaders()
{
  const char* vsCode = R"(
        cbuffer ConstantBuffer : register(b0)
        {
            float4x4 gWorld;
            float4x4 gView;
            float4x4 gProj;
            float4x4 gWorldInvTranspose;
            float4 gLightDir;
            float4 gLightColor;
            float4 gAmbientColor;
            float4 gEyePos;
            float4 gMaterialDiffuse;
            float4 gMaterialSpecular;
            float gSpecularPower;
            float gTotalTime;
            float gTexTilingX;
            float gTexTilingY;
            float gTexScrollX;
            float gTexScrollY;
            int gHasTexture;
            int3 pad;
        };

        struct VSInput
        {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD;
        };

        struct PSInput
        {
            float4 position : SV_POSITION;
            float3 worldPos : POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD;
        };

        PSInput VSMain(VSInput vin)
        {
            PSInput vout;
            float4 posW = mul(float4(vin.position, 1.0f), gWorld);
            vout.worldPos = posW.xyz;
            vout.position = mul(mul(posW, gView), gProj);
            vout.normal = mul(vin.normal, (float3x3)gWorldInvTranspose);
            vout.texCoord = vin.texCoord * float2(gTexTilingX, gTexTilingY) 
                            + float2(gTexScrollX, gTexScrollY) * gTotalTime;
            return vout;
        }
    )";

  const char* psCode = R"(
    Texture2D gDiffuseMap : register(t0);
    SamplerState gSampler : register(s0);

    cbuffer ConstantBuffer : register(b0)
    {
        float4x4 gWorld;
        float4x4 gView;
        float4x4 gProj;
        float4x4 gWorldInvTranspose;
        float4 gLightDir;
        float4 gLightColor;
        float4 gAmbientColor;
        float4 gEyePos;
        float4 gMaterialDiffuse;
        float4 gMaterialSpecular;
        float gSpecularPower;
        float gTotalTime;
        float gTexTilingX;
        float gTexTilingY;
        float gTexScrollX;
        float gTexScrollY;
        int gHasTexture;
        int3 pad;
    };

    struct PSInput
    {
        float4 position : SV_POSITION;
        float3 worldPos : POSITION;
        float3 normal : NORMAL;
        float2 texCoord : TEXCOORD;
    };

    float4 PSMain(PSInput pin) : SV_TARGET
    {
        float3 N = normalize(pin.normal);
        float3 L = normalize(-gLightDir.xyz);
        float3 V = normalize(gEyePos.xyz - pin.worldPos);
        float3 R = reflect(-L, N);

        float4 texColor = gHasTexture ? gDiffuseMap.Sample(gSampler, pin.texCoord) : float4(1,1,1,1);
        float4 baseColor = texColor * gMaterialDiffuse;

        float3 ambient = gAmbientColor.rgb * baseColor.rgb;
        float diff = max(dot(N, L), 0.0f);
        float3 diffuse = diff * gLightColor.rgb * baseColor.rgb;
        float spec = pow(max(dot(R, V), 0.0f), max(gSpecularPower, 1.0f));
        float3 specular = spec * gLightColor.rgb * gMaterialSpecular.rgb;

        return float4(ambient + diffuse + specular, baseColor.a);
    }
  )";

  ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
  HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "VSMain", "vs_5_0",
    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &errorBlob);
  if (FAILED(hr))
  {
    if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    return false;
  }

  hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "PSMain", "ps_5_0",
    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &errorBlob);
  if (FAILED(hr))
  {
    if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    return false;
  }

  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = 1;
  srvRange.BaseShaderRegister = 0;
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  D3D12_ROOT_PARAMETER rootParams[2] = {};
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[0].Descriptor.ShaderRegister = 0;
  rootParams[0].Descriptor.RegisterSpace = 0;
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.MipLODBias = 0;
  sampler.MaxAnisotropy = 1;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
  sampler.MinLOD = 0;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = 2;
  rsDesc.pParameters = rootParams;
  rsDesc.NumStaticSamplers = 1;
  rsDesc.pStaticSamplers = &sampler;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signatureBlob, signatureErrorBlob;
  hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &signatureErrorBlob);
  if (FAILED(hr))
  {
    if (signatureErrorBlob) OutputDebugStringA((char*)signatureErrorBlob->GetBufferPointer());
    return false;
  }

  hr = m_Device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
    IID_PPV_ARGS(&m_RootSignature));
  if (FAILED(hr)) return false;

  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_RootSignature.Get();
  psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
  psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

  D3D12_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
  rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
  rasterDesc.FrontCounterClockwise = FALSE;
  rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  rasterDesc.DepthClipEnable = TRUE;
  rasterDesc.MultisampleEnable = FALSE;
  rasterDesc.AntialiasedLineEnable = FALSE;
  rasterDesc.ForcedSampleCount = 0;
  rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
  psoDesc.RasterizerState = rasterDesc;

  D3D12_BLEND_DESC blendDesc = {};
  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;
  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  psoDesc.BlendState = blendDesc;

  D3D12_DEPTH_STENCIL_DESC dsDesc = {};
  dsDesc.DepthEnable = TRUE;
  dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  dsDesc.StencilEnable = FALSE;
  psoDesc.DepthStencilState = dsDesc;

  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleDesc.Count = 1;

  hr = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
  if (FAILED(hr)) return false;

  return true;
}

bool Renderer::LoadModel(const std::string& filename)
{
  std::ifstream file(filename);
  if (!file.is_open()) return false;

  std::vector<XMFLOAT3> positions;
  std::vector<XMFLOAT3> normals;
  std::vector<XMFLOAT2> texCoords;

  int currentMaterialIndex = -1;
  UINT currentSubsetStart = 0;
  std::string line;

  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    std::string type;
    iss >> type;

    if (type == "v")
    {
      XMFLOAT3 p;
      iss >> p.x >> p.y >> p.z;
      positions.push_back(p);
    }
    else if (type == "vt")
    {
      XMFLOAT2 t;
      iss >> t.x >> t.y;
      texCoords.push_back(t);
    }
    else if (type == "vn")
    {
      XMFLOAT3 n;
      iss >> n.x >> n.y >> n.z;
      normals.push_back(n);
    }
    else if (type == "f")
    {
      std::string v1, v2, v3;
      iss >> v1 >> v2 >> v3;

      auto parseFace = [](const std::string& token, int& pos, int& tex, int& norm)
        {
          std::string t = token;
          std::replace(t.begin(), t.end(), '/', ' ');
          std::istringstream tis(t);
          std::string posStr, texStr, normStr;
          tis >> posStr;
          if (!tis.eof()) tis >> texStr;
          if (!tis.eof()) tis >> normStr;
          pos = posStr.empty() ? -1 : std::stoi(posStr) - 1;
          tex = texStr.empty() ? -1 : std::stoi(texStr) - 1;
          norm = normStr.empty() ? -1 : std::stoi(normStr) - 1;
        };

      int p[3], t[3], n[3];
      parseFace(v1, p[0], t[0], n[0]);
      parseFace(v2, p[1], t[1], n[1]);
      parseFace(v3, p[2], t[2], n[2]);

      for (int i = 0; i < 3; ++i)
      {
        Vertex vert;
        vert.Position = positions[p[i]];
        if (n[i] >= 0 && n[i] < (int)normals.size())
          vert.Normal = normals[n[i]];
        else
          vert.Normal = XMFLOAT3(0, 1, 0);
        if (t[i] >= 0 && t[i] < (int)texCoords.size())
          vert.TexCoord = texCoords[t[i]];
        else
          vert.TexCoord = XMFLOAT2(0, 0);

        m_Vertices.push_back(vert);
        m_Indices.push_back(static_cast<UINT>(m_Indices.size()));
      }
    }
    else if (type == "mtllib")
    {
      std::string mtlFile;
      iss >> mtlFile;
      size_t pos = filename.find_last_of("/\\");
      std::string mtlPath = (pos == std::string::npos) ? mtlFile : filename.substr(0, pos + 1) + mtlFile;
      LoadMaterials(mtlPath);
    }
    else if (type == "usemtl")
    {
      std::string mtlName;
      iss >> mtlName;
      auto it = std::find_if(m_Materials.begin(), m_Materials.end(),
        [&](const Material& m) { return m.Name == mtlName; });
      if (it != m_Materials.end())
      {
        if (currentMaterialIndex != -1 && currentSubsetStart < (UINT)m_Indices.size())
        {
          Subset sub;
          sub.IndexStart = currentSubsetStart;
          sub.IndexCount = (UINT)m_Indices.size() - currentSubsetStart;
          sub.MaterialIndex = currentMaterialIndex;
          m_Subsets.push_back(sub);
        }
        currentMaterialIndex = (int)(it - m_Materials.begin());
        currentSubsetStart = (UINT)m_Indices.size();
      }
    }
  }

  file.close();

  if (currentMaterialIndex != -1 && currentSubsetStart < (UINT)m_Indices.size())
  {
    Subset sub;
    sub.IndexStart = currentSubsetStart;
    sub.IndexCount = (UINT)m_Indices.size() - currentSubsetStart;
    sub.MaterialIndex = currentMaterialIndex;
    m_Subsets.push_back(sub);
  }

  if (normals.empty())
  {
    for (size_t i = 0; i < m_Indices.size(); i += 3)
    {
      XMFLOAT3 n = CalculateNormal(
        m_Vertices[m_Indices[i]].Position,
        m_Vertices[m_Indices[i + 1]].Position,
        m_Vertices[m_Indices[i + 2]].Position);
      m_Vertices[m_Indices[i]].Normal = n;
      m_Vertices[m_Indices[i + 1]].Normal = n;
      m_Vertices[m_Indices[i + 2]].Normal = n;
    }
  }

  if (m_Materials.empty())
  {
    Material defaultMat;
    defaultMat.Name = "default";
    defaultMat.SrvIndex = -1;
    m_Materials.push_back(defaultMat);

    Subset whole;
    whole.IndexStart = 0;
    whole.IndexCount = (UINT)m_Indices.size();
    whole.MaterialIndex = 0;
    m_Subsets.push_back(whole);
  }

  for (auto& mat : m_Materials)
  {
      if (mat.SrvIndex == -1)
      {
          std::wstring fullPath = L"checker.png";
          int srvIdx;
          if (LoadTexture(fullPath, srvIdx))
          {
              mat.SrvIndex = srvIdx;
              OutputDebugStringW(L"Assigned checker.png to material without texture\n");
          }
      }
  }

  UINT vbSize = (UINT)(m_Vertices.size() * sizeof(Vertex));
  UINT ibSize = (UINT)(m_Indices.size() * sizeof(UINT));

  D3D12_HEAP_PROPERTIES uploadHeap = {};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC vbDesc = {};
  vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vbDesc.Width = vbSize;
  vbDesc.Height = 1;
  vbDesc.DepthOrArraySize = 1;
  vbDesc.MipLevels = 1;
  vbDesc.Format = DXGI_FORMAT_UNKNOWN;
  vbDesc.SampleDesc.Count = 1;
  vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  D3D12_RESOURCE_DESC ibDesc = vbDesc;
  ibDesc.Width = ibSize;

  ThrowIfFailed(m_Device->CreateCommittedResource(
    &uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(&m_VertexBuffer)));
  ThrowIfFailed(m_Device->CreateCommittedResource(
    &uploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(&m_IndexBuffer)));

  void* pData;
  m_VertexBuffer->Map(0, nullptr, &pData);
  memcpy(pData, m_Vertices.data(), vbSize);
  m_VertexBuffer->Unmap(0, nullptr);

  m_IndexBuffer->Map(0, nullptr, &pData);
  memcpy(pData, m_Indices.data(), ibSize);
  m_IndexBuffer->Unmap(0, nullptr);

  m_VBView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VBView.StrideInBytes = sizeof(Vertex);
  m_VBView.SizeInBytes = vbSize;

  m_IBView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
  m_IBView.Format = DXGI_FORMAT_R32_UINT;
  m_IBView.SizeInBytes = ibSize;

  if (!m_Vertices.empty())
  {
    m_MinBounds = m_MaxBounds = m_Vertices[0].Position;
    for (const auto& v : m_Vertices)
    {
      m_MinBounds.x = min(m_MinBounds.x, v.Position.x);
      m_MinBounds.y = min(m_MinBounds.y, v.Position.y);
      m_MinBounds.z = min(m_MinBounds.z, v.Position.z);
      m_MaxBounds.x = max(m_MaxBounds.x, v.Position.x);
      m_MaxBounds.y = max(m_MaxBounds.y, v.Position.y);
      m_MaxBounds.z = max(m_MaxBounds.z, v.Position.z);
    }
    m_Center.x = (m_MinBounds.x + m_MaxBounds.x) * 0.5f;
    m_Center.y = (m_MinBounds.y + m_MaxBounds.y) * 0.5f;
    m_Center.z = (m_MinBounds.z + m_MaxBounds.z) * 0.5f;
    m_Radius = 0;
    for (const auto& v : m_Vertices)
    {
      float dx = v.Position.x - m_Center.x;
      float dy = v.Position.y - m_Center.y;
      float dz = v.Position.z - m_Center.z;
      m_Radius = max(m_Radius, sqrtf(dx * dx + dy * dy + dz * dz));
    }
  }

  return true;
}

bool Renderer::LoadMaterials(const std::string& mtlPath)
{
  std::ifstream file(mtlPath);
  if (!file.is_open()) return false;

  std::string line;
  Material currentMat;
  bool inMaterial = false;

  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    std::string type;
    iss >> type;

    if (type == "newmtl")
    {
      if (inMaterial) m_Materials.push_back(currentMat);
      currentMat = Material();
      iss >> currentMat.Name;
      inMaterial = true;
    }
    else if (type == "Kd")
    {
      float r, g, b;
      iss >> r >> g >> b;
      currentMat.Diffuse = XMFLOAT4(r, g, b, 1.0f);
    }
    else if (type == "Ks")
    {
      float r, g, b;
      iss >> r >> g >> b;
      currentMat.Specular = XMFLOAT4(r, g, b, 1.0f);
    }
    else if (type == "Ns")
    {
      iss >> currentMat.Shininess;
    }
    else if (type == "map_Kd")
    {
        currentMat.DiffuseTexture = "checker.png";
        std::wstring fullPath = L"checker.png";
        int srvIdx;
        if (LoadTexture(fullPath, srvIdx))
            currentMat.SrvIndex = srvIdx;
        else
            OutputDebugStringW(L"Failed to load checker.png\n");
    }
    if (currentMat.SrvIndex == -1)
    {
        std::wstring fullPath = L"checker.png";
        int srvIdx;
        if (LoadTexture(fullPath, srvIdx))
        {
            currentMat.SrvIndex = srvIdx;
            currentMat.DiffuseTexture = "checker.png";
            OutputDebugStringW(L"Forced checker.png for material\n");
        }
    }
  }

  if (inMaterial) m_Materials.push_back(currentMat);
  return true;
}

bool Renderer::LoadTexture(const std::wstring& path, int& outSrvIndex)
{
  TextureLoader::TextureData texData;
  if (!TextureLoader::LoadFromFile(path, texData))
    return false;

  ComPtr<ID3D12CommandAllocator> tempAlloc;
  ComPtr<ID3D12GraphicsCommandList> tempList;
  ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc)));
  ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempList)));

  ComPtr<ID3D12Resource> texture, uploadBuf;
  if (!TextureLoader::CreateTexture(m_Device.Get(), tempList.Get(), texData, texture, uploadBuf))
    return false;

  tempList->Close();

  ID3D12CommandList* lists[] = { tempList.Get() };
  m_CommandQueue->ExecuteCommandLists(1, lists);

  m_FenceValues[m_FrameIndex]++;
  ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]));
  WaitForGPU();

  static int nextSlot = 1;
  outSrvIndex = nextSlot++;

  D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
  srvHandle.ptr += outSrvIndex * m_SrvDescriptorSize;

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = texData.format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;

  m_Device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);

  m_TextureUploads.push_back(texture);
  m_TextureUploads.push_back(uploadBuf);

  return true;
}

void Renderer::CreateWhiteDummyTexture()
{
  uint8_t whitePixel[4] = { 255,255,255,255 };
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = 1;
  texDesc.Height = 1;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;

  D3D12_HEAP_PROPERTIES defaultHeap = {};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

  ComPtr<ID3D12Resource> texture;
  ThrowIfFailed(m_Device->CreateCommittedResource(
    &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
    IID_PPV_ARGS(&texture)));

  UINT64 uploadSize = 4;

  D3D12_HEAP_PROPERTIES uploadHeap = {};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = uploadSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ComPtr<ID3D12Resource> uploadBuffer;
  ThrowIfFailed(m_Device->CreateCommittedResource(
    &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(&uploadBuffer)));

  void* mapped;
  uploadBuffer->Map(0, nullptr, &mapped);
  memcpy(mapped, whitePixel, uploadSize);
  uploadBuffer->Unmap(0, nullptr);

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = uploadBuffer.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint.Offset = 0;
  src.PlacedFootprint.Footprint.Format = texDesc.Format;
  src.PlacedFootprint.Footprint.Width = 1;
  src.PlacedFootprint.Footprint.Height = 1;
  src.PlacedFootprint.Footprint.Depth = 1;
  src.PlacedFootprint.Footprint.RowPitch = 4;

  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = texture.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  ComPtr<ID3D12CommandAllocator> tempAlloc;
  ComPtr<ID3D12GraphicsCommandList> tempList;
  ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAlloc)));
  ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc.Get(), nullptr, IID_PPV_ARGS(&tempList)));

  tempList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = texture.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  tempList->ResourceBarrier(1, &barrier);
  tempList->Close();

  ID3D12CommandList* lists[] = { tempList.Get() };
  m_CommandQueue->ExecuteCommandLists(1, lists);

  // Синхронизация
  m_FenceValues[m_FrameIndex]++;
  ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]));
  WaitForGPU();

  D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  m_Device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHandle);

  m_TextureUploads.push_back(texture);
  m_TextureUploads.push_back(uploadBuffer);
}

void Renderer::CreateConstantBuffer()
{
  m_ConstantBufferSlotSize = (sizeof(ConstantBufferData) + 255) & ~255;
  UINT bufferSize = m_ConstantBufferSlotSize * MaxSubsets * FrameCount;

  D3D12_HEAP_PROPERTIES uploadHeap = {};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = bufferSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ThrowIfFailed(m_Device->CreateCommittedResource(
    &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(&m_ConstantBuffer)));

  m_ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedConstantData));
}

void Renderer::SetupMatrices()
{
  m_WorldMatrix = XMMatrixIdentity();
  m_CameraDistance = m_Radius * 3.5f;
  m_CameraRotationX = XM_PIDIV4;
  m_CameraRotationY = XM_PIDIV4;
  UpdateCamera();
}

void Renderer::UpdateCamera()
{
  float cosX = cosf(m_CameraRotationX);
  float sinX = sinf(m_CameraRotationX);
  float cosY = cosf(m_CameraRotationY);
  float sinY = sinf(m_CameraRotationY);

  m_CameraPosition.x = m_Center.x + m_CameraDistance * cosX * sinY;
  m_CameraPosition.y = m_Center.y + m_CameraDistance * sinX;
  m_CameraPosition.z = m_Center.z + m_CameraDistance * cosX * cosY;

  XMVECTOR eye = XMLoadFloat3(&m_CameraPosition);
  XMVECTOR target = XMLoadFloat3(&m_Center);
  XMVECTOR up = XMVectorSet(0, 1, 0, 0);
  m_ViewMatrix = XMMatrixLookAtLH(eye, target, up);

  float aspect = (float)m_Width / (float)m_Height;
  m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, m_Radius * 10.0f);
}

void Renderer::PopulateCommandList()
{
  ThrowIfFailed(m_CommandAllocators[m_FrameIndex]->Reset());
  ThrowIfFailed(m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), m_PipelineState.Get()));

  m_Timer.Tick();
  //убрал вращение модели, как в Лабе4, т.к. мешает понять как движется текстура
  //m_RotationAngle += m_Timer.GetDeltaTime() * 0.3f; 
  //XMMATRIX rotX = XMMatrixRotationX(m_RotationAngle * 0.5f);
  //XMMATRIX rotY = XMMatrixRotationY(m_RotationAngle);
  //XMMATRIX rotZ = XMMatrixRotationZ(m_RotationAngle * 0.3f);
  //m_WorldMatrix = rotX * rotY * rotZ;
  m_WorldMatrix = XMMatrixIdentity();

  UpdateCamera();

  XMMATRIX view = m_ViewMatrix;
  XMMATRIX proj = m_ProjectionMatrix;
  XMMATRIX wit = XMMatrixTranspose(XMMatrixInverse(nullptr, m_WorldMatrix));

  XMFLOAT3 eyePos = m_CameraPosition;
  XMFLOAT3 lightDir(0.3f, -1.0f, 0.5f);
  XMFLOAT4 lightColor(1, 1, 1, 1);
  XMFLOAT4 ambient(0.2f, 0.2f, 0.2f, 1.0f);

  for (size_t i = 0; i < m_Subsets.size(); ++i)
  {
    const Subset& sub = m_Subsets[i];
    int matIdx = (sub.MaterialIndex >= 0 && sub.MaterialIndex < (int)m_Materials.size()) ? sub.MaterialIndex : 0;
    const Material& mat = m_Materials[matIdx];

    UINT slot = m_FrameIndex * MaxSubsets + (UINT)i;
    UINT8* dest = m_MappedConstantData + slot * m_ConstantBufferSlotSize;

    ConstantBufferData cb = {};
    XMStoreFloat4x4(&cb.World, XMMatrixTranspose(m_WorldMatrix));
    XMStoreFloat4x4(&cb.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&cb.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&cb.WorldInvTranspose, XMMatrixTranspose(wit));
    cb.LightDir = XMFLOAT4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
    cb.LightColor = lightColor;
    cb.AmbientColor = ambient;
    cb.EyePos = XMFLOAT4(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    cb.MaterialDiffuse = mat.Diffuse;
    cb.MaterialSpecular = mat.Specular;
    cb.SpecularPower = mat.Shininess;
    cb.TotalTime = m_Timer.GetTotalTime();
    cb.TexTilingX = m_TexTiling.x;
    cb.TexTilingY = m_TexTiling.y;
    cb.TexScrollX = m_TexScroll.x;
    cb.TexScrollY = m_TexScroll.y;
    cb.HasTexture = (mat.SrvIndex >= 0) ? 1 : 0;

    memcpy(dest, &cb, sizeof(ConstantBufferData));
  }

  m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

  ID3D12DescriptorHeap* heaps[] = { m_SrvHeap.Get() };
  m_CommandList->SetDescriptorHeaps(1, heaps);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHandle.ptr += m_FrameIndex * m_RtvDescriptorSize;
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

  float clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

  m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_CommandList->IASetVertexBuffers(0, 1, &m_VBView);
  m_CommandList->IASetIndexBuffer(&m_IBView);

  for (size_t i = 0; i < m_Subsets.size(); ++i)
  {
    const Subset& sub = m_Subsets[i];
    if (sub.IndexCount == 0) continue;

    int matIdx = (sub.MaterialIndex >= 0 && sub.MaterialIndex < (int)m_Materials.size()) ? sub.MaterialIndex : 0;
    const Material& mat = m_Materials[matIdx];

    UINT slot = m_FrameIndex * MaxSubsets + (UINT)i;
    D3D12_GPU_VIRTUAL_ADDRESS cbAddr = m_ConstantBuffer->GetGPUVirtualAddress() + slot * m_ConstantBufferSlotSize;
    m_CommandList->SetGraphicsRootConstantBufferView(0, cbAddr);

    int srvIdx = (mat.SrvIndex >= 0) ? mat.SrvIndex : 0;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle.ptr += srvIdx * m_SrvDescriptorSize;
    m_CommandList->SetGraphicsRootDescriptorTable(1, srvHandle);

    m_CommandList->DrawIndexedInstanced(sub.IndexCount, 1, sub.IndexStart, 0, 0);
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  m_CommandList->ResourceBarrier(1, &barrier);

  ThrowIfFailed(m_CommandList->Close());
}

void Renderer::Render()
{
  if (!m_Initialized) return;

  PopulateCommandList();

  ID3D12CommandList* lists[] = { m_CommandList.Get() };
  m_CommandQueue->ExecuteCommandLists(1, lists);

  ThrowIfFailed(m_SwapChain->Present(1, 0));

  MoveToNextFrame();
}

void Renderer::WaitForGPU()
{
  const UINT64 fenceValue = m_FenceValues[m_FrameIndex];
  ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), fenceValue));
  if (m_Fence->GetCompletedValue() < fenceValue)
  {
    ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
    WaitForSingleObject(m_FenceEvent, INFINITE);
  }
}

void Renderer::MoveToNextFrame()
{
  const UINT64 currentFence = m_FenceValues[m_FrameIndex];
  ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), currentFence));

  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
  {
    ThrowIfFailed(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
    WaitForSingleObject(m_FenceEvent, INFINITE);
  }

  m_FenceValues[m_FrameIndex] = currentFence + 1;
}

void Renderer::Resize(int width, int height)
{
  if (!m_Initialized) return;
  if (m_Width == width && m_Height == height) return;

  m_Width = width;
  m_Height = height;

  WaitForGPU();

  for (UINT i = 0; i < FrameCount; i++)
  {
    m_RenderTargets[i].Reset();
    m_FenceValues[i] = m_FenceValues[m_FrameIndex];
  }

  DXGI_SWAP_CHAIN_DESC desc;
  m_SwapChain->GetDesc(&desc);
  ThrowIfFailed(m_SwapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags));

  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < FrameCount; i++)
  {
    ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
    m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

  m_DepthStencil.Reset();
  D3D12_RESOURCE_DESC depthDesc = {};
  depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthDesc.Width = width;
  depthDesc.Height = height;
  depthDesc.DepthOrArraySize = 1;
  depthDesc.MipLevels = 1;
  depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthDesc.SampleDesc.Count = 1;
  depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthClear = {};
  depthClear.Format = DXGI_FORMAT_D32_FLOAT;
  depthClear.DepthStencil.Depth = 1.0f;

  D3D12_HEAP_PROPERTIES defaultHeap = {};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

  ThrowIfFailed(m_Device->CreateCommittedResource(
    &defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
    IID_PPV_ARGS(&m_DepthStencil)));

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  m_Device->CreateDepthStencilView(m_DepthStencil.Get(), &dsvDesc,
    m_DsvHeap->GetCPUDescriptorHandleForHeapStart());

  m_Viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
  m_ScissorRect = { 0, 0, width, height };

  UpdateCamera();
}

void Renderer::Cleanup()
{
  if (!m_Initialized) return;

  WaitForGPU();

  if (m_FenceEvent)
  {
    CloseHandle(m_FenceEvent);
    m_FenceEvent = nullptr;
  }

  m_Initialized = false;
}