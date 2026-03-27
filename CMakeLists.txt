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

add_library(FissionCore
    src/Fission.cpp
    src/OptFission.cpp
    src/FissionNet.cpp
)

target_include_directories(FissionCore PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(FissionCore PUBLIC xtensor xtl)
target_compile_definitions(FissionCore PUBLIC XTENSOR_DISABLE_CONCEPTS)
if(MSVC)
    target_compile_options(FissionCore PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O2>)
else()
    target_compile_options(FissionCore PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O3>)
endif()

if(EMSCRIPTEN)
    add_executable(FissionOpt
        src/Bindings.cpp
    )

    target_link_libraries(FissionOpt PRIVATE FissionCore)
    target_compile_definitions(FissionOpt PRIVATE EMSCRIPTEN)
    target_compile_options(FissionOpt PRIVATE -flto)
    set_target_properties(FissionOpt PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
        OUTPUT_NAME "FissionOpt"
        LINK_FLAGS "-O3 -flto -s MODULARIZE=1 -s EXPORT_NAME=FissionOpt -s ALLOW_MEMORY_GROWTH=1 --bind"
    )

    # Expose user-configurable fuel JSON files under web/config for runtime loading.
    add_custom_command(TARGET FissionOpt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/web/config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/config/fission_fuel"
                "${CMAKE_SOURCE_DIR}/web/config/fission_fuel"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/config/heat_sinks"
                "${CMAKE_SOURCE_DIR}/web/config/heat_sinks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/config/metadata.json"
                "${CMAKE_SOURCE_DIR}/web/metadata.json"
        COMMAND ${CMAKE_COMMAND}
                -DFISSION_FUEL_DIR="${CMAKE_SOURCE_DIR}/web/config/fission_fuel"
                -DMANIFEST_OUT="${CMAKE_SOURCE_DIR}/web/config/fission_fuel/files.json"
                -DUNIFIED_OUT="${CMAKE_SOURCE_DIR}/web/fission_fuel.json"
                -P "${CMAKE_SOURCE_DIR}/cmake/GenerateFuelManifest.cmake"
        COMMAND ${CMAKE_COMMAND}
                -DHEAT_SINK_DIR="${CMAKE_SOURCE_DIR}/web/config/heat_sinks"
                -DMANIFEST_OUT="${CMAKE_SOURCE_DIR}/web/config/heat_sinks/files.json"
                -DUNIFIED_OUT="${CMAKE_SOURCE_DIR}/web/heat_sinks.json"
                -P "${CMAKE_SOURCE_DIR}/cmake/GenerateHeatSinkManifest.cmake"
    )
else()
    add_executable(FissionApp
        app/main.cpp
    )
    target_link_libraries(FissionApp PRIVATE FissionCore)
    set_target_properties(FissionApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/app"
        OUTPUT_NAME "fission-app"
    )
endif()
