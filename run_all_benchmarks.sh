#!/bin/bash

# 基础设置
EXE="./build/qcsim"
CONFIG="./configs/baseline.cfg"
OUT_DIR="results/benchmarks"
# 这里的任务数可以根据你电脑性能调整，50000条数据足以看出HDF5和MultiFile的区别
TASKS=50000
QUBITS=15

# 创建输出目录
mkdir -p $OUT_DIR

echo "=================================================="
echo "开始量子仿真存储策略自动化测试"
echo "任务规模: $TASKS tasks, $QUBITS qubits"
echo "=================================================="

# 定义要测试的模式
MODES=("memory" "hdf5" "multifile")

# 准备汇总表格文件
SUMMARY_FILE="$OUT_DIR/final_comparison.csv"
echo "Mode,Total_Time(s),Write_Time(s),Throughput(task/s)" > $SUMMARY_FILE

for MODE in "${MODES[@]}"
do
    echo "正在测试模式: $MODE ..."

    # 执行程序，使用 --set 动态覆盖配置
    # 我们强制设 omp_threads=1 确保 Qulacs 稳定
    $EXE --config $CONFIG \
         --set storage_mode=$MODE \
         --set total_tasks=$TASKS \
         --set num_qubits=$QUBITS \
         --set omp_threads=1 \
         --set output_dir="$OUT_DIR/$MODE" \
         --set verbose=false > "$OUT_DIR/${MODE}_log.txt"

    # 从生成的 summary.csv 中提取数据 (假设是最后一行)
    # 注意：这里根据你具体的 csv 格式提取 Total_s, Write_s, Throughput
    RESULT_LINE=$(tail -n 1 "$OUT_DIR/$MODE/summary.csv")

    # 提取关键字段（根据你的 CSV 列索引：14是Total_s, 11是Write_s, 19是Throughput）
    TOTAL_S=$(echo $RESULT_LINE | cut -d',' -f14)
    WRITE_S=$(echo $RESULT_LINE | cut -d',' -f11)
    TPS=$(echo $RESULT_LINE | cut -d',' -f19)

    # 写入汇总表
    echo "$MODE,$TOTAL_S,$WRITE_S,$TPS" >> $SUMMARY_FILE

    echo "完成 $MODE: 耗时 ${TOTAL_S}s, 吞吐量 ${TPS} task/s"
    echo "--------------------------------------------------"
done

echo "所有测试已完成！"
echo "汇总表格已生成: $SUMMARY_FILE"
column -t -s, $SUMMARY_FILE # 在终端打印漂亮表格