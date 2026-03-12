#include "engine.hpp"
#include "config.hpp"
#include "circuit.hpp"

namespace qcs {

namespace {

void configure_openmp(const AppConfig& cfg) {
#ifdef _OPENMP
    if (cfg.omp_threads > 0) {
        omp_set_num_threads(cfg.omp_threads);
    }
    omp_sched_t sched = omp_sched_static;
    if (cfg.omp_schedule == "dynamic") sched = omp_sched_dynamic;
    else if (cfg.omp_schedule == "guided") sched = omp_sched_guided;
    else if (cfg.omp_schedule == "auto") sched = omp_sched_auto;
    omp_set_schedule(sched, cfg.omp_chunk > 0 ? cfg.omp_chunk : 0);
#endif
}

int effective_omp_threads() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

} // namespace

ReconstructionEngine::ReconstructionEngine(MPI_Comm comm) : comm_(comm) {
    MPI_Comm_rank(comm_, &rank_);
    MPI_Comm_size(comm_, &world_size_);
}

RunMetrics ReconstructionEngine::run(const AppConfig& cfg_in) {
    AppConfig cfg = cfg_in;
    configure_openmp(cfg);

    const auto circuit = make_circuit(cfg);
    const auto range = split_tasks(circuit.total_tasks, rank_, world_size_);
    auto storage = make_storage_backend(cfg.storage_mode);
    storage->open(circuit, cfg, rank_, world_size_, comm_);

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

    MPI_Barrier(comm_);
    const auto total_t0 = MPI_Wtime();
    double compute_seconds = 0.0;

    for (int repeat = 0; repeat < std::max(1, cfg.repeats); ++repeat) {
        for (std::uint64_t offset = range.begin; offset < range.end; offset += cfg.batch_size) {
            const std::uint64_t batch_end = std::min<std::uint64_t>(range.end, offset + cfg.batch_size);
            const std::size_t batch_n = static_cast<std::size_t>(batch_end - offset);
            std::vector<ResultRecord> batch(batch_n);

            const auto compute_t0 = MPI_Wtime();
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime)
#endif
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(batch_n); ++i) {
                const auto task_id = offset + static_cast<std::uint64_t>(i);
                batch[static_cast<std::size_t>(i)] = evaluate_task(circuit, task_id, rank_, repeat);
            }
            const auto compute_t1 = MPI_Wtime();
            compute_seconds += (compute_t1 - compute_t0);

            storage->write_batch(batch);
        }
    }

    const auto storage_stats = storage->finalize();
    const auto total_t1 = MPI_Wtime();

    metrics.compute_seconds = compute_seconds;
    metrics.write_seconds = storage_stats.write_seconds;
    metrics.read_seconds = storage_stats.read_seconds;
    metrics.total_seconds = total_t1 - total_t0;
    metrics.partial_sum = storage_stats.readback_sum;
    metrics.records_written = storage_stats.records_written;
    metrics.bytes_written = storage_stats.bytes_written;
    metrics.files_created = storage_stats.files_created;
    metrics.max_buffer_mb = storage_stats.peak_buffer_mb;

    MPI_Reduce(&metrics.partial_sum, &metrics.global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, comm_);

    double local_tasks = static_cast<double>(metrics.records_written);
    double global_tasks = 0.0;
    MPI_Reduce(&local_tasks, &global_tasks, 1, MPI_DOUBLE, MPI_SUM, 0, comm_);
    if (rank_ == 0) {
        metrics.tasks_per_second = global_tasks / std::max(metrics.total_seconds, 1e-9);
    }

    std::uint64_t total_bytes = 0;
    std::uint64_t total_files = 0;
    double max_write = 0.0;
    double max_read = 0.0;
    double max_compute = 0.0;
    double max_total = 0.0;
    double max_buffer = 0.0;
    std::uint64_t local_bytes = metrics.bytes_written;
    std::uint64_t local_files = metrics.files_created;
    MPI_Reduce(&local_bytes, &total_bytes, 1, MPI_UINT64_T, MPI_SUM, 0, comm_);
    MPI_Reduce(&local_files, &total_files, 1, MPI_UINT64_T, MPI_SUM, 0, comm_);
    MPI_Reduce(&metrics.write_seconds, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    MPI_Reduce(&metrics.read_seconds, &max_read, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    MPI_Reduce(&metrics.compute_seconds, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    MPI_Reduce(&metrics.total_seconds, &max_total, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);
    MPI_Reduce(&metrics.max_buffer_mb, &max_buffer, 1, MPI_DOUBLE, MPI_MAX, 0, comm_);

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
