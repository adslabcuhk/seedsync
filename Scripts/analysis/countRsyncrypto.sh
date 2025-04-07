#!/bin/bash

LOG_FILE=$1

total_original_size=0
total_processed_size=0
total_sent_bytes=0
total_rsyncrypto_rsync_time=0
total_encryption_time=0
total_rsync_time=0

current_logical_size=0
current_physical_size=0
current_execution_time=0
line_count=0

while IFS= read -r line; do
    # Original size (before rsyncrypto processing)
    if [[ "$line" =~ Original\ file\ size:\ ([0-9]+) ]]; then
        total_original_size=$((total_original_size + ${BASH_REMATCH[1]}))
        current_logical_size=${BASH_REMATCH[1]}
        line_count=$((line_count + 1))
    fi

    # Rsyncrypto processed size (after rsyncrypto processing)
    if [[ "$line" =~ Total\ file\ size:\ ([0-9,]+)\ bytes ]]; then
        rsync_file_size=$(echo "${BASH_REMATCH[1]}" | tr -d ',')
        total_processed_size=$((total_processed_size + rsync_file_size))
    fi

    # Rsync sent bytes
    if [[ "$line" =~ sent\ ([0-9,]+)\ bytes ]]; then
        sent_bytes=$(echo "${BASH_REMATCH[1]}" | tr -d ',')
        total_sent_bytes=$((total_sent_bytes + sent_bytes))
        current_physical_size=$sent_bytes
        line_count=$((line_count + 1))
    fi

    # Total time
    if [[ "$line" =~ Total\ execution\ time\ of\ Rsyncrypto\ \(total\)\ for.*:\ ([0-9]+\.[0-9]+)\ s ]]; then
        total_rsyncrypto_rsync_time=$(echo "$total_rsyncrypto_rsync_time + ${BASH_REMATCH[1]}" | bc)
        current_execution_time=${BASH_REMATCH[1]}
        line_count=$((line_count + 1))
    fi

    # Enc time
    if [[ "$line" =~ Total\ execution\ time\ of\ Rsyncrypto\ encryption\ for.*:\ ([0-9]+\.[0-9]+)\ s ]]; then
        total_encryption_time=$(echo "$total_encryption_time + ${BASH_REMATCH[1]}" | bc)
    fi
    # send time
    if [[ "$line" =~ Total\ execution\ time\ of\ Rsyncrypto\ rsync\ for.*:\ ([0-9]+\.[0-9]+)\ s ]]; then
        total_rsync_time=$(echo "$total_rsync_time + ${BASH_REMATCH[1]}" | bc)
    fi

    if [[ "$line_count" = 3 ]]; then
        echo "Physical size: $current_physical_size bytes, Logical size: $current_logical_size bytes, Execution time: $current_execution_time seconds"
        base_name=$(basename "$LOG_FILE")
        echo "Physical throughput: $(echo "scale=2; $current_physical_size / $current_execution_time / 1024 / 1024" | bc) MiB/s" >>"${base_name}_physical_thpt.log"
        echo "Logical throughput: $(echo "scale=2; $current_logical_size / $current_execution_time / 1024 / 1024" | bc) MiB/s" >>"${base_name}_logical_thpt.log"
        line_count=0
    fi
done <"$LOG_FILE"

echo "Total Original Size: $total_original_size bytes"
echo "Total Rsync Processed Size: $total_processed_size bytes"
echo "Total Sent Bytes: $total_sent_bytes bytes"
echo "Total Execution Time: $total_rsyncrypto_rsync_time seconds"
echo "Total Encryption Time: $total_encryption_time seconds"
echo "Total Rsync Time: $total_rsync_time seconds"
echo "Physical throughput: $(echo "scale=2; $total_sent_bytes / $total_rsyncrypto_rsync_time / 1024 / 1024" | bc) MiB/s"
echo "Logical throughput: $(echo "scale=2; $total_original_size / $total_rsyncrypto_rsync_time / 1024 / 1024" | bc) MiB/s"
reduction_ratio=$(echo "scale=10; $total_original_size / $total_sent_bytes" | bc)
printf "Reduction ratio (total): %.2f\n" "$reduction_ratio"
reduction_ratio_rsyncrypto=$(echo "scale=10; $total_original_size / $total_processed_size" | bc)
printf "Reduction ratio (rsyncrypto): %.2f\n" "$reduction_ratio_rsyncrypto"
