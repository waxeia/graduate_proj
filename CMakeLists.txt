cmake_minimum_required(VERSION 3.20)
project(quantum_cutting_storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_OPENMP "Enable OpenMP" ON)

find_package(MPI REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS C)

if(ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
endif()

add_executable(qcsim
  src/main.cpp
  src/config.cpp
  src/circuit.cpp
  src/storage.cpp
  src/engine.cpp
  src/experiment.cpp
)

target_include_directories(qcsim PRIVATE include ${HDF5_INCLUDE_DIRS})
target_link_libraries(qcsim PRIVATE MPI::MPI_CXX ${HDF5_LIBRARIES})

if(ENABLE_OPENMP)
  target_link_libraries(qcsim PRIVATE OpenMP::OpenMP_CXX)
endif()

if(MSVC)
  target_compile_options(qcsim PRIVATE /W4)
else()
  target_compile_options(qcsim PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()
