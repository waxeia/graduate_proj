# 存储策略对量子线路切割并行重构性能影响的研究：原型系统源码

这是一个**C++ 简化量子线路切割并行重构原型系统**，面向你的毕设题目，核心特征如下：

- **并行框架**：MPI + OpenMP 混合并行
- **研究对象**：在有限节点条件下比较三种存储策略对并行重构性能的影响
  - 内存存储（memory）
  - 单文件 HDF5（single_hdf5）
  - 分块多文件（multi_file）
- **研究重点**：
  - 批量写入（`batch_size`）
  - HDF5 / 分块文件块大小（`chunk_records`）
  - 不同切割数（`num_cuts`）下的可扩展性与最大可处理规模
- **实验目标**：输出可批量复现的 CSV 结果，便于论文画图与对比分析

> 说明：这是一个“面向存储策略性能研究”的简化原型，而不是完整物理精确量子模拟器。它保留了**线路切割任务指数膨胀、重构任务并行执行、结果批量持久化、回读校验与性能测量**等关键结构，非常适合做毕设的系统实现与实验部分。

---

## 1. 目录结构

```text
quantum_cutting_storage/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── common.hpp
│   ├── config.hpp
│   ├── circuit.hpp
│   ├── storage.hpp
│   ├── engine.hpp
│   └── experiment.hpp
├── src/
│   ├── main.cpp
│   ├── config.cpp
│   ├── circuit.cpp
│   ├── storage.cpp
│   ├── engine.cpp
│   └── experiment.cpp
├── configs/
│   ├── baseline.cfg
│   ├── hdf5.cfg
│   └── multifile.cfg
└── scripts/
    ├── build.sh
    ├── run_one.sh
    ├── run_experiments.sh
    └── sweep.py
```

---

## 2. 设计说明

### 2.1 为什么 main.cpp 很轻

按照你的要求，`main.cpp` 只做四件事：

1. 初始化 MPI
2. 读取配置与命令行覆盖参数
3. 调用 `ExperimentRunner`
4. 统一处理异常并退出

实际逻辑分散在：

- `config.*`：配置加载与参数覆盖
- `circuit.*`：简化线路与重构任务建模
- `storage.*`：三种存储后端的完整语义实现
- `engine.*`：MPI + OpenMP 混合并行执行引擎
- `experiment.*`：重复实验、CSV 汇总、批量复现入口

### 2.2 简化量子线路切割模型

系统使用以下抽象：

1. 根据 `num_qubits / circuit_depth / num_cuts` 生成一个线路描述 `CircuitDescriptor`
2. 根据切割数推导指数级任务数 `total_tasks`
3. 将总任务按 MPI rank 均匀切分
4. 每个 rank 内按 `batch_size` 分批，用 OpenMP 并行计算一个 batch 的重构贡献
5. 每个 batch 形成 `ResultRecord[]` 批量写入存储后端
6. 所有任务完成后，对持久化结果执行回读校验（`verify_readback=true`）并求和
7. 最终由 rank 0 输出 CSV 指标

### 2.3 HDF5 后端语义完整性

这里专门补你强调的点：

- 单文件 HDF5 后端不是“只写一个裸二进制 blob”，而是有**完整语义结构**：
  - 文件级属性：`num_qubits`、`circuit_depth`、`num_cuts`、`total_tasks`、`backend`、`experiment_name`
  - 组层级：`/ranks/rank_i/`
  - 数据集：
    - `/ranks/rank_i/records`：可扩展 compound dataset，保存结果记录
    - `/ranks/rank_i/batch_offsets`：保存每次批量追加的 batch 大小，用于恢复批次边界
- 多文件后端也使用 HDF5 shard，每个 shard 保留：
  - `rank`
  - `shard_index`
  - `records`
  - `backend`
  - `/records` 数据集

也就是说，你后面写论文时，可以明确说明：

- 单文件策略侧重**统一命名空间与集中元数据管理**
- 多文件策略侧重**降低单热点写入竞争、提升并发写吞吐**
- 内存策略作为理论上界/磁盘写零成本基线

