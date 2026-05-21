# FindDirectX.cmake
find_path(DirectX_INCLUDE_DIRS
    NAMES d3d12.h
    PATHS
        "$ENV{DXSDK_DIR}Include"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Include/${WindowsSDK_VERSION}/um"
        "$ENV{ProgramFiles}/Windows Kits/10/Include/${WindowsSDK_VERSION}/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/${WindowsSDK_VERSION}/um"
    DOC "DirectX include directory"
)

find_library(DirectX_D3D12_LIBRARY
    NAMES d3d12
    PATHS
        "$ENV{DXSDK_DIR}Lib/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
    DOC "DirectX D3D12 library"
)

find_library(DirectX_DXGI_LIBRARY
    NAMES dxgi
    PATHS
        "$ENV{DXSDK_DIR}Lib/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
    DOC "DirectX DXGI library"
)

find_library(DirectX_D3DCOMPILER_LIBRARY
    NAMES d3dcompiler
    PATHS
        "$ENV{DXSDK_DIR}Lib/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
    DOC "DirectX D3DCompiler library"
)

set(DirectX_LIBRARIES
    ${DirectX_D3D12_LIBRARY}
    ${DirectX_DXGI_LIBRARY}
    ${DirectX_D3DCOMPILER_LIBRARY}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DirectX
    FOUND_VAR DirectX_FOUND
    REQUIRED_VARS
        DirectX_INCLUDE_DIRS
        DirectX_D3D12_LIBRARY
        DirectX_DXGI_LIBRARY
        DirectX_D3DCOMPILER_LIBRARY
)

if(DirectX_FOUND)
    set(DirectX_LIBRARY_DIRS
        "$ENV{DXSDK_DIR}Lib/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Lib/${WindowsSDK_VERSION}/um/x64"
    )
endif()

mark_as_advanced(
    DirectX_INCLUDE_DIRS
    DirectX_D3D12_LIBRARY
    DirectX_DXGI_LIBRARY
    DirectX_D3DCOMPILER_LIBRARY
    DirectX_LIBRARIES
    DirectX_LIBRARY_DIRS
)