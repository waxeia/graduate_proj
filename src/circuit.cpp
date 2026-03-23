// #include "circuit.hpp"
// //#include <cppsim/state.hpp>
//
// namespace qcs {
// namespace {//匿名命名空间：仅当前文件可见，避免全局命名污染
// //splitmix64伪随机数生成器（快速、无状态，用于生成可复现的随机数）
// std::uint64_t splitmix64(std::uint64_t x)
// {
//     x += 0x9e3779b97f4a7c15ULL;//初始偏移（固定魔法数，提升随机性）
//     x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;//异或右移 + 乘法混洗
//     x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;//再次混洗
//     return x ^ (x >> 31U);//最终异或右移，返回随机数
// }//作用：替代标准随机数生成器，保证相同输入x得到相同输出，且随机性足够。
// //把64位无符号整数转成[0,1)范围内的浮点数
// double unit_from_u64(std::uint64_t x) {
//     //取x的低53位（double的有效精度是53位），除以2^53，得到0~1的浮点数
//     return static_cast<double>(x & 0x1FFFFFFFFFFFFFULL) / static_cast<double>(0x1FFFFFFFFFFFFFULL);
// }//作用：把随机整数转换成仿真需要的浮点权重（比如门权重、相位等）。
// }//namespace
// //根据配置生成量子电路描述符
// CircuitDescriptor make_circuit(const AppConfig& cfg) {
//     CircuitDescriptor circuit;//创建空的电路描述符
//     //赋值基础参数（从配置拷贝）
//     circuit.num_qubits = cfg.num_qubits;
//     circuit.circuit_depth = cfg.circuit_depth;
//     circuit.num_cuts = cfg.num_cuts;
//     circuit.seed = cfg.seed;
//     //计算总任务数（derive_total_tasks是common.hpp中的函数，根据配置推导任务数）
//     circuit.total_tasks = derive_total_tasks(cfg);
//     //预分配内存（避免vector频繁扩容，提升效率）
//     circuit.cut_locations.reserve(cfg.num_cuts);//切割位置数组预留空间
//     circuit.gate_weights.reserve(cfg.circuit_depth);//门权重数组预留空间
//     //计算电路切割位置（均匀切割）
//     for (int i = 0; i < cfg.num_cuts; ++i) {
//         //切割位置公式：(总深度 * (i+1)) / (切割次数+1) → 均匀分布在电路深度中，比如深度120，切割6次 → 位置：120*1/7≈17, 120*2/7≈34, ..., 120*6/7≈102
//         const int loc = (cfg.circuit_depth * (i + 1)) / (cfg.num_cuts + 1);
//         circuit.cut_locations.push_back(loc);//存入切割位置
//     }
//     //生成每个量子门的权重（可复现的随机值）
//     for (int d = 0; d < cfg.circuit_depth; ++d) {
//         //混合种子：配置种子 ^ (深度*17+31) → 保证不同深度的门权重不同，且可复现
//         const auto mixed = splitmix64(static_cast<std::uint64_t>(cfg.seed) ^ static_cast<std::uint64_t>(d * 17 + 31));
//         //门权重范围：0.75 ~ 1.25（unit_from_u64返回0~1，乘以0.5加0.75）
//         const double weight = 0.75 + unit_from_u64(mixed) * 0.5;
//         circuit.gate_weights.push_back(weight);//存入门权重
//     }
//     return circuit;//返回生成的电路描述符
// }
// //拆分总任务数到各个 MPI 进程（负载均衡）
// LocalTaskRange split_tasks(std::uint64_t total_tasks, int rank, int world_size) {
//     //计算每个进程的基础任务数（总任务数 / 进程数，向下取整）
//     const auto base = total_tasks / static_cast<std::uint64_t>(world_size);
//     //计算余数（总任务数 % 进程数，即无法均分的任务数）
//     const auto rem = total_tasks % static_cast<std::uint64_t>(world_size);
//     //计算当前进程的任务起始ID，前rem个进程多分配1个任务，保证负载均衡。比如总任务100，进程数8 → base=12，rem=4 → rank0-3各13个任务，rank4-7各12个
//     const auto begin = base * static_cast<std::uint64_t>(rank) + std::min<std::uint64_t>(rem, static_cast<std::uint64_t>(rank));
//     //计算当前进程是否需要多分配1个任务（前rem个进程多1个）
//     const auto extra = static_cast<std::uint64_t>(rank) < rem ? 1ULL : 0ULL;
//     //返回任务范围（begin=起始ID，end=起始ID+基础任务数+额外任务数）
//     return LocalTaskRange{begin, begin + base + extra};
// }
//     // 模拟一个简单的 2x2 矩阵收缩过程
// ResultRecord evaluate_task(const CircuitDescriptor& circuit,
//                                std::uint64_t global_task_id,
//                                int rank,
//                                int repeat_id) {
//     ResultRecord record;//创建空的结果记录
//     //赋值基础标识（任务ID、进程rank、重复次数）
//     record.task_id = global_task_id;
//     record.rank = rank;
//     record.repeat_id = repeat_id;
//
//     // 1. 真实的分配逻辑：量子电路切割通常产生 4^k 个任务
//     // 将 task_id 映射为 Pauli 算符组合 (I, X, Y, Z)
//     record.assignment = global_task_id;
//     /*TODO：量子背景：
//      *量子电路切割是并行化量子仿真的核心技术，切割后会将原电路分解为多个子电路，
//      *每个子电路对应一组Pauli算符（I/X/Y/Z）的组合（共4^k种，k为切割数）；
//     */
//     // 2. 引入计算密集型模拟：张量收缩
//     // 我们模拟对每个切割点进行矩阵运算，增加 CPU 耗时
//     double r_sum = 0.0, i_sum = 0.0;
//     int complexity = circuit.num_qubits; //量子比特数越多，张量维度越高，计算量呈指数增长
//
//     for (int i = 0; i < complexity; ++i) {
//         //模拟矩阵乘法累加：A * B + C
//         double angle = (double)(global_task_id % 100) * 0.01 * M_PI;//生成 0~π 的角度值
//         //实部累加（矩阵乘法的实部），张量收缩的实部计算（量子态的实振幅）
//         r_sum += std::cos(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
//         //虚部累加（矩阵乘法的虚部），张量收缩的虚部计算（量子态的虚振幅）
//         i_sum += std::sin(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
//         // 故意增加循环计算量，以体现计算与 I/O 的平衡
//     }
//
//     // 3. 模拟系数计算（基于 assignment 的奇偶性）
//     double parity = (__builtin_popcountll(record.assignment) % 2 == 0) ? 1.0 : -1.0;
//
//     record.coefficient = parity / (double)(1ULL << circuit.num_cuts);
//     record.value_real = r_sum / complexity;
//     record.value_imag = i_sum / complexity;
//
//     // 最终贡献值：模拟振幅模长
//     record.contribution = record.coefficient * (record.value_real * record.value_real);
//     record.checksum = deterministic_checksum(record);
//
//     return record;
// }
//
// // std::uint64_t deterministic_checksum(const ResultRecord& record) {
// //     std::uint64_t h = 1469598103934665603ULL;
// //     auto mix = [&](std::uint64_t x) {
// //         h ^= x;
// //         h *= 1099511628211ULL;
// //     };
// //
// //     mix(record.task_id);
// //     mix(record.assignment);
// //     mix(static_cast<std::uint64_t>(record.rank));
// //     mix(static_cast<std::uint64_t>(record.repeat_id));
// //     mix(static_cast<std::uint64_t>(std::llround(record.coefficient * 1e12)));
// //     mix(static_cast<std::uint64_t>(std::llround(record.value * 1e12)));
// //     mix(static_cast<std::uint64_t>(std::llround(record.contribution * 1e12)));
// //     return h;
// // }
// std::uint64_t deterministic_checksum(const ResultRecord& record) {
//     std::uint64_t h = 1469598103934665603ULL;
//     auto mix = [&](std::uint64_t x) {
//         h ^= x;
//         h *= 1099511628211ULL;
//     };
//
//     mix(record.task_id);
//     mix(record.assignment);
//     mix(static_cast<std::uint64_t>(record.rank));
//     mix(static_cast<std::uint64_t>(record.repeat_id));
//     mix(static_cast<std::uint64_t>(std::llround(record.coefficient * 1e12)));
//
//     // 关键点：将原先的 value 替换为实部和虚部
//     mix(static_cast<std::uint64_t>(std::llround(record.value_real * 1e12)));
//     mix(static_cast<std::uint64_t>(std::llround(record.value_imag * 1e12)));
//
//     mix(static_cast<std::uint64_t>(std::llround(record.contribution * 1e12)));
//     return h;
// }
// } // namespace qcs