---

## 3. Ubuntu 24.04 环境准备

你的机器：

- Ubuntu 24.04
- Intel Core i9-12900H
- 联想拯救者 Y9000P 2022

建议安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake openmpi-bin libopenmpi-dev libhdf5-dev python3 python3-pip
```

如果系统里的 HDF5 是串行版也能跑本项目，因为本项目的单文件 HDF5 写入采用**MPI rank 顺序进入临界区**的安全方式，避免串行 HDF5 的并发写破坏。

---

## 4. 编译

```bash
cd quantum_cutting_storage
bash scripts/build.sh
```

生成可执行文件：

```bash
build/qcsim
```

---

## 5. 运行示例

### 5.1 内存基线

```bash
bash scripts/run_one.sh 1 configs/baseline.cfg \
  --set storage_mode=memory \
  --set output_dir=results/memory_test
```

### 5.2 单文件 HDF5

```bash
bash scripts/run_one.sh 2 configs/baseline.cfg \
  --set storage_mode=single_hdf5 \
  --set output_dir=results/hdf5_test
```

### 5.3 分块多文件

```bash
bash scripts/run_one.sh 4 configs/baseline.cfg \
  --set storage_mode=multi_file \
  --set output_dir=results/multifile_test
```

### 5.4 调节批量写入与块大小

```bash
bash scripts/run_one.sh 4 configs/baseline.cfg \
  --set storage_mode=single_hdf5 \
  --set batch_size=16384 \
  --set chunk_records=65536 \
  --set num_cuts=8 \
  --set omp_threads=8 \
  --set output_dir=results/hdf5_big_batch
```

---

## 6. 批量复现实验

### 6.1 Shell 批量实验

```bash
bash scripts/run_experiments.sh
```

它会自动 sweep：

- MPI ranks：1 / 2 / 4
- backend：memory / single_hdf5 / multi_file
- batch_size：1024 / 4096 / 16384
- chunk_records：4096 / 16384 / 65536
- num_cuts：4 / 6 / 8
- omp_threads：4 / 8 / 12

### 6.2 Python 批量实验

```bash
python3 scripts/sweep.py --summary results/final_summary.csv
```

这会把每次实验末尾输出的 CSV 行自动汇总到一个总表中，适合后续用 pandas / Excel / Origin / matplotlib 画图。

---

## 7. 输出指标说明

程序会输出如下 CSV 字段：

- `experiment_name`
- `backend`
- `mpi_ranks`
- `omp_threads`
- `num_qubits`
- `circuit_depth`
- `num_cuts`
- `total_tasks`
- `batch_size`
- `chunk_records`
- `compute_seconds`
- `write_seconds`
- `read_seconds`
- `total_seconds`
- `global_sum`
- `records_written`
- `bytes_written`
- `files_created`
- `tasks_per_second`
- `max_buffer_mb`

建议论文画图：

1. **总耗时 vs 存储策略**
2. **写入耗时 vs batch_size**
3. **写入耗时 vs chunk_records**
4. **吞吐量（tasks/s）vs MPI ranks**
5. **可处理任务规模 / 切割数 vs 存储策略**
6. **文件数 / 写放大 vs 多文件策略参数**

---

## 8. 建议你的论文实验怎么写

你可以按下面逻辑组织实验：

### 实验一：三种存储策略总体对比

固定：

- `num_qubits=28`
- `circuit_depth=120`
- `num_cuts=6`
- `batch_size=4096`
- `chunk_records=16384`

比较：

- memory
- single_hdf5
- multi_file

指标：

- `total_seconds`
- `write_seconds`
- `read_seconds`
- `tasks_per_second`

### 实验二：批量写入优化

固定 backend，变化：

- `batch_size=1024/4096/16384/65536`

观察：

- 写入时间
- 总时间
- 峰值缓存

### 实验三：分块参数优化

对 HDF5 / 多文件后端分别测试：

- `chunk_records=4096/16384/65536/131072`

观察：

- 小 chunk：元数据和 I/O 调用更频繁
- 大 chunk：单次 I/O 更高效，但内存峰值和尾部浪费可能增加

### 实验四：扩展性

固定任务规模，改变：

- `MPI ranks = 1/2/4`
- `OMP threads = 4/8/12`

观察：

- 混合并行对吞吐量提升
- 单文件 HDF5 的集中写热点瓶颈
- 多文件策略在中高并发下更有优势

### 实验五：最大可处理规模

逐步增加：

- `num_cuts`
- `circuit_depth`
- `total_tasks`

观察：

- 内存策略先受内存约束
- 单文件 HDF5 受集中 I/O 串行瓶颈约束
- 多文件策略在可扩展性和规模容纳方面可能更优

---

## 9. 对你的机器的建议参数

你的 CPU 是 **i9-12900H（14 核 20 线程）**。

建议本机单机实验先这样跑：

- `MPI ranks = 2 或 4`
- `OMP threads = 4 / 8`

一个实用组合：

```text
MPI=2, OMP=8
MPI=4, OMP=4
```

这样一般比 `MPI=1, OMP=16` 更容易观察到“通信 + 本地并行 + 存储竞争”的差异。

如果你要做“有限节点条件”论证，这台单机完全可以视作一个小型共享内存多核实验平台。

---

## 10. 额外说明

### 关于“最大可处理线路规模”

本原型中，线路规模通过以下几项共同控制：

- `num_qubits`
- `circuit_depth`
- `num_cuts`
- `total_tasks`（可显式指定）

默认情况下程序会根据切割数自动推导 `total_tasks`。你也可以手动覆盖，例如：

```bash
bash scripts/run_one.sh 4 configs/baseline.cfg \
  --set total_tasks=2000000 \
  --set storage_mode=multi_file \
  --set output_dir=results/scale_2m
