#cmake_minimum_required(VERSION 3.20)
#project(quantum_cutting_storage LANGUAGES CXX)
#
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)
#
#option(ENABLE_OPENMP "Enable OpenMP" ON)
#
#find_package(MPI REQUIRED)
#find_package(HDF5 REQUIRED COMPONENTS C)
#
#if(ENABLE_OPENMP)
#  find_package(OpenMP REQUIRED)
#endif()
#
#add_executable(qcsim
#  src/main.cpp
#  src/config.cpp
#  src/circuit.cpp
#  src/storage.cpp
#  src/engine.cpp
#  src/experiment.cpp
#)
#
#target_include_directories(qcsim PRIVATE include ${HDF5_INCLUDE_DIRS})
#target_link_libraries(qcsim PRIVATE MPI::MPI_CXX ${HDF5_LIBRARIES})
#
#if(ENABLE_OPENMP)
#  target_link_libraries(qcsim PRIVATE OpenMP::OpenMP_CXX)
#endif()
#
#if(MSVC)
#  target_compile_options(qcsim PRIVATE /W4)
#else()
#  target_compile_options(qcsim PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
#endif()




cmake_minimum_required(VERSION 3.20)
project(quantum_cutting_storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 查找系统依赖 ---
find_package(MPI REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS C)
find_package(OpenMP REQUIRED)

# --- 2. 设置 Qulacs 路径 ---
set(QULACS_DIR "/home/waxeia/A_Waxeia/A_Myself/graduate_project/qulacs")

# --- 3. 定义程序目标 ---
add_executable(qcsim
        src/main.cpp
        src/config.cpp
        src/circuit.cpp
        src/storage.cpp
        src/engine.cpp
        src/experiment.cpp
)

# --- 4. 配置包含路径 ---
target_include_directories(qcsim PRIVATE
        include
        ${HDF5_INCLUDE_DIRS}
        ${QULACS_DIR}/src
        ${QULACS_DIR}/include
)

# --- 5. 链接库文件 (严格匹配你 ls 出来的文件名) ---
target_link_libraries(qcsim PRIVATE
        MPI::MPI_CXX
        ${HDF5_LIBRARIES}
        OpenMP::OpenMP_CXX
        ${QULACS_DIR}/lib/libcppsim_static.a
        ${QULACS_DIR}/lib/libcsim_static.a
        ${QULACS_DIR}/lib/libvqcsim_static.a
)

# --- 6. 编译选项 ---
if(NOT MSVC)
    target_compile_options(qcsim PRIVATE -Wall -Wextra -Wno-deprecated-copy)
endif()