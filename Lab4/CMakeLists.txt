cmake_minimum_required(VERSION 3.20)
project(Lab4 VERSION 1.0.0 LANGUAGES CXX)

# Настройки компилятора
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройки для Windows
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    
    # Настройки компилятора MSVC
    if(MSVC)
        add_compile_options(
            /W4           # Уровень предупреждений 4
            /WX           # Обрабатывать предупреждения как ошибки
            /permissive-  # Строгое соответствие стандарту
            /Zc:__cplusplus
            /Zc:inline
            /EHsc         # Обработка исключений C++
        )
        
        # Отладочные настройки
        add_compile_options(
            "$<$<CONFIG:Debug>:/Od>"      # Отключение оптимизации
            "$<$<CONFIG:Debug>:/Zi>"      # Отладочная информация
            "$<$<CONFIG:Debug>:/MDd>"     # Отладочная библиотека CRT
            "$<$<CONFIG:Release>:/O2>"    # Оптимизация скорости
            "$<$<CONFIG:Release>:/MD>"    # Релизная библиотека CRT
        )
    endif()
endif()

# Поиск пакетов DirectX и Windows SDK
find_package(DirectX REQUIRED)
if(NOT DirectX_FOUND)
    message(FATAL_ERROR "DirectX not found. Please install Windows SDK.")
endif()

find_package(WindowsSDK REQUIRED)
if(NOT WindowsSDK_FOUND)
    message(FATAL_ERROR "Windows SDK not found.")
endif()

# Добавление исполняемого файла
add_executable(${PROJECT_NAME}
    Main.cpp
    CubeRenderer.cpp
    CubeRenderer.h
    Window.cpp
    Window.h
    InputDevice.cpp
    InputDevice.h
    Timer.cpp
    Timer.h
    Framework.h
    d3dx12.h
    Shaders.hlsl
)

# Настройка свойств для шейдеров (если используете компиляцию шейдеров)
set_source_files_properties(Shaders.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")

# Настройки цели
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DirectX_INCLUDE_DIRS}
    ${WindowsSDK_INCLUDE_DIRS}
)

# Настройки компоновщика
target_link_directories(${PROJECT_NAME} PRIVATE
    ${DirectX_LIBRARY_DIRS}
    ${WindowsSDK_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    winmm.lib
)

# Настройки подсистемы Windows
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# Отладочные настройки
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:_DEBUG>"
    "$<$<CONFIG:Debug>:UNICODE;_UNICCODE>"
)

# Установка пост-строчных команд для копирования DLL (если нужно)
if(WIN32 AND MSVC)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Build completed: $<TARGET_FILE:${PROJECT_NAME}>"
    )
endif()

# Установка выходного каталога
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Дополнительные настройки для Visual Studio
if(MSVC)
    # Группировка файлов в Visual Studio
    source_group("Header Files" FILES
        CubeRenderer.h
        Window.h
        InputDevice.h
        Timer.h
        Framework.h
        d3dx12.h
    )
    
    source_group("Source Files" FILES
        CubeRenderer.cpp
        Window.cpp
        InputDevice.cpp
        Timer.cpp
        Main.cpp
    )
    
    source_group("Shaders" FILES
        Shaders.hlsl
    )
    
    # Настройки свойств проекта
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/Lab4.vcxproj.user"
    )
endif()

# Установка (опционально)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Тесты (если будут добавлены позже)
# enable_testing()
# add_test(NAME Lab4Test COMMAND ${PROJECT_NAME})

message(STATUS "Project ${PROJECT_NAME} configured successfully")
message(STATUS "  - C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  - DirectX SDK: ${DirectX_INCLUDE_DIRS}")
message(STATUS "  - Windows SDK: ${WindowsSDK_INCLUDE_DIRS}")