#!/bin/bash
. /etc/profile

PROCESS_NAME=$1
LOG_PATH=$2
INTERVAL=1
ETH_DEVICE="ens3f0np0"
LOGFILE="${LOG_PATH}-resource.log"

get_main_pid() {
    echo $(pgrep -o -x $PROCESS_NAME)
}

# Count when the process is terminated
log_latest_stats() {
    TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

    CPU_BEFORE=($(grep 'cpu ' /proc/stat))
    PID_STAT_BEFORE=($(grep ' ' /proc/$MAIN_PID/stat))
    IO_STAT_BEFORE_READ=$(awk '/rchar/ {print $2}' /proc/$MAIN_PID/io)
    IO_STAT_BEFORE_WRITE=$(awk '/wchar/ {print $2}' /proc/$MAIN_PID/io)
    sleep ${INTERVAL}
    CPU_AFTER=($(grep 'cpu ' /proc/stat))
    PID_STAT_AFTER=($(grep ' ' /proc/$MAIN_PID/stat))
    IO_STAT_AFTER_READ=$(awk '/rchar/ {print $2}' /proc/$MAIN_PID/io)
    IO_STAT_AFTER_WRITE=$(awk '/wchar/ {print $2}' /proc/$MAIN_PID/io)

    CPU_USAGE=$(awk -v b_utime=${PID_STAT_BEFORE[13]} -v b_stime=${PID_STAT_BEFORE[14]} \
        -v a_utime=${PID_STAT_AFTER[13]} -v a_stime=${PID_STAT_AFTER[14]} \
        -v b_total=${CPU_BEFORE[1]} -v b_nice=${CPU_BEFORE[2]} \
        -v b_system=${CPU_BEFORE[3]} -v b_idle=${CPU_BEFORE[4]} \
        -v a_total=${CPU_AFTER[1]} -v a_nice=${CPU_AFTER[2]} \
        -v a_system=${CPU_AFTER[3]} -v a_idle=${CPU_AFTER[4]} \
        'BEGIN {
                        total_before = b_total + b_nice + b_system + b_idle;
                        total_after = a_total + a_nice + a_system + a_idle;
                        process_before = b_utime + b_stime;
                        process_after = a_utime + a_stime;
                        cpu_usage = (process_after - process_before) / (total_after - total_before) * 100;
                        print cpu_usage;
                     }')

    RSS=$(awk '/VmRSS/ {print $2}' /proc/$MAIN_PID/status)

    CPU_TIME=$((${PID_STAT_AFTER[13]} + ${PID_STAT_AFTER[14]}))
    HZ=$(getconf CLK_TCK)
    CPU_TIME_SECONDS=$(awk -v jiffies=$CPU_TIME -v hz=$HZ 'BEGIN {print jiffies / hz}')

    IO_READ=$(((IO_STAT_AFTER_READ - IO_STAT_BEFORE_READ) / 1024))
    IO_WRITE=$(((IO_STAT_AFTER_WRITE - IO_STAT_BEFORE_WRITE) / 1024))

    NET_STATS_AFTER=($(get_network_stats))
    RX_AFTER=${NET_STATS_AFTER[0]}
    TX_AFTER=${NET_STATS_AFTER[1]}

    NET_RX=$((RX_AFTER - RX_BEFORE))
    NET_TX=$((TX_AFTER - TX_BEFORE))

    RX_BEFORE=$RX_AFTER
    TX_BEFORE=$TX_AFTER

    echo "$TIMESTAMP, $CPU_USAGE, $RSS, $CPU_TIME, $CPU_TIME_SECONDS, $IO_READ, $IO_WRITE, $IO_STAT_AFTER_READ, $IO_STAT_AFTER_WRITE, $NET_RX, $NET_TX, $RX_AFTER, $TX_AFTER" >>$LOGFILE
}

trap log_latest_stats SIGTERM SIGINT

MAIN_PID=$(get_main_pid)
while [ -z "$MAIN_PID" ]; do
    echo "Waiting for $PROCESS_NAME to start..."
    sleep 1
    MAIN_PID=$(get_main_pid)
done

START_TIME=$(date +"%Y-%m-%d %H:%M:%S")
echo "$PROCESS_NAME started with MAIN_PID: $MAIN_PID at $START_TIME" >$LOGFILE

