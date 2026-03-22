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

关键结论：每个 Pauli 算符组合对应一个独立的仿真任务，因此总任务数是 `4^k`。

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







#### 毕设方案：

做一个 C++ 的简化量子线路切割并行重构原型系统，采用 MPI + OpenMP 混合并行，在有限节点条件下比较内存、单文件 HDF5、分块多文件三种存储策略对系统性能、扩展性和最大可处理线路规模的影响，并重点研究批量写入与分块参数优化。