```

### 关于“简化原型”的合理性

你答辩时可以这样解释：

> 本研究的重点不在于量子门级物理仿真的保真度，而在于线路切割后重构任务的并行组织方式与存储行为。为此，系统将重构项计算抽象为与切割规模强相关的批量独立任务流，并保留结果写入、回读校验、单/多文件布局和并行调度等关键机制，从而能够有效研究存储策略对整体重构性能的影响。

---

## 11. 后续你还可以自己再加的内容

如果你后面还想继续完善，我建议你加三类东西：

1. **结果分析脚本**：自动画柱状图、折线图、热力图
2. **内存监控**：读取 `/proc/self/status` 或 `getrusage` 做峰值 RSS 记录
3. **更真实的切割语义**：把 assignment 从位串扩展成 Pauli 基展开标签

---

如果你愿意，我下一条还能继续直接给你：

1. **论文“系统设计”章节初稿**
2. **论文“实验设计与结果分析”章节初稿**
3. **基于本项目的结果绘图 Python 脚本**



遇到的构建问题：

![image-20260311135806505](/home/waxeia/.config/Typora/typora-user-images/image-20260311135806505.png)

![image-20260311135837549](/home/waxeia/.config/Typora/typora-user-images/image-20260311135837549.png)

运行方式一：点击main函数前面的运行小三角，和点击总的运行按钮效果是一致的。当前状态是都没有传参数，执行编译生成的qcsim可执行文件，当然可以从configuration edit里传特定的参数，这样每次点击运行按钮就省去终端执行输入参数的麻烦了。

![image-20260312172058316](/home/waxeia/.config/Typora/typora-user-images/image-20260312172058316.png)

![image-20260312172402133](/home/waxeia/.config/Typora/typora-user-images/image-20260312172402133.png)

运行方式二：终端传参指令，例如：bash scripts/run_one.sh 1 configs/baseline.cfg \
  --set storage_mode=memory \
  --set output_dir=results/memory_test

跑长时间实验：bash scripts/sweep.py

#### C++语言：

const char* 指向字符串字面量（原生字符数组）

const char* c_ptr = "hello";

string* 指向 std::string 对象    

std::string str_obj = "world";    

std::string* s_ptr = &str_obj;

MPI_Finalize()函数跳转：

函数使用--->openMPI头文件声明（对该函数的声明）--->

![image-20260312200237880](/home/waxeia/.config/Typora/typora-user-images/image-20260312200237880.png)

只链接 OpenMPI 的库文件（`.so`/`.a`），没有把 OpenMPI 源码加入项目，依然可以正常使用该库，只是找不到源码实现而已。

![image-20260312200748269](/home/waxeia/.config/Typora/typora-user-images/image-20260312200748269.png)

接下来就能够通过ctrl+alt+b跳转到MPI_Finalize()的实现了。

项目的命名空间qcs，里面有结构体以及函数的声明，主要是为了避免函数名的冲突。

HDF5（Hierarchical Data Format 5）是一种专为大规模科学数据设计的二进制文件格式

Hierarchical/ˌhaɪəˈrɑːrkɪkl/分层的

concurrency/kənˈkɜːrənsi/n 并行性

```C++
MPI_Reduce(
    &metrics.partial_sum,  // 输入：当前进程的局部数据（待汇总）
    &metrics.global_sum,   // 输出：主进程的全局汇总结果（其他进程此变量无意义）
    1,                     // 数据个数（1个double）
    MPI_DOUBLE,            // 数据类型（双精度浮点数）
    MPI_SUM,               // 归约规则（求和）
    0,                     // 目标进程（rank=0的主进程）
    comm_                  // MPI通信域（所有进程在同一个域内通信）
);
```

MPI_Rdduce的归约操作：是所有在 `comm_` 通信域内的进程都会执行的集体通信操作，每个进程都把自己的局部输入数据（比如 `metrics.partial_sum`）提交给 MPI 框架；MPI 框架在内部按指定规则（`MPI_SUM`/`MPI_MAX` 等）对所有进程的数据做归约计算；最终只有目标进程（这里是 rank=0） 会得到汇总后的结果，其他进程的输出变量（`metrics.global_sum`）不会被修改，保持原值或无意义。

属于MPI框架的底层操作。



需要「重复实验」：

![image-20260314161450715](/home/waxeia/.config/Typora/typora-user-images/image-20260314161450715.png)

#### **IDE使用：**

带圆圈的 `p`（紫色）：Property / Function / Method（函数 / 方法 / 属性）

圆圈的 `v`（红色）：Variable（变量）

`S`（紫色方块）：Struct / Class（结构体 / 类）

`f`（紫色方框）：Function（函数）

#### 量子计算：

![image-20260316151055197](/home/waxeia/.config/Typora/typora-user-images/image-20260316151055197.png)

4<sup>k</sup>是量子电路切割的总任务数（物理本质）。

关键结论：每个 Pauli 算符组合对应一个独立的仿真任务，因此总任务数是 `4^k`。

2ᵏ：归一化系数的分母（数学简化）。

![image-20260326164304796](/home/waxeia/.config/Typora/typora-user-images/image-20260326164304796.png)

![image-20260326171604982](/home/waxeia/.config/Typora/typora-user-images/image-20260326171604982.png)

一个 task_id 对应一条 n 量子比特的子线路 

子线路之间必须保证差异

量子计算只在一类特殊问题上有 “指数级加速”。

量子计算机快在：

- 大规模并行叠加
- 指数级状态空间搜索
- 干涉（能自动挑出正确答案）

量子计算，本质是：初始状态 → 一连串量子门变换 → 最终得到答案。

线路越长 = 步骤越多 = 问题越难

线路越宽（比特数越多）= 处理的状态空间越大

任何能被 “一步步分解” 的计算问题，都能写成量子线路

只要问题的复杂度随规模 “指数增长”，就能用长量子线路加速。普通计算机遇到指数问题直接卡死。量子计算机用叠加态天然处理指数规模的数据。

经典计算机：用电开关的 0/1 做加减乘除，擅长通用任务

量子计算机：用微观粒子的叠加 / 纠缠做指数级并行，只在特定问题上碾压经典

对量子具有优势的问题（如大数分解、量子化学、无结构搜索等）：子计算机 ＞＞＞ 经典最优算法 ＞＞＞ 经典仿真量子。但因为当前量子硬件受限，我们必须依靠仿真与线路切割来研究长深度量子算法。

CFL3D 无法在量子计算机上运行，也无法被量子计算加速；量子计算不适合经典流体力学数值模拟。

量子计算机擅长：

- 指数级搜索
- 量子系统模拟
- 傅里叶变换
- 线性方程组 特殊类型（HHL 算法）
- 组合优化

量子计算机 不擅长、甚至做不了****：

- 双精度浮点数运算
- 复杂差分格式
- 有限体积法
- 大规模稀疏矩阵迭代
- CFD 这种经典数值计算

换句话说：CFD 不是量子计算机的菜。

有人说 “量子流体力学”？

确实有量子流体力学（QFD），但：

- 它是理论研究
- 只能算极小系统
- 完全不能工程应用
- 和 CFL3D 这种航空航天级 CFD 完全不是一回事

简单说：能算 CFL3D 的量子计算机还没诞生，也不知道什么时候能诞生。







##### 复数域上的加权旋转操作

```C++
//实部累加（矩阵乘法的实部），张量收缩的实部计算（量子态的实振幅）
r_sum += std::cos(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
//虚部累加（矩阵乘法的虚部），张量收缩的虚部计算（量子态的虚振幅）
i_sum += std::sin(angle) * circuit.gate_weights[i % circuit.gate_weights.size()];
```

这段代码的核心是在做**复数累加**。在经典计算机模拟量子态时，量子态的振幅（Amplitude）是一个复数 <em>a+bi</em> 。

- `r_sum` (Real Sum)：实部累加器。
- `i_sum` (Imaginary Sum)：虚部累加器。
- `angle`：旋转角度（通常是神经网络的可训练参数 θ*θ* ）。
- `circuit.gate_weights[i % ...]`：权重系数。这里使用了取模运算 `%`，说明权重数组可能比循环次数短，权重是**复用**的（类似于卷积神经网络中的权值共享，或者是为了节省内存）。

手动计算一个复数的加权和。它把一个角度（`angle`）转换成复平面上的坐标 (cos⁡,sin⁡)(cos,sin) ，乘以一个权重，然后累加到总结果中。这是模拟量子比特状态演化的基础数学操作。

- **量子门（Quantum Gate）**：
  在量子计算中，最基本的操作是旋转门（如 Rz(θ)*R**z*​(*θ*) 或 Ry(θ)*R**y*​(*θ*) ）。这些门在数学上表现为**酉矩阵（Unitary Matrix）**。
  一个简单的旋转操作作用于量子态时，本质上就是在复平面上进行旋转。

  - cos⁡(angle)cos(angle) 对应旋转后的实轴投影。

  - sin⁡(angle)sin(angle) 对应旋转后的虚轴投影。

“张量收缩（Tensor Contraction）就是广义的矩阵乘法。

- **张量收缩（Tensor Contraction）**：

  模拟多量子比特系统时，状态是一个高维张量。当应用一个门操作时，需要将该门的矩阵与状态张量进行收缩（类似高维矩阵乘法）。
  由于量子态必须是复数，所以计算过程中必须分别维护实部和虚部（或者直接使用 `std::complex<double>`，但为了极致性能，高性能计算库常手动拆分实虚部以利用SIMD指令优化）。![image-20260322135338642](/home/waxeia/.config/Typora/typora-user-images/image-20260322135338642.png)

2个量子比特的状态是一个长度为4的向量（[α₀₀, α₀₁, α₁₀, α₁₁]），每个元素是复数。
3个量子比特 → 长度8的向量。
n个量子比特 → 长度 2ⁿ 的向量！

该向量所处的空间维度非常高的一阶张量，而不是阶数高，“状态是一个高维张量” = “量子态用一个大数组表示，每个元素是复数”。

![image-20260322142605766](/home/waxeia/.config/Typora/typora-user-images/image-20260322142605766.png)

行向量是第1阶，列向量是第2阶。

![image-20260322142915141](/home/waxeia/.config/Typora/typora-user-images/image-20260322142915141.png)

![image-20260322143126888](/home/waxeia/.config/Typora/typora-user-images/image-20260322143126888.png)

全局任务标识符task_id：标识不同泡利测量的子线路。

切断处纠缠态在经典计算中的投影：

![image-20260327152257131](/home/waxeia/.config/Typora/typora-user-images/image-20260327152257131.png)

**`n`**：量子比特的总数

**`task_id`**：当前任务的唯一编号，4<sup>k</sup>的其中之一。

**`q_circuit`**：Qulacs 的量子线路对象。它是空的，等着我们往上“挂”量子门。

![image-20260327204639662](/home/waxeia/.config/Typora/typora-user-images/image-20260327204639662.png)

![image-20260327210955860](/home/waxeia/.config/Typora/typora-user-images/image-20260327210955860.png)

`world_size`（也就是 MPI 的总进程数量，由 `mpirun` 的 `-n` 或 `-np` 参数控制），运行不同的测试脚本就会有不同的值。

![image-20260327211248710](/home/waxeia/.config/Typora/typora-user-images/image-20260327211248710.png)

record.coefficient：重构系数，结合符号翻转（parity，模拟泡利测量的相位变化）和归一化因子（每切一刀引入 1/2 的权重）。它确保切割后的子任务正确聚合。

1ULL << 3 = 8（即 2^3）

`run_all_benchmarks.sh` 存储压力单次测试脚本。

run_experiments.sh 全量实验脚本。

batch_size（内存端的批量）：

- 定义**：** 指在程序运行过程中，在内存中累积多少条计算结果（`ResultRecord`）后再执行一次写入硬盘的操作。
- 比喻： 就像送快递，你不是出一件货就跑一趟（I/O 次数多，效率低），而是攒够一车（一个 batch）再出发。
- 影响： `batch_size` 越大，硬盘写入次数越少，吞吐量通常越高；但它直接占用内存空间。

chunk_records（硬盘端的物理分块）：

- 定义： 这是 HDF5 或文件系统底层存储的概念。它决定了数据在硬盘上是如何物理组织的。一个 chunk 是文件系统一次读写的最小逻辑单元。
- 比喻： 就像仓库里的货架格子。无论你一次运来多少货（batch_size），最后都要按格子（chunk）的大小填进去。
- 影响： 它主要影响 HDF5 文件的检索效率和对齐性能。如果 `chunk_records` 设置得太小，文件元数据负担重；如果设置得太大，读取一小条数据也要加载一整个大块，造成浪费。

###  在你项目中的实际运作

1. **生产**：OpenMP 线程在内存中快速计算出量子线路重构的结果。
2. **积压**：这些结果并不直接写进硬盘，而是先存放在**缓冲区**（内存）里。
3. **触发**：当缓冲区里的结果数量达到了你设定的 `batch_size`（比如 4096 条）时。
4. **存盘**：系统执行一次集中的磁盘写入操作（I/O），把这 4096 条数据一次性“倒”进 HDF5 文件或多文件中。

理解 HDF5 复合数据集 (Compound Dataset) 最直观的方式是把它看作 “磁盘上的 C++ 结构体数组









amplitude 振幅

实部（real part）

虚部（imaginary part）

Quantum Circuit Cutting 量子电路切割 ˈkwɑːntəm / ˈsɜːrkɪt /

归一化因子（normalization factor）

dock /dɑːk/ 码头；停靠处

parity / ˈpærəti / 奇偶性；*n.*（尤指薪金或地位）平等

Resource Acquisition Is Initialization 资源获取即初始化操作

Sharding n.分片；分区

Qulacs 标准发音：/ˈkwælæks/（近似中文：夸 - 拉克斯）

HDF5读作：H-D-F-five

HDF5 = Hierarchical Data Format 5 /ˌhaɪəˈrɑːkɪkl ˈdeɪtə ˈfɔːmæt faɪv/











#### C++

**std::unique_ptr 是什么？**
std::unique_ptr 是 C++ 标准库中的一种智能指针，位于 <memory> 头文件中。
独占所有权：一个对象只能被一个 std::unique_ptr 拥有。如果想转移所有权，可以用 std::move()。
自动释放：当 std::unique_ptr 变量超出作用域（scope）时，自动调用 delete 删除对象。
语法：
创建：std::unique_ptr<Type> ptr = std::make_unique<Type>(构造函数参数);
使用：ptr->method(); 或 (*ptr).method();（像普通指针一样）。
不能复制：std::unique_ptr 不能直接赋值给另一个变量（防止多个指针管理同一个对象）。

**std::make_unique**

比直接 new 更安全：如果构造函数抛出异常，std::make_unique 不会泄漏内存。
语法简洁：std::make_unique<Type>(args) 相当于 std::unique_ptr<Type>(new Type(args))，但更安全。

**RAII**（Resource Acquisition Is Initialization）模式，确保资源在对象生命周期结束时自动释放。

在 C++ 中，当一个局部对象离开它所在的作用域（比如一个函数执行结束，或者遇到一个右大括号 `}`）时，系统会自动调用它的**析构函数**。

在 HDF5 中，想要读写数据，第一步必须是创建或打开一个 `.h5` 文件（调用 C 接口 `H5Fcreate` 或 `H5Fopen`）。成功后，HDF5 会返回一个文件句柄 `hid_t`。 `ScopedH5File` 就是用来妥善保管这个文件句柄的。













#### 毕设方案：

做一个 C++ 的简化量子线路切割并行重构原型系统，采用 MPI + OpenMP 混合并行，在有限节点条件下比较内存、单文件 HDF5、分块多文件三种存储策略对系统性能、扩展性和最大可处理线路规模的影响，并重点研究批量写入与分块参数优化。



#### 给毕设的下一步建议（如何让论文数据更好看）

为了让你的论文曲线更有说服力（即体现出“存储瓶颈”），你可以尝试以下操作：

1. **开启 MPI 并行**：使用 `mpirun -n 4 ./qcsim ...` 跑同样的脚本。当多个进程同时挤向一个 HDF5 文件时，文件锁冲突会让 HDF5 的写入时间成倍增加，而 Multi-File 的优势会非常巨大。
2. **增加任务数到 50 万**：让写入的数据量达到 30MB 以上。
3. **降低计算量**：在 `circuit.cpp` 里暂时减小量子电路的层数（或比特数），让计算变快，这样存储开销在总耗时里的占比就会提高，图表看起来更直观。



#### 实验流程：

从“单进程到多进程（MPI 并行）”以及“小数据块到大数据块（Batch Size 优化）”的跨越。



补充单进程的脚本：

```bash
#!/bin/bash

