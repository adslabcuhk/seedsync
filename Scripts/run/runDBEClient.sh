#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

Workloads=$1
LogName="${2}-Client.log"

function startDEBEClient {
    workload=$1
    kill -9 $(ps aux | grep DEBEClient | grep -v grep | awk 'NR == 1' | awk {'print $2'})
    sleep 2

    cd ${PathToDBEPrototype} || exit
    cd bin || exit
    echo "DEBEClient ${workload} start." >>${LogName}
    ./DEBEClient ./DEBEClient -t u -i "${PathToTraces}/${Workloads}/${workload}" >>${LogName} 2>&1
    echo "DEBEClient ${workload} end." >>${LogName}
}

if [ -z "$Workloads" ]; then
    echo "No workload specified. Exiting..." >>${LogName}
    exit 1
elif [ "$Workloads" == "gcc" ]; then
    echo "Starting gcc workloads..." >>${LogName}
    for workload in "${workloadsGCC[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "gcc workloads completed." >>${LogName}
elif [ "$Workloads" == "web" ]; then
    echo "Starting web workloads..." >>${LogName}
    for workload in "${workloadsWEB[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "web workloads completed." >>${LogName}
elif [ "$Workloads" == "linux" ]; then
    echo "Starting linux workloads..." >>${LogName}
    for workload in "${workloadsLinux[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "linux workloads completed." >>${LogName}
elif [ "$Workloads" == "cassandra" ]; then
    echo "Starting cassandra workloads..." >>${LogName}
    for workload in "${workloadsCassandra[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "cassandra workloads completed." >>${LogName}
elif [ "$Workloads" == "tensorflow" ]; then
    echo "Starting tensorflow workloads..." >>${LogName}
    for workload in "${workloadsTensor[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "tensorflow workloads completed." >>${LogName}
elif [ "$Workloads" == "chromium" ]; then
    echo "Starting chromium workloads..." >>${LogName}
    for workload in "${workloadsChromium[@]}"; do
        startDEBEClient "${workload}"
    done
    echo "chromium workloads completed." >>${LogName}
else
    echo "Invalid workload specified. Exiting..." >>${LogName}
    exit 1
fi
