#!/bin/sh

EXPECTED_T1M_DEVICES=20
EXPECTED_T1C_DEVICES=12

device_list=$(busctl tree xyz.openbmc_project.PLDM | grep '/xyz/openbmc_project/inventory/Item/Board/' | wc -l)
if [ "$device_list" -ne "$EXPECTED_T1M_DEVICES" ] && [ "$device_list" -ne "$EXPECTED_T1C_DEVICES" ]; then
    echo "Restarting pldmd due to incomplete device"
    systemctl restart pldmd
fi
