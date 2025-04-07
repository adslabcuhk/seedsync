#!/bin/bash
. /etc/profile
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source ${SCRIPT_DIR}/settings.sh

TARGET_ANSIBLE_HOSTS_TYPE="yml"

function checkFolders() {
    # Check if the folders exist, if not create them
    if [ ! -d "${PathToRunningLogs}" ]; then
        echo "Creating logs folder at ${PathToRunningLogs}..."
        mkdir -p "${PathToRunningLogs}"
    fi
}

function createAnsibleCfg() {
    # Create the ansible.cfg file
    if [ ! -f "ansible.cfg" ]; then
        echo "Creating ansible.cfg file..."
        cp "${SCRIPT_DIR}/ansible/ansible.cfg" ansible.cfg
    fi
}

function resetNetworkBound() {
    bandwidth=${1:-"10gbit"}
    # Reset the network bandwidth to 10gbit
    printf ${sudoPasswd} | sudo -S tc qdisc del dev ${EthInterface} root
    printf ${sudoPasswd} | sudo -S tc qdisc add dev ${EthInterface} root netem rate ${bandwidth}
}

function removeTempFilesInExp() {
    find ${SCRIPT_DIR}/exp/ -type f ! -name "*.sh" -exec rm {} +
    rm -rf ${SCRIPT_DIR}/exp/encryptedData ${SCRIPT_DIR}/exp/keyData
}

function copyWorkload() {
    # Copy the workload to the remote host
    WORKLOAD=$1
    if [ $WORKLOAD == "gcc" ]; then
        WORKLOADS=("${workloadsGCC[@]}")
    elif [ $WORKLOAD == "web" ]; then
        WORKLOADS=("${workloadsWEB[@]}")
    elif [ $WORKLOAD == "linux" ]; then
        WORKLOADS=("${workloadsLinux[@]}")
    elif [ $WORKLOAD == "cassandra" ]; then
        WORKLOADS=("${workloadsCassandra[@]}")
    elif [ $WORKLOAD == "chromium" ]; then
        WORKLOADS=("${workloadsChromium[@]}")
    elif [ $WORKLOAD == "tensorflow" ]; then
        WORKLOADS=("${workloadsTensor[@]}")
    else
        echo "Invalid workload specified. Exiting..."
        exit 1
    fi
    if [ ! -d "$PathToTraces/$WORKLOAD" ]; then
        echo "Creating directory $PathToTraces/$WORKLOAD"
        mkdir -p $PathToTraces/$WORKLOAD
    fi

    for current in "${WORKLOADS[@]}"; do
        echo "Copying $current ..."
        cp -r /mnt/old/home/yjren/Data/${WORKLOAD}/$current $PathToTraces/${WORKLOAD}/$current
    done
}

