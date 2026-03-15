#include "circuit.hpp"

namespace qcs {
namespace {
std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31U);
}

double unit_from_u64(std::uint64_t x) {
    return static_cast<double>(x & 0x1FFFFFFFFFFFFFULL) / static_cast<double>(0x1FFFFFFFFFFFFFULL);
}
}//namespace

CircuitDescriptor make_circuit(const AppConfig& cfg) {
    CircuitDescriptor circuit;
    circuit.num_qubits = cfg.num_qubits;
    circuit.circuit_depth = cfg.circuit_depth;
    circuit.num_cuts = cfg.num_cuts;
    circuit.seed = cfg.seed;
    circuit.total_tasks = derive_total_tasks(cfg);
    circuit.cut_locations.reserve(cfg.num_cuts);
    circuit.gate_weights.reserve(cfg.circuit_depth);

    for (int i = 0; i < cfg.num_cuts; ++i) {
        const int loc = (cfg.circuit_depth * (i + 1)) / (cfg.num_cuts + 1);
        circuit.cut_locations.push_back(loc);
    }

    for (int d = 0; d < cfg.circuit_depth; ++d) {
        const auto mixed = splitmix64(static_cast<std::uint64_t>(cfg.seed) ^ static_cast<std::uint64_t>(d * 17 + 31));
        const double weight = 0.75 + unit_from_u64(mixed) * 0.5;
        circuit.gate_weights.push_back(weight);
    }
    return circuit;
}

LocalTaskRange split_tasks(std::uint64_t total_tasks, int rank, int world_size) {
    const auto base = total_tasks / static_cast<std::uint64_t>(world_size);
    const auto rem = total_tasks % static_cast<std::uint64_t>(world_size);
    const auto begin = base * static_cast<std::uint64_t>(rank) + std::min<std::uint64_t>(rem, static_cast<std::uint64_t>(rank));
    const auto extra = static_cast<std::uint64_t>(rank) < rem ? 1ULL : 0ULL;
    return LocalTaskRange{begin, begin + base + extra};
}

// ResultRecord evaluate_task(const CircuitDescriptor& circuit,
//                            std::uint64_t global_task_id,
//                            int rank,
//                            int repeat_id) {
//     ResultRecord record;
//     record.task_id = global_task_id;
//     record.assignment = global_task_id & ((1ULL << std::min(20, std::max(1, circuit.num_cuts * 2))) - 1ULL);
//     record.rank = rank;
//     record.repeat_id = repeat_id;
//
//     const auto mix0 = splitmix64(global_task_id ^ static_cast<std::uint64_t>(circuit.seed));
//     const auto mix1 = splitmix64(mix0 + static_cast<std::uint64_t>(circuit.num_qubits * 131 + circuit.num_cuts * 17));
//     const auto mix2 = splitmix64(mix1 ^ static_cast<std::uint64_t>(circuit.circuit_depth * 29 + repeat_id * 7));
//
//     const double phase = unit_from_u64(mix0) * 2.0 * M_PI;
//     const double amplitude = 0.15 + 0.85 * unit_from_u64(mix1);
//     const double parity = ((__builtin_popcountll(record.assignment) % 2) == 0) ? 1.0 : -1.0;
//
//     double spectral = 0.0;
//     const int stride = std::max(1, circuit.circuit_depth / std::max(1, circuit.num_cuts + 1));
//     for (int idx = 0; idx < circuit.circuit_depth; idx += stride) {
//         spectral += std::sin(phase + circuit.gate_weights[static_cast<std::size_t>(idx)] * 0.5 * (idx + 1));
//         spectral += std::cos(phase * 0.5 + circuit.gate_weights[static_cast<std::size_t>(idx)] * 0.25 * (idx + 1));
//     }
//
//     record.coefficient = parity * amplitude / static_cast<double>(1ULL << std::min(20, std::max(1, circuit.num_cuts)));
//     record.value = spectral / static_cast<double>(2 * ((circuit.circuit_depth + stride - 1) / stride));
//     record.contribution = record.coefficient * record.value;
//     record.checksum = deterministic_checksum(record) ^ mix2;
//     return record;
// }
    // src/circuit.cpp

    // 模拟一个简单的 2x2 矩阵收缩过程
    ResultRecord evaluate_task(const CircuitDescriptor& circuit,
                               std::uint64_t global_task_id,
                               int rank,
                               int repeat_id) {
    ResultRecord record;
    record.task_id = global_task_id;
    record.rank = rank;
    record.repeat_id = repeat_id;

    // 1. 真实的分配逻辑：量子电路切割通常产生 4^k 个任务
    // 将 task_id 映射为 Pauli 算符组合 (I, X, Y, Z)
    record.assignment = global_task_id;

    // 2. 引入计算密集型模拟：张量收缩
    // 我们模拟对每个切割点进行矩阵运算，增加 CPU 耗时
    double r_sum = 0.0, i_sum = 0.0;
    int complexity = circuit.num_qubits; // 模拟与量子比特数相关的计算复杂度

    for (int i = 0; i < complexity; ++i) {
        // 模拟矩阵乘法累加：A * B + C
        double angle = (double)(global_task_id % 100) * 0.01 * M_PI;
        r_sum += std::cos(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
        i_sum += std::sin(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
        // 故意增加循环计算量，以体现计算与 I/O 的平衡
    }

    // 3. 模拟系数计算（基于 assignment 的奇偶性）
    double parity = (__builtin_popcountll(record.assignment) % 2 == 0) ? 1.0 : -1.0;

    record.coefficient = parity / (double)(1ULL << circuit.num_cuts);
    record.value_real = r_sum / complexity;
    record.value_imag = i_sum / complexity;

    // 最终贡献值：模拟振幅模长
    record.contribution = record.coefficient * (record.value_real * record.value_real);
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