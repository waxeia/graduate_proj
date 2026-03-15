#include "storage.hpp"
#include "config.hpp"

namespace qcs {

namespace {

class ScopedH5Type {
public:
    explicit ScopedH5Type(hid_t id = -1) : id_(id) {}
    ~ScopedH5Type() { if (id_ >= 0) H5Tclose(id_); }
    hid_t get() const { return id_; }
    ScopedH5Type(const ScopedH5Type&) = delete;
    ScopedH5Type& operator=(const ScopedH5Type&) = delete;
private:
    hid_t id_;
};

class ScopedH5Space {
public:
    explicit ScopedH5Space(hid_t id = -1) : id_(id) {}
    ~ScopedH5Space() { if (id_ >= 0) H5Sclose(id_); }
    hid_t get() const { return id_; }
    ScopedH5Space(const ScopedH5Space&) = delete;
    ScopedH5Space& operator=(const ScopedH5Space&) = delete;
private:
    hid_t id_;
};

class ScopedH5File {
public:
    explicit ScopedH5File(hid_t id = -1) : id_(id) {}
    ~ScopedH5File() { if (id_ >= 0) H5Fclose(id_); }
    hid_t get() const { return id_; }
    ScopedH5File(const ScopedH5File&) = delete;
    ScopedH5File& operator=(const ScopedH5File&) = delete;
private:
    hid_t id_;
};

class ScopedH5Dataset {
public:
    explicit ScopedH5Dataset(hid_t id = -1) : id_(id) {}
    ~ScopedH5Dataset() { if (id_ >= 0) H5Dclose(id_); }
    hid_t get() const { return id_; }
    ScopedH5Dataset(const ScopedH5Dataset&) = delete;
    ScopedH5Dataset& operator=(const ScopedH5Dataset&) = delete;
private:
    hid_t id_;
};

class ScopedH5Group {
public:
    explicit ScopedH5Group(hid_t id = -1) : id_(id) {}
    ~ScopedH5Group() { if (id_ >= 0) H5Gclose(id_); }
    hid_t get() const { return id_; }
    ScopedH5Group(const ScopedH5Group&) = delete;
    ScopedH5Group& operator=(const ScopedH5Group&) = delete;
private:
    hid_t id_;
};

// hid_t create_record_type() {
//     const hid_t type = H5Tcreate(H5T_COMPOUND, sizeof(ResultRecord));
//     H5Tinsert(type, "task_id", HOFFSET(ResultRecord, task_id), H5T_NATIVE_UINT64);
//     H5Tinsert(type, "assignment", HOFFSET(ResultRecord, assignment), H5T_NATIVE_UINT64);
//     H5Tinsert(type, "rank", HOFFSET(ResultRecord, rank), H5T_NATIVE_INT);
//     H5Tinsert(type, "repeat_id", HOFFSET(ResultRecord, repeat_id), H5T_NATIVE_INT);
//     H5Tinsert(type, "coefficient", HOFFSET(ResultRecord, coefficient), H5T_NATIVE_DOUBLE);
//     H5Tinsert(type, "value", HOFFSET(ResultRecord, value), H5T_NATIVE_DOUBLE);
//     H5Tinsert(type, "contribution", HOFFSET(ResultRecord, contribution), H5T_NATIVE_DOUBLE);
//     H5Tinsert(type, "checksum", HOFFSET(ResultRecord, checksum), H5T_NATIVE_UINT64);
//     return type;
// }
    // src/storage.cpp 中的 create_record_type 函数
    hid_t create_record_type() {
        const hid_t type = H5Tcreate(H5T_COMPOUND, sizeof(ResultRecord));
        H5Tinsert(type, "task_id", HOFFSET(ResultRecord, task_id), H5T_NATIVE_UINT64);
        H5Tinsert(type, "assignment", HOFFSET(ResultRecord, assignment), H5T_NATIVE_UINT64);
        H5Tinsert(type, "rank", HOFFSET(ResultRecord, rank), H5T_NATIVE_INT);
        H5Tinsert(type, "value_real", HOFFSET(ResultRecord, value_real), H5T_NATIVE_DOUBLE);
        H5Tinsert(type, "value_imag", HOFFSET(ResultRecord, value_imag), H5T_NATIVE_DOUBLE);
        H5Tinsert(type, "contribution", HOFFSET(ResultRecord, contribution), H5T_NATIVE_DOUBLE);
        H5Tinsert(type, "checksum", HOFFSET(ResultRecord, checksum), H5T_NATIVE_UINT64);
        return type;
    }

void write_scalar_attr(hid_t obj, const char* name, std::uint64_t value) {
    const hsize_t dims[1] = {1};
    ScopedH5Space space(H5Screate_simple(1, dims, nullptr));
    hid_t attr = H5Acreate2(obj, name, H5T_NATIVE_UINT64, space.get(), H5P_DEFAULT, H5P_DEFAULT);
    if (attr < 0) throw std::runtime_error(std::string("Failed to create attr: ") + name);
    if (H5Awrite(attr, H5T_NATIVE_UINT64, &value) < 0) {
        H5Aclose(attr);
        throw std::runtime_error(std::string("Failed to write attr: ") + name);
    }
    H5Aclose(attr);
}

void write_string_attr(hid_t obj, const char* name, const std::string& value) {
    hid_t type = H5Tcopy(H5T_C_S1);
    H5Tset_size(type, value.size());
    ScopedH5Type scoped_type(type);
    const hsize_t dims[1] = {1};
    ScopedH5Space space(H5Screate_simple(1, dims, nullptr));
    hid_t attr = H5Acreate2(obj, name, type, space.get(), H5P_DEFAULT, H5P_DEFAULT);
    if (attr < 0) throw std::runtime_error(std::string("Failed to create string attr: ") + name);
    const char* ptr = value.c_str();
    if (H5Awrite(attr, type, ptr) < 0) {
        H5Aclose(attr);
        throw std::runtime_error(std::string("Failed to write string attr: ") + name);
    }
    H5Aclose(attr);
}

std::string dataset_path_for_rank(int rank) {
    return "/ranks/rank_" + std::to_string(rank) + "/records";
}

std::string batches_path_for_rank(int rank) {
    return "/ranks/rank_" + std::to_string(rank) + "/batch_offsets";
}

void create_rank_group_and_datasets(hid_t file_id, int rank, std::uint64_t chunk_records) {
    const std::string root = "/ranks";
    if (H5Lexists(file_id, root.c_str(), H5P_DEFAULT) <= 0) {
        ScopedH5Group root_group(H5Gcreate2(file_id, root.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
        if (root_group.get() < 0) throw std::runtime_error("Failed to create /ranks group");
    }

    const std::string group_path = root + "/rank_" + std::to_string(rank);
    ScopedH5Group group(H5Gcreate2(file_id, group_path.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    if (group.get() < 0) throw std::runtime_error("Failed to create rank group: " + group_path);

    ScopedH5Type record_type(create_record_type());
    hsize_t dims[1] = {0};
    hsize_t max_dims[1] = {H5S_UNLIMITED};
    ScopedH5Space space(H5Screate_simple(1, dims, max_dims));

    hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
    if (dcpl < 0) throw std::runtime_error("Failed to create HDF5 dcpl");
    hsize_t chunk_dims[1] = {std::max<std::uint64_t>(1, chunk_records)};
    H5Pset_chunk(dcpl, 1, chunk_dims);

    ScopedH5Dataset dataset(H5Dcreate2(file_id,
                                       dataset_path_for_rank(rank).c_str(),
                                       record_type.get(),
                                       space.get(),
                                       H5P_DEFAULT,
                                       dcpl,
                                       H5P_DEFAULT));
    if (dataset.get() < 0) {
        H5Pclose(dcpl);
        throw std::runtime_error("Failed to create records dataset for rank");
    }

    ScopedH5Dataset batch_dataset(H5Dcreate2(file_id,
                                             batches_path_for_rank(rank).c_str(),
                                             H5T_NATIVE_UINT64,
                                             space.get(),
                                             H5P_DEFAULT,
                                             dcpl,
                                             H5P_DEFAULT));
    if (batch_dataset.get() < 0) {
        H5Pclose(dcpl);
        throw std::runtime_error("Failed to create batch_offsets dataset for rank");
    }
    H5Pclose(dcpl);
}

void append_u64_dataset(hid_t file_id, const std::string& path, std::uint64_t value) {
    ScopedH5Dataset ds(H5Dopen2(file_id, path.c_str(), H5P_DEFAULT));
    if (ds.get() < 0) throw std::runtime_error("Failed to open dataset for append: " + path);
    ScopedH5Space old_space(H5Dget_space(ds.get()));
    hsize_t old_dims[1] = {0};
    H5Sget_simple_extent_dims(old_space.get(), old_dims, nullptr);
    const hsize_t new_dims[1] = {old_dims[0] + 1};
    if (H5Dset_extent(ds.get(), new_dims) < 0) throw std::runtime_error("Failed to extend dataset: " + path);
    ScopedH5Space file_space(H5Dget_space(ds.get()));
    const hsize_t start[1] = {old_dims[0]};
    const hsize_t count[1] = {1};
    H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr);
    ScopedH5Space mem_space(H5Screate_simple(1, count, nullptr));
    if (H5Dwrite(ds.get(), H5T_NATIVE_UINT64, mem_space.get(), file_space.get(), H5P_DEFAULT, &value) < 0) {
        throw std::runtime_error("Failed to append u64 dataset: " + path);
    }
}

void append_record_dataset(hid_t file_id, const std::string& path, const std::vector<ResultRecord>& batch) {
    ScopedH5Dataset ds(H5Dopen2(file_id, path.c_str(), H5P_DEFAULT));
    if (ds.get() < 0) throw std::runtime_error("Failed to open records dataset: " + path);

    ScopedH5Space old_space(H5Dget_space(ds.get()));
    hsize_t old_dims[1] = {0};
    H5Sget_simple_extent_dims(old_space.get(), old_dims, nullptr);
    const hsize_t new_dims[1] = {old_dims[0] + batch.size()};
    if (H5Dset_extent(ds.get(), new_dims) < 0) throw std::runtime_error("Failed to extend record dataset: " + path);

    ScopedH5Space file_space(H5Dget_space(ds.get()));
    const hsize_t start[1] = {old_dims[0]};
    const hsize_t count[1] = {batch.size()};
    H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr);
    ScopedH5Space mem_space(H5Screate_simple(1, count, nullptr));
    ScopedH5Type record_type(create_record_type());
    if (H5Dwrite(ds.get(), record_type.get(), mem_space.get(), file_space.get(), H5P_DEFAULT, batch.data()) < 0) {
        throw std::runtime_error("Failed to write record batch: " + path);
    }
}

std::vector<ResultRecord> read_all_records(hid_t file_id, const std::string& path) {
    ScopedH5Dataset ds(H5Dopen2(file_id, path.c_str(), H5P_DEFAULT));
    if (ds.get() < 0) throw std::runtime_error("Failed to open records dataset for readback: " + path);
    ScopedH5Space space(H5Dget_space(ds.get()));
    hsize_t dims[1] = {0};
    H5Sget_simple_extent_dims(space.get(), dims, nullptr);
    std::vector<ResultRecord> out(static_cast<std::size_t>(dims[0]));
    if (!out.empty()) {
        ScopedH5Type record_type(create_record_type());
        if (H5Dread(ds.get(), record_type.get(), H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data()) < 0) {
            throw std::runtime_error("Failed to read records dataset: " + path);
        }
    }
    return out;
}

class MemoryStorageBackend final : public StorageBackend {
public:
    void open(const CircuitDescriptor&, const AppConfig& cfg, int rank, int world_size, MPI_Comm comm) override {
        cfg_ = cfg;
        rank_ = rank;
        world_size_ = world_size;
        comm_ = comm;
        records_.clear();
        stats_ = {};
    }

    void write_batch(const std::vector<ResultRecord>& batch) override {
        const auto t0 = MPI_Wtime();
        records_.insert(records_.end(), batch.begin(), batch.end());
        const auto t1 = MPI_Wtime();
        stats_.write_seconds += (t1 - t0);
        stats_.records_written += batch.size();
        stats_.bytes_written += batch.size() * sizeof(ResultRecord);
        stats_.peak_buffer_mb = std::max(stats_.peak_buffer_mb,
                                         static_cast<double>(records_.size() * sizeof(ResultRecord)) / (1024.0 * 1024.0));
    }

    StorageStats finalize() override {
        const auto t0 = MPI_Wtime();
        double sum = 0.0;
        if (cfg_.verify_readback) {
            for (const auto& r : records_) sum += r.contribution;
        }
        const auto t1 = MPI_Wtime();
        stats_.read_seconds = (t1 - t0);
        stats_.readback_sum = sum;
        return stats_;
    }

    std::string name() const override { return "memory"; }

private:
    AppConfig cfg_;
    int rank_ = 0;
    int world_size_ = 1;
    MPI_Comm comm_ = MPI_COMM_WORLD;
    std::vector<ResultRecord> records_;
    StorageStats stats_;
};

class SingleHdf5StorageBackend final : public StorageBackend {
public:
    void open(const CircuitDescriptor& circuit, const AppConfig& cfg, int rank, int world_size, MPI_Comm comm) override {
        circuit_ = circuit;
        cfg_ = cfg;
        rank_ = rank;
        world_size_ = world_size;
        comm_ = comm;
        stats_ = {};
        file_path_ = std::filesystem::path(cfg.output_dir) / "single_file_store.h5";

        if (rank_ == 0) {
            std::filesystem::create_directories(cfg.output_dir);
            if (cfg.cleanup_existing && std::filesystem::exists(file_path_)) {
                std::filesystem::remove(file_path_);
            }
            ScopedH5File file(H5Fcreate(file_path_.string().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT));
            if (file.get() < 0) throw std::runtime_error("Failed to create single HDF5 file");
            write_scalar_attr(file.get(), "num_qubits", static_cast<std::uint64_t>(circuit.num_qubits));
            write_scalar_attr(file.get(), "circuit_depth", static_cast<std::uint64_t>(circuit.circuit_depth));
            write_scalar_attr(file.get(), "num_cuts", static_cast<std::uint64_t>(circuit.num_cuts));
            write_scalar_attr(file.get(), "total_tasks", static_cast<std::uint64_t>(circuit.total_tasks));
            write_string_attr(file.get(), "backend", "single_hdf5");
            write_string_attr(file.get(), "experiment_name", cfg.experiment_name);
        }
        MPI_Barrier(comm_);

        for (int owner = 0; owner < world_size_; ++owner) {
            if (owner == rank_) {
                ScopedH5File file(H5Fopen(file_path_.string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
                if (file.get() < 0) throw std::runtime_error("Failed to open single HDF5 file for dataset init");
                create_rank_group_and_datasets(file.get(), rank_, cfg.chunk_records);
            }
            MPI_Barrier(comm_);
        }
        if (rank_ == 0) stats_.files_created = 1;
    }

    void write_batch(const std::vector<ResultRecord>& batch) override {
        const auto t0 = MPI_Wtime();
        for (int owner = 0; owner < world_size_; ++owner) {
            if (owner == rank_) {
                ScopedH5File file(H5Fopen(file_path_.string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
                if (file.get() < 0) throw std::runtime_error("Failed to open single HDF5 file for write");
                append_record_dataset(file.get(), dataset_path_for_rank(rank_), batch);
                append_u64_dataset(file.get(), batches_path_for_rank(rank_), static_cast<std::uint64_t>(batch.size()));
                H5Fflush(file.get(), H5F_SCOPE_GLOBAL);
            }
            MPI_Barrier(comm_);
        }
        const auto t1 = MPI_Wtime();
        stats_.write_seconds += (t1 - t0);
        stats_.records_written += batch.size();
        stats_.bytes_written += batch.size() * sizeof(ResultRecord);
        stats_.peak_buffer_mb = std::max(stats_.peak_buffer_mb,
                                         static_cast<double>(batch.size() * sizeof(ResultRecord)) / (1024.0 * 1024.0));
    }

    StorageStats finalize() override {
        const auto t0 = MPI_Wtime();
        double sum = 0.0;
        if (cfg_.verify_readback) {
            for (int owner = 0; owner < world_size_; ++owner) {
                if (owner == rank_) {
                    ScopedH5File file(H5Fopen(file_path_.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT));
                    const auto records = read_all_records(file.get(), dataset_path_for_rank(rank_));
                    for (const auto& r : records) sum += r.contribution;
                }
                MPI_Barrier(comm_);
            }
        }
        const auto t1 = MPI_Wtime();
        stats_.read_seconds = (t1 - t0);
        stats_.readback_sum = sum;
        return stats_;
    }

    std::string name() const override { return "single_hdf5"; }

private:
    CircuitDescriptor circuit_;
    AppConfig cfg_;
    int rank_ = 0;
    int world_size_ = 1;
    MPI_Comm comm_ = MPI_COMM_WORLD;
    std::filesystem::path file_path_;
    StorageStats stats_;
};

class MultiFileStorageBackend final : public StorageBackend {
public:
    void open(const CircuitDescriptor& circuit, const AppConfig& cfg, int rank, int world_size, MPI_Comm comm) override {
        circuit_ = circuit;
        cfg_ = cfg;
        rank_ = rank;
        world_size_ = world_size;
        comm_ = comm;
        stats_ = {};
        shard_index_ = 0;
        rank_dir_ = std::filesystem::path(cfg.output_dir) / ("rank_" + std::to_string(rank_));
        std::filesystem::create_directories(rank_dir_);
        if (cfg.cleanup_existing) {
            for (const auto& item : std::filesystem::directory_iterator(rank_dir_)) {
                std::filesystem::remove_all(item.path());
            }
        }
        MPI_Barrier(comm_);
    }

    void write_batch(const std::vector<ResultRecord>& batch) override {
        const auto t0 = MPI_Wtime();
        if (!cfg_.persist_records) {
            stats_.write_seconds += (MPI_Wtime() - t0);
            stats_.records_written += batch.size();
            stats_.bytes_written += batch.size() * sizeof(ResultRecord);
            return;
        }

        const std::filesystem::path shard_path = rank_dir_ / shard_name();
        ScopedH5File file(H5Fcreate(shard_path.string().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT));
        if (file.get() < 0) throw std::runtime_error("Failed to create shard HDF5 file");

        write_scalar_attr(file.get(), "rank", static_cast<std::uint64_t>(rank_));
        write_scalar_attr(file.get(), "shard_index", static_cast<std::uint64_t>(shard_index_));
        write_scalar_attr(file.get(), "records", static_cast<std::uint64_t>(batch.size()));
        write_string_attr(file.get(), "backend", "multi_file");

        ScopedH5Type record_type(create_record_type());
        const hsize_t dims[1] = {batch.size()};
        ScopedH5Space space(H5Screate_simple(1, dims, nullptr));
        hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
        hsize_t chunk_dims[1] = {std::min<std::uint64_t>(std::max<std::uint64_t>(1, cfg_.chunk_records), batch.size())};
        H5Pset_chunk(dcpl, 1, chunk_dims);
        ScopedH5Dataset ds(H5Dcreate2(file.get(), "/records", record_type.get(), space.get(), H5P_DEFAULT, dcpl, H5P_DEFAULT));
        if (ds.get() < 0) {
            H5Pclose(dcpl);
            throw std::runtime_error("Failed to create shard records dataset");
        }
        if (!batch.empty() && H5Dwrite(ds.get(), record_type.get(), H5S_ALL, H5S_ALL, H5P_DEFAULT, batch.data()) < 0) {
            H5Pclose(dcpl);
            throw std::runtime_error("Failed to write shard records dataset");
        }
        H5Pclose(dcpl);
        H5Fflush(file.get(), H5F_SCOPE_LOCAL);

        const auto t1 = MPI_Wtime();
        stats_.write_seconds += (t1 - t0);
        stats_.records_written += batch.size();
        stats_.bytes_written += batch.size() * sizeof(ResultRecord);
        stats_.files_created += 1;
        stats_.peak_buffer_mb = std::max(stats_.peak_buffer_mb,
                                         static_cast<double>(batch.size() * sizeof(ResultRecord)) / (1024.0 * 1024.0));
        ++shard_index_;
    }

    StorageStats finalize() override {
        const auto t0 = MPI_Wtime();
        double sum = 0.0;
        if (cfg_.verify_readback && cfg_.persist_records) {
            for (std::uint64_t i = 0; i < shard_index_; ++i) {
                const auto path = rank_dir_ / shard_name(i);
                ScopedH5File file(H5Fopen(path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT));
                if (file.get() < 0) throw std::runtime_error("Failed to open shard for readback: " + path.string());
                const auto records = read_all_records(file.get(), "/records");
                for (const auto& r : records) sum += r.contribution;
            }
        }
        const auto t1 = MPI_Wtime();
        stats_.read_seconds = (t1 - t0);
        stats_.readback_sum = sum;
        return stats_;
    }

    std::string name() const override { return "multi_file"; }

private:
    std::string shard_name() const {
        return shard_name(shard_index_);
    }

    std::string shard_name(std::uint64_t idx) const {
        std::ostringstream oss;
        oss << "chunk_" << std::setw(6) << std::setfill('0') << idx << ".h5";
        return oss.str();
    }

    CircuitDescriptor circuit_;
    AppConfig cfg_;
    int rank_ = 0;
    int world_size_ = 1;
    MPI_Comm comm_ = MPI_COMM_WORLD;
    std::uint64_t shard_index_ = 0;
    std::filesystem::path rank_dir_;
    StorageStats stats_;
};
} // namespace

std::unique_ptr<StorageBackend> make_storage_backend(StorageMode mode) {
    switch (mode) {
        case StorageMode::Memory: return std::make_unique<MemoryStorageBackend>();
        case StorageMode::SingleHdf5: return std::make_unique<SingleHdf5StorageBackend>();
        case StorageMode::MultiFile: return std::make_unique<MultiFileStorageBackend>();
    }
    throw std::runtime_error("Unsupported storage backend");
}
} // namespace qcs