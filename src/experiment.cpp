#include "experiment.hpp"

namespace qcs {

ExperimentRunner::ExperimentRunner(MPI_Comm comm) : comm_(comm) {
    MPI_Comm_rank(comm_, &rank_);
    MPI_Comm_size(comm_, &world_size_);
}

int ExperimentRunner::execute(AppConfig cfg) {
    if (cfg.omp_threads <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        cfg.omp_threads = std::max(1u, hw == 0 ? 1u : hw / 2u);
    }

    ReconstructionEngine engine(comm_);
    RunMetrics final_metrics;

    for (int i = 0; i < std::max(1, cfg.repeats); ++i) {
        AppConfig once = cfg;
        once.repeats = 1;
        once.seed = cfg.seed + i;
        once.experiment_name = cfg.experiment_name + "_rep" + std::to_string(i);
        once.output_dir = (std::filesystem::path(cfg.output_dir) / ("rep_" + std::to_string(i))).string();
        final_metrics = engine.run(once);

        if (rank_ == 0 && cfg.emit_csv) {
            const auto csv_path = std::filesystem::path(cfg.output_dir) / "summary.csv";
            const bool exists = std::filesystem::exists(csv_path);
            std::filesystem::create_directories(csv_path.parent_path());
            std::ofstream out(csv_path, std::ios::app);
            if (!exists) out << metrics_csv_header() << '\n';
            out << metrics_to_csv(final_metrics) << '\n';
        }

        if (rank_ == 0) {
            std::cout << "[run] backend=" << final_metrics.backend
                      << " mpi=" << final_metrics.mpi_ranks
                      << " omp=" << final_metrics.omp_threads
                      << " tasks=" << final_metrics.total_tasks
                      << " total_s=" << std::fixed << std::setprecision(4) << final_metrics.total_seconds
                      << " write_s=" << final_metrics.write_seconds
                      << " read_s=" << final_metrics.read_seconds
                      << " throughput=" << final_metrics.tasks_per_second
                      << " task/s"
                      << std::endl;
            if (cfg.emit_csv) {
                std::cout << metrics_to_csv(final_metrics) << std::endl;
            }
        }
    }

    return 0;
}

} // namespace qcs
