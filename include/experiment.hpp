#pragma once
#include "common.hpp"//通用工具
#include "engine.hpp"//核心仿真引擎

namespace qcs {
class ExperimentRunner {
public:
    explicit ExperimentRunner(MPI_Comm comm);//显式构造函数（禁止隐式转换），接收 MPI 通信域。显式：主动写代码而不是自动来的
    int execute(AppConfig cfg);//核心方法，执行实验，接收配置对象，返回退出码（0=成功）

private://类的私有成员变量
    MPI_Comm comm_;//保存 MPI 通信域（多进程通信的核心句柄handle）
    int rank_ = 0;//当前进程的 rank（0=主进程，>0=从进程）
    int world_size_ = 1;//MPI 总进程数（并行度）
};
} // namespace qcs