#include "circuit.hpp"
#include <cppsim/state.hpp>    // 新增：Qulacs 状态头文件
#include <cppsim/circuit.hpp>  // 新增：Qulacs 线路头文件
#include <cppsim/gate_factory.hpp>
#include <complex>

namespace qcs {
namespace {//匿名命名空间：仅当前文件可见，避免全局命名污染
//splitmix64伪随机数生成器（快速、无状态，用于生成可复现的随机数）
std::uint64_t splitmix64(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;//初始偏移（固定魔法数，提升随机性）
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;//异或右移 + 乘法混洗
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;//再次混洗
    return x ^ (x >> 31U);//最终异或右移，返回随机数
}//作用：替代标准随机数生成器，保证相同输入x得到相同输出，且随机性足够。
//把64位无符号整数转成[0,1)范围内的浮点数
double unit_from_u64(std::uint64_t x) {
    //取x的低53位（double的有效精度是53位），除以2^53，得到0~1的浮点数
    return static_cast<double>(x & 0x1FFFFFFFFFFFFFULL) / static_cast<double>(0x1FFFFFFFFFFFFFULL);
}//作用：把随机整数转换成仿真需要的浮点权重（比如门权重、相位等）。
}//namespace
//根据配置生成量子电路描述符
CircuitDescriptor make_circuit(const AppConfig& cfg) {
    CircuitDescriptor circuit;//创建空的电路描述符
    //赋值基础参数（从配置拷贝）
    circuit.num_qubits = cfg.num_qubits;
    circuit.circuit_depth = cfg.circuit_depth;
    circuit.num_cuts = cfg.num_cuts;
    circuit.seed = cfg.seed;
    //计算总任务数（derive_total_tasks是common.hpp中的函数，根据配置推导任务数）
    circuit.total_tasks = derive_total_tasks(cfg);
    //预分配内存（避免vector频繁扩容，提升效率）
    circuit.cut_locations.reserve(cfg.num_cuts);//切割位置数组预留空间
    circuit.gate_weights.reserve(cfg.circuit_depth);//门权重数组预留空间
    //计算电路切割位置（均匀切割）
    for (int i = 0; i < cfg.num_cuts; ++i) {
        //切割位置公式：(总深度 * (i+1)) / (切割次数+1) → 均匀分布在电路深度中，比如深度120，切割6次 → 位置：120*1/7≈17, 120*2/7≈34, ..., 120*6/7≈102
        const int loc = (cfg.circuit_depth * (i + 1)) / (cfg.num_cuts + 1);
        circuit.cut_locations.push_back(loc);//存入切割位置
    }
    //生成每个量子门的权重（可复现的随机值）
    for (int d = 0; d < cfg.circuit_depth; ++d) {
        //混合种子：配置种子 ^ (深度*17+31) → 保证不同深度的门权重不同，且可复现
        const auto mixed = splitmix64(static_cast<std::uint64_t>(cfg.seed) ^ static_cast<std::uint64_t>(d * 17 + 31));
        //门权重范围：0.75 ~ 1.25（unit_from_u64返回0~1，乘以0.5加0.75）
        const double weight = 0.75 + unit_from_u64(mixed) * 0.5;
        circuit.gate_weights.push_back(weight);//存入门权重
    }
    return circuit;//返回生成的电路描述符
}
//拆分总任务数到各个 MPI 进程（负载均衡）
LocalTaskRange split_tasks(std::uint64_t total_tasks, int rank, int world_size) {
    //计算每个进程的基础任务数（总任务数 / 进程数，向下取整）
    const auto base = total_tasks / static_cast<std::uint64_t>(world_size);
    //计算余数（总任务数 % 进程数，即无法均分的任务数）
    const auto rem = total_tasks % static_cast<std::uint64_t>(world_size);
    //计算当前进程的任务起始ID，前rem个进程多分配1个任务，保证负载均衡。比如总任务100，进程数8 → base=12，rem=4 → rank0-3各13个任务，rank4-7各12个
    const auto begin = base * static_cast<std::uint64_t>(rank) + std::min<std::uint64_t>(rem, static_cast<std::uint64_t>(rank));
    //计算当前进程是否需要多分配1个任务（前rem个进程多1个）
    const auto extra = static_cast<std::uint64_t>(rank) < rem ? 1ULL : 0ULL;
    //返回任务范围（begin=起始ID，end=起始ID+基础任务数+额外任务数）
    return LocalTaskRange{begin, begin + base + extra};
}
    // 模拟一个简单的 2x2 矩阵收缩过程
// ResultRecord evaluate_task(const CircuitDescriptor& circuit,
//                                std::uint64_t global_task_id,
//                                int rank,
//                                int repeat_id) {
//     ResultRecord record;//创建空的结果记录
//     //赋值基础标识（任务ID、进程rank、重复次数）
//     record.task_id = global_task_id;
//     record.rank = rank;
//     record.repeat_id = repeat_id;
//
//     // 1. 真实的分配逻辑：量子电路切割通常产生 4^k 个任务
//     // 将 task_id 映射为 Pauli 算符组合 (I, X, Y, Z)
//     record.assignment = global_task_id;
//     /*TODO：量子背景：
//      *量子电路切割是并行化量子仿真的核心技术，切割后会将原电路分解为多个子电路，
//      *每个子电路对应一组Pauli算符（I/X/Y/Z）的组合（共4^k种，k为切割数）；
//     */
//     // 2. 引入计算密集型模拟：张量收缩
//     // 我们模拟对每个切割点进行矩阵运算，增加 CPU 耗时
//     double r_sum = 0.0, i_sum = 0.0;
//     int complexity = circuit.num_qubits; //量子比特数越多，张量维度越高，计算量呈指数增长
//
//     for (int i = 0; i < complexity; ++i) {
//         //模拟矩阵乘法累加：A * B + C
//         double angle = (double)(global_task_id % 100) * 0.01 * M_PI;//生成 0~π 的角度值
//         //实部累加（矩阵乘法的实部），张量收缩的实部计算（量子态的实振幅）
//         r_sum += std::cos(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
//         //虚部累加（矩阵乘法的虚部），张量收缩的虚部计算（量子态的虚振幅）
//         i_sum += std::sin(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
//         // 故意增加循环计算量，以体现计算与 I/O 的平衡
//     }
//
//     // 3. 模拟系数计算（基于 assignment 的奇偶性）
//     double parity = (__builtin_popcountll(record.assignment) % 2 == 0) ? 1.0 : -1.0;
//
//     record.coefficient = parity / (double)(1ULL << circuit.num_cuts);
//     record.value_real = r_sum / complexity;
//     record.value_imag = i_sum / complexity;
//
//     // 最终贡献值：模拟振幅模长
//     record.contribution = record.coefficient * (record.value_real * record.value_real);
//     record.checksum = deterministic_checksum(record);
//
//     return record;
// }

