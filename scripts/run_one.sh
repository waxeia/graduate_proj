#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <mpi_ranks> <config> [extra --set key=value ...]" >&2
  exit 1
fi

MPI_RANKS="$1"
shift
CONFIG="$1"
shift

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/qcsim"

if [[ ! -x "${BIN}" ]]; then
  "${ROOT_DIR}/scripts/build.sh"
fi

export OMP_PROC_BIND=close
export OMP_PLACES=cores
mpirun -np "${MPI_RANKS}" "${BIN}" --config "${CONFIG}" "$@"
