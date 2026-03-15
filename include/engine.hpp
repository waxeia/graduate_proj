#pragma once
#include "common.hpp"
#include "storage.hpp"

namespace qcs {
struct RunMetrics {//指标结构体，记录实验的所有关键数据（性能、结果、资源占用）
    std::string experiment_name;
    std::string backend;//存储后端名称
    int mpi_ranks = 1;//MPI 总进程数（并行度）
    int omp_threads = 1;//OpenMP 实际使用的线程数
    int num_qubits = 0;
    int circuit_depth = 0;
    int num_cuts = 0;
    std::uint64_t total_tasks = 0;
    std::uint64_t batch_size = 0;
    std::uint64_t chunk_records = 0;
    double compute_seconds = 0.0;//纯计算耗时（秒）
    double write_seconds = 0.0;
    double read_seconds = 0.0;
    double total_seconds = 0.0;
    double partial_sum = 0.0;//当前进程的结果校验和（局部）
    double global_sum = 0.0;
    std::uint64_t records_written = 0;
    std::uint64_t bytes_written = 0;
    std::uint64_t files_created = 0;
    double tasks_per_second = 0.0;
    double max_buffer_mb = 0.0;//内存缓冲区峰值（MB）
};

class ReconstructionEngine {//核心仿真引擎（量子计算的核心算法层）
public:
    explicit ReconstructionEngine(MPI_Comm comm);//构造函数：初始化MPI环境
    RunMetrics run(const AppConfig& cfg);//核心方法：执行仿真，返回指标

private:
    MPI_Comm comm_;
    int rank_ = 0;
    int world_size_ = 1;//MPI总进程数
};
//easy
std::string metrics_csv_header();
std::string metrics_to_csv(const RunMetrics& m);
} // namespace qcs