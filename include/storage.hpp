#pragma once
#include "common.hpp"
#include "circuit.hpp"//结果存储后端（HDF5/多文件/内存）

namespace qcs {
struct StorageStats {
    std::uint64_t records_written = 0;//写入的记录数量
    std::uint64_t bytes_written = 0;//写入的总字节数
    std::uint64_t files_created = 0;//存储后端创建的文件总数
    double write_seconds = 0.0;//写入操作的总耗时
    double read_seconds = 0.0;//读取操作的总耗时
    double peak_buffer_mb = 0.0;//峰值缓冲区大小
    double readback_sum = 0.0;//TODO：验证读回的结果总和，不太理解
    //读回（readback）是指在存储操作完成后，再次从存储介质（文件或内存）读取数据，以验证数据的完整性和正确性。
};

class StorageBackend {
//纯虚函数，不能直接实例化，必须由派生类（HDF5StorageBackend, MultifileStorageBackend, MemoryStorageBackend）实现这些函数
public:
    virtual ~StorageBackend() = default;//虚析构函数
    virtual void open(const CircuitDescriptor& circuit, const AppConfig& cfg, int rank, int world_size, MPI_Comm comm) = 0;//初始化存储后端
    virtual void write_batch(const std::vector<ResultRecord>& batch) = 0;//批量写入结果记录到存储介质
    //=0是纯虚函数，子类必须实现，否则无法实例化
    virtual StorageStats finalize() = 0;//完成存储操作
    virtual std::string name() const = 0;//返回存储后端的名称字符串
};
//函数声明--工厂函数
std::unique_ptr<StorageBackend> make_storage_backend(StorageMode mode);
} // namespace qcs