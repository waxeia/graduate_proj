#!/bin/bash

# 路径设置 (请在项目根目录运行)
EXE="./build/qcsim"
CONFIG="./configs/baseline.cfg"
OUT_BASE="results/batch_size_sweep"

# 实验参数设置
TASKS=10000000
QUBITS=15
MPI_RANKS=8
OMP_THREADS=1

# 定义 batch_size 的 2 幂次方序列 (共 11 个点)
BATCH_SIZES=(64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576)

mkdir -p $OUT_BASE

# 初始化汇总表
FINAL_SUMMARY="$OUT_BASE/test131072.csv"
echo "Mode,BatchSize,Total_Time(s),Write_Time(s),Throughput(task/s),MaxBuffer(MB)" > $FINAL_SUMMARY

echo "=================================================="
echo "开始存储策略参数扫略测试 (Batch Size 梯度)"
echo "并行规模: $MPI_RANKS 进程 | 任务: $TASKS tasks"
echo "=================================================="

# 重点测试 HDF5 和 Multi-file 模式
for MODE in "hdf5" "multifile"
do
    echo ">>> 正在测试模式: $MODE ..."

    for BS in "${BATCH_SIZES[@]}"
    do
        SUB_DIR="$OUT_BASE/${MODE}_bs${BS}"
        mkdir -p $SUB_DIR

        echo "  正在执行: batch_size = $BS ..."

        # 使用 --set 动态覆盖配置中的 batch_size
        mpirun -n $MPI_RANKS $EXE --config $CONFIG \
             --set storage_mode=$MODE \
             --set total_tasks=$TASKS \
             --set batch_size=$BS \
             --set num_qubits=$QUBITS \
             --set omp_threads=$OMP_THREADS \
             --set output_dir="$SUB_DIR" \
             --set verbose=false > /dev/null 2>&1

        # 提取汇总数据 (从生成的 summary.csv 提取)
        if [ -f "$SUB_DIR/summary.csv" ]; then
            # 提取最后一行数据
            LINE=$(tail -n 1 "$SUB_DIR/summary.csv")

            # 根据索引提取字段: 14=Total, 11=Write, 19=TPS, 20=MaxBuffer
            TOTAL_S=$(echo $LINE | cut -d',' -f14)
            WRITE_S=$(echo $LINE | cut -d',' -f11)
            TPS=$(echo $LINE | cut -d',' -f19)
            MEM_MB=$(echo $LINE | cut -d',' -f20)

            echo "$MODE,$BS,$TOTAL_S,$WRITE_S,$TPS,$MEM_MB" >> $FINAL_SUMMARY
        else
            echo "  错误: $MODE/bs$BS 实验失败"
        fi
    done
    echo "--------------------------------------------------"
done

echo "梯度实验完成！汇总表已生成: $FINAL_SUMMARY"
column -t -s, $FINAL_SUMMARY