#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="${ROOT_DIR}/build/pointcloud_demo"

if [[ ! -x "${EXECUTABLE}" ]]; then
  echo "未找到可执行文件，请先执行：cmake -S . -B build && cmake --build build -j\$(nproc)" >&2
  exit 1
fi

if [[ $# -gt 0 ]]; then
  exec "${EXECUTABLE}" "$1"
else
  exec "${EXECUTABLE}"
fi
