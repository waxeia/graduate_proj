#include "config.hpp"
#include "experiment.hpp"

namespace {
void print_usage() {
    std::cout << "Usage: qcsim --config path/to/file.cfg [--set key=value ...]\n";
}
} //“匿名命名空间”作用：把 print_usage() 函数的作用域限制在当前 .cpp 文件内

int main(int argc, char** argv) {//argc：命令行参数个数，argv：参数数组指针的指针
    MPI_Init(&argc, &argv);//对应MPI_Finalize()销毁环境

    //获取当前进程的 rank（编号）
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);//获取当前进程在MPI通信域(MPI_COMM_WORLD)中的编号
    //printf("%d", rank);

    //异常处理与参数解析
    int exit_code = 0;//程序退出码（0=成功，非0=失败)
    try {
        std::string config_path;//存储配置文件路径
        std::vector<std::string> overrides;//存储 --set 覆盖的参数
        //遍历命令行参数
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];//读取 --config 后的路径
            } else if (arg == "--set" && i + 1 < argc) {
                overrides.emplace_back(argv[++i]);//收集 --set 后的参数
            } else if (arg == "--help" || arg == "-h") {
                if (rank == 0) print_usage();//0号进程
                MPI_Finalize();
                return 0;
            } else {
                throw std::runtime_error("Unknown argument: " + arg);//抛出运行时异常
            }
        }

        if (config_path.empty()) {
            throw std::runtime_error("Missing --config argument");
        }
        //加载配置 + 执行实验
        auto cfg = qcs::load_config(config_path);//qcs::：项目的命名空间
        qcs::apply_overrides(cfg, overrides);//用 --set 参数覆盖配置
        qcs::ExperimentRunner runner(MPI_COMM_WORLD);//创建并行实验运行器
        exit_code = runner.execute(cfg);//执行实验，返回退出码，0 表示成功，非 0 表示失败
    } catch (const std::exception& ex) {//捕获所有标准异常
        if (rank == 0) {//仅主进程打印错误信息
            std::cerr << "Fatal: " << ex.what() << std::endl;
            //std::cerr和 std::cout 类似
            print_usage();//提示正确用法
        }
        exit_code = 1;
    }

    MPI_Finalize();
    return exit_code;
}