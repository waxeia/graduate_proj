import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据 (确保路径正确)
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

# 2. 数据预处理
plot_df = df.groupby(['backend', 'num_cuts'])['total_seconds'].mean().reset_index()

# 3. 开始绘图
plt.figure(figsize=(10, 6), dpi=150) # 提高了 dpi 让论文图更清晰

# 【关键修改】：加入 style, markers 和 alpha 处理重合
sns.lineplot(
    data=plot_df,
    x='num_cuts',
    y='total_seconds',
    hue='backend',
    style='backend',           # 根据不同后端使用不同线型
    markers=['o', 's', '^'],   # 分别使用 圆圈、方块、正三角 标记
    dashes=[(1, 0), (4, 2), (1, 2)], # 分别使用 实线、长虚线、短点线
    markersize=10,             # 放大标记点
    linewidth=2.5,
    alpha=0.75                 # 增加 25% 的透明度，让底部的蓝线透出来
)

# 4. 设置对数坐标轴
plt.yscale('log')

# 5. 图表修饰
plt.title('Overall Performance: Total Execution Time vs. Number of Cuts', fontsize=14, pad=15)
plt.xlabel('Number of Cuts (n)', fontsize=12)
plt.ylabel('Total Execution Time (s) [Log Scale]', fontsize=12)
plt.xticks([4, 6, 8])
plt.grid(True, which="both", ls="--", alpha=0.4)

# 优化图例
plt.legend(title='Storage Backend', title_fontsize='12', loc='upper left', framealpha=0.9)

# 6. 保存并显示
plt.tight_layout()
plt.savefig('total_time_vs_cuts_optimized.png')
plt.show()