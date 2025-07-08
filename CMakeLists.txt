cmake_minimum_required(VERSION 3.14)
project(FissionOpt)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(FissionOpt
  Fission.h
  Fission.cpp
  OptFission.h
  OptFission.cpp
  FissionNet.h
  FissionNet.cpp
  Benchmark.cpp
  OverhaulFission.h
  OverhaulFission.cpp
  OptOverhaulFission.h
  OptOverhaulFission.cpp
  OverhaulFissionNet.h
  OverhaulFissionNet.cpp
)

target_include_directories(FissionOpt SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/xtensor/include/xtensor
  ${CMAKE_SOURCE_DIR}/xtl/include/xtl
)
