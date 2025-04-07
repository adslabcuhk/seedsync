#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

LogName=${1:-DEBEServer.log}
LogName=${LogName}-Server.log

echo "LogName: ${LogName}"
kill -9 $(ps aux | grep SeedSync | grep -v grep | awk 'NR == 1' | awk {'print $2'})
kill -9 $(ps aux | grep DEBEServer | grep -v grep | awk 'NR == 1' | awk {'print $2'})

cd ${PathToDBEPrototype} || exit
cd bin || exit

echo "Start runing the DEBEServer"
nohup ./DEBEServer -m 4 >"${LogName}" 2>&1 &
