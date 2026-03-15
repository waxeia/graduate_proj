#include "engine.hpp"
#include "config.hpp"
#include "circuit.hpp"

namespace qcs {
namespace {//匿名命名空间（文件私有工具函数）
void configure_openmp(const AppConfig& cfg) {//配置 OpenMP 并行参数
#ifdef _OPENMP//仅当编译时开启OpenMP才生效
    if (cfg.omp_threads > 0) {
        omp_set_num_threads(cfg.omp_threads);//设置OpenMP线程数
    }
    //解析调度策略（static/dynamic/guided/auto）
    omp_sched_t sched = omp_sched_static;
    if (cfg.omp_schedule == "dynamic") sched = omp_sched_dynamic;
    else if (cfg.omp_schedule == "guided") sched = omp_sched_guided;
    else if (cfg.omp_schedule == "auto") sched = omp_sched_auto;
    omp_set_schedule(sched, cfg.omp_chunk > 0 ? cfg.omp_chunk : 0);//设置分块大小
#endif
}
//获取实际使用的 OpenMP 线程数
int effective_omp_threads() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}
}//namespace
//构造函数：初始化 MPI 进程信息
ReconstructionEngine::ReconstructionEngine(MPI_Comm comm) : comm_(comm) {
    MPI_Comm_rank(comm_, &rank_);//获取当前进程rank
    MPI_Comm_size(comm_, &world_size_);//获取总进程数
}
//核心方法：量子仿真的完整执行流程
RunMetrics ReconstructionEngine::run(const AppConfig& cfg_in) {
    AppConfig cfg = cfg_in;
    configure_openmp(cfg);//配置OpenMP
    //初始化核心组件
    const auto circuit = make_circuit(cfg);//创建量子电路对象
    const auto range = split_tasks(circuit.total_tasks, rank_, world_size_);
    //拆分任务（每个进程负责一部分）
    auto storage = make_storage_backend(cfg.storage_mode);
    //创建存储后端（Memory/SingleHdf5/MultiFile）
    storage->open(circuit, cfg, rank_, world_size_, comm_);//初始化存储
    //open:storage 指向的对象的一个成员函数
    RunMetrics metrics;
    metrics.experiment_name = cfg.experiment_name;
    metrics.backend = storage->name();
    metrics.mpi_ranks = world_size_;
    metrics.omp_threads = effective_omp_threads();
    metrics.num_qubits = cfg.num_qubits;
    metrics.circuit_depth = cfg.circuit_depth;
    metrics.num_cuts = cfg.num_cuts;
    metrics.total_tasks = circuit.total_tasks;
    metrics.batch_size = cfg.batch_size;
    metrics.chunk_records = cfg.chunk_records;
    //同步所有进程，开始计时
    MPI_Barrier(comm_);
    const auto total_t0 = MPI_Wtime();
    double compute_seconds = 0.0;
    //循环执行任务（支持重复次数）
    for (int repeat = 0; repeat < std::max(1, cfg.repeats); ++repeat) {
        //按批次处理当前进程的任务范围
        for (std::uint64_t offset = range.begin; offset < range.end; offset += cfg.batch_size) {
            const std::uint64_t batch_end = std::min<std::uint64_t>(range.end, offset + cfg.batch_size);
            //避免最后一批越界
            //uint64_t 能避免溢出
            //算出当前批次实际要处理的任务数量batch_n
            const std::size_t batch_n = static_cast<std::size_t>(batch_end - offset);
            std::vector<ResultRecord> batch(batch_n);//创建存储当前批次结果的容器
            //ResultRecord:单个任务的仿真结果结构体
            //并行执行量子任务（核心计算）
            const auto compute_t0 = MPI_Wtime();//记录计算开始时间
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime)//OpenMP 并行指令：将后续的 for 循环拆分成多个线程并行执行
#endif
            //内层循环：每个线程处理一个任务ID，调用evaluate_task计算结果，并存储在batch中
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(batch_n); ++i) {
                const auto task_id = offset + static_cast<std::uint64_t>(i);
                batch[static_cast<std::size_t>(i)] = evaluate_task(circuit, task_id, rank_, repeat);
            }
            const auto compute_t1 = MPI_Wtime();
            compute_seconds += (compute_t1 - compute_t0);
            storage->write_batch(batch);//把当前批次的所有结果写入存储
        }
    }
    //结束存储后端的写入，获取当前进程的存储统计（局部指标-->后面有）
    const auto storage_stats = storage->finalize();
    //记录实验总结束时间（MPI高精度计时），MPI_Wtime()：MPI 提供的高精度计时函数（精度达微秒级），比普通系统计时更适合并行环境
    const auto total_t1 = MPI_Wtime();
    //把局部存储指标 + 计算耗时填充到metrics（当前进程的指标容器）
    metrics.compute_seconds = compute_seconds;//当前进程的纯计算耗时
    metrics.write_seconds = storage_stats.write_seconds;
    metrics.read_seconds = storage_stats.read_seconds;
    metrics.total_seconds = total_t1 - total_t0;
    metrics.partial_sum = storage_stats.readback_sum;//当前进程的结果校验和
    metrics.records_written = storage_stats.records_written;//当前进程写入的记录数
    metrics.bytes_written = storage_stats.bytes_written;
    metrics.files_created = storage_stats.files_created;
    metrics.max_buffer_mb = storage_stats.peak_buffer_mb;//当前进程的内存缓冲区峰值
    //归约1：汇总所有进程的校验和（partial_sum → global_sum，求和）
    MPI_Reduce(&metrics.partial_sum, &metrics.global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, comm_);
    /*MPI_Reduce 参数详解
    MPI_Reduce(
        &metrics.partial_sum,  // 输入：当前进程的局部数据（待汇总）
        &metrics.global_sum,   // 输出：主进程的全局汇总结果（其他进程此变量无意义）
        1,                     // 数据个数（1个double）
        MPI_DOUBLE,            // 数据类型（双精度浮点数）
        MPI_SUM,               // 归约规则（求和）
        0,                     // 目标进程（rank=0的主进程）
        comm_                  // MPI通信域（所有进程在同一个域内通信）
    );
     */
    //归约2：汇总所有进程的总任务数（local_tasks → global_tasks，求和）
    double local_tasks = static_cast<double>(metrics.records_written);
    double global_tasks = 0.0;
    MPI_Reduce(&local_tasks, &global_tasks, 1, MPI_DOUBLE, MPI_SUM, 0, comm_);
    if (rank_ == 0) {
        metrics.tasks_per_second = global_tasks / std::max(metrics.total_seconds, 1e-9);
        //std::max(..., 1e-9) 避免总耗时为 0 导致除 0 错误
    }
    //MPI 归约 - 汇总 “取最大值类” 全局指标
    std::uint64_t total_bytes = 0;//所有进程总写入字节数
    std::uint64_t total_files = 0;//所有进程总创建文件数
    double max_write = 0.0;//所有进程中最长的写耗时
    double max_read = 0.0;//所有进程中最长的读耗时
    double max_compute = 0.0;//所有进程中最长的计算耗时
    double max_total = 0.0;//所有进程中最长的总耗时
    double max_buffer = 0.0;//所有进程中最大的内存缓冲区峰值
    std::uint64_t local_bytes = metrics.bytes_written;//赋值当前进程的局部指标
    std::uint64_t local_files = metrics.files_created;
    //归约3：汇总总字节数（求和）
    MPI_Reduce(&local_bytes, &total_bytes, 1, MPI_UINT64_T, MPI_SUM, 0, comm_);
    //归约4：汇总总文件数（求和）
    MPI_Reduce(&local_files, &total_files, 1, MPI_UINT64_T, MPI_SUM, 0, comm_);
    //归约5：取所有进程中最长的写耗时（MPI_MAX）
    MPI_Reduce(&metrics.write_seconds, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    //归约6：取所有进程中最长的读耗时
    MPI_Reduce(&metrics.read_seconds, &max_read, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    //归约7：取所有进程中最长的计算耗时
    MPI_Reduce(&metrics.compute_seconds, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    //归约8：取所有进程中最长的总耗时
    MPI_Reduce(&metrics.total_seconds, &max_total, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    //归约9：取所有进程中最大的内存缓冲区峰值
    MPI_Reduce(&metrics.max_buffer_mb, &max_buffer, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    //仅主进程（rank=0）把归约后的全局指标赋值到metrics
    if (rank_ == 0) {
        metrics.bytes_written = total_bytes;
        metrics.files_created = total_files;
        metrics.write_seconds = max_write;
        metrics.read_seconds = max_read;
        metrics.compute_seconds = max_compute;
        metrics.total_seconds = max_total;
        metrics.max_buffer_mb = max_buffer;
    }
    return metrics;
}

std::string metrics_csv_header() {
    return "experiment_name,backend,mpi_ranks,omp_threads,num_qubits,circuit_depth,num_cuts,total_tasks,batch_size,chunk_records,compute_seconds,write_seconds,read_seconds,total_seconds,global_sum,records_written,bytes_written,files_created,tasks_per_second,max_buffer_mb";
}

std::string metrics_to_csv(const RunMetrics& m) {
    std::ostringstream oss;
    oss << m.experiment_name << ','
        << m.backend << ','
        << m.mpi_ranks << ','
        << m.omp_threads << ','
        << m.num_qubits << ','
        << m.circuit_depth << ','
        << m.num_cuts << ','
        << m.total_tasks << ','
        << m.batch_size << ','
        << m.chunk_records << ','
        << std::fixed << std::setprecision(6)
        << m.compute_seconds << ','
        << m.write_seconds << ','
        << m.read_seconds << ','
        << m.total_seconds << ','
        << m.global_sum << ','
        << m.records_written << ','
        << m.bytes_written << ','
        << m.files_created << ','
        << m.tasks_per_second << ','
        << m.max_buffer_mb;
    return oss.str();
}
} // namespace qcs