#pragma once
#include "common.hpp"

namespace qcs {

enum class StorageMode {
    Memory,
    SingleHdf5,
    MultiFile
};

struct AppConfig {
    int num_qubits = 28;
    int circuit_depth = 120;
    int num_cuts = 6;
    std::uint64_t total_tasks = 0;
    std::uint64_t batch_size = 4096;
    std::uint64_t chunk_records = 16384;
    std::uint64_t max_records_in_memory = 500000;
    int repeats = 1;
    int seed = 2026;
    int omp_threads = 0;
    std::string omp_schedule = "static";
    int omp_chunk = 0;
    StorageMode storage_mode = StorageMode::Memory;
    std::string output_dir = "results/run";
    std::string experiment_name = "baseline";
    bool verify_readback = true;
    bool cleanup_existing = true;
    bool emit_csv = true;
    bool persist_records = true;
    bool verbose = true;
};

std::string trim(const std::string& s);
std::string to_lower(std::string s);
StorageMode parse_storage_mode(const std::string& value);
std::string to_string(StorageMode mode);
AppConfig load_config(const std::string& path);
void apply_overrides(AppConfig& cfg, const std::vector<std::string>& overrides);
std::string cfg_value(const std::map<std::string, std::string>& kv, const std::string& key, const std::string& fallback);
std::uint64_t derive_total_tasks(const AppConfig& cfg);

} // namespace qcs
