#!/bin/bash

declare base_dir="$(dirname $(dirname $(realpath $0)))"

cd ${base_dir}/build
set -e
make -j
set +e

fast_query_dir = "/mnt/nvme/fast_query_project"

./streamifier $fast_query_dir/static/kron_18 $fast_query_dir/binary_streams/kron_18_ff_binary 100 0 --fixed-forest
./queryifier $fast_query_dir/binary_streams/kron_18_stream_binary $fast_query_dir/binary_streams kron_18_query10_binary 0.1 1000 2000
./queryifier $fast_query_dir/binary_streams/kron_18_ff_binary $fast_query_dir/binary_streams kron_18_ff_query10_binary 0.1 1000 2000
