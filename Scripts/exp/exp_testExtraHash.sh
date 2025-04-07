#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

workloadSet=("gcc" "tensorflow" "cassandra" "chromium" "linux")
systemSet=("SeedSync")
cacheSizeSet=(512)
roundNumber=1
createAnsibleCfg

for round in $(seq 1 "${roundNumber}"); do
    echo "Start running round ${round}"
    for workload in "${workloadSet[@]}"; do
        for system in "${systemSet[@]}"; do
            for cacheSize in "${cacheSizeSet[@]}"; do
                echo "Running ${system} with ${workload} and cache size ${cacheSize}"
                setupCodebase "${system}" "${cacheSize}"
                if [ "${system}" == "Rsyncrypto" ]; then
                    createFileList "${workload}"
                    runRsyncrypto "${workload}"
                elif [ "${system}" == "SeedSync" ]; then
                    runSeedSync "${workload}" "${cacheSize}"
                elif [ "${system}" == "Plain" ]; then
                    runPlain "${workload}"
                elif [ "${system}" == "Rsync" ]; then
                    createFileList "${workload}"
                    runRsync "${workload}"
                fi
                # break # Remove this line to run all workloads
                echo "Copying logs for ${system} with ${workload} and cache size ${cacheSize}"
                copyLogs "${workload}" "${system}" "${cacheSize}"
            done
        done
        # break # Remove this line to run all systems
    done
done

pkill -f runStats.sh
removeTempFilesInExp
