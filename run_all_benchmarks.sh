#!/bin/bash

# 路径设置 (请在项目根目录运行)
EXE="./build/qcsim"
CONFIG="./configs/baseline.cfg"
OUT_BASE="results/mpi_benchmarks"

# 实验参数设置 (大幅增加压力)
TASKS=500000    # 50万任务，产生约 32MB 数据
QUBITS=15
MPI_RANKS=8     # 根据你的 i9 处理器建议设为 8
OMP_THREADS=1   # 必须为 1，防止与 MPI 冲突

mkdir -p $OUT_BASE

echo "=================================================="
echo "开始 MPI 并行量子仿真存储压力测试"
echo "硬件: i9-12900H | 并行规模: $MPI_RANKS 进程"
echo "任务: $TASKS tasks | 规模: $QUBITS qubits"
echo "=================================================="

MODES=("memory" "hdf5" "multifile")
SUMMARY="$OUT_BASE/mpi_comparison.csv"
echo "Mode,Total_Time(s),Write_Time(s),Throughput(task/s)" > $SUMMARY

for MODE in "${MODES[@]}"
do
    echo ">>> 正在测试模式: $MODE ..."

    # 使用 mpirun 启动并行任务
    mpirun -n $MPI_RANKS $EXE --config $CONFIG \
         --set storage_mode=$MODE \
         --set total_tasks=$TASKS \
         --set num_qubits=$QUBITS \
         --set omp_threads=$OMP_THREADS \
         --set output_dir="$OUT_BASE/$MODE" \
         --set verbose=false > "$OUT_BASE/${MODE}_mpi_log.txt"

    # 提取主进程产生的汇总数据
    if [ -f "$OUT_BASE/$MODE/summary.csv" ]; then
        RESULT_LINE=$(tail -n 1 "$OUT_BASE/$MODE/summary.csv")
        TOTAL_S=$(echo $RESULT_LINE | cut -d',' -f14)
        WRITE_S=$(echo $RESULT_LINE | cut -d',' -f11)
        TPS=$(echo $RESULT_LINE | cut -d',' -f19)

        echo "$MODE,$TOTAL_S,$WRITE_S,$TPS" >> $SUMMARY
        echo "结果: 耗时 ${TOTAL_S}s | 写入 ${WRITE_S}s | 吞吐量 ${TPS} task/s"
    else
        echo "错误: $MODE 模式未生成结果文件"
    fi
    echo "--------------------------------------------------"
done

echo "MPI 压力测试完成！"
echo "汇总表路径: $SUMMARY"
column -t -s, $SUMMARY