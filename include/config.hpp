#pragma once//防止头文件被重复包含
#include "common.hpp"

namespace qcs {
enum class StorageMode {
    Memory,//结果存内存
    SingleHdf5,//结果存入单个 HDF5 文件
    MultiFile//结果分多个文件存储（分片）
};//定义仿真结果的存储方式
//核心结构体
struct AppConfig {
    int num_qubits = 28;//量子比特数量，默认值28
    int circuit_depth = 120;//量子电路深度
    int num_cuts = 6;//电路切割次数
    std::uint64_t total_tasks = 0;
    std::uint64_t batch_size = 4096;
    std::uint64_t chunk_records = 16384;//结果分片大小
    std::uint64_t max_records_in_memory = 500000;//内存中最多缓存的记录数
    int repeats = 1;
    int seed = 2026;//随机数种子（保证实验可复现）？
    int omp_threads = 0;//OpenMP 线程数
    std::string omp_schedule = "static";//OpenMP 调度策略（static/dynamic/guided）
    int omp_chunk = 0;//OpenMP 分块大小
    StorageMode storage_mode = StorageMode::Memory;
    std::string output_dir = "results/run";
    std::string experiment_name = "baseline";
    bool verify_readback = true;
    bool cleanup_existing = true;//是否清理已有输出目录（避免脏数据）
    bool emit_csv = true;
    bool persist_records = true;
    bool verbose = true;//是否打印详细日志
};

std::string trim(const std::string& s);//去除字符串首尾空白
std::string to_lower(std::string s);
StorageMode parse_storage_mode(const std::string& value);//字符串转枚举
std::string to_string(StorageMode mode);//枚举转字符串
AppConfig load_config(const std::string& path);
void apply_overrides(AppConfig& cfg, const std::vector<std::string>& overrides);
std::string cfg_value(const std::map<std::string, std::string>& kv, const std::string& key, const std::string& fallback);
std::uint64_t derive_total_tasks(const AppConfig& cfg);//自动计算总任务数
} // namespace qcs