cmake_minimum_required(VERSION 3.20)
project(FissionOpt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

include(FetchContent)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")

FetchContent_Declare(xtl
    GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
    GIT_TAG 0.7.7
)
FetchContent_MakeAvailable(xtl)
FetchContent_Declare(xtensor
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
    GIT_TAG 0.25.0
)
FetchContent_MakeAvailable(xtensor)

add_executable(FissionOpt
    src/Bindings.cpp
    src/Fission.cpp
    src/OptFission.cpp
    src/FissionNet.cpp
)
target_link_libraries(FissionOpt PRIVATE xtensor xtl)
target_include_directories(FissionOpt PRIVATE ${xtensor_SOURCE_DIR}/include)

set_target_properties(FissionOpt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
    OUTPUT_NAME "FissionOpt"
    LINK_FLAGS "-s MODULARIZE=1 -s EXPORT_NAME=FissionOpt -s ALLOW_MEMORY_GROWTH=1 --bind"
)