function copyLogs() {
    workload=$1
    system=$2
    cacheSize=${3:-512}
    bandwidth=${4:-"10gbit"}

    echo "Copying logs for ${system} system with ${workload} workloads..."
    # Ensure required variables are set
    if [[ -z "${PathToPlainSyncPrototype}" || -z "${PathToSyncPrototype}" || -z "${PathToRunningLogs}" || -z "${RemoteHostForRsyncrypto}" ]]; then
        echo "Error: Required environment variables are not set."
        exit 1
    fi

    # Log file sets
    plainFileSet=("${PathToPlainSyncPrototype}/bin/Src-Log" "${PathToPlainSyncPrototype}/bin/Dest-Log")
    shieldFileSet=("${PathToSyncPrototype}/bin/Src-Log" "${PathToSyncPrototype}/bin/Dest-Log")
    plainDedupFileSet=("${PathToPlainDedupPrototype}/bin/server-log" "${PathToPlainDedupPrototype}/bin/client-log")
    shieldDedupFileSet=("${PathToDBEPrototype}/bin/server-log" "${PathToDBEPrototype}/bin/client-log")

    # Function to copy logs
    copy_and_scp_logs() {
        local files=("${@:1:$#-2}")
        local prefix=${@: -2:1}
        local extra=${@: -1}
        for file in "${files[@]}"; do
            timestamp=$(date +%Y%m%d-%H%M%S-%3N)
            dest_local="${PathToRunningLogs}/${prefix}-$(basename "${file}")-${workload}-${extra}-localhost-${timestamp}"
            dest_remote="${PathToRunningLogs}/${prefix}-$(basename "${file}")-${workload}-${extra}-remotehost-${timestamp}"
            if [[ -f "${file}" ]]; then
                cp "${file}" "${dest_local}"
            else
                echo "Warning: Target file ${file} (localhost) not found. Continue..."
            fi
            if ! scp "${RemoteHostForRsyncrypto}:${file}" "${dest_remote}"; then
                echo "Warning: Target file ${file} (remotehost) not found. Continue..."
            fi
        done
    }

    # Handle "Plain" system logs
    if [[ "${system}" == "Plain" ]]; then
        copy_and_scp_logs "${plainFileSet[@]}" "PlainSync" "Bandwidth-${bandwidth}"
        copy_and_scp_logs "${plainDedupFileSet[@]}" "PlainDedup" "Bandwidth-${bandwidth}"
    # Handle "SeedSync" system logs
    elif [[ "${system}" == "SeedSync" ]]; then
        copy_and_scp_logs "${shieldFileSet[@]}" "SeedSync" "Cache-${cacheSize}-Bandwidth-${bandwidth}"
        copy_and_scp_logs "${shieldDedupFileSet[@]}" "DBE" "Cache-${cacheSize}-Bandwidth-${bandwidth}"
    # Handle invalid "system" input
    elif [[ "${system}" == "DBE" ]]; then
        copy_and_scp_logs "${shieldDedupFileSet[@]}" "DBE" "Cache-${cacheSize}-Bandwidth-${bandwidth}"
    elif [[ "${system}" == "Rsyncrypto" || "${system}" == "Rsyncrypto" ]]; then
        echo "Warning: Rsyncrypto and Rsync are not supported for log copying."
    else
        echo "Error: Unknown system type '${system}'."
    fi
}

function setupCodebase() {
    system=$1
    cacheSize=${2:-512}
    # Modify the playbook to setup the LogName
    if [ -f "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" ]; then
        echo "Renew hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        rm -rf "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    else
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    fi
    if [ -f "playbook-setup.yml" ]; then
        echo "Renew playbook"
        rm -rf playbook-setup.yml
        cp "${SCRIPT_DIR}/ansible/playbook-setup.yml" playbook-setup.yml
    else
        cp "${SCRIPT_DIR}/ansible/playbook-setup.yml" playbook-setup.yml
    fi

    sed -i "s|SYSTEM_NAME|${system}|g" playbook-setup.yml
    sed -i "s|CACHE_SIZE|${cacheSize}|g" playbook-setup.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-setup.yml

    # Run the playbook for dbe load first
    echo "Setting codebase..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-setup.yml" || true
}

function runSeedSync() {
    workload=$1
    cacheSize=${2:-512}
    bandwidth=${3:-"10gbit"}
    LogNameForDBE="${PathToRunningLogs}/DBE-${workload}-Cache-${cacheSize}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S)"
    LogNameForSync="${PathToRunningLogs}/SeedSync-${workload}-Cache-${cacheSize}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S)"
    # Modify the playbook to setup the LogName
    if [ -f "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" ]; then
        echo "Renew hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        rm -rf "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    else
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    fi
    if [ -f "playbook-dbe.yml" ] || [ -f "playbook-sync.yml" ]; then
        echo "Renew playbook"
        rm -rf playbook-dbe.yml playbook-sync.yml
        cp "${SCRIPT_DIR}/ansible/playbook-dbe.yml" playbook-dbe.yml
        cp "${SCRIPT_DIR}/ansible/playbook-sync.yml" playbook-sync.yml
    else
        cp "${SCRIPT_DIR}/ansible/playbook-dbe.yml" playbook-dbe.yml
        cp "${SCRIPT_DIR}/ansible/playbook-sync.yml" playbook-sync.yml
    fi
    sed -i "s|LogName: .*|LogName: ${LogNameForDBE}|g" playbook-dbe.yml
    sed -i "s|LogName: .*|LogName: ${LogNameForSync}|g" playbook-sync.yml
    sed -i "s|Workloads: .*|Workloads: ${workload}|g" playbook-dbe.yml
    sed -i "s|Workloads: .*|Workloads: ${workload}|g" playbook-sync.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-dbe.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-sync.yml

    # Run the playbook for dbe load first
    echo "Starting DBE load for ${workload} workloads..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-dbe.yml" || true
    # Run the playbook for sync
    echo "Starting sync for ${workload} workloads..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-sync.yml" || true
}

