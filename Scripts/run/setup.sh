#!/bin/bash
. /etc/profile

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

# Function to kill a process by name and check if it was successful
systemName=$1
cacheSize=$2

bash ${SCRIPT_DIR}/run/stopSyncServer.sh
bash ${SCRIPT_DIR}/run/stopDBEServer.sh

# printf ${sudoPasswd} | sudo -S bash ${SCRIPT_DIR}/run/dropCache.sh

rm -rf ${PathToRsyncTempFolder}
echo "Removed ${PathToRsyncTempFolder}"
mkdir -p ${PathToRsyncTempFolder}
echo "Created ${PathToRsyncTempFolder}"

# Perform a git pull and capture the output
if [ ! $systemName == "DBE" ]; then
    cd ${PathToArtifact} || exit
    output=$(git pull 2>&1)
    # Check the exit status of git pull
    if [ $? -eq 0 ]; then
        echo "Successfully pulled the latest changes:"
        echo "${output}"
    else
        echo "Failed to pull changes:"
        echo "${output}"
        exit 0
    fi
else
    cd ${PathToDBEPrototype} || exit
fi

if [ $systemName == "Rsync" ] || [ $systemName == "Rsyncrypto" ]; then
    echo "No need to setup codebase for Rsync and Rsyncrypto"
    exit 0
elif [ $systemName == "Plain" ]; then
    if [ -d "${PathToPlainDedupPrototype}" ]; then
        cd ${PathToPlainDedupPrototype} || exit
        ./setup.sh
    else
        echo "${PathToPlainDedupPrototype} does not exist"
    fi

    if [ -d "${PathToPlainSyncPrototype}" ]; then
        cd ${PathToPlainSyncPrototype} || exit
        ./setup.sh
    else
        echo "${PathToPlainSyncPrototype} does not exist"
    fi
elif [ $systemName == "SeedSync" ]; then
    if [ -d "${PathToDBEPrototype}" ]; then
        cd ${PathToDBEPrototype} || exit
        ./setup.sh
    else
        echo "${PathToDBEPrototype} does not exist"
    fi

    if [ -d "${PathToSyncPrototype}" ]; then
        cd ${PathToSyncPrototype} || exit
        # May need to modify the cache size
        ./setup.sh
        cd ${PathToSyncPrototype}/bin || exit
        sed -i "s/\"enclave_cache_item\": *512/\"enclave_cache_item\": ${cacheSize}/" sync_config.json
    else
        echo "${PathToSyncPrototype} does not exist"
    fi
elif [ $systemName == "DBE" ]; then
    if [ -d "${PathToDBEPrototype}" ]; then
        cd ${PathToDBEPrototype} || exit
        ./setup.sh
    else
        echo "${PathToDBEPrototype} does not exist"
    fi
else
    echo "Unknown system name: ${systemName}"
    exit 0
fi
