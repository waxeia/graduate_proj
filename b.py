import pandas as pd
import matplotlib.pyplot as plt
import os

def plot_batch_impact(csv_path, output_path):
    # 1. 加载数据
    if not os.path.exists(csv_path):
        print(f"错误: 找不到数据文件 {csv_path}")
        return

    df = pd.read_csv(csv_path)

    # 2. 过滤出 HDF5 模式进行分析 (你也可以修改为 multifile)
    target_mode = 'hdf5'
    plot_df = df[df['Mode'] == target_mode].sort_values('BatchSize')

    # 3. 创建画布
    fig, ax1 = plt.subplots(figsize=(10, 6), dpi=150)

    # --- 绘制吞吐量曲线 (左轴) ---
    color_tps = 'tab:blue'
    ax1.set_xscale('log', base=2)  # X轴设为 Log2
    ax1.set_xlabel('Batch Size (Log2 Scale)', fontsize=12)
    ax1.set_ylabel('Throughput (Tasks/s)', color=color_tps, fontsize=12)

    line1 = ax1.plot(plot_df['BatchSize'], plot_df['Throughput(task/s)'],
                     marker='o', linestyle='-', linewidth=2, color=color_tps, label='Throughput')
    ax1.tick_params(axis='y', labelcolor=color_tps)
    ax1.grid(True, which="both", ls="-", alpha=0.3)

    # --- 绘制内存占用曲线 (右轴) ---
    ax2 = ax1.twinx()  # 共享 X 轴
    color_mem = 'tab:red'
    ax2.set_ylabel('Max Buffer Per Rank (MB)', color=color_mem, fontsize=12)

    line2 = ax2.plot(plot_df['BatchSize'], plot_df['MaxBuffer(MB)'],
                     marker='s', linestyle='--', linewidth=2, color=color_mem, label='Memory Usage')
    ax2.tick_params(axis='y', labelcolor=color_mem)

    # 4. 图例与标题
    lines = line1 + line2
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc='upper left')

    plt.title(f'Impact of Batch Size on Performance & Memory ({target_mode.upper()})', fontsize=14)

    # 5. 保存图片
    plt.tight_layout()
    plt.savefig(output_path)
    print(f"成功生成图表: {output_path}")
    plt.show()

# 运行绘图
if __name__ == "__main__":
    # 根据你提供的 GitHub 目录结构
    csv_file = 'results/batch_size_sweep/batch_size_sweep_results.csv'
    output_img = 'results/mpi_benchmarks/batch_impact.png'

    # 确保输出目录存在
    os.makedirs(os.path.dirname(output_img), exist_ok=True)

    plot_batch_impact(csv_file, output_img)