# 基础设置
EXE="./build/qcsim"
CONFIG="./configs/baseline.cfg"
OUT_DIR="results/benchmarks"
# 这里的任务数可以根据你电脑性能调整，50000条数据足以看出HDF5和MultiFile的区别
TASKS=50000 
QUBITS=15

# 创建输出目录
mkdir -p $OUT_DIR

echo "=================================================="
echo "开始量子仿真存储策略自动化测试"
echo "任务规模: $TASKS tasks, $QUBITS qubits"
echo "=================================================="

# 定义要测试的模式
MODES=("memory" "hdf5" "multifile")

# 准备汇总表格文件
SUMMARY_FILE="$OUT_DIR/final_comparison.csv"
echo "Mode,Total_Time(s),Write_Time(s),Throughput(task/s)" > $SUMMARY_FILE

for MODE in "${MODES[@]}"
do
    echo "正在测试模式: $MODE ..."
    
    # 执行程序，使用 --set 动态覆盖配置
    # 我们强制设 omp_threads=1 确保 Qulacs 稳定
    $EXE --config $CONFIG \
         --set storage_mode=$MODE \
         --set total_tasks=$TASKS \
         --set num_qubits=$QUBITS \
         --set omp_threads=1 \
         --set output_dir="$OUT_DIR/$MODE" \
         --set verbose=false > "$OUT_DIR/${MODE}_log.txt"

    # 从生成的 summary.csv 中提取数据 (假设是最后一行)
    # 注意：这里根据你具体的 csv 格式提取 Total_s, Write_s, Throughput
    RESULT_LINE=$(tail -n 1 "$OUT_DIR/$MODE/summary.csv")
    
    # 提取关键字段（根据你的 CSV 列索引：14是Total_s, 11是Write_s, 19是Throughput）
    TOTAL_S=$(echo $RESULT_LINE | cut -d',' -f14)
    WRITE_S=$(echo $RESULT_LINE | cut -d',' -f11)
    TPS=$(echo $RESULT_LINE | cut -d',' -f19)

    # 写入汇总表
    echo "$MODE,$TOTAL_S,$WRITE_S,$TPS" >> $SUMMARY_FILE
    
    echo "完成 $MODE: 耗时 ${TOTAL_S}s, 吞吐量 ${TPS} task/s"
    echo "--------------------------------------------------"
