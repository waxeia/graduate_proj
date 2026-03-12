#include "config.hpp"

namespace qcs {

namespace {

bool parse_bool(const std::string& value) {
    const auto v = to_lower(trim(value));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

void set_kv(AppConfig& cfg, const std::string& key, const std::string& value) {
    const auto k = to_lower(trim(key));
    const auto v = trim(value);
    if (k == "num_qubits") cfg.num_qubits = std::stoi(v);
    else if (k == "circuit_depth") cfg.circuit_depth = std::stoi(v);
    else if (k == "num_cuts") cfg.num_cuts = std::stoi(v);
    else if (k == "total_tasks") cfg.total_tasks = static_cast<std::uint64_t>(std::stoull(v));
    else if (k == "batch_size") cfg.batch_size = static_cast<std::uint64_t>(std::stoull(v));
    else if (k == "chunk_records") cfg.chunk_records = static_cast<std::uint64_t>(std::stoull(v));
    else if (k == "max_records_in_memory") cfg.max_records_in_memory = static_cast<std::uint64_t>(std::stoull(v));
    else if (k == "repeats") cfg.repeats = std::stoi(v);
    else if (k == "seed") cfg.seed = std::stoi(v);
    else if (k == "omp_threads") cfg.omp_threads = std::stoi(v);
    else if (k == "omp_schedule") cfg.omp_schedule = to_lower(v);
    else if (k == "omp_chunk") cfg.omp_chunk = std::stoi(v);
    else if (k == "storage_mode") cfg.storage_mode = parse_storage_mode(v);
    else if (k == "output_dir") cfg.output_dir = v;
    else if (k == "experiment_name") cfg.experiment_name = v;
    else if (k == "verify_readback") cfg.verify_readback = parse_bool(v);
    else if (k == "cleanup_existing") cfg.cleanup_existing = parse_bool(v);
    else if (k == "emit_csv") cfg.emit_csv = parse_bool(v);
    else if (k == "persist_records") cfg.persist_records = parse_bool(v);
    else if (k == "verbose") cfg.verbose = parse_bool(v);
    else throw std::runtime_error("Unknown config key: " + key);
}

} // namespace

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

StorageMode parse_storage_mode(const std::string& value) {
    const auto v = to_lower(trim(value));
    if (v == "memory") return StorageMode::Memory;
    if (v == "single_hdf5" || v == "hdf5" || v == "single-file-hdf5") return StorageMode::SingleHdf5;
    if (v == "multi_file" || v == "multifile" || v == "chunked_multi_file") return StorageMode::MultiFile;
    throw std::runtime_error("Unsupported storage mode: " + value);
}

std::string to_string(StorageMode mode) {
    switch (mode) {
        case StorageMode::Memory: return "memory";
        case StorageMode::SingleHdf5: return "single_hdf5";
        case StorageMode::MultiFile: return "multi_file";
    }
    return "unknown";
}

std::string cfg_value(const std::map<std::string, std::string>& kv, const std::string& key, const std::string& fallback) {
    const auto it = kv.find(key);
    return it == kv.end() ? fallback : it->second;
}

AppConfig load_config(const std::string& path) {
    AppConfig cfg;
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    std::string line;
    int line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        const auto content = trim(line);
        if (content.empty() || content[0] == '#') continue;
        const auto eq = content.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(line_number) + ": " + content);
        }
        set_kv(cfg, content.substr(0, eq), content.substr(eq + 1));
    }

    return cfg;
}

void apply_overrides(AppConfig& cfg, const std::vector<std::string>& overrides) {
    for (const auto& item : overrides) {
        const auto eq = item.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Override must be in key=value format: " + item);
        }
        set_kv(cfg, item.substr(0, eq), item.substr(eq + 1));
    }
}

std::uint64_t derive_total_tasks(const AppConfig& cfg) {
    if (cfg.total_tasks != 0) return cfg.total_tasks;
    const double cut_factor = std::pow(4.0, static_cast<double>(cfg.num_cuts));
    const double depth_factor = static_cast<double>(std::max(1, cfg.circuit_depth));
    const double qubit_factor = std::max(1.0, std::sqrt(static_cast<double>(std::max(1, cfg.num_qubits))));
    const double raw = 64.0 * cut_factor * qubit_factor * std::log2(depth_factor + 1.0);
    return static_cast<std::uint64_t>(std::min<double>(std::max<double>(8192.0, raw), 50000000.0));
}

} // namespace qcs
