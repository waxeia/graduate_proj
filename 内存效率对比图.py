import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 读取数据
file_path = 'results/final_summary.csv'
df = pd.read_csv(file_path)

mem_df = df.groupby(['num_cuts', 'backend'])['max_buffer_mb'].mean().reset_index()

# 固定后端显示顺序：Memory -> Single HDF5 -> Multi-File
mem_df['backend'] = pd.Categorical(
    mem_df['backend'],
    categories=['memory', 'single_hdf5', 'multi_file'],
    ordered=True
)
mem_df = mem_df.sort_values(['num_cuts', 'backend'])

# 3. 开始绘图
plt.figure(figsize=(10, 6), dpi=150)

ax = sns.barplot(
    data=mem_df,
    x='num_cuts',
    y='max_buffer_mb',
    hue='backend',
    palette=['#4C72B0', '#DD8452', '#55A868'],
    edgecolor='black',
    linewidth=1.2
)

# 4. 图表修饰
plt.title('Memory Efficiency: Peak Memory Usage vs. Circuit Cuts', fontsize=14, pad=20)
plt.xlabel('Number of Cuts (Problem Size)', fontsize=12)
plt.ylabel('Peak Memory Usage (MB)', fontsize=12)

plt.grid(axis='y', linestyle='--', alpha=0.6)
ax.set_axisbelow(True)

plt.legend(title='Storage Backend', fontsize=11, title_fontsize=12, loc='upper left')

# 5. 【关键修复】：无论高度是多少，都强制画出数值标签
for p in ax.patches:
    height = p.get_height()
    # 去掉了 height > 0 的限制，确保 0.0 也能显示出来
    if pd.notnull(height):
        # 为了美观，如果是 0，就当成 0.0 处理
        val = max(height, 0)
        ax.annotate(
            f'{val:.1f}',
            (p.get_x() + p.get_width() / 2., val),
            ha='center',
            va='bottom',
            xytext=(0, 5), # 向上偏移 5 像素
            textcoords='offset points',
            fontsize=10,
            fontweight='bold',
            color='#333333'
        )

# 动态调整顶部空间
plt.ylim(0, mem_df['max_buffer_mb'].max() * 1.25)

# 6. 保存并显示
plt.tight_layout()
plt.savefig('memory_efficiency_fixed.png')
plt.show()