    // 1. 建议新增一个辅助函数，用于根据任务 ID 构建 Qulacs 线路
    // 这样你的代码结构会更清晰
void build_actual_qulacs_circuit(unsigned int n, std::uint64_t task_id, QuantumCircuit& q_circuit) {
    // 这里的逻辑模拟线路切割后的子电路
    // 你可以根据 task_id 的位信息来决定添加什么门
    for (unsigned int i = 0; i < n; ++i) {
        q_circuit.add_H_gate(i);
        if ((task_id >> i) & 1) {
            q_circuit.add_X_gate(i);
        }
    }
    // 添加受控门增加计算量，产生真实的计算开销
    for (unsigned int i = 0; i < n - 1; ++i) {
        q_circuit.add_CNOT_gate(i, i + 1);
    }
}

ResultRecord evaluate_task(const CircuitDescriptor& circuit,
                                   std::uint64_t global_task_id,
                                   int rank,
                                   int repeat_id) {
    ResultRecord record;
    record.task_id = global_task_id;
    record.rank = rank;
    record.repeat_id = repeat_id;
    record.assignment = global_task_id;

    // --- 【关键修改：接入 Qulacs】 ---
    unsigned int n = circuit.num_qubits;
    QuantumState state(n);      // 创建量子态
    state.set_zero_state();     // 初始化

    QuantumCircuit q_circuit(n);
    build_actual_qulacs_circuit(n, global_task_id, q_circuit);

    // 执行高效率的量子仿真
    q_circuit.update_quantum_state(&state);

    // 获取底层复数数组指针 (CPPCTYPE 是 std::complex<double>)
    CPPCTYPE* raw_data = state.data_cpp();

    // 取出第 0 个分量的振幅作为模拟结果
    auto target_amplitude = raw_data[0];

    record.value_real = target_amplitude.real();
    record.value_imag = target_amplitude.imag();
    // -----------------------------

    // 3. 模拟系数计算（保留原逻辑，这是重构的数学基础）
    double parity = (__builtin_popcountll(record.assignment) % 2 == 0) ? 1.0 : -1.0;
    record.coefficient = parity / (double)(1ULL << circuit.num_cuts);

    // 4. 最终贡献值：基于实部和虚部的模长平方
    record.contribution = record.coefficient * (record.value_real * record.value_real + record.value_imag * record.value_imag);

    // 5. 计算校验和（确保你之前的 ResultRecord 结构体已经更新了 value_real/imag 字段）
    record.checksum = deterministic_checksum(record);

    return record;
}

// std::uint64_t deterministic_checksum(const ResultRecord& record) {
//     std::uint64_t h = 1469598103934665603ULL;
//     auto mix = [&](std::uint64_t x) {
//         h ^= x;
//         h *= 1099511628211ULL;
//     };
//
//     mix(record.task_id);
//     mix(record.assignment);
//     mix(static_cast<std::uint64_t>(record.rank));
//     mix(static_cast<std::uint64_t>(record.repeat_id));
//     mix(static_cast<std::uint64_t>(std::llround(record.coefficient * 1e12)));
//     mix(static_cast<std::uint64_t>(std::llround(record.value * 1e12)));
//     mix(static_cast<std::uint64_t>(std::llround(record.contribution * 1e12)));
//     return h;
// }
std::uint64_t deterministic_checksum(const ResultRecord& record) {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix = [&](std::uint64_t x) {
        h ^= x;
        h *= 1099511628211ULL;
    };

    mix(record.task_id);
    mix(record.assignment);
    mix(static_cast<std::uint64_t>(record.rank));
    mix(static_cast<std::uint64_t>(record.repeat_id));
    mix(static_cast<std::uint64_t>(std::llround(record.coefficient * 1e12)));

    // 关键点：将原先的 value 替换为实部和虚部
    mix(static_cast<std::uint64_t>(std::llround(record.value_real * 1e12)));
    mix(static_cast<std::uint64_t>(std::llround(record.value_imag * 1e12)));

    mix(static_cast<std::uint64_t>(std::llround(record.contribution * 1e12)));
    return h;
}
} // namespace qcs