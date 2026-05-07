import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 确保类型正确
df['mpi_ranks'] = df['mpi_ranks'].astype(int)

# 2. 数据处理：计算基准时间 (T1) 和 加速比 (Speedup)
baseline_df = df[df['mpi_ranks'] == 1].groupby(['backend', 'num_cuts'])['total_seconds'].mean().reset_index()
baseline_df.rename(columns={'total_seconds': 'base_time'}, inplace=True)

df_mean = df.groupby(['backend', 'num_cuts', 'mpi_ranks'])['total_seconds'].mean().reset_index()
df_scaling = pd.merge(df_mean, baseline_df, on=['backend', 'num_cuts'])
df_scaling['speedup'] = df_scaling['base_time'] / df_scaling['total_seconds']

# 3. 开始绘图
plt.figure(figsize=(10, 6), dpi=150)

# 绘制带有置信区间阴影的加速比曲线
ax = sns.lineplot(
    data=df_scaling,
    x='mpi_ranks',
    y='speedup',
    hue='backend',
    style='backend',
    markers=['o', 's', '^'],
    dashes=[(1, 0), (4, 2), (1, 2)],
    markersize=10,
    linewidth=2.5,
    alpha=0.85
    # 注意：这里我们移除了 errorbar=None，Seaborn 将默认绘制 95% 置信区间的阴影
)

# 4. 绘制“理想加速比”辅助线
ideal_x = [1, 2, 4, 8]
ideal_y = [1, 2, 4, 8]
plt.plot(ideal_x, ideal_y, 'k--', linewidth=2, label='Ideal Speedup')

# 5. 图表修饰
plt.title('Strong Scaling: MPI Ranks vs. Speedup', fontsize=14, pad=15)
plt.xlabel('Number of MPI Ranks (Processors)', fontsize=12)
plt.ylabel('Speedup Ratio ($S = T_1 / T_n$)', fontsize=12)
plt.xticks(ideal_x)
plt.ylim(0.5, 9)
plt.grid(True, which="major", ls="--", alpha=0.6)

# 6. 【核心修复】：安全地提取并清洗图例
handles, labels = ax.get_legend_handles_labels()

# Seaborn 默认会在图例里塞一个叫 'backend' 的占位标题，我们把它安全地剔除掉
if 'backend' in labels:
    idx = labels.index('backend')
    handles.pop(idx)
    labels.pop(idx)

# 重新生成干净、完整的图例
plt.legend(
    handles=handles,
    labels=labels,
    loc='upper left',
    fontsize=11,
    framealpha=0.9,
    title='Storage Backend'
)

# 7. 保存并显示
plt.tight_layout()
plt.savefig('mpi_scaling_with_shadow.png')
plt.show()