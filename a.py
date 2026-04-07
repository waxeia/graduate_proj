import pandas as pd
import matplotlib.pyplot as plt
import os

# 设置绘图风格
plt.style.use('seaborn-v0_8')
plt.rcParams['font.sans-serif'] = ['SimHei']  # 正常显示中文
plt.rcParams['axes.unicode_minus'] = False

# 路径定义
base_path = "results/mpi_benchmarks"
comparison_file = os.path.join(base_path, "mpi_comparison.csv")

# 1. 存储策略吞吐量对比图 (柱状图)
def plot_throughput_comparison():
    df = pd.read_csv(comparison_file)
    plt.figure(figsize=(10, 6))
    colors = ['#808080', '#d62728', '#2ca02c'] # 灰色(Memory), 红色(HDF5), 绿色(Multi-file)
    bars = plt.bar(df['Mode'], df['Throughput(task/s)'], color=colors, alpha=0.8)

    plt.title('不同存储策略吞吐量对比 (8 MPI Ranks)', fontsize=14)
    plt.ylabel('吞吐量 (Tasks/Second)', fontsize=12)
    plt.grid(axis='y', linestyle='--', alpha=0.7)

    # 添加数值标签
    for bar in bars:
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2, yval + 20, f'{yval:.2f}', ha='center', va='bottom')

    plt.savefig(os.path.join(base_path, 'throughput_comparison.png'))
    print("已生成: throughput_comparison.png")

# 2. 耗时构成分析 (堆叠柱状图)
def plot_time_breakdown():
    # 从各个模式的 summary.csv 提取详细耗时
    modes = ['memory', 'hdf5', 'multifile']
    plot_data = []

    for mode in modes:
        sum_df = pd.read_csv(os.path.join(base_path, mode, "summary.csv"))
        last_row = sum_df.iloc[-1]
        plot_data.append({
            'Mode': mode,
            'Compute': last_row['compute_seconds'],
            'Write': last_row['write_seconds'],
            'Read': last_row['read_seconds']
        })

    df_time = pd.DataFrame(plot_data)
    df_time.set_index('Mode').plot(kind='bar', stacked=True, figsize=(10, 6), color=['#1f77b4', '#ff7f0e', '#2ca02c'])

    plt.title('各模式耗时构成分析', fontsize=14)
    plt.ylabel('时间 (秒)', fontsize=12)
    plt.legend(['计算耗时', '写入耗时', '校验耗时'])
    plt.xticks(rotation=0)
    plt.savefig(os.path.join(base_path, 'time_breakdown.png'))
    print("已生成: time_breakdown.png")

# 3. Batch Size 与内存占用/性能平衡图 (折线图)
def plot_batch_impact():
    # 以 HDF5 为例展示不同 Batch Size 的影响
    hdf5_sum = pd.read_csv(os.path.join(base_path, "hdf5", "summary.csv"))

    fig, ax1 = plt.subplots(figsize=(10, 6))

    # 吞吐量曲线
    ax1.set_xlabel('Batch Size', fontsize=12)
    ax1.set_ylabel('吞吐量 (Tasks/s)', color='#1f77b4', fontsize=12)
    ax1.plot(hdf5_sum['batch_size'].astype(str), hdf5_sum['tasks_per_second'], color='#1f77b4', marker='o', label='吞吐量')
    ax1.tick_params(axis='y', labelcolor='#1f77b4')

    # 内存占用曲线
    ax2 = ax1.twinx()
    ax2.set_ylabel('峰值缓冲区 (MB)', color='#d62728', fontsize=12)
    ax2.plot(hdf5_sum['batch_size'].astype(str), hdf5_sum['max_buffer_mb'], color='#d62728', marker='s', linestyle='--', label='内存占用')
    ax2.tick_params(axis='y', labelcolor='#d62728')

    plt.title('Batch Size 对性能与内存占用的影响 (HDF5 模式)', fontsize=14)
    fig.tight_layout()
    plt.savefig(os.path.join(base_path, 'batch_impact.png'))
    print("已生成: batch_impact.png")

if __name__ == "__main__":
    plot_throughput_comparison()
    plot_time_breakdown()
    plot_batch_impact()