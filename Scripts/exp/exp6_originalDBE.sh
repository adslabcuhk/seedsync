#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

workloadSet=("gcc" "tensorflow" "cassandra" "chromium" "linux")
system="DBE"
cacheSizeSet=(512)
roundNumber=5
createAnsibleCfg

for round in $(seq 1 "${roundNumber}"); do
    echo "Start running round ${round}"
    for workload in "${workloadSet[@]}"; do
        for cacheSize in "${cacheSizeSet[@]}"; do
            echo "Running ${system} with ${workload} and cache size ${cacheSize}"
            setupCodebase "${system}" "${cacheSize}"
            runOriginalDBE "${workload}" "${cacheSize}"
            # break # Remove this line to run all workloads
            echo "Copying logs for ${system} with ${workload} and cache size ${cacheSize}"
            copyLogs "${workload}" "${system}" "${cacheSize}"
        done
        # break # Remove this line to run all systems
    done
done

pkill -f runStats.sh
removeTempFilesInExp
