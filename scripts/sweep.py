#!/usr/bin/env python3
import argparse
import csv
import itertools
import os
import subprocess
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser(description="Batch reproducible sweeps for qcsim")
    p.add_argument("--root", default=Path(__file__).resolve().parents[1], type=Path)
    p.add_argument("--mpi", default="1,2,4,8")
    p.add_argument("--backends", default="memory,single_hdf5,multi_file")
    p.add_argument("--batch-sizes", default="1024,4096,16384")
    p.add_argument("--chunk-records", default="4096,16384,65536")
    p.add_argument("--cuts", default="4,6,8")
    p.add_argument("--threads", default="1,2")
    p.add_argument("--summary", default="results/sweep_summary.csv")
    p.add_argument("--config", default="configs/baseline.cfg")
    return p.parse_args()


def split_csv(text):
    return [x.strip() for x in text.split(",") if x.strip()]


def ensure_build(root: Path):
    subprocess.run([str(root / "scripts" / "build.sh")], check=True)


def extract_last_csv(stdout: str):
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    for line in reversed(lines):
        if "," in line and not line.startswith("[run]"):
            return line
    raise RuntimeError("CSV summary line not found in program output")


def main():
    args = parse_args()
    root = args.root.resolve()
    ensure_build(root)

    mpi_values = split_csv(args.mpi)
    backends = split_csv(args.backends)
    batch_sizes = split_csv(args.batch_sizes)
    chunk_records = split_csv(args.chunk_records)
    cuts = split_csv(args.cuts)
    threads = split_csv(args.threads)

    summary_path = (root / args.summary).resolve()
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    header_written = summary_path.exists() and summary_path.stat().st_size > 0
    with summary_path.open("a", newline="") as fout:
        writer = None
        # for mpi, backend, batch, chunk, cut, thread in itertools.product(
        #     mpi_values, backends, batch_sizes, chunk_records, cuts, threads
        # ):
        #     exp_name = f"{backend}_mpi{mpi}_b{batch}_c{chunk}_cuts{cut}_t{thread}"
        #     out_dir = root / "results" / "batch_20260505_151229"  # 所有实验共用这个文件夹
        #     cmd = [
        #         str(root / "scripts" / "run_one.sh"),
        #         mpi,
        #         str(root / args.config),
        #         "--set", f"storage_mode={backend}",
        #         "--set", f"experiment_name={exp_name}",
        #         "--set", f"output_dir={out_dir}",
        #         "--set", f"batch_size={batch}",
        #         "--set", f"chunk_records={chunk}",
        #         "--set", f"num_cuts={cut}",
        #         "--set", f"omp_threads={thread}",
        #         "--set", "cleanup_existing=true",
        #     ]
        #     print("RUN", " ".join(cmd))
        #     # proc = subprocess.run(cmd, check=True, capture_output=True, text=True)
        #     proc = open(f"{out_dir}/summary.csv").read()
        #     csv_line = extract_last_csv(proc.stdout)
        #     cells = next(csv.reader([csv_line]))
        #     if writer is None:
        #         header = [
        #             "experiment_name", "backend", "mpi_ranks", "omp_threads", "num_qubits", "circuit_depth", "num_cuts",
        #             "total_tasks", "batch_size", "chunk_records", "compute_seconds", "write_seconds", "read_seconds",
        #             "total_seconds", "global_sum", "records_written", "bytes_written", "files_created",
        #             "tasks_per_second", "max_buffer_mb"
        #         ]
        #         writer = csv.writer(fout)
        #         if not header_written:
        #             writer.writerow(header)
        #             header_written = True
        #     writer.writerow(cells)
        #     fout.flush()
        #     print(proc.stdout)
        #     if proc.stderr:
        #         print(proc.stderr)
        for mpi, backend, batch, chunk, cut, thread in itertools.product(
                mpi_values, backends, batch_sizes, chunk_records, cuts, threads
        ):
            # 1. 构造实验文件夹名称（必须与你图片里的文件夹名格式完全一致）
            exp_name = f"{backend}_mpi{mpi}_b{batch}_c{chunk}_cuts{cut}_t{thread}"

            # 2. 指向你图片中显示的实际路径
            # 注意：这里的 "batch_20260505_151229" 建议改为变量或从参数读取
            actual_dir = root / "results" / "batch_20260505_151229" / exp_name
            csv_file_path = actual_dir / "summary.csv"

            print(f"Checking: {csv_file_path}")

            # 3. 检查文件是否存在，存在则读取，不存在则跳过
            if csv_file_path.exists():
                with open(csv_file_path, "r") as f:
                    lines = [l.strip() for l in f.readlines() if l.strip()]
                    if not lines:
                        continue

                    # 通常 summary.csv 第一行是表头，第二行是数据，我们取最后一行
                    csv_line = lines[-1]
                    cells = next(csv.reader([csv_line]))
            else:
                print(f"Warning: {exp_name} not found, skipping...")
                continue

            # 4. 写入总表逻辑（保持不变）
            if writer is None:
                header = [
                    "experiment_name", "backend", "mpi_ranks", "omp_threads", "num_qubits", "circuit_depth", "num_cuts",
                    "total_tasks", "batch_size", "chunk_records", "compute_seconds", "write_seconds", "read_seconds",
                    "total_seconds", "global_sum", "records_written", "bytes_written", "files_created",
                    "tasks_per_second", "max_buffer_mb"
                ]
                writer = csv.writer(fout)
                if not header_written:
                    writer.writerow(header)
                    header_written = True

            # 如果读取到的 cells 长度不对（比如读取到了表头），这里可以加个判断
            if cells[0] == "experiment_name":
                continue

            writer.writerow(cells)
            fout.flush()


if __name__ == "__main__":
    main()
