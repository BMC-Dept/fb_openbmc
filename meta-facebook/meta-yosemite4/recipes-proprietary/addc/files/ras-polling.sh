#!/bin/bash

# Check if the argument is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <slot_id>"
    exit 1
fi

# Assign the first argument to the variable eid
eid=$(($1 * 10))
slot=$1

# Infinite loop to execute the command every second
while true; do
    # Execute the pldmtool command for getting RasStatus
    response=$(pldmtool raw -m "$eid" -d 0x80 0x3f 0x01 0x15 0xA0 0x00 0xe0 0x2c 0x15 0xa0 0x00 0x00 0x4c)
    # Get the second line of response(first line is Tx and second line is Rx)
    second_line=$(echo "$response" | awk 'NR==2')
    # The last byte represent the value of RasStatus
    last_byte=$(echo "$second_line" | awk '{print $16}')
    # Check the last byte. If it is not "00", it means error occurred.
    if [[ -n "$last_byte" && "$last_byte" != "00" ]]; then
        echo "RasStatus != 00. Start dumping CPU register..."
        amd-ras --fru "$slot" --ncpu 1 --cid 0 0 0 65 117 116 104 170 0 0 0 117 116 104 128
        echo "Done. Please check /var/lib/amd-ras/ for result."
    fi

    # Sleep for 1 second
    sleep 1
done
