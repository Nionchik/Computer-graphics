cmake_minimum_required(VERSION 3.10)
project(SoftwareRenderer)

set(CMAKE_CXX_STANDARD 17)

add_executable(SoftwareRenderer 
    main.cpp
    graphics.cpp
    model.cpp
    shader.cpp
    texture.cpp
    tgaimage.cpp
)