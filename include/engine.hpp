#pragma once
#include "common.hpp"
#include "storage.hpp"

namespace qcs {

struct RunMetrics {
    std::string experiment_name;
    std::string backend;
    int mpi_ranks = 1;
    int omp_threads = 1;
    int num_qubits = 0;
    int circuit_depth = 0;
    int num_cuts = 0;
    std::uint64_t total_tasks = 0;
    std::uint64_t batch_size = 0;
    std::uint64_t chunk_records = 0;
    double compute_seconds = 0.0;
    double write_seconds = 0.0;
    double read_seconds = 0.0;
    double total_seconds = 0.0;
    double partial_sum = 0.0;
    double global_sum = 0.0;
    std::uint64_t records_written = 0;
    std::uint64_t bytes_written = 0;
    std::uint64_t files_created = 0;
    double tasks_per_second = 0.0;
    double max_buffer_mb = 0.0;
};

class ReconstructionEngine {
public:
    explicit ReconstructionEngine(MPI_Comm comm);
    RunMetrics run(const AppConfig& cfg);

private:
    MPI_Comm comm_;
    int rank_ = 0;
    int world_size_ = 1;
};

std::string metrics_csv_header();
std::string metrics_to_csv(const RunMetrics& m);

} // namespace qcs
