#pragma once
#include "common.hpp"
#include "config.hpp"

namespace qcs {

struct CircuitDescriptor {
    int num_qubits = 0;
    int circuit_depth = 0;
    int num_cuts = 0;
    int seed = 0;
    std::vector<int> cut_locations;
    std::vector<double> gate_weights;
    std::uint64_t total_tasks = 0;
};

struct ResultRecord {
    std::uint64_t task_id = 0;
    std::uint64_t assignment = 0;
    int rank = 0;
    int repeat_id = 0;
    double coefficient = 0.0;
    double value = 0.0;
    double contribution = 0.0;
    std::uint64_t checksum = 0;
};

struct LocalTaskRange {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
};

CircuitDescriptor make_circuit(const AppConfig& cfg);
LocalTaskRange split_tasks(std::uint64_t total_tasks, int rank, int world_size);
ResultRecord evaluate_task(const CircuitDescriptor& circuit,
                           std::uint64_t global_task_id,
                           int rank,
                           int repeat_id);
std::uint64_t deterministic_checksum(const ResultRecord& record);

} // namespace qcs
