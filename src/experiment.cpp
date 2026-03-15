#include "experiment.hpp"

namespace qcs {
//构造函数：初始化 MPI 进程信息
ExperimentRunner::ExperimentRunner(MPI_Comm comm) : comm_(comm) {//直接初始化 comm_ 成员
    //comm_	是类的私有成员变量；comm_(comm)	把参数 comm 的值，赋值给成员变量 comm_
    MPI_Comm_rank(comm_, &rank_);//获取当前进程的 rank
    MPI_Comm_size(comm_, &world_size_);//获取总进程数
}

int ExperimentRunner::execute(AppConfig cfg) {
    //步骤 1：自动设置 OpenMP 线程数（未配置时）
    if (cfg.omp_threads <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();//CPU 的逻辑核心数
        cfg.omp_threads = std::max(1u, hw == 0 ? 1u : hw / 2u);//保证至少 1 个线程（避免 0 线程导致程序崩溃）
    }

    ReconstructionEngine engine(comm_);//创建量子仿真引擎（核心计算类）
    RunMetrics final_metrics;//存储实验指标（耗时、吞吐量、任务数等）

    for (int i = 0; i < std::max(1, cfg.repeats); ++i) {//repeats:减少随机误差，提升实验结果的可信度
        AppConfig once = cfg;
        once.repeats = 1;
        once.seed = cfg.seed + i;//随机数种子偏移（保证每次实验可复现且不同）
        once.experiment_name = cfg.experiment_name + "_rep" + std::to_string(i);
        once.output_dir = (std::filesystem::path(cfg.output_dir) / ("rep_" + std::to_string(i))).string();
        final_metrics = engine.run(once);//调用引擎执行单次实验，返回指标

        if (rank_ == 0 && cfg.emit_csv) {
            const auto csv_path = std::filesystem::path(cfg.output_dir) / "summary.csv";
            const bool exists = std::filesystem::exists(csv_path);
            std::filesystem::create_directories(csv_path.parent_path());
            std::ofstream out(csv_path, std::ios::app);//以追加模式打开（避免覆盖）
            if (!exists) out << metrics_csv_header() << '\n';
            out << metrics_to_csv(final_metrics) << '\n';//写入单次实验的指标
        }
        //主进程打印实验日志
        if (rank_ == 0) {
            std::cout << "[run] backend=" << final_metrics.backend
                      << " mpi=" << final_metrics.mpi_ranks//MPI 进程数
                      << " omp=" << final_metrics.omp_threads
                      << " tasks=" << final_metrics.total_tasks
                      << " total_s=" << std::fixed << std::setprecision(4) << final_metrics.total_seconds//总耗时
                      << " write_s=" << final_metrics.write_seconds
                      << " read_s=" << final_metrics.read_seconds
                      << " throughput=" << final_metrics.tasks_per_second << " task/s"//吞吐量（任务/秒）
                      << std::endl;
            if (cfg.emit_csv) {
                std::cout << metrics_to_csv(final_metrics) << std::endl;//打印 CSV 文件里的指标
            }
        }
    }
    return 0;
}
} // namespace qcs