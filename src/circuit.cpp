#include "circuit.hpp"
#include <cppsim/state.hpp>
#include <cppsim/circuit.hpp>
#include <cppsim/gate_factory.hpp>
#include <omp.h> // 必须引入，用于控制线程

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

//使用 4^k 泡利基(I, X, Y, Z)构建 Qulacs 线路
void build_actual_qulacs_circuit(unsigned int n, std::uint64_t task_id, QuantumCircuit& q_circuit, int num_cuts) {
    //初始化叠加态
    for (unsigned int i = 0; i < n; ++i) {
        q_circuit.add_H_gate(i);
    }

    //核心：4^k 真实泡利切割逻辑
    //遍历每一个切割点，从 task_id 中每次提取 2 个 bit 作为四进制状态
    for (int i = 0; i < num_cuts; ++i) {
        // (task_id >> (2 * i))：每次向右移 2*i 位
        // & 3 (二进制 11)：截取最低的两位，得到 0, 1, 2, 3
        int pauli_type = (task_id >> (2 * i)) & 3;

        // 为了防止切割点数量多于比特数，取模分配到不同的量子线上
        unsigned int target_qubit = i % n;

        // 映射到真实的泡利算符组合
        switch (pauli_type) {
            case 0:
                // I 门 (Identity)：保持不变，不做任何操作
                break;
            case 1:
                q_circuit.add_X_gate(target_qubit); // X 门
                break;
            case 2:
                q_circuit.add_Y_gate(target_qubit); // Y 门
                break;
            case 3:
                q_circuit.add_Z_gate(target_qubit); // Z 门
                break;
        }
    }//实现了切断处纠缠态在经典计算中的投影。

    // 3. 添加受控门产生真实的计算和纠缠开销
    for (unsigned int i = 0; i < n - 1; ++i) {
        q_circuit.add_CNOT_gate(i, i + 1);
    }//向相邻的比特之间添加CNOT控制门。单比特门计算极快，加入双比特纠缠门可以强制Qulacs执行复杂的矩阵运算，从而产生真实的CPU负载，用于压测MPI集群的调度性能。
}

//任务评估函数
ResultRecord evaluate_task(const CircuitDescriptor& circuit,
                                       std::uint64_t global_task_id,
                                       int rank,
                                       int repeat_id) {
    //关键点1：强制锁定Qulacs内部OpenMP线程为1
    omp_set_num_threads(1);
    //为当前要执行的量子子任务“建档”
    ResultRecord record;//结构体对象
    record.task_id = global_task_id;//范围在0到4^k-1之间
    record.rank = rank;//记录当前是哪一个MPI进程（进程号从0到world_size-1）正在执行这个任务
    record.repeat_id = repeat_id;
    record.assignment = global_task_id;//本质是一张“量子门装配配方表”，一条子线路的配方表

    unsigned int n = circuit.num_qubits;

    // 1.初始化量子态为 |00...0>
    QuantumState state(n);
    state.set_zero_state();

    // 2.构建量子线路 (注意这里传入了 circuit.num_cuts)
    QuantumCircuit q_circuit(n);
    build_actual_qulacs_circuit(n, global_task_id, q_circuit, circuit.num_cuts);

    // 3.执行仿真计算
    q_circuit.update_quantum_state(&state);

    // 4.获取计算结果
    auto* raw_data = state.data_cpp();
    auto target_amplitude = raw_data[0];

    record.value_real = target_amplitude.real();
    record.value_imag = target_amplitude.imag();

    // 5. 升级版的重构系数计算 (Quasiprobability Decomposition Coefficient)
    // 真实切割中，不同泡利测量有不同的权重(通常带正负号)。
    // 这里我们解析 task_id 的四进制位，模拟真实的符号翻转：
    // 假设遇到 Y(2) 或 Z(3) 算符时，奇偶性翻转。
    double parity = 1.0;
    std::uint64_t temp_id = global_task_id;
    for (int i = 0; i < circuit.num_cuts; ++i) {
        int pauli_type = temp_id & 3; // 取当前切割点的状态
        if (pauli_type == 2 || pauli_type == 3) {
            parity = -parity; // 模拟 Y 和 Z 测量带来的符号变化
        }
        temp_id >>= 2; // 右移2位，检查下一个切割点
    }

    // 在 QCC 理论中，每切一刀通常会引入 1/2 的权重系数（或类似的归一化因子）
    // 这里我们保持数学框架，每刀引入系数 1/2，所以 k 刀除以 2^k
    record.coefficient = parity / (double)(1ULL << circuit.num_cuts);

    // 6. 最终贡献值：模长平方 * 重构系数
    record.contribution = record.coefficient * (record.value_real * record.value_real + record.value_imag * record.value_imag);

    // 7.计算校验和
    record.checksum = deterministic_checksum(record);

    return record;
}

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