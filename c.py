import pandas as pd
import matplotlib.pyplot as plt
import os

def plot_refined_impact(csv_path, save_path):
    """
    绘制 Batch Size 对吞吐量与内存占用的综合影响图
    """
    # 1. 检查数据文件
    if not os.path.exists(csv_path):
        print(f"错误: 找不到数据文件 '{csv_path}'")
        return

    # 2. 读取数据
    df = pd.read_csv(csv_path)

    # 3. 创建画布与双轴
    fig, ax1 = plt.subplots(figsize=(11, 7), dpi=150)
    ax2 = ax1.twinx()

    # 4. 绘制吞吐量 (对应左轴 - 实线)
    # HDF5 模式
    h_data = df[df['Mode'] == 'hdf5'].sort_values('BatchSize')
    h_line, = ax1.plot(h_data['BatchSize'], h_data['Throughput(task/s)'],
                       'ro-', label='HDF5 Throughput', linewidth=2, markersize=7)

    # Multifile 模式
    m_data = df[df['Mode'] == 'multifile'].sort_values('BatchSize')
    m_line, = ax1.plot(m_data['BatchSize'], m_data['Throughput(task/s)'],
                       'gs-', label='Multifile Throughput', linewidth=2, markersize=7)

    # 5. 绘制内存占用 (对应右轴 - 灰色虚线)
    # 内存计算逻辑在两种模式下一致，只需画一条参考线
    mem_line, = ax2.plot(h_data['BatchSize'], h_data['MaxBuffer(MB)'],
                         color='gray', linestyle='--', label='Memory Per Rank (MB)', linewidth=2)

    # 6. 图例合并修复 (关键步骤)
    # 将两个不同坐标轴的图例提取出来并合并显示在同一个框内
    lines = [h_line, m_line, mem_line]
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc='upper left', frameon=True, shadow=True, fontsize=10)

    # 7. 坐标轴与样式配置
    ax1.set_xscale('log', base=2) # X轴使用 Log2 对数
    ax1.set_xlabel('Batch Size (Log2 Scale)', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Throughput (Tasks/s)', fontsize=12, fontweight='bold')

    ax2.set_ylabel('Memory Per Rank (MB)', color='gray', fontsize=12)
    ax2.tick_params(axis='y', labelcolor='gray')

    plt.title('Performance & Resource Bottleneck Analysis\n(Quantum Circuit Reconstruction)', fontsize=14, pad=15)
    ax1.grid(True, which="both", ls="-", alpha=0.2)

    # 8. 保存并显示
    plt.tight_layout()
    plt.savefig(save_path)
    print(f"成功！图表已保存至: {os.path.abspath(save_path)}")
    plt.show()

# ==========================================
# 脚本执行入口
# ==========================================
if __name__ == "__main__":
    # 配置相对于项目根目录的路径
    DATA_PATH = 'results/batch_size_sweep/batch_size_sweep_results.csv'
    OUTPUT_PATH = 'results/batch_impact_final.png'

    # 自动创建输出文件夹
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)

    # 执行绘图
    plot_refined_impact(DATA_PATH, OUTPUT_PATH)