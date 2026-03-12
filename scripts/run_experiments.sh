#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RESULT_DIR="${ROOT_DIR}/results/batch_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${RESULT_DIR}"

"${ROOT_DIR}/scripts/build.sh"

MPI_SET=(1 2 4)
BACKENDS=(memory single_hdf5 multi_file)
BATCH_SET=(1024 4096 16384)
CHUNK_SET=(4096 16384 65536)
CUT_SET=(4 6 8)
OMP_THREADS=(4 8 12)

for mpi in "${MPI_SET[@]}"; do
  for backend in "${BACKENDS[@]}"; do
    for batch in "${BATCH_SET[@]}"; do
      for chunk in "${CHUNK_SET[@]}"; do
        for cuts in "${CUT_SET[@]}"; do
          for threads in "${OMP_THREADS[@]}"; do
            out_dir="${RESULT_DIR}/${backend}_mpi${mpi}_b${batch}_c${chunk}_cuts${cuts}_t${threads}"
            echo "[exp] backend=${backend} mpi=${mpi} batch=${batch} chunk=${chunk} cuts=${cuts} omp=${threads}"
            "${ROOT_DIR}/scripts/run_one.sh" "${mpi}" "${ROOT_DIR}/configs/baseline.cfg" \
              --set storage_mode="${backend}" \
              --set experiment_name="${backend}_mpi${mpi}_b${batch}_c${chunk}_cuts${cuts}_t${threads}" \
              --set output_dir="${out_dir}" \
              --set batch_size="${batch}" \
              --set chunk_records="${chunk}" \
              --set num_cuts="${cuts}" \
              --set omp_threads="${threads}" \
              --set cleanup_existing=true
          done
        done
      done
    done
  done
done
