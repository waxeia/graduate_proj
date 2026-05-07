import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 数据处理：过滤 num_cuts=8 的极端情况
df_8 = df[df['num_cuts'] == 8]
breakdown_df = df_8.groupby('backend')[['compute_seconds', 'write_seconds', 'read_seconds']].mean().reset_index()

# 【修改点 1】：调整顺序为 Memory -> Single HDF5 -> Multi-File
breakdown_df['backend'] = pd.Categorical(
    breakdown_df['backend'],
    categories=['memory', 'single_hdf5', 'multi_file'],
    ordered=True
)
breakdown_df = breakdown_df.sort_values('backend')

backends = breakdown_df['backend'].tolist()
compute = breakdown_df['compute_seconds'].values
write = breakdown_df['write_seconds'].values
read = breakdown_df['read_seconds'].values

# 3. 开始绘图
fig, ax = plt.subplots(figsize=(9, 7), dpi=150)
bar_width = 0.5
x = np.arange(len(backends))

# 4. 绘制堆叠柱状图
p1 = ax.bar(x, compute, bar_width, label='Compute Time',
            color='#4C72B0', edgecolor='black', linewidth=1.2)
p2 = ax.bar(x, write, bar_width, bottom=compute, label='Write Time',
            color='#DD8452', edgecolor='black', hatch='//', linewidth=1.2)
p3 = ax.bar(x, read, bar_width, bottom=compute + write, label='Read Time',
            color='#55A868', edgecolor='black', hatch='..', linewidth=1.2)

# 5. 图表修饰
# 增加 pad 以防止标题和顶部的图例重叠
plt.title('Execution Time Breakdown at Extreme Load (Cuts = 8)', fontsize=15, pad=40)
plt.xlabel('Storage Backend Strategy', fontsize=13)
plt.ylabel('Execution Time (Seconds)', fontsize=13)

# 【同步修改点】：更新横轴标签以匹配新的顺序
plt.xticks(x, ['Memory\n(Baseline)', 'Single HDF5\n(Centralized)', 'Multi-File\n(Distributed)'], fontsize=11)

ax.yaxis.grid(True, linestyle='--', alpha=0.6)
ax.set_axisbelow(True)

# 6. 数据标注
for i in range(len(backends)):
    total_io = write[i] + read[i]
    total_time = compute[i] + total_io
    annotation_text = f"IO (W+R):\n{total_io:.3f} s"

    ax.annotate(
        annotation_text,
        xy=(x[i], total_time),
        xytext=(0, 25),
        textcoords="offset points",
        ha='center', va='bottom',
        fontsize=10,
        fontweight='bold',
        color='#A63603',
        arrowprops=dict(arrowstyle="->", color='#A63603', lw=1.5, shrinkA=0, shrinkB=0)
    )

# 【修改点 2】：图例放置在绘图区外部正上方，使用 3 列横向排列
plt.legend(
    loc='lower center',
    bbox_to_anchor=(0.5, 1.02),
    ncol=3,
    fontsize=11,
    framealpha=0.9,
    edgecolor='black'
)

# 留出足够的顶部空间，防止引线文本顶破天花板
plt.ylim(0, max(compute + write + read) * 1.18)

# 7. 保存并显示
plt.tight_layout()
plt.savefig('io_breakdown_academic_fixed.png')
plt.show()