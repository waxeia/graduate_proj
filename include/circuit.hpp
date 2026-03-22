#pragma once
#include "common.hpp"
#include "config.hpp"//引入AppConfig配置结构体

namespace qcs{
//连接配置和仿真的桥梁
struct CircuitDescriptor {//量子电路描述符（存储电路核心参数）
    int num_qubits = 0;//量子比特数量（对应AppConfig的num_qubits）
    int circuit_depth = 0;//电路深度（量子门层数，对应AppConfig的circuit_depth）
    int num_cuts = 0;//电路切割次数（并行计算用，对应AppConfig的num_cuts）
    int seed = 0;//随机数种子（保证电路生成可复现）
    std::vector<int> cut_locations;//电路切割位置（比如深度120切6次，记录每次切割的层数）
    std::vector<double> gate_weights;//每个量子门的权重（用于仿真计算）
    std::uint64_t total_tasks = 0;//该电路对应的总仿真任务数
};

// struct ResultRecord {
//     std::uint64_t task_id = 0;
//     std::uint64_t assignment = 0;
//     int rank = 0;
//     int repeat_id = 0;
//     double coefficient = 0.0;
//     double value = 0.0;
//     double contribution = 0.0;
//     std::uint64_t checksum = 0;
// };
//存储单个仿真任务的完整结果
struct ResultRecord {
    std::uint64_t task_id = 0;//全局任务ID（唯一标识单个仿真任务）
    std::uint64_t assignment = 0;//量子比特赋值（比如28比特的二进制状态，用于态矢量计算）
    int rank = 0;//执行该任务的MPI进程rank
    int repeat_id = 0;//实验重复次数ID（对应cfg.repeats）
    double coefficient = 0.0; // TODO:理解：模拟算符系数,量子态系数（振幅）
    double value_real = 0.0;  // 重构项实部
    double value_imag = 0.0;  // 重构项虚部
    double contribution = 0.0; // 该任务对全局结果的贡献值
    std::uint64_t checksum = 0;//结果校验和（防止数据损坏）
};
//进程本地任务范围（MPI 任务拆分结果）
struct LocalTaskRange {
    std::uint64_t begin = 0;//当前进程负责的任务起始ID（左闭）
    std::uint64_t end = 0;//当前进程负责的任务结束ID（右开）
};//标记每个 MPI 进程的任务边界，实现负载均衡

CircuitDescriptor make_circuit(const AppConfig& cfg);//根据配置生成量子电路描述符
//拆分总任务数到各个MPI进程，返回当前进程的任务范围
LocalTaskRange split_tasks(std::uint64_t total_tasks, int rank, int world_size);
//TODO：core:执行单个量子仿真任务，返回结果记录
ResultRecord evaluate_task(const CircuitDescriptor& circuit,
                           std::uint64_t global_task_id,
                           int rank,
                           int repeat_id);
//生成结果记录的确定性校验和（用于数据校验）
std::uint64_t deterministic_checksum(const ResultRecord& record);
} // namespace qcs