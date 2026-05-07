import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 数据过滤：锁定宏观变量 (Backend 和 Cuts)
# 假设你在 cuts=6 时进行了混合并行的测试
df_filtered = df[(df['backend'] == 'multi_file') & (df['num_cuts'] == 6)].copy()

# 3. 计算总核心数 (Total Cores)
df_filtered['total_cores'] = df_filtered['mpi_ranks'] * df_filtered['omp_threads']

# 4. 提取总核心数固定的实验组 (例如: 8核)
# 如果你的数据中最大只有 4 核，请将这里的 target_cores 改为 4
target_cores = 8
df_hybrid = df_filtered[df_filtered['total_cores'] == target_cores].copy()

if df_hybrid.empty:
    print(f"提示: 未找到总核心数为 {target_cores} 的数据。请修改 target_cores 变量匹配你的实际数据。")
else:
    # 5. 生成配置标签 (例如: "8 MPI x 1 OMP")
    df_hybrid['config_label'] = df_hybrid.apply(
        lambda x: f"{int(x['mpi_ranks'])} MPI $\\times$ {int(x['omp_threads'])} OMP", axis=1
    )

    # 按 OMP 线程数从小到大排序 (即从纯 MPI 过渡到混合并行)
    df_hybrid = df_hybrid.sort_values('omp_threads')

    # 计算均值 (防止同一配置有多个批次数据)
    plot_df = df_hybrid.groupby('config_label', sort=False)['tasks_per_second'].mean().reset_index()

    # 6. 开始绘图
    plt.figure(figsize=(9, 6), dpi=150)

    # 绘制折线图，使用大标记点突出不同的架构组合
    ax = sns.lineplot(
        data=plot_df,
        x='config_label',
        y='tasks_per_second',
        marker='D',          # 菱形标记
        markersize=12,
        linewidth=3,
        color='#C44E52'      # 使用学术红
    )

    # 7. 图表修饰
    plt.title(f'Hybrid Parallelism: Configuration vs. Throughput\n(Fixed constraint: {target_cores} Total Cores, Multi-File, Cuts=6)',
              fontsize=14, pad=15)
    plt.xlabel('Parallel Configuration (MPI Ranks $\\times$ OpenMP Threads)', fontsize=12)
    plt.ylabel('Throughput (Tasks / Second)', fontsize=12)

    # 在数据点上方加上具体的数值标签，方便论文阅读
    for x, y in zip(range(len(plot_df)), plot_df['tasks_per_second']):
        plt.text(x, y + (plot_df['tasks_per_second'].max() * 0.01),
                 f"{y:.1f}",
                 ha='center', va='bottom', fontsize=11, fontweight='bold', color='black')

    # 设置网格和 Y 轴范围
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.ylim(plot_df['tasks_per_second'].min() * 0.95, plot_df['tasks_per_second'].max() * 1.08)

    # 8. 保存并显示
    plt.tight_layout()
    plt.savefig('hybrid_parallelism_analysis.png')
    plt.show()