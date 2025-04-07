#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

Workloads=$1
LogName="${2}-Client.log"

function startSyncClient {
    workload=$1

    cd ${PathToPlainSyncPrototype} || exit
    cd bin || exit
    echo "SyncClient ${workload} start." >>${LogName}
    ./Plain -t s -i "${PathToTraces}/${Workloads}/${workload}" >>${LogName} 2>&1
    echo "SyncClient ${workload} end." >>${LogName}
}

if [ -z "$Workloads" ]; then
    echo "No workload specified. Exiting..." >>${LogName}
    exit 1
elif [ "$Workloads" == "gcc" ]; then
    echo "Starting gcc workloads..." >>${LogName}
    for workload in "${workloadsGCC[@]}"; do
        startSyncClient "${workload}"
    done
    echo "gcc workloads completed." >>${LogName}
elif [ "$Workloads" == "web" ]; then
    echo "Starting web workloads..." >>${LogName}
    for workload in "${workloadsWEB[@]}"; do
        startSyncClient "${workload}"
    done
    echo "web workloads completed." >>${LogName}
elif [ "$Workloads" == "linux" ]; then
    echo "Starting linux workloads..." >>${LogName}
    for workload in "${workloadsLinux[@]}"; do
        startSyncClient "${workload}"
    done
    echo "linux workloads completed." >>${LogName}
elif [ "$Workloads" == "cassandra" ]; then
    echo "Starting cassandra workloads..." >>${LogName}
    for workload in "${workloadsCassandra[@]}"; do
        startSyncClient "${workload}"
    done
    echo "cassandra workloads completed." >>${LogName}
elif [ "$Workloads" == "tensorflow" ]; then
    echo "Starting tensorflow workloads..." >>${LogName}
    for workload in "${workloadsTensor[@]}"; do
        startSyncClient "${workload}"
    done
    echo "tensorflow workloads completed." >>${LogName}
elif [ "$Workloads" == "chromium" ]; then
    echo "Starting chromium workloads..." >>${LogName}
    for workload in "${workloadsChromium[@]}"; do
        startSyncClient "${workload}"
    done
    echo "chromium workloads completed." >>${LogName}
else
    echo "Invalid workload specified. Exiting..." >>${LogName}
    exit 1
fi
