#pragma once
//引入通用工具函数
#include <algorithm>
#include <chrono>//实验计时（比如 ExperimentRunner 统计任务执行时间）
#include <cmath>
#include <cstdint>//固定宽度整数类型
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>//日志打印时的数值格式化（比如保留小数位数）、CSV 对齐
#include <iostream>
#include <limits>//仿真中数值校验（比如避免量子态概率超出 [0,1] 范围）
#include <map>
#include <memory>//量子仿真中动态分配的资源（比如 std::unique_ptr 管理量子态数组）
#include <numeric>//仿真结果统计（比如计算平均执行时间、概率求和校验）
#include <optional>
#include <sstream>
#include <stdexcept>//标准异常类
#include <string>
#include <thread>//OpenMP 之外的手动线程管理（若有）、MPI 进程间同步辅助
#include <utility>
#include <vector>

#include <mpi.h>//引入 MPI（Message Passing Interface）并行通信库的接口，MPI 并行框架

#ifdef _OPENMP//#ifdef _OPENMP：条件编译指令
#include <omp.h>
#endif