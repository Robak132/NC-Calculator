cmake_minimum_required(VERSION 3.14)
project(FissionOpt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")

add_executable(FissionOpt
    src/Bindings.cpp
    src/Fission.cpp
    src/OptFission.cpp
    src/FissionNet.cpp
)

add_executable(App
        app/main.cpp
        src/Fission.cpp
        src/OptFission.cpp
        src/FissionNet.cpp
)

include_directories(FissionOpt SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/xtensor/include
    ${CMAKE_SOURCE_DIR}/lib/xtl/include
)

include_directories(App SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/xtensor/include
        ${CMAKE_SOURCE_DIR}/lib/xtl/include
)

set_target_properties(FissionOpt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
    OUTPUT_NAME "FissionOpt"
    LINK_FLAGS "-s MODULARIZE=1 -s EXPORT_NAME=FissionOpt -s ALLOW_MEMORY_GROWTH=1 --bind"
)

set_target_properties(App PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/app"
        OUTPUT_NAME "FissionOpt"
)