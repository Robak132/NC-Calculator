cmake_minimum_required(VERSION 3.20)
project(FissionOpt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(xtl
    GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
    GIT_TAG 0.7.7
    GIT_SHALLOW TRUE
)
FetchContent_Declare(xtensor
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
    GIT_TAG 0.25.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(xtl xtensor)

add_executable(FissionOpt
    src/Bindings.cpp
    src/Fission.cpp
    src/OptFission.cpp
    src/FissionNet.cpp
)

target_link_libraries(FissionOpt PRIVATE xtensor xtl)
target_compile_definitions(FissionOpt PRIVATE XTENSOR_DISABLE_CONCEPTS EMSCRIPTEN)
target_compile_options(FissionOpt PRIVATE -O3 -flto)
set_target_properties(FissionOpt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
    OUTPUT_NAME "FissionOpt"
    LINK_FLAGS "-O3 -flto -s MODULARIZE=1 -s EXPORT_NAME=FissionOpt -s ALLOW_MEMORY_GROWTH=1 --bind"
)