function runOriginalDBE() {
    workload=$1
    cacheSize=${2:-512}
    bandwidth=${3:-"10gbit"}
    LogNameForDBE="${PathToRunningLogs}/DBE-${workload}-Cache-${cacheSize}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S)"
    # Modify the playbook to setup the LogName
    if [ -f "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" ]; then
        echo "Renew hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        rm -rf "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    else
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    fi
    if [ -f "playbook-dbe.yml" ]; then
        echo "Renew playbook"
        rm -rf playbook-dbe.yml
        cp "${SCRIPT_DIR}/ansible/playbook-dbe.yml" playbook-dbe.yml
    else
        cp "${SCRIPT_DIR}/ansible/playbook-dbe.yml" playbook-dbe.yml
    fi
    sed -i "s|LogName: .*|LogName: ${LogNameForDBE}|g" playbook-dbe.yml
    sed -i "s|Workloads: .*|Workloads: ${workload}|g" playbook-dbe.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-dbe.yml

    # Run the playbook for dbe load first
    echo "Starting DBE load for ${workload} workloads..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-dbe.yml" || true
}

function runPlain() {
    workload=$1
    bandwidth=${2:-"10gbit"}
    LogNameForDBE="${PathToRunningLogs}/PlainDedup-${workload}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S)"
    LogNameForSync="${PathToRunningLogs}/PlainSync-${workload}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S)"
    # Modify the playbook to setup the LogName
    if [ -f "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" ]; then
        echo "Renew hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        rm -rf "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    else
        cp "${SCRIPT_DIR}/ansible/hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}"
    fi
    if [ -f "playbook-plain-dedup.yml" ] || [ -f "playbook-plain-sync.yml" ]; then
        echo "Renew playbook"
        rm -rf playbook-plain-dedup.yml playbook-plain-sync.yml
        cp "${SCRIPT_DIR}/ansible/playbook-plain-dedup.yml" playbook-plain-dedup.yml
        cp "${SCRIPT_DIR}/ansible/playbook-plain-sync.yml" playbook-plain-sync.yml
    else
        cp "${SCRIPT_DIR}/ansible/playbook-plain-dedup.yml" playbook-plain-dedup.yml
        cp "${SCRIPT_DIR}/ansible/playbook-plain-sync.yml" playbook-plain-sync.yml
    fi
    sed -i "s|LogName: .*|LogName: ${LogNameForDBE}|g" playbook-plain-dedup.yml
    sed -i "s|LogName: .*|LogName: ${LogNameForSync}|g" playbook-plain-sync.yml
    sed -i "s|Workloads: .*|Workloads: ${workload}|g" playbook-plain-dedup.yml
    sed -i "s|Workloads: .*|Workloads: ${workload}|g" playbook-plain-sync.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-plain-dedup.yml
    sed -i "s|PATH_TO_RUN_SCRIPTS|${PathToScripts}|g" playbook-plain-sync.yml

    # Run the playbook for dbe load first
    echo "Starting DBE load for ${workload} workloads..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-plain-dedup.yml" || true
    # Run the playbook for sync
    echo "Starting sync for ${workload} workloads..."
    ansible-playbook -i "hosts.${TARGET_ANSIBLE_HOSTS_TYPE}" "playbook-plain-sync.yml" || true
}

