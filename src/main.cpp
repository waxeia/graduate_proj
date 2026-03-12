#include "config.hpp"
#include "experiment.hpp"

namespace {

void print_usage() {
    std::cout << "Usage: qcsim --config path/to/file.cfg [--set key=value ...]\n";
}

} // namespace

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int exit_code = 0;
    try {
        std::string config_path;
        std::vector<std::string> overrides;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--set" && i + 1 < argc) {
                overrides.emplace_back(argv[++i]);
            } else if (arg == "--help" || arg == "-h") {
                if (rank == 0) print_usage();
                MPI_Finalize();
                return 0;
            } else {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }

        if (config_path.empty()) {
            throw std::runtime_error("Missing --config argument");
        }

        auto cfg = qcs::load_config(config_path);
        qcs::apply_overrides(cfg, overrides);
        qcs::ExperimentRunner runner(MPI_COMM_WORLD);
        exit_code = runner.execute(cfg);
    } catch (const std::exception& ex) {
        if (rank == 0) {
            std::cerr << "Fatal: " << ex.what() << std::endl;
            print_usage();
        }
        exit_code = 1;
    }

    MPI_Finalize();
    return exit_code;
}
