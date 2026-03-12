#pragma once
#include "common.hpp"
#include "engine.hpp"

namespace qcs {

class ExperimentRunner {
public:
    explicit ExperimentRunner(MPI_Comm comm);
    int execute(AppConfig cfg);

private:
    MPI_Comm comm_;
    int rank_ = 0;
    int world_size_ = 1;
};

} // namespace qcs