function createFileList() {
    # Create file list path
    workload=$1
    FILE_LIST_PATH="${workload}_versions.list"

    workloadsSet=()
    if [ "${workload}" == "gcc" ]; then
        workloadsSet=("${workloadsGCC[@]}")
    elif [ "${workload}" == "web" ]; then
        workloadsSet=("${workloadsWEB[@]}")
    elif [ "${workload}" == "linux" ]; then
        workloadsSet=("${workloadsLinux[@]}")
    elif [ "${workload}" == "cassandra" ]; then
        workloadsSet=("${workloadsCassandra[@]}")
    elif [ "${workload}" == "chromium" ]; then
        workloadsSet=("${workloadsChromium[@]}")
    elif [ "${workload}" == "tensorflow" ]; then
        workloadsSet=("${workloadsTensor[@]}")
    else
        echo "Invalid workload specified. Exiting..."
        exit 1
    fi

    if [ -f "$FILE_LIST_PATH" ]; then
        echo "File list $FILE_LIST_PATH already exists, deleting..."
        rm "$FILE_LIST_PATH"
    fi

    for file in "${workloadsSet[@]}"; do
        # Construct the full path of the GCC tar file
        FULL_PATH="$PathToTraces/$workload/$file"
        echo "Checking $FULL_PATH..."
        # Write the full path to the file list if the file exists
        if [ -f "$FULL_PATH" ]; then
            echo "$FULL_PATH" >>"$FILE_LIST_PATH"
        else
            echo "Warning: $FULL_PATH does not exist, skipping..."
        fi
    done

    echo "File list created successfully at $FILE_LIST_PATH."
}