echo "Timestamp, CPU (%), Memory (RSS KiB), CPU Time (jiffies), CPU Time (Sec), I/O Read (KiB), I/O Write (KiB), Total I/O Read (B), Total I/O Write (B), Net RX (B), Net TX (B), Net RX Total (B), Net TX Total (B)" >>$LOGFILE

get_network_stats() {
    RX=$(cat /proc/$MAIN_PID/net/dev | awk '/$ETH_DEVICE/ {print $2}')
    TX=$(cat /proc/$MAIN_PID/net/dev | awk '/$ETH_DEVICE/ {print $10}')
    echo "$RX $TX"
}

NET_STATS_BEFORE=($(get_network_stats))
RX_BEFORE=${NET_STATS_BEFORE[0]}
TX_BEFORE=${NET_STATS_BEFORE[1]}

while [ -d /proc/$MAIN_PID ]; do
    TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

    CPU_BEFORE=($(grep 'cpu ' /proc/stat))
    PID_STAT_BEFORE=($(grep ' ' /proc/$MAIN_PID/stat))
    IO_STAT_BEFORE_READ=$(awk '/rchar/ {print $2}' /proc/$MAIN_PID/io)
    IO_STAT_BEFORE_WRITE=$(awk '/wchar/ {print $2}' /proc/$MAIN_PID/io)
    sleep ${INTERVAL}
    CPU_AFTER=($(grep 'cpu ' /proc/stat))
    PID_STAT_AFTER=($(grep ' ' /proc/$MAIN_PID/stat))
    IO_STAT_AFTER_READ=$(awk '/rchar/ {print $2}' /proc/$MAIN_PID/io)
    IO_STAT_AFTER_WRITE=$(awk '/wchar/ {print $2}' /proc/$MAIN_PID/io)

    # 计算CPU使用率
    CPU_USAGE=$(awk -v b_utime=${PID_STAT_BEFORE[13]} -v b_stime=${PID_STAT_BEFORE[14]} \
        -v a_utime=${PID_STAT_AFTER[13]} -v a_stime=${PID_STAT_AFTER[14]} \
        -v b_total=${CPU_BEFORE[1]} -v b_nice=${CPU_BEFORE[2]} \
        -v b_system=${CPU_BEFORE[3]} -v b_idle=${CPU_BEFORE[4]} \
        -v a_total=${CPU_AFTER[1]} -v a_nice=${CPU_AFTER[2]} \
        -v a_system=${CPU_AFTER[3]} -v a_idle=${CPU_AFTER[4]} \
        'BEGIN {
                        total_before = b_total + b_nice + b_system + b_idle;
                        total_after = a_total + a_nice + a_system + a_idle;
                        process_before = b_utime + b_stime;
                        process_after = a_utime + a_stime;
                        cpu_usage = (process_after - process_before) / (total_after - total_before) * 100;
                        print cpu_usage;
                     }')

    RSS=$(awk '/VmRSS/ {print $2}' /proc/$MAIN_PID/status)

    CPU_TIME=$((${PID_STAT_AFTER[13]} + ${PID_STAT_AFTER[14]}))
    HZ=$(getconf CLK_TCK)
    CPU_TIME_SECONDS=$(awk -v jiffies=$CPU_TIME -v hz=$HZ 'BEGIN {print jiffies / hz}')

    IO_READ=$(((IO_STAT_AFTER_READ - IO_STAT_BEFORE_READ) / 1024))
    IO_WRITE=$(((IO_STAT_AFTER_WRITE - IO_STAT_BEFORE_WRITE) / 1024))

    NET_STATS_AFTER=($(get_network_stats))
    RX_AFTER=${NET_STATS_AFTER[0]}
    TX_AFTER=${NET_STATS_AFTER[1]}

    NET_RX=$((RX_AFTER - RX_BEFORE))
    NET_TX=$((TX_AFTER - TX_BEFORE))

    RX_BEFORE=$RX_AFTER
    TX_BEFORE=$TX_AFTER

    echo "$TIMESTAMP, $CPU_USAGE, $RSS, $CPU_TIME, $CPU_TIME_SECONDS, $IO_READ, $IO_WRITE, $IO_STAT_AFTER_READ, $IO_STAT_AFTER_WRITE, $NET_RX, $NET_TX, $RX_AFTER, $TX_AFTER" >>$LOGFILE
done

log_latest_stats

END_TIME=$(date +"%Y-%m-%d %H:%M:%S")
echo "$PROCESS_NAME with MAIN_PID $MAIN_PID has terminated at $END_TIME." >>$LOGFILE
