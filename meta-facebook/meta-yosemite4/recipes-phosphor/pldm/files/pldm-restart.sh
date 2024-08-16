#!/bin/sh

EXPECTED_T1M_DEVICES=20
EXPECTED_T1C_DEVICES=12

retry_count=0
max_retries=3
delay=30

while [ $retry_count -lt $max_retries ]; do
    device_list=$(busctl tree xyz.openbmc_project.PLDM | grep '/xyz/openbmc_project/inventory/Item/Board/' | wc -l)

    if [ "$device_list" -eq "$EXPECTED_T1M_DEVICES" ] || [ "$device_list" -eq "$EXPECTED_T1C_DEVICES" ]; then
        exit 0
    fi

    systemctl stop pldmd
    sleep 5
    systemctl start pldmd
    retry_count=$((retry_count + 1))

    if [ $retry_count -lt $max_retries ]; then
        sleep $delay
    fi
done