function runRsyncrypto() {
    # Assign parameters to variables
    WORKLOAD=$1
    bandwidth=${2:-"10gbit"}
    FILELIST="${WORKLOAD}_versions.list"
    ENC_DIR="./encryptedData"
    KEY_DIR="./keyData"
    REMOTE_HOST="${RemoteHostForRsyncrypto}:~/"
    CERT_FILE="backup.crt"
    LOGFILE="${PathToRunningLogs}/Rsyncrypto-${WORKLOAD}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S).log"

    if command -v rsyncrypto >/dev/null 2>&1; then
        echo "rsyncrypto is installed."
    else
        echo "rsyncrypto is not installed. Please install it before proceeding."
        exit 1
    fi

    # Create keys
    if [ ! -f backup.key ] || [ ! -f backup.crt ]; then
        echo "Creating backup.key and backup.crt for rsyncrypto..." | tee -a "$LOGFILE"
        openssl req -nodes -newkey rsa:2048 -x509 -keyout backup.key -out backup.crt -subj "/C=/ST=/L=/O=/CN="
    fi

    # Check if folder exists, if not create it
    echo "Creating directories $ENC_DIR and $KEY_DIR..."
    if [ ! -d "$ENC_DIR" ]; then
        mkdir -p "$ENC_DIR"
        printf ${sudoPasswd} | sudo -S mount -t tmpfs -o size=10G tmpfs "$ENC_DIR"
    else
        if mountpoint -q "$ENC_DIR"; then
            printf ${sudoPasswd} | sudo -S umount "$ENC_DIR"
        fi
        rm -rf "$ENC_DIR"
        mkdir -p "$ENC_DIR"
        printf ${sudoPasswd} | sudo -S mount -t tmpfs -o size=10G tmpfs "$ENC_DIR"
    fi
    rm -rf "$KEY_DIR"
    mkdir -p "$KEY_DIR"

    # Start the process
    echo "Logfile: $LOGFILE"
    echo "Starting encryption and synchronization process..." | tee -a "$LOGFILE"

    # Check if filelist exists
    if [ ! -f "$FILELIST" ]; then
        echo "Filelist $FILELIST does not exist." | tee -a "$LOGFILE"
        exit 1
    fi

    # Read the filelist and encrypt each file
    echo "Encrypting files from $FILELIST..." | tee -a "$LOGFILE"
    CURRENT_PROCESS_NAME="evaluateFileForRsyncrypto.enc"
    # Remove the previous files in the remote host
    ssh $RemoteHostForRsyncrypto "rm -rf ~/$CURRENT_PROCESS_NAME"
    # Check if the command was successful
    if [ $? -eq 0 ]; then
        echo "Successfully removed files in the remote host."
    else
        echo "Failed to remove files in the remote host, it may not exist." >&2
    fi
    # Start the encryption and synchronization process
    while IFS= read -r FILE; do
        if [ -f "$FILE" ]; then
            echo "Encrypting $FILE to $ENC_DIR/$CURRENT_PROCESS_NAME..." | tee -a "$LOGFILE"
            if [ -f "$ENC_DIR/$CURRENT_PROCESS_NAME" ]; then
                echo "Encrypted file $ENC_DIR/$CURRENT_PROCESS_NAME already exists, delete first..." | tee -a "$LOGFILE"
                rm "$ENC_DIR/$CURRENT_PROCESS_NAME"
            fi

            # start time recording

            # bash ${SCRIPT_DIR}/status/runStats.sh "rsyncrypto" "${PathToRunningLogs}/Rsyncrypto_${WORKLOAD}_$(date +%Y%m%d-%H%M%S)-crypto" &
            # bash ${SCRIPT_DIR}/status/runStats.sh "rsync" "${PathToRunningLogs}/Rsyncrypto_${WORKLOAD}_$(date +%Y%m%d-%H%M%S)-rsync" &

            START_TIME=$(date +%s%N)
            # Rsyncrypto encryption
            ${PathToRsyncrypto}/rsyncrypto "$FILE" "$ENC_DIR/$CURRENT_PROCESS_NAME" "$KEY_DIR/$CURRENT_PROCESS_NAME.key" "$CERT_FILE"
            END_TIME_ENC=$(date +%s%N)

            if [ $? -ne 0 ]; then
                echo "Failed to encrypt $FILE" | tee -a "$LOGFILE"
            else
                echo "Successfully encrypted $FILE, size: $(du --bytes "$ENC_DIR/$CURRENT_PROCESS_NAME" | awk '{print $1}')" | tee -a "$LOGFILE"
                echo "Original file size: $(du --bytes "$FILE" | awk '{print $1}')" | tee -a "$LOGFILE"
                # Sync encrypted files to remote host
                echo "Syncing encrypted files to $REMOTE_HOST..." | tee -a "$LOGFILE"
                rsync -avz --stats --compress --compress-choice=none --temp-dir=${PathToRsyncTempFolder} "$ENC_DIR/$CURRENT_PROCESS_NAME" "$REMOTE_HOST" | tee -a "$LOGFILE"

                if [ $? -eq 0 ]; then
                    echo "Syncing to $REMOTE_HOST completed successfully." | tee -a "$LOGFILE"
                else
                    echo "Failed to sync files to $REMOTE_HOST." | tee -a "$LOGFILE"
                    exit 1
                fi

                # End time recording
                END_TIME=$(date +%s%N)
                DURATION_TOTAL=$((END_TIME - START_TIME))
                DURATION_RESYNCRYPTO=$((END_TIME_ENC - START_TIME))
                DURATION_RESYNC=$((END_TIME - END_TIME_ENC))
                SECONDS_TOTAL=$((DURATION_TOTAL / 1000000000))
                NANOSECONDS_TOTAL=$((DURATION_TOTAL % 1000000000))
                SECONDS_RESYNCRYPTO=$((DURATION_RESYNCRYPTO / 1000000000))
                NANOSECONDS_RESYNCRYPTO=$((DURATION_RESYNCRYPTO % 1000000000))
                SECONDS_RESYNC=$((DURATION_RESYNC / 1000000000))
                NANOSECONDS_RESYNC=$((DURATION_RESYNC % 1000000000))

                # Print total execution time
                TIME_FORMATTED_TOTAL=$(printf "%d.%09d s" "$SECONDS_TOTAL" "$NANOSECONDS_TOTAL")
                TIME_FORMATTED_RESYNCRYPTO=$(printf "%d.%09d s" "$SECONDS_RESYNCRYPTO" "$NANOSECONDS_RESYNCRYPTO")
                TIME_FORMATTED_RESYNC=$(printf "%d.%09d s" "$SECONDS_RESYNC" "$NANOSECONDS_RESYNC")
                echo "Total execution time of Rsyncrypto (total) for $FILE: $TIME_FORMATTED_TOTAL" | tee -a "$LOGFILE"
                echo "Total execution time of Rsyncrypto encryption for $FILE: $TIME_FORMATTED_RESYNCRYPTO" | tee -a "$LOGFILE"
                echo "Total execution time of Rsyncrypto rsync for $FILE: $TIME_FORMATTED_RESYNC" | tee -a "$LOGFILE"
            fi
        else
            echo "File $FILE does not exist, skipping..." | tee -a "$LOGFILE"
        fi
    done <"$FILELIST"
    if mountpoint -q "$ENC_DIR"; then
        printf ${sudoPasswd} | sudo -S umount "$ENC_DIR"
    fi
}

