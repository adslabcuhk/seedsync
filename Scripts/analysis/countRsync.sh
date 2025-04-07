#!/bin/bash

LOG_FILE=$1

total_sent_bytes=0
total_execution_time=0
total_file_size=0

current_logical_size=0
current_physical_size=0
current_execution_time=0
line_count=0

while IFS= read -r line; do

    if [[ "$line" =~ sent\ ([0-9,]+)\ bytes ]]; then
        sent_bytes=$(echo "${BASH_REMATCH[1]}" | tr -d ',')
        total_sent_bytes=$((total_sent_bytes + sent_bytes))
        current_physical_size=$sent_bytes
        line_count=$((line_count + 1))
    fi

    if [[ "$line" =~ Total\ execution\ time\ of\ Rsync\ for.*:\ ([0-9]+\.[0-9]+)\ s ]]; then
        total_execution_time=$(echo "$total_execution_time + ${BASH_REMATCH[1]}" | bc)
        current_execution_time=${BASH_REMATCH[1]}
        line_count=$((line_count + 1))
    fi

    if [[ "$line" =~ Original\ file\ size:\ ([0-9]+) ]]; then
        total_file_size=$((total_file_size + ${BASH_REMATCH[1]}))
        current_logical_size=${BASH_REMATCH[1]}
        line_count=$((line_count + 1))
    fi
    if [[ "$line_count" = 3 ]]; then
        echo "Physical size: $current_physical_size bytes, Logical size: $current_logical_size bytes, Execution time: $current_execution_time seconds"
        base_name=$(basename "$LOG_FILE")
        echo "Physical throughput: $(echo "scale=2; $current_physical_size / $current_execution_time / 1024 / 1024" | bc) MiB/s" >>"${base_name}_physical_thpt.log"
        echo "Logical throughput: $(echo "scale=2; $current_logical_size / $current_execution_time / 1024 / 1024" | bc) MiB/s" >>"${base_name}_logical_thpt.log"
        line_count=0
    fi
done <"$LOG_FILE"

echo "Total File Size: $total_file_size bytes"
echo "Total Sent Bytes: $total_sent_bytes bytes"
echo "Total Execution Time: $total_execution_time seconds"
echo "Physical throughput: $(echo "scale=2; $total_sent_bytes / $total_execution_time / 1024 / 1024" | bc) MiB/s"
echo "Logical throughput: $(echo "scale=2; $total_file_size / $total_execution_time / 1024 / 1024" | bc) MiB/s"
reduction_ratio=$(echo "scale=10; $total_file_size / $total_sent_bytes" | bc)
printf "Reduction ratio: %.2f\n" "$reduction_ratio"
