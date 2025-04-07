#!/bin/bash
. /etc/profile

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
source "${SCRIPT_DIR}/../common.sh"

# Function to kill a process by name and check if it was successful
kill_process() {
    local process_name=$1
    # Get process IDs
    pids=$(pgrep -f "$process_name")

    if [[ -z "$pids" ]]; then
        echo "No process found for $process_name"
    else
        # Send SIGINT (Ctrl+C equivalent) to the processes
        kill -2 $pids
        if [[ $? -eq 0 ]]; then
            echo "Successfully sent SIGINT to $process_name processes with PIDs: $pids"
        else
            echo "Failed to send SIGINT to $process_name processes."
        fi
    fi
}

# Kill runDBEServer.sh and DEBEServer processes
kill_process "DEBEServer"