function runRsync() {
    # Assign parameters to variables
    WORKLOAD=$1
    bandwidth=${2:-"10gbit"}
    FILELIST="${WORKLOAD}_versions.list"
    REMOTE_HOST="${RemoteHostForRsyncrypto}:~/"
    LOGFILE="${PathToRunningLogs}/Rsync-${WORKLOAD}-Bandwidth-${bandwidth}-$(date +%Y%m%d-%H%M%S).log"

    if command -v rsync >/dev/null 2>&1; then
        echo "rsync is installed."
    else
        echo "rsync is not installed. Please install it before proceeding."
        exit 1
    fi

    # Start the process
    echo "Logfile: $LOGFILE"
    echo "Starting synchronization process..." | tee -a "$LOGFILE"

    # Check if filelist exists
    if [ ! -f "$FILELIST" ]; then
        echo "Filelist $FILELIST does not exist." | tee -a "$LOGFILE"
        exit 1
    fi

    # Read the filelist and encrypt each file
    echo "Copy files from $FILELIST..." | tee -a "$LOGFILE"
    CURRENT_PROCESS_NAME="evaluateFileForRsync"
    # Remove the previous files in the remote host
    ssh $RemoteHostForRsyncrypto "rm -rf ~/$CURRENT_PROCESS_NAME"
    # Check if the command was successful
    if [ $? -eq 0 ]; then
        echo "Successfully removed files in the remote host."
    else
        echo "Failed to remove files in the remote host, it may not exist." >&2
    fi
    # Start the encryption and synchronization process
    while IFS= read -r FILE; do
        if [ -f "$FILE" ]; then
            echo "Copying $FILE to $CURRENT_PROCESS_NAME..." | tee -a "$LOGFILE"
            if [ -f "$CURRENT_PROCESS_NAME" ]; then
                echo "Encrypted file $CURRENT_PROCESS_NAME already exists, delete first..." | tee -a "$LOGFILE"
                rm "$CURRENT_PROCESS_NAME"
            fi
            cp "$FILE" "$CURRENT_PROCESS_NAME" | tee -a "$LOGFILE"

            # start time recording

            # bash ${SCRIPT_DIR}/status/runStats.sh "rsyncrypto" "${PathToRunningLogs}/Rsyncrypto_${WORKLOAD}_$(date +%Y%m%d-%H%M%S)-crypto" &
            # bash ${SCRIPT_DIR}/status/runStats.sh "rsync" "${PathToRunningLogs}/Rsyncrypto_${WORKLOAD}_$(date +%Y%m%d-%H%M%S)-rsync" &

            START_TIME=$(date +%s%N)

            # Sync files to remote host
            echo "Syncing files to $REMOTE_HOST..." | tee -a "$LOGFILE"
            rsync -avz --stats --compress --compress-choice=lz4 --temp-dir=${PathToRsyncTempFolder} "$CURRENT_PROCESS_NAME" "$REMOTE_HOST" | tee -a "$LOGFILE"

            if [ $? -eq 0 ]; then
                echo "Syncing to $REMOTE_HOST completed successfully." | tee -a "$LOGFILE"
            else
                echo "Failed to sync files to $REMOTE_HOST." | tee -a "$LOGFILE"
                exit 1
            fi

            # End time recording
            END_TIME=$(date +%s%N)
            DURATION=$((END_TIME - START_TIME))
            SECONDS=$((DURATION / 1000000000))
            NANOSECONDS=$((DURATION % 1000000000))

            # Print total execution time
            echo "Original file size: $(du --bytes "$FILE" | awk '{print $1}')" | tee -a "$LOGFILE"
            TIME_FORMATTED=$(printf "%d.%09d s" "$SECONDS" "$NANOSECONDS")
            echo "Total execution time of Rsync for $FILE: $TIME_FORMATTED" | tee -a "$LOGFILE"
        fi
    done <"$FILELIST"
}

# Default runing

checkFolders
