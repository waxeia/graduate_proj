import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 数据过滤：锁定宏观定调的最优参数组合
# 锁定：Multi-File 后端, 切分次数=4, MPI进程=2
df_filtered = df[(df['backend'] == 'multi_file') &
                 (df['num_cuts'] == 4) &
                 (df['mpi_ranks'] == 2)]

# 3. 数据聚合：
# 因为这批数据中还隐含了 threads 这个变量，为了考察纯粹的 IO 与 Batch 关系，
# 我们对 threads 维度取均值（或者你也可以用 max() 代表该配置下的极限性能）
df_agg = df_filtered.groupby(['batch_size', 'chunk_records'])['tasks_per_second'].mean().reset_index()

# 4. 构建二维矩阵
pivot_df = df_agg.pivot(index='batch_size', columns='chunk_records', values='tasks_per_second')

# 翻转 Y 轴，让最大的 batch_size (16384) 在最上面，符合物理直觉（大在上面）
pivot_df = pivot_df.sort_index(ascending=False)

# 5. 开始绘图
plt.figure(figsize=(8, 6), dpi=150)

# 使用深邃的蓝绿色系 (YlGnBu)，体现底层优化的科技感
ax = sns.heatmap(
    pivot_df,
    annot=True,
    fmt=".1f",           # 调优阶段，保留一位小数看细节差异
    cmap="YlGnBu",
    cbar_kws={'label': 'Throughput (Tasks / Second)'},
    linewidths=1,        # 稍微加粗分割线
    linecolor='white'
)

# 6. 图表修饰
plt.title('IO Buffer Tuning: Batch Size vs. Chunk Records\n(Fixed: Multi-File, MPI=2, Cuts=4)', fontsize=14, pad=15)
plt.xlabel('Chunk Records (IO Flush Granularity)', fontsize=12)
plt.ylabel('Batch Size (Compute Granularity)', fontsize=12)

# 让坐标轴刻度的字体稍微大一点
plt.xticks(fontsize=11)
plt.yticks(fontsize=11, rotation=0)

# 7. 保存并显示
plt.tight_layout()
plt.savefig('io_tuning_heatmap.png')
plt.show()