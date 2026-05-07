import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 过滤数据：锁定宏观最优解
df_filtered = df[(df['backend'] == 'multi_file') &
                 (df['num_cuts'] == 4) &
                 (df['mpi_ranks'] == 2)]

# 获取该配置下所有跑过的线程数并排序 (例如：[1, 2])
threads_list = sorted(df_filtered['omp_threads'].unique())
n_threads = len(threads_list)

# 3. 准备绘图环境 (1行 N 列)
fig, axes = plt.subplots(1, n_threads, figsize=(5 * n_threads + 2, 5.5), sharey=True, dpi=150)

# 如果只有一个线程数据，统一转换为列表格式防报错
if n_threads == 1:
    axes = [axes]

# 【关键】：统一全局颜色标尺 (vmin, vmax)
vmin = df_filtered['tasks_per_second'].min()
vmax = df_filtered['tasks_per_second'].max()

# 4. 循环为每个 Thread 数值画一个热力图
for i, thread in enumerate(threads_list):
    # 提取特定线程数的数据
    df_thread = df_filtered[df_filtered['omp_threads'] == thread]

    # 聚合并转换为矩阵
    pivot_df = df_thread.groupby(['batch_size', 'chunk_records'])['tasks_per_second'].mean().reset_index()
    pivot_df = pivot_df.pivot(index='batch_size', columns='chunk_records', values='tasks_per_second')
    pivot_df = pivot_df.sort_index(ascending=False) # 翻转 Y 轴，大 Batch 在上

    # 画图
    sns.heatmap(
        pivot_df,
        ax=axes[i],
        annot=True,
        fmt=".1f",
        cmap="YlGnBu",     # 依然使用科技感的蓝绿色系
        vmin=vmin,
        vmax=vmax,
        cbar=(i == n_threads - 1), # 只在最后一个图画颜色条
        cbar_kws={'label': 'Throughput (Tasks / Second)'} if i == n_threads - 1 else None,
        linewidths=1,
        linecolor='white'
    )

    # 修饰每个子图
    axes[i].set_title(f'OpenMP Threads = {thread}', fontsize=13, pad=12)
    axes[i].set_xlabel('Chunk Records (IO Granularity)', fontsize=12)

    if i == 0:
        axes[i].set_ylabel('Batch Size (Compute Granularity)', fontsize=12)
    else:
        axes[i].set_ylabel('')

# 5. 总标题与排版
plt.suptitle('Micro-Tuning: Thread Concurrency vs. Buffer Granularity\n(Fixed: Multi-File, MPI=2, Cuts=4)',
             fontsize=15, fontweight='bold', y=1.05)

plt.tight_layout()
plt.savefig('faceted_micro_tuning.png', bbox_inches='tight')
plt.show()