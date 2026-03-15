#pragma once
#include "common.hpp"
#include "circuit.hpp"//结果存储后端（HDF5/多文件/内存）
#include <hdf5.h>

namespace qcs {
struct StorageStats {
    std::uint64_t records_written = 0;
    std::uint64_t bytes_written = 0;
    std::uint64_t files_created = 0;
    double write_seconds = 0.0;
    double read_seconds = 0.0;
    double peak_buffer_mb = 0.0;
    double readback_sum = 0.0;
};

class StorageBackend {
public:
    virtual ~StorageBackend() = default;
    virtual void open(const CircuitDescriptor& circuit, const AppConfig& cfg, int rank, int world_size, MPI_Comm comm) = 0;
    virtual void write_batch(const std::vector<ResultRecord>& batch) = 0;
    virtual StorageStats finalize() = 0;
    virtual std::string name() const = 0;
};

std::unique_ptr<StorageBackend> make_storage_backend(StorageMode mode);
} // namespace qcs