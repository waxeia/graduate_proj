import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 数据处理：计算平均吞吐量
# 按后端、MPI进程数、切分数量分组，求 tasks_per_second 的均值
df_agg = df.groupby(['backend', 'mpi_ranks', 'num_cuts'])['tasks_per_second'].mean().reset_index()

# 3. 准备绘图环境 (1行3列的子图，共享Y轴)
fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), sharey=True, dpi=150)
backends = ['memory', 'single_hdf5', 'multi_file']
titles = ['Memory (Baseline)', 'Single HDF5 (Centralized)', 'Multi-File (Distributed)']

# 【关键学术操守】：找出全局的吞吐量最大和最小值，统一所有子图的颜色标尺
vmin = df_agg['tasks_per_second'].min()
vmax = df_agg['tasks_per_second'].max()

# 4. 循环绘制三个热力图
for i, backend in enumerate(backends):
    # 提取当前后端的数据并转换为热力图所需的矩阵 (Pivot Table)
    pivot_df = df_agg[df_agg['backend'] == backend].pivot(index='mpi_ranks', columns='num_cuts', values='tasks_per_second')

    # 翻转 Y 轴的顺序，让最大的 mpi_ranks (8) 物理位置在最上方，符合人类直觉
    pivot_df = pivot_df.sort_index(ascending=False)

    # 绘制当前子图的热力图
    sns.heatmap(
        pivot_df,
        ax=axes[i],
        annot=True,          # 在方格内显示具体数值
        fmt=".0f",           # 数值保留整数 (吞吐量通常看整数即可)
        cmap="YlOrRd",       # 使用 黄-橙-红 渐变色 (越红代表吞吐量越高)
        vmin=vmin,           # 统一最低颜色阈值
        vmax=vmax,           # 统一最高颜色阈值
        cbar=(i == 2),       # 只在最后一个子图画出颜色条（Colorbar）
        cbar_kws={'label': 'Throughput (Tasks / Second)'} if i == 2 else None,
        linewidths=0.5,      # 增加方格之间的白线分隔，显得更精美
        linecolor='white'
    )

    # 子图修饰
    axes[i].set_title(titles[i], fontsize=13, pad=12)
    axes[i].set_xlabel('Number of Cuts (n)', fontsize=12)

    # 只有第一个图需要显示 Y 轴标签
    if i == 0:
        axes[i].set_ylabel('MPI Ranks (Processors)', fontsize=12)
    else:
        axes[i].set_ylabel('')
        axes[i].tick_params(axis='y', left=False) # 隐藏右侧子图的刻度线

# 5. 总标题与排版
plt.suptitle('System Throughput Parameter Space Exploration', fontsize=16, fontweight='bold', y=1.02)
plt.tight_layout()

# 6. 保存并显示
plt.savefig('throughput_heatmap.png', bbox_inches='tight')
plt.show()