done

echo "所有测试已完成！"
echo "汇总表格已生成: $SUMMARY_FILE"
column -t -s, $SUMMARY_FILE # 在终端打印漂亮表格
```



#### 后续改进方向：

在 `src/storage.cpp` 中，两个版本都统一将 `ResultRecord` 的 compound dataset 结构从早期的系数模式改为实部/虚部模式（`value_real`, `value_imag`）。这更符合量子态振幅的物理本质，且手动拆分实虚部有利于在后续扩展中使用 SIMD 指令进行并行加速。



```C++
for (unsigned int i = 0; i < n - 1; ++i) {
        q_circuit.add_CNOT_gate(i, i + 1);
    }
```

这一步虽然和线路切割理论本身无关，但对于测试你的 MPI 分布式系统性能是极其必要的。如果没有这堆 CNOT，计算速度太快，你根本测不出多节点并行的加速比，全变成 I/O 瓶颈了。

![image-20260327212321540](/home/waxeia/.config/Typora/typora-user-images/image-20260327212321540.png)

assignment 位态分派”或“算符配置



#### 答辩问题：

评委老师问：**“你的并行任务规模是如何计算的？为什么是这个规模？”**

你可以非常自信地回答：

> “在我的项目中，任务规模严格遵循量子线路切割的准概率分解（QPD）理论。每次切割量子线，都需要用完整的泡利群（I, X, Y, Z）进行基底展开。因此，理论上 $k$ 次切割会产生严格的 $4^k$ 个独立子线路仿真任务。
>
> 在代码实现中，我摒弃了早期用于盲目压测的非物理经验公式，直接采用了位移运算 `1ULL << (2 * k)` 来精准映射 $4^k$。这样不仅在数学和物理上 100% 严谨，而且将任务 ID 与泡利算符的四进制位操作完美绑定，极大提升了任务分发和物理重构的效率。”

如果老师让你展示代码亮点，你可以把这三个类（`ScopedH5File`, `ScopedH5Space`, `ScopedH5Type`）放在一页 PPT 里，总结为：

> “为了确保在数十万次高频 MPI 读写任务中，底层 C 语言库（HDF5）不会发生任何内存泄漏或文件锁死问题，我采用了 C++ 的 RAII 设计模式对所有底层句柄进行了面向对象的安全封装，实现了资源的自动回收，极大提升了分布式集群在极端压力下的稳定性。”

如果老师问你为什么不直接存成二进制文件，而要用 HDF5 的复合数据集：

> - 自解释性 (Self-describing)：即使不看你的 C++ 代码，别人用 Python 的 `h5py` 打开文件，也能看到字段名叫做 `task_id` 和 `value_real`。
> - 异构性支持：它允许你在同一个数据条目里同时存放整数 (`uint64_t`) 和浮点数 (`double`)，且能保证跨平台（比如从 Linux 到 Windows）时的字节序正确。
> - 高性能读取：未来如果只想分析“概率分布”，你可以只读取 `contribution` 这一列，而不需要把整个结构体都读进内存。
