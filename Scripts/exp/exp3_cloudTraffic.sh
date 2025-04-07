#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

workloadSet=("linux")                                 # "tensorflow" "cassandra" "chromium" "linux" "gcc") # "web"
systemSet=("SeedSync" "Rsync" "Plain" "Rsyncrypto") #  "SeedSync"  "Plain")
bandwidth="100mbit"
cacheSize=512
roundNumber=5

createAnsibleCfg
resetNetworkBound "${bandwidth}"
for round in $(seq 1 "${roundNumber}"); do
    echo "Start running round ${round}"
    for system in "${systemSet[@]}"; do
        for workload in "${workloadSet[@]}"; do
            setupCodebase "${system}" "${cacheSize}"
            if [ "${system}" == "Rsyncrypto" ]; then
                createFileList "${workload}"
                runRsyncrypto "${workload}" "${bandwidth}"
            elif [ "${system}" == "SeedSync" ]; then
                runSeedSync "${workload}" "${cacheSize}" "${bandwidth}"
            elif [ "${system}" == "Plain" ]; then
                runPlain "${workload}" "${bandwidth}"
            elif [ "${system}" == "Rsync" ]; then
                createFileList "${workload}"
                runRsync "${workload}" "${bandwidth}"
            fi
            # break # Remove this line to run all workloads
            copyLogs "${workload}" "${system}" "${cacheSize}" "${bandwidth}"
        done
        # break # Remove this line to run all systems
    done
    # break # Remove this line to run all bandwidths
done

pkill -f runStats.sh
removeTempFilesInExp
